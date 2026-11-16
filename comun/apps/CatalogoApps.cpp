#include "CatalogoApps.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace apps {

Sistema sistemaActual()
{
#if defined(Q_OS_ANDROID)
    return Sistema::Android;
#elif defined(Q_OS_WIN)
    return Sistema::Windows;
#else
    return Sistema::Linux;
#endif
}

QString InfoApp::archivo(Sistema s) const
{
    switch (s) {
    case Sistema::Linux: return archivoLinux;
    case Sistema::Android: return archivoAndroid;
    case Sistema::Windows: return archivoWindows;
    }
    return {};
}

namespace {

// Paquete de Arch: <nombre>-<pkgver>-<pkgrel>-x86_64.pkg.tar.zst. Se captura «pkgver-pkgrel».
QString paqueteArch(const QString &nombre)
{
    return QStringLiteral(R"(^%1-(\d+\.\d+\.\d+-\d+)-x86_64\.pkg\.tar\.zst$)").arg(QRegularExpression::escape(nombre));
}

QList<InfoApp> armarCatalogo()
{
    const QString usuario = QStringLiteral("Sebas-Git-Tail/");
    QList<InfoApp> lista;

    InfoApp a;
    a.id = QStringLiteral("Arch-Linux-Manager");
    a.nombre = QStringLiteral("Arch Linux Manager");
    a.descripcion = QStringLiteral("Respaldos, tutoriales, Minecraft, Steam y descargas para Arch Linux.");
    // El paquete del servidor es el mismo que instala pacman desde el repositorio de la casa
    a.fuentes = {{Fuente::Servidor, QStringLiteral("/Repo/")}, {Fuente::GitHub, usuario + a.id}};
    a.archivoLinux = paqueteArch(QStringLiteral("Arch-Linux-Manager"));
    a.paqueteLinux = QStringLiteral("Arch-Linux-Manager");
    lista << a;

    InfoApp g;
    g.id = QStringLiteral("gestor-stock");
    g.nombre = QStringLiteral("Gestor de stock");
    g.descripcion = QStringLiteral("Productos, precios y ventas del negocio, con la app del celular sincronizada.");
    g.fuentes = {{Fuente::GitHub, usuario + QStringLiteral("gestor-stock-cpp")}};
    g.archivoLinux = paqueteArch(QStringLiteral("gestor-stock"));
    g.archivoAndroid = QStringLiteral(R"(^gestor-stock-movil-(\d+\.\d+\.\d+)\.apk$)");
    g.paqueteLinux = QStringLiteral("gestor-stock");
    g.paqueteAndroid = QStringLiteral("ar.gestorstock.app");
    lista << g;

    InfoApp i;
    i.id = QStringLiteral("ingresos-gastos");
    i.nombre = QStringLiteral("Ingresos y gastos");
    i.descripcion = QStringLiteral("Ingresos, gastos, cuentas y presupuestos, sincronizado entre la PC y el celular.");
    i.fuentes = {{Fuente::GitHub, usuario + QStringLiteral("ingresos-gastos")}};
    i.archivoLinux = paqueteArch(QStringLiteral("ingresos-gastos"));
    i.archivoAndroid = QStringLiteral(R"(^ingresos-gastos-movil-(\d+\.\d+\.\d+)\.apk$)");
    i.paqueteLinux = QStringLiteral("ingresos-gastos");
    i.paqueteAndroid = QStringLiteral("ar.ingresosgastos.app");
    lista << i;

    InfoApp am;
    am.id = QStringLiteral("Android-Manager");
    am.nombre = QStringLiteral("Android Manager");
    am.descripcion = QStringLiteral("Descargas de videos y música y respaldo del celular.");
    am.fuentes = {{Fuente::GitHub, usuario + am.id}};
    am.archivoAndroid = QStringLiteral(R"(^Android-Manager-(\d+\.\d+\.\d+)\.apk$)");
    am.paqueteAndroid = QStringLiteral("com.sebastian.androidmanager");
    lista << am;

    InfoApp w;
    w.id = QStringLiteral("Windows-Manager");
    w.nombre = QStringLiteral("Windows Manager");
    w.descripcion = QStringLiteral("Descargas de videos y música y respaldos en Windows.");
    w.fuentes = {{Fuente::GitHub, usuario + w.id}};
    w.archivoWindows = QStringLiteral(R"(^Windows-Manager-(\d+\.\d+\.\d+)-instalador\.exe$)");
    w.appIdWindows = QStringLiteral("{6B1E6F1C-55D5-4E0B-9C1A-3F1B7E2D9A41}");
    lista << w;

    // ---- Ocultas: solo se actualizan solas, nunca se listan

    InfoApp b;
    b.id = QStringLiteral("Bodrio-Supremo");
    b.nombre = QStringLiteral("Bodrio Supremo");
    b.oculta = true;
    b.fuentes = {{Fuente::Servidor, QStringLiteral("/Bodrio-Supremo/")}};
    b.archivoLinux = paqueteArch(QStringLiteral("bodrio-supremo"));
    b.archivoAndroid = QStringLiteral(R"(^Bodrio-Supremo-(\d+\.\d+\.\d+)\.apk$)");
    b.archivoWindows = QStringLiteral(R"(^Bodrio-Supremo-(\d+\.\d+\.\d+)-instalador\.exe$)");
    b.paqueteLinux = QStringLiteral("bodrio-supremo");
    b.paqueteAndroid = QStringLiteral("com.sebastian.bodriosupremo");
    b.appIdWindows = QStringLiteral("{9C4B2E71-3A5D-4E8F-B6A2-7D1F0C9E5B34}");
    lista << b;

    InfoApp c;
    c.id = QStringLiteral("Carina");
    c.nombre = QStringLiteral("Carina");
    c.oculta = true;
    c.fuentes = {{Fuente::Servidor, QStringLiteral("/Carina/")}};
    c.archivoWindows = QStringLiteral(R"(^Carina-(\d+\.\d+\.\d+)-instalador\.exe$)");
    c.appIdWindows = QStringLiteral("{3E1A7C52-9B4D-4F6A-8C21-5D0B9E7F4A13}");
    lista << c;

    return lista;
}

} // namespace

