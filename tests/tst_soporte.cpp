// Pruebas de soporte/: novedades (lo «[Sebastián]» solo en sus equipos, qué versiones se muestran al abrir)
// e informe de fallas (cierre con señal en otro proceso, qCritical, errores de QML, que no se repita, que
// se limpie lo personal y que llegue al servidor).
#include "Fallas.h"
#include "Novedades.h"
#include "VentanaNovedades.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QtTest>

#include <csignal>

using namespace soporte;

namespace {

const char *Ejemplo = R"(# Novedades de Prueba

Texto suelto de arriba que no es de ninguna versión.
- tampoco esto

## 2.1.0 (21/11/2026)
- Informe de fallas: si la app se cierra,
  manda un informe.
- [Sebastián] Sección Servidor con más acciones.

## 2.0.1
- [sebastian] Solo un arreglo de la sección Servidor.

## 2.0.0 (10/11/2026)
* Interfaz nueva.
- Modo claro.

## 3.0.0
- Todavía no salió.
)";

QStringList pendientes()
{
    QDir d(Fallas::instancia().carpeta() + QStringLiteral("/pendientes"));
    return d.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
}

QJsonObject leerPendiente(const QString &nombre)
{
    QFile f(Fallas::instancia().carpeta() + QStringLiteral("/pendientes/") + nombre);
    f.open(QIODevice::ReadOnly);
    return QJsonDocument::fromJson(f.readAll()).object();
}

void borrarPendientes()
{
    for (const QString &p : pendientes())
        QFile::remove(Fallas::instancia().carpeta() + QStringLiteral("/pendientes/") + p);
}

}

