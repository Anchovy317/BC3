#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("bc3_visor"));
    QApplication::setOrganizationName(QStringLiteral("obra"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("json"), QStringLiteral("JSON de bc3_extraer.py o proyecto .bc3proj"));
    parser.process(app);

    MainWindow ventana;
    const QStringList pos = parser.positionalArguments();
    if (!pos.isEmpty())
        ventana.cargarRuta(pos.first());
    ventana.show();
    return app.exec();
}
