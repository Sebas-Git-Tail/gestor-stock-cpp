#include "Fallas.h"

#include "Actualizador.h"     // raicesServidor()
#include "ClienteHttp.h"
#include "Novedades.h"        // esDesarrollador()

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace soporte {

namespace {

constexpr int MaxRenglones = 300;
constexpr int MaxLargoRenglon = 500;
constexpr int MaxPorDia = 8;
constexpr int MaxPendientes = 20;

QMutex candado;                       // protege lo de abajo (el manejador de mensajes corre en cualquier hilo)
QStringList renglones;                // los últimos MaxRenglones, ya limpios
QFile *archivoRegistro = nullptr;     // proceso-<pid>.registro: si la app muere, lo lee la próxima vez
QtMessageHandler manejadorAnterior = nullptr;
thread_local bool dentro = false;     // para no volver a entrar si algo de acá adentro escribe un mensaje
QJsonObject equipo;                   // datosEquipo(), armado una vez al iniciar (en el hilo principal)

// Para los manejadores de señales: rutas ya armadas (ahí no se puede reservar memoria)
char rutaSenal[1024];
char rutaMarca[1024];

QString rutaProceso(const QString &carpeta, qint64 pid, const char *tipo)
{
    return QDir(carpeta).filePath(QStringLiteral("proceso-%1.%2").arg(pid).arg(QLatin1String(tipo)));
}

bool procesoVivo(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    if (!h)
        return false;
    DWORD codigo = 0;
    const bool vivo = GetExitCodeProcess(h, &codigo) && codigo == STILL_ACTIVE;
    CloseHandle(h);
    return vivo;
#else
    return ::kill(pid_t(pid), 0) == 0 || errno == EPERM;
#endif
}

bool esErrorQml(const QMessageLogContext &contexto, const QString &mensaje)
{
    const bool deQml = (contexto.file && QByteArray(contexto.file).endsWith(".qml"))
                       || mensaje.contains(QStringLiteral(".qml:"))
                       || qstrcmp(contexto.category, "qml") == 0 || qstrcmp(contexto.category, "js") == 0;
    if (!deQml)
        return false;
    static const QRegularExpression grave(QStringLiteral(
        R"(TypeError|ReferenceError|SyntaxError|RangeError|is not a type|is not installed|failed to load|)"
        R"(Cannot assign|Cannot read property|Cannot call method|Unable to assign|Non-existent attached object)"));
    return grave.match(mensaje).hasMatch();
}

QString letraTipo(QtMsgType tipo)
{
    switch (tipo) {
    case QtDebugMsg: return QStringLiteral("D");
    case QtInfoMsg: return QStringLiteral("I");
    case QtWarningMsg: return QStringLiteral("W");
    case QtCriticalMsg: return QStringLiteral("C");
    case QtFatalMsg: return QStringLiteral("F");
    }
    return QStringLiteral("?");
}

void manejador(QtMsgType tipo, const QMessageLogContext &contexto, const QString &mensaje)
{
    if (manejadorAnterior)
        manejadorAnterior(tipo, contexto, mensaje);
    if (dentro || tipo == QtDebugMsg)
        return;
    dentro = true;

    QString renglon = QTime::currentTime().toString(QStringLiteral("HH:mm:ss ")) + letraTipo(tipo) + u' ';
    if (contexto.category && qstrcmp(contexto.category, "default") != 0)
        renglon += QLatin1String(contexto.category) + QStringLiteral(": ");
    renglon += limpiar(mensaje).left(MaxLargoRenglon).replace(u'\n', QStringLiteral(" | "));
    {
        QMutexLocker l(&candado);
        renglones << renglon;
        if (renglones.size() > MaxRenglones)
            renglones.remove(0, renglones.size() - MaxRenglones);
        if (archivoRegistro) {
            archivoRegistro->write(renglon.toUtf8() + '\n');
            archivoRegistro->flush();
        }
    }

    if (tipo == QtFatalMsg) {
        // La app se va a cerrar ahora mismo (abort): se guarda el informe ya; se manda la próxima vez
        informar(QStringLiteral("fatal"), limpiar(mensaje));
    } else if (tipo == QtCriticalMsg) {
        informar(QStringLiteral("critico"), limpiar(mensaje));
    } else if (tipo == QtWarningMsg && esErrorQml(contexto, mensaje)) {
        informar(QStringLiteral("qml"), limpiar(mensaje));
    }
    dentro = false;
}

#ifdef Q_OS_WIN
LONG WINAPI alFallarWindows(EXCEPTION_POINTERS *info)
{
    HANDLE h = CreateFileA(rutaSenal, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        char texto[64];
        const int largo = wsprintfA(texto, "excepcion 0x%08lx", info->ExceptionRecord->ExceptionCode);
        DWORD escrito = 0;
        WriteFile(h, texto, DWORD(largo), &escrito, nullptr);
        CloseHandle(h);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
struct sigaction anteriores[32];

const char *nombreSenal(int senal)
{
    switch (senal) {
    case SIGSEGV: return "SIGSEGV";
    case SIGABRT: return "SIGABRT";
    case SIGBUS: return "SIGBUS";
    case SIGFPE: return "SIGFPE";
    case SIGILL: return "SIGILL";
    default: return "senal";
    }
}

void alRecibirSenal(int senal)
{
    if (senal == SIGTERM || senal == SIGHUP || senal == SIGINT) {
        ::unlink(rutaMarca);                  // lo cerraron (apagado, cierre de sesión, Ctrl+C): no es una falla
    } else {
        const int fd = ::open(rutaSenal, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            const char *nombre = nombreSenal(senal);
            ssize_t r = ::write(fd, nombre, strlen(nombre));
            (void)r;
            ::close(fd);
        }
    }
    // Lo que había antes (Qt, el runtime de Android o lo predeterminado) sigue como siempre
    sigaction(senal, &anteriores[senal], nullptr);
    raise(senal);
}

void instalarSenales()
{
    struct sigaction accion {};
    accion.sa_handler = alRecibirSenal;
    sigemptyset(&accion.sa_mask);
    accion.sa_flags = SA_RESETHAND;
    for (int senal : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGTERM, SIGHUP, SIGINT}) {
        struct sigaction anterior {};
        sigaction(senal, nullptr, &anterior);
        if ((senal == SIGTERM || senal == SIGHUP || senal == SIGINT) && anterior.sa_handler != SIG_DFL)
            continue;     // la app (o Qt) ya maneja esa señal: no meterse
        anteriores[senal] = anterior;
        sigaction(senal, &accion, nullptr);
    }
}
#endif

void copiarRuta(char *destino, size_t tam, const QString &ruta)
{
#ifdef Q_OS_WIN
    const QByteArray bytes = QDir::toNativeSeparators(ruta).toLocal8Bit();
#else
    const QByteArray bytes = QFile::encodeName(ruta);
#endif
    qstrncpy(destino, bytes.constData(), tam);
}

QString normalizar(const QString &texto)
{
    // Para reconocer «el mismo error»: sin números ni direcciones de memoria
    static const QRegularExpression numeros(QStringLiteral(R"(0x[0-9a-fA-F]+|\d+)"));
    QString t = texto;
    return t.replace(numeros, QStringLiteral("#")).left(300);
}

QString leerTexto(const QString &ruta, qint64 maximo = 64 * 1024)
{
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    if (f.size() > maximo)
        f.seek(f.size() - maximo);
    return QString::fromUtf8(f.readAll());
}

// Motivos de ApplicationExitInfo (Android 11+) y la importancia «en caché» (la app no se veía ni tenía
// un servicio en primer plano)
enum MotivoAndroid {
    MotivoDesconocido = 0, SalioSola = 1, Senal = 2, PocaMemoria = 3, ExcepcionJava = 4, FallaNativa = 5,
    NoRespondia = 6, NoArranco = 7, CambioDePermiso = 8, DemasiadosRecursos = 9, PedidoPorLaPersona = 10,
    DetenidaPorLaPersona = 11, MurioDependencia = 12, Otro = 13, Congelada = 14, CambioDelPaquete = 15,
    Actualizada = 16, Anomalia = 17
};
constexpr int ImportanciaEnCache = 400;

#ifdef Q_OS_ANDROID
QString textoMotivoAndroid(int motivo)
{
    switch (motivo) {
    case ExcepcionJava: return QStringLiteral("excepción de Java");
    case FallaNativa: return QStringLiteral("falla nativa");
    case NoRespondia: return QStringLiteral("la app no respondía (ANR)");
    case NoArranco: return QStringLiteral("no pudo arrancar");
    case DemasiadosRecursos: return QStringLiteral("usaba demasiados recursos");
    case Anomalia: return QStringLiteral("anomalía de recursos");
    case Senal: return QStringLiteral("señal");
    default: return QStringLiteral("motivo %1").arg(motivo);
    }
}

struct SalidaAndroid
{
    int motivo = -1;           // -1: el sistema no sabe (o es Android 10 o anterior)
    int importancia = 0;
    int estado = 0;            // la señal, si fue una
    QString descripcion;
};

// ActivityManager.getHistoricalProcessExitReasons(null, pid, 1): por qué terminó ese proceso de esta app
SalidaAndroid salidaAndroid(qint64 pid)
{
    SalidaAndroid s;
    if (QNativeInterface::QAndroidApplication::sdkVersion() < 30)
        return s;
    QJniEnvironment entorno;
    const QJniObject contexto(QNativeInterface::QAndroidApplication::context());
    const QJniObject servicio = contexto.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                                          QJniObject::fromString(QStringLiteral("activity")).object<jstring>());
    if (entorno.checkAndClearExceptions() || !servicio.isValid())
        return s;
    const QJniObject lista = servicio.callObjectMethod("getHistoricalProcessExitReasons",
                                                       "(Ljava/lang/String;II)Ljava/util/List;",
                                                       static_cast<jstring>(nullptr), jint(pid), jint(1));
    if (entorno.checkAndClearExceptions() || !lista.isValid())
        return s;
    const jint cantidad = lista.callMethod<jint>("size", "()I");
    if (entorno.checkAndClearExceptions() || cantidad <= 0)
        return s;
    const QJniObject info = lista.callObjectMethod("get", "(I)Ljava/lang/Object;", jint(0));
    if (entorno.checkAndClearExceptions() || !info.isValid())
        return s;
    s.motivo = info.callMethod<jint>("getReason", "()I");
    s.importancia = info.callMethod<jint>("getImportance", "()I");
    s.estado = info.callMethod<jint>("getStatus", "()I");
    s.descripcion = info.callObjectMethod("getDescription", "()Ljava/lang/String;").toString();
    if (entorno.checkAndClearExceptions())
        s.motivo = -1;
    return s;
}
#endif

} // namespace

CierreAndroid evaluarCierreAndroid(int motivo, int importancia)
{
    switch (motivo) {
    case ExcepcionJava:
    case FallaNativa:
    case NoRespondia:
    case NoArranco:
    case DemasiadosRecursos:
    case Anomalia:
        return CierreAndroid::Falla;
    case SalioSola:            // salió por su cuenta (Qt a veces termina así al cerrar la última ventana)
    case PocaMemoria:
    case CambioDePermiso:      // por ejemplo, le sacaron el acceso a los archivos en Ajustes
    case PedidoPorLaPersona:   // la deslizaron en Recientes o «Forzar detención»
    case DetenidaPorLaPersona:
    case MurioDependencia:
    case Congelada:
    case CambioDelPaquete:
    case Actualizada:
        return CierreAndroid::NoEsFalla;
    case Senal:
    case Otro:
        // El sistema la mató sin dar un motivo claro: si no se veía (en caché), es limpieza de segundo plano
        return importancia >= ImportanciaEnCache ? CierreAndroid::NoEsFalla : CierreAndroid::NoSe;
    default:
        return CierreAndroid::NoSe;
    }
}

QString limpiar(const QString &texto)
{
    QString t = texto;
    static const QRegularExpression url(QStringLiteral(R"(\b(https?|ftp|wss?)://([^/\s:?#"'<>]+)[^\s"'<>]*)"));
    t.replace(url, QStringLiteral("\\1://\\2/…"));
    static const QRegularExpression archivoUrl(QStringLiteral(R"(\bfile://[^\s"'<>]*)"));
    t.replace(archivoUrl, QStringLiteral("file://…"));
    static const QRegularExpression correo(QStringLiteral(R"([\w.+-]+@[\w-]+\.[\w.-]+)"));
    t.replace(correo, QStringLiteral("<correo>"));
    const QString casa = QDir::homePath();
    if (casa.size() > 1)
        t.replace(casa, QStringLiteral("~"));
    t.replace(QStringLiteral("/storage/emulated/0"), QStringLiteral("<almacenamiento>"));
    for (const char *variable : {"USER", "USERNAME", "LOGNAME"}) {
        const QString usuario = qEnvironmentVariable(variable);
        if (usuario.size() >= 3) {
            const QRegularExpression palabra(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(usuario)),
                                             QRegularExpression::CaseInsensitiveOption);
            t.replace(palabra, QStringLiteral("<usuario>"));
        }
    }
    return t;
}

Fallas &Fallas::instancia()
{
    static Fallas fallas;
    return fallas;
}

void iniciarFallas()
{
    Fallas::instancia().iniciar();
}

void informar(const QString &contexto, const QString &detalle)
{
    Fallas &f = Fallas::instancia();
    if (!f.iniciado_)
        return;
    QStringList copia;
    {
        QMutexLocker l(&candado);
        copia = renglones;
    }
    if (contexto == u"fatal" || QThread::currentThread() == f.thread()) {
        f.guardarInforme(QStringLiteral("error"), contexto, limpiar(detalle), copia);
        if (contexto != u"fatal")
            f.programarEnvio();
    } else {
        QMetaObject::invokeMethod(&f, [contexto, detalle, copia] {
            Fallas &ff = Fallas::instancia();
            ff.guardarInforme(QStringLiteral("error"), contexto, limpiar(detalle), copia);
            ff.programarEnvio();
        }, Qt::QueuedConnection);
    }
}

void Fallas::iniciar()
{
    if (iniciado_ || !QCoreApplication::instance())
        return;
    iniciado_ = true;
    moveToThread(QCoreApplication::instance()->thread());
    carpeta_ = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("fallas"));
    QDir().mkpath(carpeta_ + QStringLiteral("/pendientes"));
    equipo = datosEquipo();

    revisarCierreAnterior();

    const qint64 pid = QCoreApplication::applicationPid();
    archivoRegistro = new QFile(rutaProceso(carpeta_, pid, "registro"));
    archivoRegistro->open(QIODevice::WriteOnly | QIODevice::Truncate);
    copiarRuta(rutaSenal, sizeof rutaSenal, rutaProceso(carpeta_, pid, "senal"));
    copiarRuta(rutaMarca, sizeof rutaMarca, rutaProceso(carpeta_, pid, "marca"));
    marcarEnCurso(true);

    manejadorAnterior = qInstallMessageHandler(manejador);
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(alFallarWindows);
#else
    instalarSenales();
#endif

    // Salida normal (también si la app sale antes de exec): la marca se borra al destruirse la aplicación
    qAddPostRoutine([] {
        Fallas &f = Fallas::instancia();
        f.marcarEnCurso(false);
        QMutexLocker l(&candado);
        if (archivoRegistro) {
            archivoRegistro->close();
            archivoRegistro->remove();
        }
        QFile::remove(QString::fromLocal8Bit(rutaSenal));
    });

#ifdef Q_OS_ANDROID
    // Android cierra las apps en segundo plano cuando quiere: eso no es una falla. En segundo plano la
    // marca no está; vuelve al volver a primer plano.
    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        QObject::connect(app, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState estado) {
            if (estado == Qt::ApplicationActive)
                marcarEnCurso(true);
            else if (estado == Qt::ApplicationSuspended || estado == Qt::ApplicationHidden)
                marcarEnCurso(false);
        });
    }
