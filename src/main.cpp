#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Zelo"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Zelo"));
    window.resize(1000, 640);
    window.show();

    return QApplication::exec();
}
