// Gestor de Stock para Android (Qt Quick). En la PC también se puede ejecutar, para probarlo.
#include "controlador.h"
#include "db.h"
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(GestorStockPlugin)

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("gestor-stock"));
    QGuiApplication::setApplicationName(QStringLiteral("gestor-stock-movil"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Gestor de Stock"));
    QGuiApplication::setApplicationVersion(QStringLiteral(GS_VERSION));
    QLocale::setDefault(QLocale(QLocale::Spanish, QLocale::Argentina));
    QQuickStyle::setStyle(QStringLiteral("Material"));

#ifdef Q_OS_ANDROID
    const QString ruta = Stock::rutaPorDefecto();
#else
    // Probándolo en la PC: una base aparte, para no mezclarse con la del programa de escritorio
    const QStringList args = app.arguments();
    const QString ruta = args.size() > 1 ? args.at(1)
                                         : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/stock.db");
#endif
    Stock stock(ruta);
    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    if (!stock.abierta()) {
        engine.loadData(QStringLiteral("import QtQuick; import QtQuick.Controls; ApplicationWindow { visible: true; width: 400; height: 300; "
                                       "Label { anchors.fill: parent; anchors.margins: 20; wrapMode: Text.WordWrap; text: %1 } }")
                            .arg(QStringLiteral("\"No se pudo abrir la base de datos: ") + QString(stock.errorApertura()).replace('"', '\'') + "\"")
                            .toUtf8());
        return app.exec();
    }

    Controlador controlador(stock);
    Controlador::setInstancia(&controlador);
    engine.load(QUrl(QStringLiteral("qrc:/GestorStock/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
