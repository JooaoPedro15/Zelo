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
#include <QTextStream>
#include <QTimer>

namespace {

/// Mostra exatamente o que a limpeza removeria, sem remover nada.
///
/// Existe por dois motivos. Para o usuario, e a forma de conferir a lista antes
/// de confiar no botao. Para o projeto, e o unico jeito honesto de verificar o
/// caminho da limpeza sem apagar arquivo de alguem durante o desenvolvimento.
int simulate_cleanup() {
    QTextStream out(stdout);

    const auto protected_paths =
        zelo::collectors::build_protected_paths(zelo::collectors::collect_system_paths());

    const zelo::collectors::TemporaryFilesCollector collector{protected_paths};

    out << "Pastas de temporarios consideradas:\n";
    for (const auto& folder : collector.folders()) {
        out << "  " << QString::fromStdString(folder.string()) << "\n";
    }

    std::vector<std::string> paths;
    for (const auto& file : collector.list_files()) {
        paths.push_back(file.string());
    }

    const zelo::storage::QuarantineStore quarantine{
        zelo::storage::default_data_directory() / "quarentena", protected_paths};
    const zelo::storage::CleanupService cleanup{quarantine, protected_paths};

    const auto plan = cleanup.plan(paths, "storage.excessive-temporary-files");

    out << "\nSeriam removidos " << plan.items.size() << " arquivos, liberando "
        << QString::fromStdString(zelo::core::format_bytes(plan.total_bytes())) << ".\n";

    out << "\nOs dez maiores:\n";
    auto largest = plan.items;
    std::sort(largest.begin(), largest.end(),
              [](const zelo::core::CleanupItem& left, const zelo::core::CleanupItem& right) {
                  return left.size_bytes > right.size_bytes;
              });
    for (std::size_t index = 0; index < std::min<std::size_t>(10, largest.size()); ++index) {
        out << "  " << QString::fromStdString(zelo::core::format_bytes(largest[index].size_bytes))
            << "\t" << QString::fromStdString(largest[index].path) << "\n";
    }

    if (!plan.rejected.empty()) {
        out << "\nRecusados (" << plan.rejected.size() << "):\n";
        for (std::size_t index = 0; index < std::min<std::size_t>(5, plan.rejected.size());
             ++index) {
            out << "  " << QString::fromStdString(plan.rejected[index]) << "\n";
        }
    }

    out << "\nNenhum arquivo foi removido: isto e apenas uma simulacao.\n";
    return 0;
}

}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Zelo"));
    QApplication::setApplicationVersion(QStringLiteral("0.2.0"));
    QApplication::setOrganizationName(QStringLiteral("Zelo"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Zelo — analisa e explica a saude do seu Windows. Alteracoes so acontecem "
                       "com autorizacao explicita, e o que sai vai para a quarentena."));
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

    const auto logs = zelo::storage::default_data_directory() / "logs";
    zelo::storage::initialize_logging(logs);
    zelo::storage::apply_log_retention(logs, 30);
    spdlog::info("Zelo {} iniciado", QApplication::applicationVersion().toStdString());

    if (parser.isSet(simulate)) {
        return simulate_cleanup();
    }

    zelo::ui::MainWindow window;
    window.show();

    if (parser.isSet(analyze_on_start)) {
        // Depois da janela aparecer, para o usuario ver o progresso em vez de
        // encarar uma tela congelada durante a varredura.
        QTimer::singleShot(0, &window, &zelo::ui::MainWindow::start_analysis);
    }

    return QApplication::exec();
}
