#include <collectors/known_locations.hpp>
#include <collectors/system_paths.hpp>
#include <collectors/temporary_files_collector.hpp>
#include <core/rules/format.hpp>
#include <storage/cleanup_service.hpp>
#include <storage/history_store.hpp>
#include <storage/logging.hpp>
#include <storage/quarantine_store.hpp>
#include <ui/main_window.hpp>

#include <spdlog/spdlog.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QTimer>

#include <windows.h>

#include <shellapi.h>

namespace {

/// Mostra exatamente o que a limpeza removeria, sem remover nada.
///
/// Existe por dois motivos. Para o usuario, e a forma de conferir a lista antes
/// de confiar no botao. Para o projeto, e o unico jeito honesto de verificar o
/// caminho da limpeza sem apagar arquivo de alguem durante o desenvolvimento.
int simulate_cleanup() {
    // O relatorio vai para arquivo, nao para a tela. O programa e compilado
    // como aplicativo grafico para abrir sem console, e nesse modo a saida
    // padrao nao chega de volta a quem chamou. Arquivo funciona igual seja
    // pelo terminal ou por um atalho, e ainda fica guardado para consulta.
    const auto report_path =
        cleaner::storage::default_data_directory() / "simulacao-limpeza.txt";

    std::error_code error;
    std::filesystem::create_directories(report_path.parent_path(), error);

    QFile report(QString::fromStdWString(report_path.wstring()));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, QStringLiteral("Simulacao"),
                             QStringLiteral("Nao foi possivel gravar o relatorio em %1.")
                                 .arg(QString::fromStdWString(report_path.wstring())));
        return 1;
    }

    QTextStream out(&report);

    const auto protected_paths =
        cleaner::collectors::build_protected_paths(cleaner::collectors::collect_system_paths());

    const cleaner::storage::QuarantineStore quarantine{
        cleaner::storage::default_data_directory() / "quarentena", protected_paths};
    const cleaner::storage::CleanupService cleanup{quarantine, protected_paths};

    const cleaner::collectors::ReclaimableCollector reclaimable{protected_paths};

    std::uint64_t grand_total = 0;
    std::size_t grand_count = 0;

    for (const auto& location : reclaimable.collect().locations) {
        if (!location.present || location.size_bytes == 0) {
            continue;
        }

        const auto plan = cleanup.plan_folder(location.path, location.id);

        // Local que a analise mediu mas o plano nao alcancou precisa aparecer
        // com o motivo. Pular em silencio esconderia justamente o caso em que o
        // Cleaner promete espaco que nao consegue liberar.
        if (plan.empty()) {
            out << "\n" << QString::fromStdString(location.display_name) << "\n"
                << "  " << QString::fromStdString(location.path) << "\n"
                << "  a analise mediu "
                << QString::fromStdString(cleaner::core::format_bytes(location.size_bytes))
                << ", mas nenhum arquivo entrou no plano\n";

            for (const auto& reason : plan.rejected) {
                out << "  motivo: " << QString::fromStdString(reason) << "\n";
            }
            continue;
        }

        grand_total += plan.total_bytes();
        grand_count += plan.items.size();

        out << "\n" << QString::fromStdString(location.display_name) << "\n"
            << "  " << QString::fromStdString(location.path) << "\n"
            << "  " << plan.items.size() << " arquivos, "
            << QString::fromStdString(cleaner::core::format_bytes(plan.total_bytes())) << "\n"
            << "  o que voce perde: " << QString::fromStdString(location.what_you_lose) << "\n";
    }

    // Os temporarios vem de um coletor proprio, que ja sabe quais pastas o
    // sistema usa para isso.
    const cleaner::collectors::TemporaryFilesCollector temporary{protected_paths};

    std::vector<std::string> temporary_paths;
    for (const auto& file : temporary.list_files()) {
        temporary_paths.push_back(file.string());
    }

    if (const auto plan = cleanup.plan(temporary_paths, "storage.excessive-temporary-files");
        !plan.empty()) {
        grand_total += plan.total_bytes();
        grand_count += plan.items.size();

        out << "\nArquivos temporarios do sistema\n"
            << "  " << plan.items.size() << " arquivos, "
            << QString::fromStdString(cleaner::core::format_bytes(plan.total_bytes())) << "\n";
    }

    out << "\n----------------------------------------\n"
        << "Total: " << grand_count << " arquivos, "
        << QString::fromStdString(cleaner::core::format_bytes(grand_total)) << "\n"
        << "\nNenhum arquivo foi removido: isto e apenas uma simulacao.\n";
    out.flush();
    report.close();

    ::ShellExecuteW(nullptr, L"open", report_path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    spdlog::info("simulacao gravada em {}", report_path.string());
    return 0;
}

}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Cleaner"));
    QApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QApplication::setOrganizationName(QStringLiteral("Cleaner"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Cleaner — analisa, explica e acompanha o espaco do seu Windows. "
                       "Alteracoes so acontecem com autorizacao explicita, e ficam registradas."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption analyze_on_start(
        QStringList{QStringLiteral("a"), QStringLiteral("analisar")},
        QStringLiteral("Ja abre executando a analise."));
    parser.addOption(analyze_on_start);

    const QCommandLineOption simulate(
        QStringList{QStringLiteral("s"), QStringLiteral("simular-limpeza")},
        QStringLiteral("Lista o que a limpeza removeria e encerra, sem remover nada."));
    parser.addOption(simulate);
    parser.process(app);

    // Antes de qualquer coisa tocar a pasta de dados: o programa ja se chamou
    // Zelo, e o historico gravado com o nome antigo continua sendo do usuario.
    const bool adotou = cleaner::storage::adopt_previous_data_directory("Zelo");

    const auto logs = cleaner::storage::default_data_directory() / "logs";
    cleaner::storage::initialize_logging(logs);
    cleaner::storage::apply_log_retention(logs, 30);
    spdlog::info("Cleaner {} iniciado", QApplication::applicationVersion().toStdString());

    if (adotou) {
        spdlog::info("dados do nome anterior adotados em {}",
                     cleaner::storage::default_data_directory().string());
    }

    if (parser.isSet(simulate)) {
        return simulate_cleanup();
    }

    cleaner::ui::MainWindow window;
    window.show();

    if (parser.isSet(analyze_on_start)) {
        // Depois da janela aparecer, para o usuario ver o progresso em vez de
        // encarar uma tela congelada durante a varredura.
        QTimer::singleShot(0, &window, &cleaner::ui::MainWindow::start_analysis);
    }

    return QApplication::exec();
}