class PruebaSoporte : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // La primera prueba de fallas necesita que otro proceso de la «misma app» se haya caído antes de
        // iniciarFallas() en este: se lanza este mismo programa con --chocar
        QProcess hijo;
        hijo.setProgram(QCoreApplication::applicationFilePath());
        hijo.setArguments({QStringLiteral("--chocar")});
        QProcessEnvironment entorno = QProcessEnvironment::systemEnvironment();
        entorno.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        hijo.setProcessEnvironment(entorno);
        hijo.start();
        QVERIFY(hijo.waitForFinished(20000));
        QCOMPARE(hijo.exitStatus(), QProcess::CrashExit);

        qputenv("COMUN_FALLAS_SIN_RED", "1");
        iniciarFallas();
    }

    void novedadesParaTodos()
    {
        const auto v = leerNovedades(QString::fromUtf8(Ejemplo), false);
        QCOMPARE(v.size(), 3);                                   // la 2.0.1 queda vacía: no aparece
        QCOMPARE(v.at(0).textoVersion, QStringLiteral("3.0.0"));
        QCOMPARE(v.at(1).textoVersion, QStringLiteral("2.1.0"));
        QCOMPARE(v.at(1).fecha, QStringLiteral("21/11/2026"));
        QCOMPARE(v.at(1).cambios, QStringList{QStringLiteral("Informe de fallas: si la app se cierra, manda un informe.")});
        QCOMPARE(v.at(2).cambios.size(), 2);
    }

    void novedadesParaSebastian()
    {
        const auto v = leerNovedades(QString::fromUtf8(Ejemplo), true);
        QCOMPARE(v.size(), 4);
        QCOMPARE(v.at(1).cambios.size(), 2);
        QCOMPARE(v.at(1).cambios.at(1), QStringLiteral("Sección Servidor con más acciones."));   // sin la marca
        QCOMPARE(v.at(1).soloDesarrollador, (QList<bool>{false, true}));
        QCOMPARE(v.at(2).textoVersion, QStringLiteral("2.0.1"));
        const QString md = aMarkdown(v.mid(1, 1), true);
        QVERIFY(md.contains(QStringLiteral("*(solo vos)* Sección Servidor")));
        QVERIFY(md.startsWith(QStringLiteral("**Versión 2.1.0** · 21/11/2026")));
    }

    void cualesSeMuestranAlAbrir()
    {
        const auto v = leerNovedades(QString::fromUtf8(Ejemplo), true);
        const QVersionNumber actual(2, 1, 0);
        // Desde la 2.0.0: la 2.0.1 y la 2.1.0 (no la 3.0.0, que todavía no salió)
        auto n = nuevasDesde(v, QVersionNumber(2, 0, 0), actual);
        QCOMPARE(n.size(), 2);
        QCOMPARE(n.at(0).textoVersion, QStringLiteral("2.1.0"));
        // Nunca se vio ninguna: solo la actual
        n = nuevasDesde(v, QVersionNumber(), actual);
        QCOMPARE(n.size(), 1);
        QCOMPARE(n.at(0).textoVersion, QStringLiteral("2.1.0"));
        // Ya vista
        QVERIFY(nuevasDesde(v, actual, actual).isEmpty());
    }

    void objetoNovedades()
    {
        QTemporaryDir dir;
        const QString md = dir.filePath(QStringLiteral("NOVEDADES.md"));
        QFile f(md);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(Ejemplo);
        f.close();
        QSettings ajustes(dir.filePath(QStringLiteral("a.ini")), QSettings::IniFormat);
        ajustes.setValue(QStringLiteral("novedades/vista"), QStringLiteral("2.0.0"));
        QCoreApplication::setApplicationVersion(QStringLiteral("2.1.0"));

        // Equipo ajeno: la 2.0.1 (solo de Sebastián) no está y lo de Sebastián en la 2.1.0 tampoco
        qputenv("COMUN_DESARROLLADOR", "0");
        {
            Novedades n(nullptr, md, &ajustes);
            QVERIFY(n.hayNuevas());
            QCOMPARE(n.nuevas().size(), 1);
            QVERIFY(!n.textoNuevas().contains(QStringLiteral("Servidor")));
            QVERIFY(!n.textoTodas().contains(QStringLiteral("3.0.0")));
        }
        // Sebastián
        qputenv("COMUN_DESARROLLADOR", "1");
        {
            Novedades n(nullptr, md, &ajustes);
            QCOMPARE(n.nuevas().size(), 2);
            QVERIFY(n.textoNuevas().contains(QStringLiteral("(solo vos)")));
            n.marcarVistas();
            QVERIFY(!n.hayNuevas());
        }
        QCOMPARE(ajustes.value(QStringLiteral("novedades/vista")).toString(), QStringLiteral("2.1.0"));
        {
            Novedades n(nullptr, md, &ajustes);
            QVERIFY(!n.hayNuevas());
        }

        // Una versión cuyas novedades son todas de Sebastián, en un equipo ajeno: no molesta y queda vista
        QCoreApplication::setApplicationVersion(QStringLiteral("2.0.1"));
        ajustes.setValue(QStringLiteral("novedades/vista"), QStringLiteral("2.0.0"));
        qputenv("COMUN_DESARROLLADOR", "0");
        {
            Novedades n(nullptr, md, &ajustes);
            QVERIFY(!n.hayNuevas());
        }
        QCOMPARE(ajustes.value(QStringLiteral("novedades/vista")).toString(), QStringLiteral("2.0.1"));
        qunsetenv("COMUN_DESARROLLADOR");
    }

    void ventanaDeNovedades()
    {
        // Que se arme (Widgets) con lo que haya
        VentanaNovedades v(false);
        QVERIFY(v.findChild<QTextBrowser *>());
    }

    void limpiaLoPersonal()
    {
        const QString casa = QDir::homePath();
        const QString t = limpiar(casa + QStringLiteral("/Videos/x.mkv https://www.youtube.com/watch?v=abc&list=1 "
                                                        "persona@correo.com file:///home/x/y.qml"));
        QVERIFY(!t.contains(casa));
        QVERIFY(t.contains(QStringLiteral("~/Videos")));
        QVERIFY(t.contains(QStringLiteral("https://www.youtube.com/…")));
        QVERIFY(!t.contains(QStringLiteral("watch")));
        QVERIFY(t.contains(QStringLiteral("<correo>")));
        QVERIFY(t.contains(QStringLiteral("file://…")));
    }

    void cierreConSenalDeOtroProceso()
    {
        // El hijo de initTestCase se cayó con SIGSEGV: al iniciar, quedó su informe
        bool hay = false;
        for (const QString &p : pendientes()) {
            const QJsonObject o = leerPendiente(p);
            if (o.value(QStringLiteral("tipo")).toString() == u"cierre") {
                hay = true;
#ifdef Q_OS_WIN
                QVERIFY(o.value(QStringLiteral("detalle")).toString().contains(QStringLiteral("excepcion")));
#else
                QVERIFY2(o.value(QStringLiteral("detalle")).toString().contains(QStringLiteral("SIGSEGV")),
                         qPrintable(o.value(QStringLiteral("detalle")).toString()));
#endif
                const QJsonArray registro = o.value(QStringLiteral("registro")).toArray();
                QVERIFY(!registro.isEmpty());
                QVERIFY(registro.last().toString().contains(QStringLiteral("a punto de chocar")));
                QCOMPARE(o.value(QStringLiteral("app")).toString(), QCoreApplication::applicationName());
                QCOMPARE(o.value(QStringLiteral("equipo")).toString().size(), 12);
            }
        }
        QVERIFY(hay);
        // Y no quedaron los archivos del proceso muerto
        const QStringList restos = QDir(Fallas::instancia().carpeta()).entryList({QStringLiteral("proceso-*")}, QDir::Files);
        for (const QString &r : restos)
            QVERIFY2(r.startsWith(QStringLiteral("proceso-%1.").arg(QCoreApplication::applicationPid())), qPrintable(r));
        borrarPendientes();
    }

    void cierresAndroid()
    {
        // Motivos de ApplicationExitInfo: 4 excepción de Java, 5 nativa, 6 ANR, 10 la cerró la persona,
        // 3 poca memoria, 1 salió sola, 8 cambio de permiso, 2 señal, 13 otro, 0 desconocido
        QCOMPARE(evaluarCierreAndroid(6, 100), CierreAndroid::Falla);
        QCOMPARE(evaluarCierreAndroid(4, 400), CierreAndroid::Falla);      // aunque estuviera en segundo plano
        QCOMPARE(evaluarCierreAndroid(5, 125), CierreAndroid::Falla);
        QCOMPARE(evaluarCierreAndroid(10, 100), CierreAndroid::NoEsFalla); // aunque estuviera abierta
        QCOMPARE(evaluarCierreAndroid(3, 100), CierreAndroid::NoEsFalla);
        QCOMPARE(evaluarCierreAndroid(1, 100), CierreAndroid::NoEsFalla);
        QCOMPARE(evaluarCierreAndroid(8, 100), CierreAndroid::NoEsFalla);
        QCOMPARE(evaluarCierreAndroid(2, 400), CierreAndroid::NoEsFalla);  // limpieza de segundo plano
        QCOMPARE(evaluarCierreAndroid(13, 1000), CierreAndroid::NoEsFalla);
        QCOMPARE(evaluarCierreAndroid(2, 100), CierreAndroid::NoSe);       // a la vista: decide la marca
        QCOMPARE(evaluarCierreAndroid(0, 100), CierreAndroid::NoSe);
        QCOMPARE(evaluarCierreAndroid(-1, 0), CierreAndroid::NoSe);        // Android 10 o sin datos
    }

    void criticoYQmlUnaVez()
    {
        borrarPendientes();
        qCritical("No se pudo abrir la base %d", 42);
        qCritical("No se pudo abrir la base %d", 43);       // el mismo error (salvo el número): uno solo
        qWarning("qrc:/qt/qml/App/Main.qml:12: TypeError: Cannot read property 'x' of null");
        qWarning("Binding loop detected for property \"width\"");   // esto no es una falla
        QCoreApplication::processEvents();
        const QStringList p = pendientes();
        QCOMPARE(p.size(), 2);
        QStringList contextos;
        for (const QString &n : p)
            contextos << leerPendiente(n).value(QStringLiteral("contexto")).toString();
        contextos.sort();
        QCOMPARE(contextos, (QStringList{QStringLiteral("critico"), QStringLiteral("qml")}));
        QVERIFY(Fallas::instancia().registro().last().contains(QStringLiteral("Binding loop")));
        borrarPendientes();
    }

    void llegaAlServidor()
    {
        // Un servidor HTTP mínimo que guarda lo que le mandan y responde 200
        QTcpServer servidor;
        QVERIFY(servidor.listen(QHostAddress::LocalHost));
        QByteArray recibido;
        connect(&servidor, &QTcpServer::newConnection, this, [&] {
            QTcpSocket *s = servidor.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [s, &recibido] {
                recibido += s->readAll();
                const int fin = recibido.indexOf("\r\n\r\n");
                if (fin < 0)
                    return;
                const QRegularExpression largo(QStringLiteral("Content-Length: *(\\d+)"), QRegularExpression::CaseInsensitiveOption);
                const auto m = largo.match(QString::fromLatin1(recibido.left(fin)));
                if (m.hasMatch() && recibido.size() - fin - 4 < m.captured(1).toInt())
                    return;
                s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 11\r\nConnection: close\r\n\r\n{\"ok\":true}");
                s->disconnectFromHost();
            });
        });
        qputenv("COMUN_FALLAS_URL", QStringLiteral("http://127.0.0.1:%1/fallas/api/informe").arg(servidor.serverPort()).toUtf8());
        qunsetenv("COMUN_FALLAS_SIN_RED");

        QSignalSpy espia(&Fallas::instancia(), &Fallas::enviadoManual);
        Fallas::instancia().enviarManual(QStringLiteral("Se cerró al apretar Sincronizar en ") + QDir::homePath());
        QVERIFY(espia.wait(15000));
        QCOMPARE(espia.first().first().toBool(), true);
        QVERIFY(recibido.startsWith("POST /fallas/api/informe"));
        const QJsonObject o = QJsonDocument::fromJson(recibido.mid(recibido.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(o.value(QStringLiteral("tipo")).toString(), QStringLiteral("manual"));
        QVERIFY(o.value(QStringLiteral("detalle")).toString().endsWith(QStringLiteral("Sincronizar en ~")));
        QVERIFY(pendientes().isEmpty());

        // Sin servidor: queda guardado para la próxima
        servidor.close();
        espia.clear();
        Fallas::instancia().enviarManual();
        QVERIFY(espia.wait(25000));
        QCOMPARE(espia.first().first().toBool(), false);
        QCOMPARE(pendientes().size(), 1);
        borrarPendientes();
        qputenv("COMUN_FALLAS_SIN_RED", "1");
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("tst_soporte"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.1.0"));
    QStandardPaths::setTestModeEnabled(true);
    if (app.arguments().contains(QStringLiteral("--chocar"))) {
        iniciarFallas();
        qWarning("a punto de chocar");
        std::raise(SIGSEGV);
        return 0;
    }
    // Empezar de cero (si no, lo «ya informado» de una corrida anterior no se volvería a guardar)
    QDir(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("fallas")))
        .removeRecursively();
    PruebaSoporte prueba;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&prueba, argc, argv);
}

#include "tst_soporte.moc"
