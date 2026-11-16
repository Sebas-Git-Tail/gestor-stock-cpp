// Pruebas de apps/: catálogo, elección de versión, listados de GitHub y del servidor, pacman -Q y el
// modelo de «Instalar otras apps» (sin red: no se llama a buscar()).
#include "CatalogoApps.h"
#include "ModeloApps.h"
#include "PanelApps.h"
#include "Traducciones.h"

#include <QDialogButtonBox>

#include <QPushButton>

#include <QRegularExpression>
#include <QtTest>

using namespace apps;

class PruebaApps : public QObject
{
    Q_OBJECT

private slots:
    void ocultasNuncaSeListan()
    {
        for (Sistema s : {Sistema::Linux, Sistema::Android, Sistema::Windows}) {
            for (const QString &desde : {QStringLiteral("Carina"), QStringLiteral("Bodrio-Supremo"),
                                         QStringLiteral("gestor-stock"), QStringLiteral("Windows-Manager")}) {
                for (const InfoApp &a : paraInstalar(desde, s)) {
                    QVERIFY2(a.id != QLatin1String("Carina"), qPrintable(desde));
                    QVERIFY2(a.id != QLatin1String("Bodrio-Supremo"), qPrintable(desde));
                    QVERIFY(a.id != desde);          // nunca la propia app
                    QVERIFY(a.disponibleEn(s));
                }
            }
        }
    }

    void ocultasYPropiasSeActualizan()
    {
        // Carina y Bodrio no se listan, pero tienen archivo para actualizarse
        QVERIFY(buscar(QStringLiteral("Carina"))->oculta);
        QVERIFY(buscar(QStringLiteral("Carina"))->disponibleEn(Sistema::Windows));
        QVERIFY(buscar(QStringLiteral("Bodrio-Supremo"))->disponibleEn(Sistema::Android));
        QVERIFY(!buscar(QStringLiteral("no-existe")));
    }

    void quienPuedeInstalarQue()
    {
        auto ids = [](const QList<InfoApp> &l) {
            QStringList r;
            for (const InfoApp &a : l) r << a.id;
            return r;
        };
        QCOMPARE(ids(paraInstalar(QStringLiteral("Carina"), Sistema::Windows)), QStringList{QStringLiteral("Windows-Manager")});
        QCOMPARE(ids(paraInstalar(QStringLiteral("Bodrio-Supremo"), Sistema::Android)),
                 (QStringList{QStringLiteral("gestor-stock"), QStringLiteral("ingresos-gastos"), QStringLiteral("Android-Manager")}));
        QCOMPARE(ids(paraInstalar(QStringLiteral("gestor-stock"), Sistema::Linux)),
                 (QStringList{QStringLiteral("Arch-Linux-Manager"), QStringLiteral("ingresos-gastos")}));
    }

