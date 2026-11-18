#include "PanelApps.h"
#include "Tema.h"
#include "Traducciones.h"
#include "db.h"
#include "ventanaprincipal.h"

#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QTranslator>
#include <memory>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    apps::cargarTraduccionesQt();   // botones estándar en español (comun/apps; Android y Windows no traen la traducción)
    QApplication::setApplicationName(QStringLiteral("gestor-stock"));
    QApplication::setApplicationDisplayName(QStringLiteral("Gestor de Stock"));
    QApplication::setApplicationVersion(QStringLiteral(GS_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("gestor-stock"));      // lo une con gestor-stock.desktop en Plasma
    tema::aplicar(app, QColor(0x3a, 0xa7, 0x6d));   // tema común (claro u oscuro) con el verde del gestor (comun/ui)
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("Logo")));

    // Botones estándar (Aceptar, Cancelar, Sí, No...) en el idioma del sistema, si está instalado qt6-translations
    QTranslator traductorQt;
    if (traductorQt.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&traductorQt);

    // Para probar sin tocar los datos de todos los días: gestor-stock /ruta/otra.db (la configuración va aparte, al lado)
    const QStringList args = app.arguments();
    const QString ruta = args.size() > 1 ? args.at(1) : Stock::rutaPorDefecto();
    Stock stock(ruta);
    if (!stock.abierta()) {
        QMessageBox::critical(nullptr, QObject::tr("No se pudo abrir la base de datos"), stock.errorApertura());
        return 1;
    }
    std::unique_ptr<QSettings> ajustes = args.size() > 1
        ? std::make_unique<QSettings>(ruta + QStringLiteral(".conf"), QSettings::IniFormat)
        : std::make_unique<QSettings>(QStringLiteral("gestor-stock"), QStringLiteral("gestor-stock"));   // ~/.config/gestor-stock/gestor-stock.conf
    VentanaPrincipal ventana(&stock, ajustes.get());
    ventana.show();
    // Una vez por día: si hay una versión nueva, pregunta y la instala (pkexec pacman -U). Acá y no en la
    // ventana, para que las pruebas (que crean la ventana) no salgan a internet.
    if (args.size() <= 1)
        apps::avisarActualizaciones(&ventana, QStringLiteral("gestor-stock"));
    return app.exec();
}
