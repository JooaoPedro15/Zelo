#include <ui/main_window.hpp>

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Zelo"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("Zelo"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Zelo — analisa e explica a saude do seu Windows. Esta versao apenas "
                       "observa: nenhuma alteracao e feita no computador."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption analyze_on_start(
        QStringList{QStringLiteral("a"), QStringLiteral("analisar")},
        QStringLiteral("Ja abre executando a analise."));
    parser.addOption(analyze_on_start);
    parser.process(app);

    zelo::ui::MainWindow window;
    window.show();

    if (parser.isSet(analyze_on_start)) {
        // Depois da janela aparecer, para o usuario ver o progresso em vez de
        // encarar uma tela congelada durante a varredura.
        QTimer::singleShot(0, &window, &zelo::ui::MainWindow::start_analysis);
    }

    return QApplication::exec();
}