    void patronesCoincidenConLosArchivosReales()
    {
        const struct { const char *id; Sistema s; const char *archivo; const char *version; } casos[] = {
            {"Arch-Linux-Manager", Sistema::Linux, "Arch-Linux-Manager-4.3.1-1-x86_64.pkg.tar.zst", "4.3.1.1"},
            {"gestor-stock", Sistema::Linux, "gestor-stock-2.4.0-1-x86_64.pkg.tar.zst", "2.4.0.1"},
            {"gestor-stock", Sistema::Android, "gestor-stock-movil-2.4.0.apk", "2.4.0"},
            {"ingresos-gastos", Sistema::Android, "ingresos-gastos-movil-2.1.3.apk", "2.1.3"},
            {"Android-Manager", Sistema::Android, "Android-Manager-1.2.0.apk", "1.2.0"},
            {"Windows-Manager", Sistema::Windows, "Windows-Manager-1.1.0-instalador.exe", "1.1.0"},
            {"Bodrio-Supremo", Sistema::Linux, "bodrio-supremo-1.5.0-1-x86_64.pkg.tar.zst", "1.5.0.1"},
            {"Bodrio-Supremo", Sistema::Windows, "Bodrio-Supremo-1.5.0-instalador.exe", "1.5.0"},
            {"Carina", Sistema::Windows, "Carina-1.0.1-instalador.exe", "1.0.1"},
        };
        for (const auto &c : casos) {
            const InfoApp *a = buscar(QLatin1String(c.id));
            QVERIFY(a);
            const Candidato e = elegir({{QLatin1String(c.archivo), QUrl(QStringLiteral("https://x/"))}}, a->archivo(c.s));
            QVERIFY2(e.valido(), c.archivo);
            QCOMPARE(e.version, QVersionNumber::fromString(QLatin1String(c.version)));
        }
        // Lo que no tiene que confundirse: el .tar portable, el paquete -debug, otra app con nombre parecido
        const InfoApp *b = buscar(QStringLiteral("Bodrio-Supremo"));
        QVERIFY(!elegir({{QStringLiteral("Bodrio-Supremo-Windows-1.5.0.tar"), {}}}, b->archivoWindows).valido());
        QVERIFY(!elegir({{QStringLiteral("bodrio-supremo-debug-1.5.0-1-x86_64.pkg.tar.zst"), {}}}, b->archivoLinux).valido());
        const InfoApp *g = buscar(QStringLiteral("gestor-stock"));
        QVERIFY(!elegir({{QStringLiteral("gestor-stock-2.4.0-1-x86_64.pkg.tar.zst.sig"), {}}}, g->archivoLinux).valido());
    }

    void eligeLaMasNueva()
    {
        const InfoApp *g = buscar(QStringLiteral("gestor-stock"));
        const ListaArchivos lista = {
            {QStringLiteral("gestor-stock-2.9.0-1-x86_64.pkg.tar.zst"), QUrl(QStringLiteral("https://a/1"))},
            {QStringLiteral("gestor-stock-2.10.0-1-x86_64.pkg.tar.zst"), QUrl(QStringLiteral("https://a/2"))},
            {QStringLiteral("gestor-stock-2.10.0-2-x86_64.pkg.tar.zst"), QUrl(QStringLiteral("https://a/3"))},
            {QStringLiteral("gestor-stock-movil-9.9.9.apk"), QUrl(QStringLiteral("https://a/4"))},
        };
        const Candidato e = elegir(lista, g->archivoLinux);
        QCOMPARE(e.url, QUrl(QStringLiteral("https://a/3")));     // 2.10 > 2.9 y pkgrel 2 > 1
        QCOMPARE(e.version, QVersionNumber({2, 10, 0, 2}));
        QVERIFY(!elegir({}, g->archivoLinux).valido());
    }

    void versiones()
    {
        QCOMPARE(versionDeTexto(QStringLiteral("v1.2.3")), QVersionNumber({1, 2, 3}));
        QCOMPARE(versionDeTexto(QStringLiteral("4.3.1-2")), QVersionNumber({4, 3, 1, 2}));
        QVERIFY(versionDeTexto(QStringLiteral("1.2.3-beta")).isNull());
        QVERIFY(versionDeTexto(QString()).isNull());
        QCOMPARE(versionDePacman(QStringLiteral("gestor-stock 2.4.0-1\n")), QVersionNumber({2, 4, 0, 1}));
        QCOMPARE(versionDePacman(QStringLiteral("algo 1:3.0-5")), QVersionNumber({3, 0, 5}));
        QVERIFY(versionDePacman(QStringLiteral("error: no se encontró el paquete")).isNull());
        QCOMPARE(Actualizador::texto(QVersionNumber({4, 3, 1, 2})), QStringLiteral("4.3.1-2"));
        QCOMPARE(Actualizador::texto(QVersionNumber({1, 2, 0})), QStringLiteral("1.2.0"));
    }

