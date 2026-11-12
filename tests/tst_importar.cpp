#include "db.h"
#include "importarexcel.h"
#include "util.h"
#include <QtTest>

namespace {
const ProductoImportado* buscar(const LecturaPlanilla& l, const QString& nombre)
{
    for (const ProductoImportado& p : l.productos) if (p.nombre == nombre) return &p;
    return nullptr;
}
}

class TstImportar : public QObject
{
    Q_OBJECT
    LecturaPlanilla l_;

private slots:
    void initTestCase()
    {
        l_ = leerPlanilla(QStringLiteral(DATOS_DIR) + "/lista.xlsx");
        QVERIFY2(l_.ok, qPrintable(l_.error));
    }

    void archivoQueNoEsExcel()
    {
        QVERIFY(!leerPlanilla(QStringLiteral(DATOS_DIR) + "/no-existe.xlsx").ok);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("esto no es un zip");
        f.close();
        const auto r = leerPlanilla(f.fileName());
        QVERIFY(!r.ok);
        QVERIFY(!r.error.isEmpty());
    }

    void etiquetasDePrecioPorBloque()
    {
        const auto* p = buscar(l_, "AZUCAR MASCABO");
        QVERIFY(p);
        QCOMPARE(p->precios.size(), 2);
        QCOMPARE(p->precios[0].presentacion, QString("500 g"));
        QCOMPARE(p->precios[0].precio, 4000.0);
        QCOMPARE(p->precios[1].presentacion, QString("250 g"));
        QCOMPARE(p->categoria, QString("AZUCARES"));
        QCOMPARE(p->proveedor, QString("ZUCCHI"));
        const auto* coco = buscar(l_, "AZUCAR COCO");
        QCOMPARE(coco->precios.size(), 1);
        QCOMPARE(coco->precios[0].presentacion, QString("200 g"));          // el precio está en la 3ª columna
    }

    void vencimientos()
    {
        QCOMPARE(buscar(l_, "AZUCAR MASCABO")->vencimiento, QString("2027-01-31"));   // MM/YY -> último día del mes
        QCOMPARE(buscar(l_, "SAL ROSADA")->vencimiento, QString("2029-02-28"));
        QCOMPARE(buscar(l_, "LUPINES")->vencimiento, QString("2026-06-03"));          // una fecha de verdad en la celda
        QVERIFY(buscar(l_, "MANI CRUDO")->vencimiento.isEmpty());
        QVERIFY(buscar(l_, "MANI CRUDO")->notas.contains("u/26"));
        QVERIFY(buscar(l_, "ICE TEA x 10 saq")->notas.contains("0.5"));
    }

    void medidaDeLaBolsa()
    {
        QCOMPARE(buscar(l_, "AZUCAR MASCABO")->medidaBolsa, QString("10x20/12x25"));   // X mayúscula unificada
        QCOMPARE(buscar(l_, "RIKAS INTEGRALES x 150 g")->medidaBolsa, QString("12x20"));
    }

    void gruposSeAntePonenYPasanElProveedor()
    {
        QVERIFY(!buscar(l_, "ACEITE OLIVA"));                                        // el grupo no es un producto
        const auto* hijo = buscar(l_, "ACEITE OLIVA — EBENEZER x 1 L");
        QVERIFY(hijo);
        QCOMPARE(hijo->proveedor, QString("ZUCCHI"));
        QCOMPARE(hijo->precios.first().presentacion, QString(""));
        QCOMPARE(buscar(l_, "ACEITE OLIVA — GRIEGO x 500 ml")->precios.first().presentacion, QString("Precio 3"));
        QCOMPARE(buscar(l_, "INTEGRA — PLATEADA")->proveedor, QString("CALFRA"));
        QCOMPARE(buscar(l_, "GHEE x 300 gr")->categoria, QString("ACEITES"));        // suelto: sin prefijo
    }

