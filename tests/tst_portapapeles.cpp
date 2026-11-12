#include "portapapeles.h"
#include <QtTest>

namespace {
Producto ejemplo()
{
    Producto p;
    p.codigo = "A1";
    p.nombre = "Yerba Mate Suave";
    p.categoria = "Infusiones";
    p.proveedor = "ZUCCHI";
    p.cantidad = 7;
    p.stockMinimo = 2;
    p.vencimiento = "2027-01-31";
    p.medidaBolsa = "10x20/12x25";
    p.notas = "una nota | otra";
    p.precios = {{"500 g", 4000}, {"1000 g", 7500.5}};
    return p;
}
}

class TstPortapapeles : public QObject
{
    Q_OBJECT
private slots:
    void textoConTitulosYUnaFilaPorProducto()
    {
        Producto b; b.nombre = "Sal"; b.cantidad = 3;
        const QString t = productosATexto({ejemplo(), b});
        const QStringList filas = t.trimmed().split('\n');
        QCOMPARE(filas.size(), 3);
        QCOMPARE(filas[0], QString("Código\tNombre\tPrecios\tCategoría\tProveedor\tCantidad\tMedida bolsa\tVencimiento\tStock mín."));
        QCOMPARE(filas[1], QString("A1\tYerba Mate Suave\t500 g: $ 4.000 · 1000 g: $ 7.500,50\tInfusiones\tZUCCHI\t7\t10x20/12x25\t31/01/2027\t2"));
        QCOMPARE(filas[2].split('\t').size(), 9);                       // las celdas vacías también cuentan
    }

    void lasCeldasNoRompenLasColumnas()
    {
        Producto p; p.nombre = "Con\ttab y\nsalto";
        const QStringList filas = productosATexto({p}).trimmed().split('\n');
        QCOMPARE(filas.size(), 2);
        QCOMPARE(filas[1].split('\t')[1], QString("Con tab y salto"));
    }

    void idaYVueltaPorTexto()
    {
        const auto l = productosDesdeTexto(productosATexto({ejemplo()}));
        QCOMPARE(l.productos.size(), 1);
        const Producto& p = l.productos.first();
        const Producto e = ejemplo();
        QCOMPARE(p.codigo, e.codigo);
        QCOMPARE(p.nombre, e.nombre);
        QCOMPARE(p.categoria, e.categoria);
        QCOMPARE(p.proveedor, e.proveedor);
        QCOMPARE(p.cantidad, e.cantidad);
        QCOMPARE(p.stockMinimo, e.stockMinimo);
        QCOMPARE(p.vencimiento, e.vencimiento);
        QCOMPARE(p.medidaBolsa, e.medidaBolsa);
        QCOMPARE(p.precios.size(), 2);
        QCOMPARE(p.precios[0].presentacion, QString("500 g"));
        QCOMPARE(p.precios[0].precio, 4000.0);
        QCOMPARE(p.precios[1].precio, 7500.5);
    }

    void idaYVueltaPorLaVersionCompleta()
    {
        const auto lista = productosDesdeJson(productosAJson({ejemplo()}));
        QCOMPARE(lista.size(), 1);
        QCOMPARE(lista[0].notas, QString("una nota | otra"));             // el texto no lleva notas; esta versión sí
        QCOMPARE(lista[0].precios[1].presentacion, QString("1000 g"));
        QCOMPARE(lista[0].vencimiento, QString("2027-01-31"));
        QVERIFY(productosDesdeJson("esto no es json").isEmpty());
    }

    void titulosEnOtroOrdenYConOtrasPalabras()
    {
        // como si viniera de una planilla propia: columnas en otro orden, mayúsculas, sin tildes, una que no se usa
        const QString t = "PRODUCTO\tCantidad\tPrecio\tObservación\tVto\tStock minimo\n"
                          "Avena fina\t12\t$ 1.900\tlo que sea\t15/08/2027\t3\n";
        const auto l = productosDesdeTexto(t);
        QCOMPARE(l.productos.size(), 1);
        const Producto& p = l.productos.first();
        QCOMPARE(p.nombre, QString("Avena fina"));
        QCOMPARE(p.cantidad, 12);
        QCOMPARE(p.precios.size(), 1);
        QCOMPARE(p.precios[0].precio, 1900.0);
        QCOMPARE(p.vencimiento, QString("2027-08-15"));
        QCOMPARE(p.stockMinimo, 3);
    }

