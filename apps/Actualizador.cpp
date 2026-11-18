#include "Actualizador.h"

#include "ClienteHttp.h"
#include "ConfigServidor.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#else
#include <QNetworkReply>
#endif
#if defined(Q_OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif

namespace apps {

namespace {
#if defined(Q_OS_ANDROID)
const char *const CLASE_JAVA = "com/sebastian/comun/Instalador";
// Devuelve el QJniObject y no el jobject: si se devolviera context().object<jobject>(), el QJniObject
// temporal se destruye al salir de la función, libera la referencia y Java recibe un puntero muerto
// (SIGSEGV en Instalador.version al abrir «Apps y actualizaciones»).
QJniObject contexto() { return QNativeInterface::QAndroidApplication::context(); }
#endif
} // namespace

QStringList raicesServidor()
{
    // La dirección pública primero y después las de los videos de Bodrio (Tailscale), sin repetir
    QStringList raices;
    auto agregar = [&raices](const QUrl &url) {
        if (!url.isValid() || url.host().isEmpty())
            return;
        const QString raiz = url.resolved(QUrl(QStringLiteral("/"))).toString();
        if (!raices.contains(raiz))
            raices << raiz;
    };
    const ConfigServidor &config = ConfigServidor::instancia();
    agregar(QUrl(config.webPublica()));
    for (const QString &r : config.raicesBodrio())
        agregar(QUrl(r));
    return raices;
}

QVersionNumber versionInstalada(const InfoApp &app, bool esEstaApp)
{
    QVersionNumber v;
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    // La app que está corriendo sabe su versión (y así también anda la versión portable de Windows)
    if (esEstaApp)
        return versionDeTexto(QCoreApplication::applicationVersion());
#endif
#if defined(Q_OS_ANDROID)
    if (!app.paqueteAndroid.isEmpty())
        v = versionDeTexto(QJniObject::callStaticObjectMethod(CLASE_JAVA, "version",
            "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;", contexto().object<jobject>(),
            QJniObject::fromString(app.paqueteAndroid).object<jstring>()).toString());
#elif defined(Q_OS_WIN)
    if (!app.appIdWindows.isEmpty()) {
        // Inno Setup deja la versión en Uninstall\{GUID}_is1 (del usuario o de todo el equipo)
        for (const char *raiz : {"HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE"}) {
            const QSettings registro(QStringLiteral("%1\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%2_is1")
                                         .arg(QLatin1String(raiz), app.appIdWindows), QSettings::NativeFormat);
            v = versionDeTexto(registro.value(QStringLiteral("DisplayVersion")).toString());
            if (!v.isNull())
                break;
        }
    }
#else
    if (!app.paqueteLinux.isEmpty() && !QStandardPaths::findExecutable(QStringLiteral("pacman")).isEmpty()) {
        QProcess pacman;
        pacman.start(QStringLiteral("pacman"), {QStringLiteral("-Q"), app.paqueteLinux});
        if (pacman.waitForFinished(5000) && pacman.exitStatus() == QProcess::NormalExit && pacman.exitCode() == 0)
            v = versionDePacman(QString::fromUtf8(pacman.readAllStandardOutput()));
    }
    // En Linux no se usa la versión del programa: si no lo instaló pacman (una compilación de prueba),
    // no hay nada que actualizar.
    return v;
#endif
    return v;
}

QString Actualizador::texto(const QVersionNumber &v)
{
    if (v.segmentCount() == 4)
        return QStringLiteral("%1.%2.%3-%4").arg(v.segmentAt(0)).arg(v.segmentAt(1)).arg(v.segmentAt(2)).arg(v.segmentAt(3));
    return v.toString();
}

Actualizador::Actualizador(const InfoApp &app, bool esEstaApp, QObject *parent)
    : QObject(parent), app_(app), esEstaApp_(esEstaApp)
{
}

void Actualizador::buscarUnaVezPorDia()
{
    if (ocupado_)
        return;
    const QString clave = QStringLiteral("actualizador/%1/ultima").arg(app_.id);
    const QDateTime ultima = QSettings().value(clave).toDateTime();
    tocaAvisar_ = !ultima.isValid() || ultima.secsTo(QDateTime::currentDateTime()) >= 24 * 3600;
    if (tocaAvisar_)
        connect(this, &Actualizador::encontrada, this, [clave] {
            QSettings().setValue(clave, QDateTime::currentDateTime());
        }, Qt::SingleShotConnection);
    empezarBusqueda();
}

void Actualizador::buscar()
{
    if (ocupado_)
        return;
    tocaAvisar_ = false;
    empezarBusqueda();
}

void Actualizador::empezarBusqueda()
{
    const Sistema sistema = sistemaActual();
    // COMUN_APPS_SIN_RED=1: las pruebas de las apps abren las pantallas sin salir a internet
    if (!app_.disponibleEn(sistema) || qEnvironmentVariableIntValue("COMUN_APPS_SIN_RED") == 1) {
        emit noEncontrada();
        return;
    }
#if !defined(Q_OS_WIN) && !defined(Q_OS_ANDROID)
    if (QSysInfo::currentCpuArchitecture() != QLatin1String("x86_64")) {
        emit noEncontrada();                     // en Linux solo hay paquetes para x86_64
        return;
    }
#endif
    instalada_ = versionInstalada(app_, esEstaApp_);
    disponible_ = {};
    ocupado_ = true;
    probar(0, 0);
}

void Actualizador::probar(int fuente, int raiz)
{
    if (fuente >= app_.fuentes.size()) {
        terminarBusqueda();
        return;
    }
    const Fuente &f = app_.fuentes.at(fuente);
    const QString patron = app_.archivo(sistemaActual());

    if (f.tipo == Fuente::GitHub) {
        const QUrl url(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(f.lugar));
        http::pedir(this, url, [this, fuente, patron](int codigo, const QByteArray &cuerpo) {
            if (codigo == 200)
                disponible_ = elegir(archivosDeGitHub(cuerpo), patron);
            if (disponible_.valido())
                terminarBusqueda();
            else
                probar(fuente + 1, 0);
        });
        return;
    }

    const QStringList raices = raicesServidor();
    if (raiz >= raices.size()) {
        probar(fuente + 1, 0);
        return;
    }
    const QUrl carpeta = QUrl(raices.at(raiz)).resolved(QUrl(f.lugar));
    QUrl lista = carpeta;
    lista.setQuery(QStringLiteral("formato=json"));
    http::pedir(this, lista, [this, fuente, raiz, carpeta, patron](int codigo, const QByteArray &cuerpo) {
        if (codigo != 200) {
            probar(fuente, raiz + 1);          // otra dirección del mismo servidor
            return;
        }
        disponible_ = elegir(archivosDeServidor(cuerpo, carpeta), patron);
        if (disponible_.valido())
            terminarBusqueda();
        else
            probar(fuente + 1, 0);
    });
}

void Actualizador::terminarBusqueda()
{
    ocupado_ = false;
    if (!disponible_.valido()) {
        emit noEncontrada();
        return;
    }
    emit encontrada(texto(disponible_.version));
    if (!instalada_.isNull() && disponible_.version > instalada_)
        emit hayVersionNueva(texto(disponible_.version));
}

void Actualizador::descargarEInstalar()
{
    if (!disponible_.valido() || ocupado_)
        return;
    ocupado_ = true;
    const QString version = texto(disponible_.version);
    emit mensaje(tr("Descargando %1 %2…").arg(app_.nombre, version));

#ifdef Q_OS_ANDROID
    // El APK va a la caché de la app, que es lo que comparte el FileProvider (res/xml/rutas_archivos.xml)
    const QString carpeta = QJniObject::callStaticObjectMethod(CLASE_JAVA, "carpeta",
        "(Landroid/content/Context;)Ljava/lang/String;", contexto().object<jobject>()).toString();
    const QString archivo = QDir(carpeta).filePath(disponible_.nombre);
    emit progreso(0, -1);
    // descargar() y no pedir(): pedir() pasa el cuerpo por un String de Java y el APK llegaba roto
    // («error durante el análisis del paquete»)
    http::descargar(this, disponible_.url, archivo, [this, archivo](int codigo) {
        if (codigo != 200) {
            ocupado_ = false;
            emit fallo(tr("No se pudo descargar (código %1).").arg(codigo));
            return;
        }
        instalar(archivo);
    });
#else
    const QString carpeta = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("instalar-apps"));
    QDir().mkpath(carpeta);
    const QString archivo = QDir(carpeta).filePath(disponible_.nombre);
    auto *salida = new QSaveFile(archivo, this);
    if (!salida->open(QIODevice::WriteOnly)) {
        ocupado_ = false;
        emit fallo(tr("No se pudo guardar en %1.").arg(archivo));
        salida->deleteLater();
        return;
    }
    QNetworkRequest pedido(disponible_.url);
    pedido.setTransferTimeout(60000);
    pedido.setHeader(QNetworkRequest::UserAgentHeader, QCoreApplication::applicationName());
    pedido.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *respuesta = red_.get(pedido);
    connect(respuesta, &QNetworkReply::downloadProgress, this, &Actualizador::progreso);
    connect(respuesta, &QNetworkReply::readyRead, this, [respuesta, salida] { salida->write(respuesta->readAll()); });
    connect(respuesta, &QNetworkReply::finished, this, [this, respuesta, salida, archivo] {
        respuesta->deleteLater();
        salida->deleteLater();
        if (respuesta->error() != QNetworkReply::NoError) {
            salida->cancelWriting();
            ocupado_ = false;
            emit fallo(tr("No se pudo descargar: %1.").arg(respuesta->errorString()));
            return;
        }
        salida->write(respuesta->readAll());
        if (!salida->commit()) {
            ocupado_ = false;
            emit fallo(tr("No se pudo guardar el archivo descargado."));
            return;
        }
        instalar(archivo);
    });
#endif
}