#endif

    // Lo que quedó de antes se manda un rato después de abrir (que la app arranque tranquila)
    QTimer::singleShot(8000, this, [this] { enviarPendientes(); });
}

void Fallas::marcarEnCurso(bool enCurso)
{
    const QString marca = QString::fromLocal8Bit(rutaMarca);
    if (marca.isEmpty())
        return;
    if (enCurso) {
        QFile f(marca);
        if (f.open(QIODevice::WriteOnly))
            f.write(QByteArray::number(QCoreApplication::applicationPid()));
    } else {
        QFile::remove(marca);
    }
}

void Fallas::revisarCierreAnterior()
{
    // proceso-<pid>.{marca,registro,senal} de procesos que ya no están: si hay señal, fue una falla; si quedó la
    // marca, se cerró sin terminar bien. Los de otra ventana de la misma app que sigue abierta, no se tocan.
    static const QRegularExpression nombre(QStringLiteral(R"(^proceso-(\d+)\.(marca|registro|senal)$)"));
    QSet<qint64> pids;
    const QStringList archivos = QDir(carpeta_).entryList(QDir::Files);
    for (const QString &a : archivos) {
        const auto m = nombre.match(a);
        if (m.hasMatch())
            pids.insert(m.captured(1).toLongLong());
    }
    const qint64 propio = QCoreApplication::applicationPid();
    for (qint64 pid : std::as_const(pids)) {
        if (pid == propio || procesoVivo(pid))
            continue;
        const QString marca = rutaProceso(carpeta_, pid, "marca");
        const QString senal = rutaProceso(carpeta_, pid, "senal");
        const QString registro = rutaProceso(carpeta_, pid, "registro");
        const QString textoSenal = leerTexto(senal).trimmed();
        bool falla = !textoSenal.isEmpty() || QFileInfo::exists(marca);
        QString detalle = textoSenal.isEmpty() ? QStringLiteral("La app se cerró sin terminar bien")
                                               : QStringLiteral("La app se cerró por una falla (%1)").arg(textoSenal);
#ifdef Q_OS_ANDROID
        // Lo que dice Android manda sobre la marca (la señal, que la escribió la app misma, gana siempre)
        const SalidaAndroid salida = salidaAndroid(pid);
        switch (evaluarCierreAndroid(salida.motivo, salida.importancia)) {
        case CierreAndroid::Falla: {
            falla = true;
            QString motivo = textoMotivoAndroid(salida.motivo);
            if (!salida.descripcion.trimmed().isEmpty())
                motivo += QStringLiteral(": ") + limpiar(salida.descripcion.trimmed()).left(300);
            detalle = textoSenal.isEmpty() ? QStringLiteral("La app se cerró por una falla (%1)").arg(motivo)
                                           : QStringLiteral("La app se cerró por una falla (%1; %2)").arg(textoSenal, motivo);
            break;
        }
        case CierreAndroid::NoEsFalla:
            if (textoSenal.isEmpty())
                falla = false;
            break;
        case CierreAndroid::NoSe:
            break;
        }
#endif
        if (falla) {
            QStringList lineas = leerTexto(registro).split(u'\n', Qt::SkipEmptyParts);
            if (lineas.size() > MaxRenglones)
                lineas = lineas.mid(lineas.size() - MaxRenglones);
            guardarInforme(QStringLiteral("cierre"), QStringLiteral("cierre"), detalle, lineas);
        }
        QFile::remove(marca);
        QFile::remove(senal);
        QFile::remove(registro);
    }
}

