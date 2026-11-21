// Pruebas de red/: servidor.json y comparación de versiones (sin conectarse a nada)
#include "Actualizaciones.h"
#include "ConfigServidor.h"
#include "ClienteHttp.h"

#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <QStandardPaths>
#include <QTest>

class TstRed : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void init() { ConfigServidor::instancia().restablecer(); }

    void valoresDeFabrica()
    {
        const ConfigServidor &c = ConfigServidor::instancia();
        QCOMPARE(c.ipsSsh(), (QStringList{QStringLiteral("192.168.100.14"), QStringLiteral("100.101.213.44")}));
        QCOMPARE(c.usuario(), QStringLiteral("sebastian"));
        QVERIFY(c.carpetaRespaldos().endsWith(u'/'));
        QCOMPARE(c.raicesBodrio().size(), 2);
    }

    void aplicaServidorJson()
    {
        ConfigServidor &c = ConfigServidor::instancia();
        QVERIFY(c.aplicar("{\"version\":1,\"ip_local\":\"10.0.0.2\",\"ip_tailscale\":\"100.9.9.9\",\"usuario\":\"otro\", \"respaldos\":\"/srv/r\",\"bodrio\":[\"https://a.example/Bodrio\"]}"));
        QCOMPARE(c.ipLocal(), QStringLiteral("10.0.0.2"));
        QCOMPARE(c.ipTailscale(), QStringLiteral("100.9.9.9"));
        QCOMPARE(c.usuario(), QStringLiteral("otro"));
        QCOMPARE(c.carpetaRespaldos(), QStringLiteral("/srv/r/"));
        QCOMPARE(c.raicesBodrio(), QStringList{QStringLiteral("https://a.example/Bodrio/")});
    }

    void rechazaValoresRaros()
    {
        ConfigServidor &c = ConfigServidor::instancia();
        const QByteArray raro = "{\"version\":1,\"ip_local\":\"1.2.3.4; rm -rf /\",\"usuario\":\"$(id)\", \"respaldos\":\"/home/../etc\",\"bodrio\":[\"javascript:alert(1)\"]}";
        QVERIFY(c.aplicar(raro));
        QCOMPARE(c.ipLocal(), QStringLiteral("192.168.100.14"));
        QCOMPARE(c.usuario(), QStringLiteral("sebastian"));
        QCOMPARE(c.carpetaRespaldos(), QStringLiteral("/home/sebastian/Respaldos/"));
        QCOMPARE(c.raicesBodrio().size(), 2);
        QVERIFY(!c.aplicar("no es json"));
        QVERIFY(!c.aplicar("{\"version\":2,\"ip_local\":\"10.0.0.9\"}"));
        QCOMPARE(c.ipLocal(), QStringLiteral("192.168.100.14"));
    }

    void huellas()
    {
        const QStringList l = ConfigServidor::huellasSsh({QStringLiteral("servidor-respaldos")});
        QVERIFY(l.size() >= 1);
        QVERIFY(l.first().startsWith(QStringLiteral("servidor-respaldos ssh-ed25519 AAAA")));
        QCOMPARE(ConfigServidor::huellasSsh({QStringLiteral("a"), QStringLiteral("b")}).size(), 2 * l.size());
    }

    void versiones()
    {
        QVERIFY(Actualizaciones::compararVersiones(QStringLiteral("1.10.0"), QStringLiteral("1.9.2")) > 0);
        QVERIFY(Actualizaciones::compararVersiones(QStringLiteral("v1.2.0"), QStringLiteral("1.2")) == 0);
        QVERIFY(Actualizaciones::compararVersiones(QStringLiteral("1.2.0"), QStringLiteral("1.2.1")) < 0);
        QVERIFY(Actualizaciones::compararVersiones(QStringLiteral("2.0"), QStringLiteral("1.99.99")) > 0);
    }

    // descargar() tiene que dejar el archivo byte por byte (antes, en Android, un APK llegaba roto
    // porque el cuerpo pasaba por texto UTF-8)
    void descargarBinario()
    {
        QByteArray datos;
        for (int vuelta = 0; vuelta < 300; ++vuelta)
            for (int b = 0; b < 256; ++b)
                datos.append(char(b));
        QTcpServer servidor;
        QVERIFY(servidor.listen(QHostAddress::LocalHost));
        connect(&servidor, &QTcpServer::newConnection, this, [&] {
            QTcpSocket *s = servidor.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [s, datos] {
                const QByteArray pedido = s->readAll();
                if (pedido.startsWith("GET /falta")) {
                    s->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                } else {
                    s->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "
                             + QByteArray::number(datos.size()) + "\r\nConnection: close\r\n\r\n");
                    s->write(datos);
                }
                s->disconnectFromHost();
            });
        });
        QTemporaryDir dir;
        const QString ruta = dir.filePath(QStringLiteral("app.apk"));
        const QUrl base(QStringLiteral("http://127.0.0.1:%1/").arg(servidor.serverPort()));
        int codigo = -1;
        http::descargar(this, base.resolved(QUrl(QStringLiteral("app.apk"))), ruta, [&](int c) { codigo = c; });
        QTRY_COMPARE(codigo, 200);
        QFile f(ruta);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), datos);

        codigo = -1;
        const QString otra = dir.filePath(QStringLiteral("no.apk"));
        http::descargar(this, base.resolved(QUrl(QStringLiteral("falta"))), otra, [&](int c) { codigo = c; });
        QTRY_COMPARE(codigo, 404);
        QVERIFY(!QFile::exists(otra));          // sin archivo a medias
    }

    // enviar(): POST con JSON; con un error HTTP también llega el cuerpo (el mensaje del servidor)
    void enviarJson()
    {
        QTcpServer servidor;
        QVERIFY(servidor.listen(QHostAddress::LocalHost));
        QByteArray recibido;
        connect(&servidor, &QTcpServer::newConnection, this, [&] {
            QTcpSocket *s = servidor.nextPendingConnection();
            connect(s, &QTcpSocket::readyRead, s, [s, &recibido] {
                recibido += s->readAll();
                if (!recibido.contains("\r\n\r\n") || !recibido.endsWith('}'))
                    return;     // falta el cuerpo
                const QByteArray cuerpo = recibido.startsWith("POST /mal")
                                              ? QByteArray(R"({"error": "Pedido inválido."})") : QByteArray(R"({"id": "abc"})");
                s->write((recibido.startsWith("POST /mal") ? "HTTP/1.1 400 Bad Request\r\n" : "HTTP/1.1 201 Created\r\n")
                         + QByteArray("Content-Type: application/json\r\nContent-Length: ") + QByteArray::number(cuerpo.size())
                         + "\r\nConnection: close\r\n\r\n" + cuerpo);
                s->disconnectFromHost();
            });
        });
        const QUrl base(QStringLiteral("http://127.0.0.1:%1/").arg(servidor.serverPort()));
        int codigo = -1;
        QByteArray respuesta;
        http::enviar(this, base.resolved(QUrl(QStringLiteral("bien"))), R"({"urls": ["ñ"]})",
                     [&](int c, const QByteArray &r) { codigo = c; respuesta = r; });
        QTRY_COMPARE(codigo, 201);
        QCOMPARE(respuesta, QByteArray(R"({"id": "abc"})"));
        QVERIFY(recibido.startsWith("POST /bien"));
        QVERIFY(recibido.contains("application/json"));
        QVERIFY(recibido.endsWith(QByteArray(R"({"urls": ["ñ"]})")));

        recibido.clear();
        codigo = -1;
        http::enviar(this, base.resolved(QUrl(QStringLiteral("mal"))), "{}", [&](int c, const QByteArray &r) { codigo = c; respuesta = r; });
        QTRY_COMPARE(codigo, 400);
        QCOMPARE(QString::fromUtf8(respuesta), QStringLiteral(R"({"error": "Pedido inválido."})"));
    }
};

QTEST_MAIN(TstRed)
#include "tst_red.moc"
