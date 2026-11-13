// Pruebas de red/: servidor.json y comparación de versiones (sin conectarse a nada)
#include "Actualizaciones.h"
#include "ConfigServidor.h"

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
};

QTEST_MAIN(TstRed)
#include "tst_red.moc"