QJsonObject Fallas::datosEquipo() const
{
    QJsonObject d;
    d[QStringLiteral("app")] = QCoreApplication::applicationName();
    d[QStringLiteral("version")] = QCoreApplication::applicationVersion();
    d[QStringLiteral("sistema")] = QSysInfo::prettyProductName();
    d[QStringLiteral("nucleo")] = QSysInfo::kernelVersion();
    d[QStringLiteral("arquitectura")] = QSysInfo::currentCpuArchitecture();
    d[QStringLiteral("qt")] = QString::fromLatin1(qVersion());
    d[QStringLiteral("desarrollador")] = esDesarrollador();
#ifdef Q_OS_ANDROID
    const QString fabricante = QJniObject::getStaticObjectField<jstring>("android/os/Build", "MANUFACTURER").toString();
    const QString modelo = QJniObject::getStaticObjectField<jstring>("android/os/Build", "MODEL").toString();
    d[QStringLiteral("modelo")] = (fabricante + u' ' + modelo).trimmed();
#endif
    // Un número al azar por instalación: para saber si varios informes son del mismo equipo (no dice cuál es)
    QFile archivoId(QDir(carpeta_).filePath(QStringLiteral("equipo")));
    QString id;
    if (archivoId.open(QIODevice::ReadOnly))
        id = QString::fromLatin1(archivoId.readAll()).trimmed();
    archivoId.close();
    if (id.size() != 12) {
        id = QUuid::createUuid().toString(QUuid::Id128).left(12);
        if (archivoId.open(QIODevice::WriteOnly))
            archivoId.write(id.toLatin1());
    }
    d[QStringLiteral("equipo")] = id;
    return d;
}