void Actualizador::instalar(const QString &archivo)
{
    const QString version = texto(disponible_.version);
#if defined(Q_OS_ANDROID)
    emit mensaje(tr("Abriendo el instalador de Android…"));
    QJniObject::callStaticMethod<void>(CLASE_JAVA, "instalar", "(Landroid/content/Context;Ljava/lang/String;)V",
        contexto().object<jobject>(), QJniObject::fromString(archivo).object<jstring>());
    ocupado_ = false;
    emit instalado(version);
#elif defined(Q_OS_WIN)
    // ShellExecute (y no QProcess) para que Windows pida permiso si el instalador lo necesita
    const QString parametros = esEstaApp_
        ? QStringLiteral("/SILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS")   // cierra la app, instala y la vuelve a abrir
        : QString();
    const auto resultado = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open",
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(archivo).utf16()),
        parametros.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(parametros.utf16()), nullptr, SW_SHOWNORMAL));
    ocupado_ = false;
    if (resultado <= 32) {
        emit fallo(tr("No se pudo abrir el instalador (%1).").arg(archivo));
        return;
    }
    if (esEstaApp_) {
        emit mensaje(tr("Instalando la versión %1… la app se va a cerrar y volver a abrir sola.").arg(version));
        QTimer::singleShot(500, qApp, &QCoreApplication::quit);
    } else {
        emit mensaje(tr("Se abrió el instalador de %1.").arg(app_.nombre));
    }
    emit instalado(version);
