#include "Traducciones.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

// Fuera de cualquier namespace: Q_INIT_RESOURCE declara una función global
static void iniciarRecursos() { Q_INIT_RESOURCE(traducciones); }   // el .qrc está en una biblioteca estática

namespace apps {

void cargarTraduccionesQt()
{
    iniciarRecursos();
    auto *traductor = new QTranslator(QCoreApplication::instance());
    const QLocale espanol(QLocale::Spanish);
    if (traductor->load(espanol, QStringLiteral("qtbase"), QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath))
        || traductor->load(QStringLiteral(":/comun/qtbase_es.qm")))
        QCoreApplication::installTranslator(traductor);
    else
        delete traductor;
}

} // namespace apps