void Fallas::guardarInforme(const QString &tipo, const QString &contexto, const QString &detalle,
                            const QStringList &registro, bool siempre)
{
    if (carpeta_.isEmpty())
        return;
    const QString version = QCoreApplication::applicationVersion();
    const QString hoy = QDate::currentDate().toString(Qt::ISODate);
    if (!siempre) {
        // El mismo error, una vez por versión
        const QString huella = QString::fromLatin1(QCryptographicHash::hash(
            (tipo + u'|' + contexto + u'|' + normalizar(detalle)).toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
        const QString rutaVistos = QDir(carpeta_).filePath(QStringLiteral("vistos.txt"));
        QStringList vistos = leerTexto(rutaVistos).split(u'\n', Qt::SkipEmptyParts);
        const QString clave = version + u' ' + huella;
        if (vistos.contains(clave))
            return;
        // Como mucho MaxPorDia por día
        const QString rutaCuenta = QDir(carpeta_).filePath(QStringLiteral("hoy.txt"));
        const QStringList cuenta = leerTexto(rutaCuenta).split(u' ', Qt::SkipEmptyParts);
        const int hechos = (cuenta.size() == 2 && cuenta.at(0) == hoy) ? cuenta.at(1).toInt() : 0;
        if (hechos >= MaxPorDia)
            return;
        vistos << clave;
        if (vistos.size() > 200)
            vistos = vistos.mid(vistos.size() - 200);
        QSaveFile fv(rutaVistos);
        if (fv.open(QIODevice::WriteOnly)) {
            fv.write(vistos.join(u'\n').toUtf8() + '\n');
            fv.commit();
        }
        QSaveFile fc(rutaCuenta);
        if (fc.open(QIODevice::WriteOnly)) {
            fc.write((hoy + u' ' + QString::number(hechos + 1)).toUtf8());
            fc.commit();
        }
    }

    QJsonObject informe = equipo.isEmpty() ? datosEquipo() : equipo;
    informe[QStringLiteral("tipo")] = tipo;
    informe[QStringLiteral("contexto")] = contexto;
    informe[QStringLiteral("detalle")] = detalle.left(4000);
    informe[QStringLiteral("fecha")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    informe[QStringLiteral("registro")] = QJsonArray::fromStringList(registro.mid(qMax(0, registro.size() - MaxRenglones)));

    QDir pendientes(carpeta_ + QStringLiteral("/pendientes"));
    const QString nombre = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz-"))
                           + QUuid::createUuid().toString(QUuid::Id128).left(6) + QStringLiteral(".json");
    QSaveFile f(pendientes.filePath(nombre));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(informe).toJson(QJsonDocument::Compact));
        f.commit();
    }
    // No juntar más de MaxPendientes (si nunca hay conexión): se van los más viejos
    QStringList todos = pendientes.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    while (todos.size() > MaxPendientes)
        pendientes.remove(todos.takeFirst());
}

QStringList Fallas::registro() const
{
    QMutexLocker l(&candado);
    return renglones;
}

QUrl Fallas::destino()
{
    const QString prueba = qEnvironmentVariable("COMUN_FALLAS_URL");
    if (!prueba.isEmpty())
        return QUrl(prueba);
    const QStringList raices = apps::raicesServidor();
    return QUrl(raices.value(0, QStringLiteral("https://bodrio.seb-dev.fyi/"))).resolved(QUrl(QStringLiteral("/fallas/api/informe")));
}

void Fallas::programarEnvio()
{
    if (envioProgramado_)
        return;
    envioProgramado_ = true;
    QTimer::singleShot(5000, this, [this] {
        envioProgramado_ = false;
        enviarPendientes();
    });
}

void Fallas::enviarPendientes(std::function<void()> alTerminar, int segundos)
{
    auto terminar = std::make_shared<std::function<void()>>(std::move(alTerminar));
    auto llamar = [terminar] {
        if (*terminar) {
            auto f = std::move(*terminar);
            *terminar = nullptr;
            f();
        }
    };
    if (enviando_ || carpeta_.isEmpty() || qEnvironmentVariableIntValue("COMUN_FALLAS_SIN_RED") == 1) {
        llamar();
        return;
    }
    const QStringList archivos = QDir(carpeta_ + QStringLiteral("/pendientes"))
                                     .entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (archivos.isEmpty()) {
        llamar();
        return;
    }
    enviando_ = true;
    emit enviandoCambio();
    QTimer::singleShot(segundos * 1000, this, llamar);     // no esperar más que eso (la app puede estar saliendo)
    mandarSiguiente(archivos, 0, llamar);
}

void Fallas::mandarSiguiente(QStringList archivos, int raiz, std::function<void()> alTerminar)
{
    if (archivos.isEmpty()) {
        enviando_ = false;
        emit enviandoCambio();
        alTerminar();
        return;
    }
    const QString ruta = QDir(carpeta_ + QStringLiteral("/pendientes")).filePath(archivos.first());
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly)) {
        archivos.removeFirst();
        mandarSiguiente(archivos, 0, alTerminar);
        return;
    }
    const QByteArray cuerpo = f.readAll();
    f.close();

    QUrl url = destino();
    if (qEnvironmentVariable("COMUN_FALLAS_URL").isEmpty()) {
        const QStringList raices = apps::raicesServidor();
        if (raiz >= raices.size()) {          // no hay forma de llegar al servidor: queda para la próxima
            enviando_ = false;
            emit enviandoCambio();
            alTerminar();
            return;
        }
        url = QUrl(raices.at(raiz)).resolved(QUrl(QStringLiteral("/fallas/api/informe")));
    } else if (raiz > 0) {
        enviando_ = false;
        emit enviandoCambio();
        alTerminar();
        return;
    }

    http::enviar(this, url, cuerpo, [this, archivos, raiz, ruta, alTerminar](int codigo, const QByteArray &) mutable {
        if ((codigo >= 200 && codigo < 300) || codigo == 400 || codigo == 413) {
            QFile::remove(ruta);              // llegó (o el servidor nunca lo va a aceptar): no se repite
            archivos.removeFirst();
            mandarSiguiente(archivos, raiz, alTerminar);
        } else if (codigo == 429) {           // el servidor pide esperar: el resto, la próxima vez
            enviando_ = false;
            emit enviandoCambio();
            alTerminar();
        } else {
            mandarSiguiente(archivos, raiz + 1, alTerminar);   // probar por otro camino (Tailscale)
        }
    }, 20);
}