const QList<InfoApp> &catalogo()
{
    static const QList<InfoApp> lista = armarCatalogo();
    return lista;
}

const InfoApp *buscar(const QString &id)
{
    for (const InfoApp &a : catalogo())
        if (a.id == id)
            return &a;
    return nullptr;
}

QList<InfoApp> paraInstalar(const QString &estaApp, Sistema s)
{
    QList<InfoApp> lista;
    for (const InfoApp &a : catalogo())
        if (!a.oculta && a.id != estaApp && a.disponibleEn(s))
            lista << a;
    return lista;
}

QVersionNumber versionDeTexto(const QString &texto)
{
    QString t = texto.trimmed();
    if (t.startsWith(u'v') || t.startsWith(u'V'))
        t.remove(0, 1);
    t.replace(u'-', u'.');
    static const QRegularExpression soloNumeros(QStringLiteral(R"(^\d+(\.\d+)*$)"));
    if (!soloNumeros.match(t).hasMatch())
        return {};
    return QVersionNumber::fromString(t);
}

Candidato elegir(const ListaArchivos &archivos, const QString &patron)
{
    Candidato mejor;
    if (patron.isEmpty())
        return mejor;
    const QRegularExpression re(patron);
    for (const auto &[nombre, url] : archivos) {
        const QRegularExpressionMatch m = re.match(nombre);
        if (!m.hasMatch())
            continue;
        const QVersionNumber v = versionDeTexto(m.captured(1));
        if (!v.isNull() && (mejor.version.isNull() || v > mejor.version))
            mejor = {nombre, url, v};
    }
    return mejor;
}

ListaArchivos archivosDeGitHub(const QByteArray &json)
{
    ListaArchivos lista;
    const QJsonObject release = QJsonDocument::fromJson(json).object();
    if (release.value(QLatin1String("draft")).toBool() || release.value(QLatin1String("prerelease")).toBool())
        return lista;
    for (const QJsonValue &v : release.value(QLatin1String("assets")).toArray()) {
        const QJsonObject o = v.toObject();
        const QUrl url(o.value(QLatin1String("browser_download_url")).toString());
        // Solo descargas de GitHub por HTTPS
        if (url.scheme() == QLatin1String("https") && url.host() == QLatin1String("github.com"))
            lista.append({o.value(QLatin1String("name")).toString(), url});
    }
    return lista;
}

ListaArchivos archivosDeServidor(const QByteArray &json, const QUrl &carpeta)
{
    ListaArchivos lista;
    for (const QJsonValue &v : QJsonDocument::fromJson(json).array()) {
        const QJsonObject o = v.toObject();
        if (o.value(QLatin1String("carpeta")).toBool())
            continue;
        const QString nombre = o.value(QLatin1String("nombre")).toString();
        if (nombre.isEmpty() || nombre.contains(u'/') || nombre.contains(u'\\'))
            continue;
        lista.append({nombre, carpeta.resolved(QUrl(QString::fromLatin1(QUrl::toPercentEncoding(nombre))))});
    }
    return lista;
}

QVersionNumber versionDePacman(const QString &salida)
{
    const QStringList partes = salida.trimmed().split(u' ', Qt::SkipEmptyParts);
    if (partes.size() != 2)
        return {};
    QString v = partes.at(1);
    const int dosPuntos = v.indexOf(u':');          // época («1:2.0-1»): se ignora
    if (dosPuntos >= 0)
        v = v.mid(dosPuntos + 1);
    return versionDeTexto(v);
}

} // namespace apps