    void listadoDeGitHub()
    {
        // (sin cadenas R"(...)" de varias líneas: moc no las entiende y deja la clase sin generar)
        const QByteArray json =
            "{\"tag_name\":\"v2.4.0\",\"draft\":false,\"prerelease\":false,\"assets\":["
            "{\"name\":\"gestor-stock-movil-2.4.0.apk\",\"browser_download_url\":"
            "\"https://github.com/Sebas-Git-Tail/gestor-stock-cpp/releases/download/v2.4.0/gestor-stock-movil-2.4.0.apk\"},"
            "{\"name\":\"trampa.apk\",\"browser_download_url\":\"http://github.com/x/trampa.apk\"},"
            "{\"name\":\"otra.apk\",\"browser_download_url\":\"https://otro-sitio.com/otra.apk\"}]}";
        const ListaArchivos l = archivosDeGitHub(json);
        QCOMPARE(l.size(), 1);     // solo HTTPS y de github.com
        QCOMPARE(l.first().first, QStringLiteral("gestor-stock-movil-2.4.0.apk"));
        QVERIFY(archivosDeGitHub("{\"prerelease\":true,\"assets\":[{\"name\":\"a\",\"browser_download_url\":\"https://github.com/a\"}]}").isEmpty());
        QVERIFY(archivosDeGitHub("no es json").isEmpty());
    }

    void listadoDelServidor()
    {
        const QByteArray json = R"([{"nombre":"Carina-1.0.1-instalador.exe","carpeta":false},
            {"nombre":"viejas","carpeta":true},{"nombre":"../../etc/passwd","carpeta":false},
            {"nombre":"Con espacio 1.0.exe","carpeta":false}])";
        const ListaArchivos l = archivosDeServidor(json, QUrl(QStringLiteral("https://bodrio.seb-dev.fyi/Carina/")));
        QCOMPARE(l.size(), 2);
        QCOMPARE(l.at(0).second, QUrl(QStringLiteral("https://bodrio.seb-dev.fyi/Carina/Carina-1.0.1-instalador.exe")));
        QCOMPARE(l.at(1).second.toString(QUrl::FullyEncoded), QStringLiteral("https://bodrio.seb-dev.fyi/Carina/Con%20espacio%201.0.exe"));
    }

    void modeloNoMuestraOcultas()
    {
        ModeloApps m(QStringLiteral("Carina"));
        for (int f = 0; f < m.rowCount(); ++f) {
            const QString id = m.index(f).data(ModeloApps::Id).toString();
            QVERIFY(id != QLatin1String("Carina") && id != QLatin1String("Bodrio-Supremo"));
            QVERIFY(!m.index(f).data(ModeloApps::Nombre).toString().isEmpty());
            QVERIFY(m.index(f).data(ModeloApps::Accion).toString().isEmpty());   // sin buscar no ofrece instalar
        }
        QVERIFY(m.roleNames().values().contains("estado"));
        QVERIFY(m.versionNueva().isEmpty());
    }

    void botonesEstandarEnEspanol()
    {
        // Con la traducción cargada, un botón estándar no queda en inglés (en Android decía «Close»)
        apps::cargarTraduccionesQt();
        QDialogButtonBox caja(QDialogButtonBox::Close | QDialogButtonBox::Yes);
        QCOMPARE(caja.button(QDialogButtonBox::Close)->text().remove(u'&'), QStringLiteral("Cerrar"));
        QCOMPARE(caja.button(QDialogButtonBox::Yes)->text().remove(u'&'), QStringLiteral("Sí"));
    }

    void panelSinMostrarNoBusca()
    {
        // Se arma sin mostrarlo (mostrarlo saldría a buscar por la red): un botón por app y la propia
        PanelApps p(QStringLiteral("Bodrio-Supremo"));
        QCOMPARE(p.findChildren<QPushButton *>().size(), p.modelo()->rowCount() + 1);
        for (int f = 0; f < p.modelo()->rowCount(); ++f)
            QVERIFY(p.modelo()->index(f).data(ModeloApps::Estado).toString().contains(QStringLiteral("Buscando")));
    }
};

QTEST_MAIN(PruebaApps)
#include "tst_apps.moc"