    void aclaracionesYPreciosConTexto()
    {
        QCOMPARE(buscar(l_, "RIKAS INTEGRALES x 150 g")->notas, QString("(cacao, arandanos, naranja)").mid(0));
        QCOMPARE(buscar(l_, "BOLDO")->proveedor, QString("SMILE"));                  // hereda el de la categoría YUYITOS
        QCOMPARE(buscar(l_, "BOLDO")->precios.first().presentacion, QString("25 g"));
        const auto* artemisa = buscar(l_, "ARTEMISA");
        QVERIFY(artemisa->precios.isEmpty());
        QVERIFY(artemisa->notas.contains("SV"));
        bool hayAviso = false;
        for (const QString& a : l_.avisos) if (a.contains("SV")) hayAviso = true;
        QVERIFY(hayAviso);
    }

    void parrafosLargosSeIgnoran()
    {
        for (const auto& p : l_.productos) QVERIFY(!p.nombre.startsWith("KOMBU"));
        bool aviso = false;
        for (const QString& a : l_.avisos) if (a.contains("párrafo")) aviso = true;
        QVERIFY(aviso);
    }

    void noSePierdenProductos()
    {
        QStringList nombres;
        for (const auto& p : l_.productos) nombres << p.nombre;
        nombres.sort();
        QStringList esperados = {"AZUCAR MASCABO", "AZUCAR COCO", "ACEITE OLIVA — EBENEZER x 1 L", "ACEITE OLIVA — GRIEGO x 500 ml",
                                 "GHEE x 300 gr", "INTEGRA — PLATEADA", "INTEGRA — COMUNES", "RIKAS INTEGRALES x 150 g", "BOLDO",
                                 "ARTEMISA", "MANI CRUDO", "SAL ROSADA", "LUPINES", "ICE TEA x 10 saq"};
        esperados.sort();
        QCOMPARE(nombres, esperados);
    }

    void deExcelALaBase()
    {
        Stock s(":memory:");
        QCOMPARE(s.importar(l_.productos), qMakePair(14, 0));
        QCOMPARE(s.importar(l_.productos), qMakePair(0, 14));                       // reimportar no duplica
        for (const Producto& p : s.listar()) {
            if (p.nombre == "AZUCAR MASCABO") QCOMPARE(textoPrecios(p.precios), QString("500 g: $ 4.000 · 250 g: $ 2.200"));
        }
    }

    // ---------------------------------------------------------------- piezas sueltas
    void normalizarEtiqueta_data()
    {
        QTest::addColumn<QString>("entrada");
        QTest::addColumn<QString>("esperada");
        QTest::newRow("grs") << "500grs" << "500 g";
        QTest::newRow("GRS mayúsculas") << "1000GRS" << "1000 g";
        QTest::newRow("ya normal") << "250 g" << "250 g";
        QTest::newRow("sin espacio") << "200g " << "200 g";
        QTest::newRow("x unidad") << "x UNIDAD" << "unidad";
        QTest::newRow("por unidad") << "por unidad " << "unidad";
        QTest::newRow("doble") << "100/150 g" << "100/150 g";
        QTest::newRow("con x") << "x100grs" << "100 g";
        QTest::newRow("GR") << "50 GR" << "50 g";
        QTest::newRow("raro") << "raro" << "raro";
    }
    void normalizarEtiqueta()
    {
        QFETCH(QString, entrada);
        QFETCH(QString, esperada);
        QCOMPARE(::normalizarEtiqueta(entrada), esperada);
    }

    void vencimientosSueltos()
    {
        QCOMPARE(parsearVencimiento(Celda::deTexto("02/29")).iso, QString("2029-02-28"));
        QCOMPARE(parsearVencimiento(Celda::deTexto("11/2026")).iso, QString("2026-11-30"));
        QCOMPARE(parsearVencimiento(Celda::deTexto("15/08/2027")).iso, QString("2027-08-15"));
        QVERIFY(parsearVencimiento(Celda::deTexto("13/27")).iso.isEmpty());                       // mes inválido
        QCOMPARE(parsearVencimiento(Celda::deTexto("u/26")).nota, QString("Vto: u/26"));
        QCOMPARE(parsearVencimiento(Celda::deNumero(2)).nota, QString("Dato en Vto: 2"));
        QVERIFY(parsearVencimiento(Celda()).iso.isEmpty() && parsearVencimiento(Celda()).nota.isEmpty());
    }
};

QTEST_MAIN(TstImportar)
#include "tst_importar.moc"
