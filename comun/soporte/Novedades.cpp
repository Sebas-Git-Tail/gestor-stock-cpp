#include "Novedades.h"

#include "Fallas.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace soporte {

namespace {

const QString ClaveVista = QStringLiteral("novedades/vista");
const QString ClaveDesarrollador = QStringLiteral("soporte/desarrollador");

QVersionNumber versionActual()
{
    return QVersionNumber::fromString(QCoreApplication::applicationVersion()).normalized();
}

}

QString archivoDesarrollador()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("comun-qt/desarrollador"));
}

bool esDesarrollador()
{
    const QByteArray forzado = qgetenv("COMUN_DESARROLLADOR");
    if (!forzado.isEmpty())
        return forzado != "0";
#ifdef Q_OS_ANDROID
    return QSettings().value(ClaveDesarrollador, false).toBool();
#else
    return QFileInfo::exists(archivoDesarrollador());
#endif
}

void ponerDesarrollador(bool si)
{
#ifdef Q_OS_ANDROID
    QSettings().setValue(ClaveDesarrollador, si);
#else
    const QString ruta = archivoDesarrollador();
    if (si) {
        QDir().mkpath(QFileInfo(ruta).path());
        QFile archivo(ruta);
        if (archivo.open(QIODevice::WriteOnly))
            archivo.write("Este equipo es de Sebastián: las apps muestran también las novedades marcadas [Sebastián].\n");
    } else {
        QFile::remove(ruta);
    }
#endif
}

QList<VersionNovedades> leerNovedades(const QString &texto, bool desarrollador)
{
    static const QRegularExpression titulo(QStringLiteral(R"(^##\s+v?(\d+(?:\.\d+)*)\s*(?:\((.*)\))?\s*$)"));
    static const QRegularExpression item(QStringLiteral(R"(^\s{0,3}[-*]\s+(.*)$)"));
    static const QRegularExpression marca(QStringLiteral(R"(^\[sebasti[aá]n\]\s*)"),
                                          QRegularExpression::CaseInsensitiveOption);

    QList<VersionNovedades> versiones;
    VersionNovedades *actual = nullptr;
    bool enCambio = false;
    for (const QString &linea : texto.split(u'\n')) {
        const QString limpia = linea.trimmed();
        if (limpia.startsWith(u'#')) {
            enCambio = false;
            const auto m = titulo.match(limpia);
            if (m.hasMatch()) {
                VersionNovedades v;
                v.textoVersion = m.captured(1);
                v.version = QVersionNumber::fromString(v.textoVersion).normalized();
                v.fecha = m.captured(2).trimmed();
                versiones << v;
                actual = &versiones.last();
            } else {
                actual = nullptr;      // otro título (el de arriba de todo): lo que sigue no es de ninguna versión
            }
            continue;
        }
        if (!actual)
            continue;
        const auto m = item.match(linea);
        if (m.hasMatch()) {
            QString cambio = m.captured(1).trimmed();
            const auto mm = marca.match(cambio);
            const bool solo = mm.hasMatch();
            if (solo)
                cambio.remove(0, mm.capturedLength());
            actual->cambios << cambio;
            actual->soloDesarrollador << solo;
            enCambio = true;
        } else if (limpia.isEmpty()) {
            enCambio = false;
        } else if (enCambio && !actual->cambios.isEmpty()) {
            actual->cambios.last() += u' ' + limpia;      // el cambio sigue en la línea de abajo
        }
    }

    QList<VersionNovedades> resultado;
    for (VersionNovedades &v : versiones) {
        if (!desarrollador) {
            VersionNovedades filtrada = v;
            filtrada.cambios.clear();
            filtrada.soloDesarrollador.clear();
            for (int i = 0; i < v.cambios.size(); ++i)
                if (!v.soloDesarrollador.at(i)) {
                    filtrada.cambios << v.cambios.at(i);
                    filtrada.soloDesarrollador << false;
                }
            v = filtrada;
        }
        if (!v.cambios.isEmpty() && !v.version.isNull())
            resultado << v;
    }
    std::stable_sort(resultado.begin(), resultado.end(),
                     [](const VersionNovedades &a, const VersionNovedades &b) { return a.version > b.version; });
    return resultado;
}