    void sinTitulosUsaElOrdenDeLaTabla()
    {
        const auto l = productosDesdeTexto("\tArroz yamani\t500 g: $ 2.000\tARROCES\tZUCCHI\t4\t13X20\t31/05/2027\t1\n");
        QCOMPARE(l.productos.size(), 1);
        QCOMPARE(l.productos[0].nombre, QString("Arroz yamani"));
        QCOMPARE(l.productos[0].categoria, QString("ARROCES"));
        QCOMPARE(l.productos[0].cantidad, 4);
        QCOMPARE(l.productos[0].vencimiento, QString("2027-05-31"));
    }

    void unaSolaColumnaSonNombres()
    {
        const auto l = productosDesdeTexto("Chía\nLino\n\nSésamo\n");
        QCOMPARE(l.productos.size(), 3);
        QCOMPARE(l.productos[2].nombre, QString("Sésamo"));
    }

    void renglonesSinNombreSeAvisan()
    {
        const auto l = productosDesdeTexto("Nombre\tCantidad\nYerba\t2\n\t5\n");
        QCOMPARE(l.productos.size(), 1);
        QCOMPARE(l.avisos.size(), 1);
        QVERIFY(l.avisos.first().contains("Renglón 3"));
    }

    void celdasConComillasComoLasPegaExcel()
    {
        const auto filas = partirTabulado("Nombre\tNotas\r\n\"Yerba \"\"especial\"\"\"\t\"línea 1\nlínea 2\"\r\nSal\t\r\n");
        QCOMPARE(filas.size(), 3);
        QCOMPARE(filas[1][0], QString("Yerba \"especial\""));
        QCOMPARE(filas[1][1], QString("línea 1\nlínea 2"));
        QCOMPARE(filas[2], (QStringList{"Sal", ""}));
        const auto l = productosDesdeTexto("Nombre\tNotas\n\"Yerba\"\t\"a\nb\"\n");
        QCOMPARE(l.productos[0].notas, QString("a\nb"));
    }

    void formatosDePrecio()
    {
        QCOMPARE(preciosDesdeTexto("500 g: $ 4.000 · 250 g: $ 2.200").size(), 2);
        QCOMPARE(preciosDesdeTexto("500 g: 4000 | 250 g: 2200")[1].precio, 2200.0);   // como lo exporta el CSV
        QCOMPARE(preciosDesdeTexto("$ 1.500,50")[0].precio, 1500.5);
        QCOMPARE(preciosDesdeTexto("$ 1.500,50")[0].presentacion, QString());
        QVERIFY(preciosDesdeTexto("SV").isEmpty());                      // texto que no es precio: se ignora
        QVERIFY(preciosDesdeTexto("").isEmpty());
    }

    void fechasYNumerosRaros()
    {
        const QString t = "Nombre\tVencimiento\tCantidad\n"
                          "A\t31/01/27\t-3\n"       // año de 2 cifras; cantidad negativa -> 0
                          "B\t2027-02-28\tdiez\n"   // formato ISO; texto -> 0
                          "C\t01/27\t4\n";          // mes/año no es fecha completa -> sin vencimiento
        const auto l = productosDesdeTexto(t);
        QCOMPARE(l.productos.size(), 3);
        QCOMPARE(l.productos[0].vencimiento, QString("2027-01-31"));
        QCOMPARE(l.productos[0].cantidad, 0);
        QCOMPARE(l.productos[1].vencimiento, QString("2027-02-28"));
        QCOMPARE(l.productos[1].cantidad, 0);
        QCOMPARE(l.productos[2].vencimiento, QString());
        QCOMPARE(l.productos[2].cantidad, 4);
    }

    void textoVacio()
    {
        QVERIFY(productosDesdeTexto("").productos.isEmpty());
        QVERIFY(productosDesdeTexto("\n\n  \n").productos.isEmpty());
    }
};

QTEST_MAIN(TstPortapapeles)
#include "tst_portapapeles.moc"