void Fallas::enviarManual(const QString &comentario)
{
    if (!iniciado_) {
        emit enviadoManual(false);
        return;
    }
    const QString detalle = comentario.trimmed().isEmpty() ? QStringLiteral("Informe enviado a mano desde el menú")
                                                           : limpiar(comentario.trimmed());
    guardarInforme(QStringLiteral("manual"), QStringLiteral("manual"), detalle, registro(), true);
    const QString pendientes = carpeta_ + QStringLiteral("/pendientes");
    QPointer<Fallas> yo(this);
    auto revisar = [yo, pendientes] {
        if (!yo)
            return;
        // Salió bien si ya no queda ningún informe manual esperando
        bool queda = false;
        const QStringList archivos = QDir(pendientes).entryList({QStringLiteral("*.json")}, QDir::Files);
        for (const QString &a : archivos) {
            QFile f(QDir(pendientes).filePath(a));
            if (f.open(QIODevice::ReadOnly) && f.readAll().contains("\"tipo\":\"manual\"")) {
                queda = true;
                break;
            }
        }
        emit yo->enviadoManual(!queda);
    };
    if (enviando_) {
        // Hay un envío en curso: esperar a que termine y volver a probar
        auto *conexion = new QMetaObject::Connection;
        *conexion = connect(this, &Fallas::enviandoCambio, this, [this, revisar, conexion] {
            if (enviando_)
                return;
            disconnect(*conexion);
            delete conexion;
            enviarPendientes(revisar);
        });
        return;
    }
    enviarPendientes(revisar);
}

void Fallas::informarYSalir(const QString &contexto, const QString &detalle, int codigo)
{
    if (!iniciado_) {
        QCoreApplication::exit(codigo);
        return;
    }
    guardarInforme(QStringLiteral("error"), contexto, limpiar(detalle), registro());
    enviarPendientes([codigo] { QCoreApplication::exit(codigo); }, 8);
}

} // namespace soporte