QList<VersionNovedades> nuevasDesde(const QList<VersionNovedades> &todas, const QVersionNumber &vistaDada,
                                    const QVersionNumber &actualDada)
{
    // Todo normalizado: para QVersionNumber «2.1» es menor que «2.1.0»
    const QVersionNumber vista = vistaDada.normalized(), actual = actualDada.normalized();
    QList<VersionNovedades> nuevas;
    for (const VersionNovedades &v : todas) {
        if (v.version > actual)
            continue;                                 // escrita para una versión que todavía no salió
        if (vista.isNull() ? v.version == actual : v.version > vista)
            nuevas << v;
    }
    return nuevas;
}

QString aMarkdown(const QList<VersionNovedades> &versiones, bool marcarDesarrollador)
{
    QStringList partes;
    for (const VersionNovedades &v : versiones) {
        QString parte = QStringLiteral("**Versión %1**").arg(v.textoVersion);
        if (!v.fecha.isEmpty())
            parte += QStringLiteral(" · ") + v.fecha;
        parte += QStringLiteral("\n\n");
        for (int i = 0; i < v.cambios.size(); ++i) {
            const bool solo = v.soloDesarrollador.value(i);
            parte += QStringLiteral("- ") + (solo && marcarDesarrollador ? QStringLiteral("*(solo vos)* ") : QString())
                     + v.cambios.at(i) + u'\n';
        }
        partes << parte;
    }
    return partes.join(u'\n');
}

Novedades::Novedades(QObject *parent, const QString &archivo, QSettings *ajustes)
    : QObject(parent), archivo_(archivo), ajustes_(ajustes ? ajustes : new QSettings(this))
{
    QFile f(archivo_);
    if (f.open(QIODevice::ReadOnly))
        texto_ = QString::fromUtf8(f.readAll());
    cargar();
}

void Novedades::cargar()
{
    desarrollador_ = esDesarrollador();
    todas_ = leerNovedades(texto_, desarrollador_);
    const QVersionNumber actual = versionActual();
    const QVersionNumber vista = QVersionNumber::fromString(ajustes_->value(ClaveVista).toString()).normalized();
    nuevas_ = (!vista.isNull() && vista >= actual) ? QList<VersionNovedades>{} : nuevasDesde(todas_, vista, actual);
    if (nuevas_.isEmpty() && vista != actual && !actual.isNull())
        ajustes_->setValue(ClaveVista, QCoreApplication::applicationVersion());   // nada para mostrar: queda vista
    emit cambio();
}

QString Novedades::textoNuevas() const
{
    return aMarkdown(nuevas_, desarrollador_);
}

QString Novedades::textoTodas() const
{
    const QVersionNumber actual = versionActual();
    QList<VersionNovedades> hasta;
    for (const VersionNovedades &v : todas_)
        if (v.version <= actual)
            hasta << v;
    return aMarkdown(hasta, desarrollador_);
}

QString Novedades::version() const
{
    return QCoreApplication::applicationVersion();
}

void Novedades::marcarVistas()
{
    const QVersionNumber actual = versionActual();
    if (!actual.isNull())
        ajustes_->setValue(ClaveVista, QCoreApplication::applicationVersion());
    if (!nuevas_.isEmpty()) {
        nuevas_.clear();
        emit cambio();
    }
}

void Novedades::recargar()
{
    cargar();
}

void Novedades::alternarDesarrollador()
{
    ponerDesarrollador(!esDesarrollador());
    const bool habiaNuevas = !nuevas_.isEmpty();
    cargar();
    if (!habiaNuevas)
        nuevas_.clear();       // al cambiar el modo no se vuelven a ofrecer al abrir
    emit cambio();
}

void Novedades::enviarInforme(const QString &comentario)
{
    if (enviandoInforme_)
        return;
    enviandoInforme_ = true;
    emit enviandoInformeCambio();
    auto *conexion = new QMetaObject::Connection;
    *conexion = connect(&Fallas::instancia(), &Fallas::enviadoManual, this, [this, conexion](bool ok) {
        disconnect(*conexion);
        delete conexion;
        enviandoInforme_ = false;
        emit enviandoInformeCambio();
        emit informeEnviado(ok);
    });
    Fallas::instancia().enviarManual(comentario);
}

} // namespace soporte