#else
    // pkexec pide la contraseña con la ventana del sistema y corre pacman como administrador
    if (QStandardPaths::findExecutable(QStringLiteral("pkexec")).isEmpty()) {
        ocupado_ = false;
        emit fallo(tr("Falta pkexec. Instalalo a mano: sudo pacman -U %1").arg(archivo));
        return;
    }
    emit mensaje(tr("Instalando %1 %2 (pide la contraseña)…").arg(app_.nombre, version));
    auto *pacman = new QProcess(this);
    connect(pacman, &QProcess::finished, this, [this, pacman, archivo, version](int codigo) {
        pacman->deleteLater();
        ocupado_ = false;
        if (codigo != 0) {
            emit fallo(tr("No se instaló (¿se canceló la contraseña?). A mano: sudo pacman -U %1").arg(archivo));
            return;
        }
        instalada_ = disponible_.version;
        if (esEstaApp_) {
            emit mensaje(tr("¡Actualizado a la %1! Volviendo a abrir…").arg(version));
            QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments().mid(1));
            QTimer::singleShot(300, qApp, &QCoreApplication::quit);
        } else {
            emit mensaje(tr("%1 %2 instalado.").arg(app_.nombre, version));
        }
        emit instalado(version);
    });
    pacman->start(QStringLiteral("pkexec"), {QStringLiteral("pacman"), QStringLiteral("-U"), QStringLiteral("--noconfirm"), archivo});
#endif
}

} // namespace apps
