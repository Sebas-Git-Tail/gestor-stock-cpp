#include "util.h"
#include <QtTest>

class TstUtil : public QObject
{
    Q_OBJECT
private slots:
    void parsearPrecio_data()
    {
        QTest::addColumn<QString>("texto");
        QTest::addColumn<bool>("valido");
        QTest::addColumn<double>("valor");
        QTest::newRow("entero") << "4000" << true << 4000.0;
        QTest::newRow("con signo y miles") << "$ 4.000" << true << 4000.0;
        QTest::newRow("argentino") << "1.500,50" << true << 1500.5;
        QTest::newRow("coma decimal") << "1500,5" << true << 1500.5;
        QTest::newRow("punto decimal") << "1500.5" << true << 1500.5;
        QTest::newRow("vacío") << "" << false << 0.0;
        QTest::newRow("texto") << "abc" << false << 0.0;
        QTest::newRow("negativo") << "-5" << false << 0.0;
        QTest::newRow("dos comas") << "1,5,3" << false << 0.0;
    }
    void parsearPrecio()
    {
        QFETCH(QString, texto);
        QFETCH(bool, valido);
        QFETCH(double, valor);
        double v = -1;
        QCOMPARE(::parsearPrecio(texto, &v), valido);
        if (valido) QCOMPARE(v, valor);
    }

    void formatearPrecio()
    {
        QCOMPARE(::formatearPrecio(4000), QString("$ 4.000"));
        QCOMPARE(::formatearPrecio(1500.5), QString("$ 1.500,50"));
        QCOMPARE(::formatearPrecio(0), QString("$ 0"));
        QCOMPARE(::formatearPrecio(1234567), QString("$ 1.234.567"));
        QCOMPARE(textoPrecios({{"500 g", 4000}, {"", 2200}}), QString("500 g: $ 4.000 · $ 2.200"));
    }

    void estadoVencimiento()
    {
        const QDate hoy(2026, 10, 20);
        QVERIFY(::estadoVencimiento("", hoy) == EstadoVencimiento::Ninguno);
        QVERIFY(::estadoVencimiento("2026-10-19", hoy) == EstadoVencimiento::Vencido);
        QVERIFY(::estadoVencimiento("2026-10-20", hoy) == EstadoVencimiento::PorVencer);   // vence hoy
        QVERIFY(::estadoVencimiento("2026-11-19", hoy) == EstadoVencimiento::PorVencer);   // justo 30 días
        QVERIFY(::estadoVencimiento("2026-11-20", hoy) == EstadoVencimiento::Ok);
        QCOMPARE(*diasParaVencer("2026-10-25", hoy), 5);
    }

    void normalizarBusqueda()
    {
        QCOMPARE(::normalizarBusqueda("AZÚCAR Mascabó"), QString("azucar mascabo"));
        QCOMPARE(::normalizarBusqueda("Pezuña"), QString("pezuna"));
    }

    // ---------------------------------------------------------------- búsqueda por precio y texto
    void filtroDePrecio()
    {
        QVERIFY(interpretarFiltroPrecio("4000")->coincide(4000));
        QVERIFY(!interpretarFiltroPrecio("4000")->coincide(4001));
        QVERIFY(!interpretarFiltroPrecio("4000")->soloPrecio);          // un número suelto también busca como texto
        const auto rango = interpretarFiltroPrecio("2000-5000");
        QVERIFY(rango->soloPrecio && rango->coincide(2000) && rango->coincide(5000) && !rango->coincide(5001));
        QVERIFY(interpretarFiltroPrecio("5000-2000")->coincide(3000));  // al revés también
        QVERIFY(interpretarFiltroPrecio("<3000")->coincide(2999) && !interpretarFiltroPrecio("<3000")->coincide(3000));
        QVERIFY(interpretarFiltroPrecio("<=3000")->coincide(3000));
        QVERIFY(interpretarFiltroPrecio(">3000")->coincide(3001) && !interpretarFiltroPrecio(">3000")->coincide(3000));
        QVERIFY(interpretarFiltroPrecio(">=3000")->coincide(3000));
        QVERIFY(interpretarFiltroPrecio("$1.500,50")->coincide(1500.5));
        QVERIFY(!interpretarFiltroPrecio("avena"));
        QVERIFY(!interpretarFiltroPrecio("pan-dulce"));
    }

    void buscar()
    {
        Producto a; a.id = 1; a.nombre = "AZÚCAR MASCABO"; a.categoria = "AZUCARES"; a.proveedor = "ZUCCHI"; a.precios = {{"500 g", 4000}, {"250 g", 2200}};
        Producto b; b.id = 2; b.nombre = "Avena instantánea 500 g"; b.categoria = "AVENAS"; b.proveedor = "SMILE"; b.precios = {{"", 1800}};
        Producto c; c.id = 3; c.nombre = "Miel"; c.codigo = "M-4000"; c.precios = {};
        const QList<Producto> todos = {a, b, c};
        auto ids = [&](const QString& q) { QList<qint64> r; for (const auto& p : filtrarProductos(todos, q)) r << p.id; return r; };

        QCOMPARE(ids(""), (QList<qint64>{1, 2, 3}));
        QCOMPARE(ids("azucar"), (QList<qint64>{1}));                 // sin tildes ni mayúsculas
        QCOMPARE(ids("AZÚCAR"), (QList<qint64>{1}));
        QCOMPARE(ids("zucchi"), (QList<qint64>{1}));                  // proveedor
        QCOMPARE(ids("4000"), (QList<qint64>{1, 3}));                 // precio 4000 (a) y texto "M-4000" (c)
        QCOMPARE(ids("2200"), (QList<qint64>{1}));                    // cualquiera de los precios del producto
        QCOMPARE(ids("1000-2500"), (QList<qint64>{1, 2}));            // rango: 2200 (a) y 1800 (b)
        QCOMPARE(ids("1000 - 2500"), (QList<qint64>{1, 2}));          // con espacios
        QCOMPARE(ids("<2000"), (QList<qint64>{2}));
        QCOMPARE(ids("> 3000"), (QList<qint64>{1}));                  // con espacio después del símbolo
        QCOMPARE(ids("500"), (QList<qint64>{2}));                     // texto "500 g" del nombre; ningún precio vale 500
        QCOMPARE(ids("$ 4.000"), (QList<qint64>{1}));                 // con signo y espacio (solo el precio, no el código M-4000)
        QCOMPARE(ids("1800 $"), (QList<qint64>{2}));
        QCOMPARE(ids("avena 1800"), (QList<qint64>{2}));              // varias palabras: todas tienen que coincidir
        QCOMPARE(ids("avena 4000"), (QList<qint64>{}));
        QCOMPARE(ids("inexistente"), (QList<qint64>{}));
    }

    // ---------------------------------------------------------------- columnas propias
    void numerosConSigno()
    {
        double v = 0;
        QVERIFY(::parsearNumero("-3,5", &v));       QCOMPARE(v, -3.5);
        QVERIFY(::parsearNumero("-1.500", &v));     QCOMPARE(v, -1500.0);       // el signo no confunde los miles
        QVERIFY(::parsearNumero("1.234,5", &v));    QCOMPARE(v, 1234.5);
        QVERIFY(::parsearNumero("+7", &v));         QCOMPARE(v, 7.0);
        QVERIFY(!::parsearNumero("abc", &v));
        QVERIFY(!::parsearNumero("", &v));
        QVERIFY(!::parsearNumero("1,5,3", &v));
        double p = 0;
        QVERIFY(!::parsearPrecio("-5", &p));                                     // los precios siguen sin aceptar negativos
        QVERIFY(::parsearPrecio("1.500,50", &p));   QCOMPARE(p, 1500.5);
    }

    void formatoDeNumeros()
    {
        QCOMPARE(formatearNumero(12), QString("12"));
        QCOMPARE(formatearNumero(1234.5), QString("1.234,5"));
        QCOMPARE(formatearNumero(1234567.25), QString("1.234.567,25"));
        QCOMPARE(formatearNumero(-0.5), QString("-0,5"));
        QCOMPARE(formatearNumero(0.0), QString("0"));
        QCOMPARE(formatearNumero(2.000000), QString("2"));
        QCOMPARE(formatearNumero(0.125), QString("0,125"));
    }

    void valoresDeColumnasSegunSuTipo()
    {
        QString g;
        QVERIFY(normalizarValorColumna(TipoColumna::Numero, " 1.234,5 ", &g));   QCOMPARE(g, QString("1234.5"));
        QVERIFY(normalizarValorColumna(TipoColumna::Numero, "", &g));            QCOMPARE(g, QString());      // vacío es válido
        QVERIFY(!normalizarValorColumna(TipoColumna::Numero, "mucho", &g));
        QVERIFY(normalizarValorColumna(TipoColumna::Entero, "12", &g));          QCOMPARE(g, QString("12"));
        QVERIFY(!normalizarValorColumna(TipoColumna::Entero, "1,5", &g));
        QVERIFY(normalizarValorColumna(TipoColumna::Fecha, "01/11/2026", &g));   QCOMPARE(g, QString("2026-11-01"));
        QVERIFY(normalizarValorColumna(TipoColumna::Fecha, "1/11/26", &g));      QCOMPARE(g, QString("2026-11-01"));
        QVERIFY(normalizarValorColumna(TipoColumna::Fecha, "2026-11-01", &g));   QCOMPARE(g, QString("2026-11-01"));
        QVERIFY(!normalizarValorColumna(TipoColumna::Fecha, "31/02/2026", &g));
        QVERIFY(!normalizarValorColumna(TipoColumna::Fecha, "ayer", &g));
        QVERIFY(normalizarValorColumna(TipoColumna::Texto, "  hola  mundo ", &g)); QCOMPARE(g, QString("hola  mundo"));

        QCOMPARE(textoValorColumna(TipoColumna::Numero, "1234.5"), QString("1.234,5"));
        QCOMPARE(textoValorColumna(TipoColumna::Numero, "1.234"), QString("1,234"));    // lo guardado usa el punto como decimal
        QCOMPARE(textoValorColumna(TipoColumna::Numero, "texto raro"), QString("texto raro"));   // si no encaja, se ve tal cual
        QCOMPARE(textoValorColumna(TipoColumna::Fecha, "2026-11-01"), QString("01/11/2026"));
        QCOMPARE(textoValorColumna(TipoColumna::Texto, "hola"), QString("hola"));
        QCOMPARE(textoValorColumna(TipoColumna::Fecha, ""), QString());

        QVERIFY(valorOrdenColumna(TipoColumna::Numero, "9").toDouble() < valorOrdenColumna(TipoColumna::Numero, "10").toDouble());
        QVERIFY(valorOrdenColumna(TipoColumna::Numero, "").toDouble() > valorOrdenColumna(TipoColumna::Numero, "999999").toDouble());   // sin valor: al final
        QVERIFY(valorOrdenColumna(TipoColumna::Fecha, "2026-01-01").toString() < valorOrdenColumna(TipoColumna::Fecha, "").toString());
        QCOMPARE(valorOrdenColumna(TipoColumna::Texto, "Hola"), QVariant(QString("hola")));
    }

    void laBusquedaEntiendeLosNumerosComoSeEscriben()
    {
        Producto a; a.id = 1; a.nombre = "Avena"; a.extra = {{"u1", "1234.5"}};     // así se guarda un «1.234,5»
        QVERIFY(productoCoincide(a, "1234.5"));
        QVERIFY(productoCoincide(a, "1.234,5"));
        QVERIFY(productoCoincide(a, "1234,5"));
        QVERIFY(!productoCoincide(a, "1.234,6"));
    }

    void laBusquedaMiraLasColumnasPropias()
    {
        Producto a; a.id = 1; a.nombre = "Avena"; a.extra = {{"u1", "Krocante"}};
        Producto b; b.id = 2; b.nombre = "Sal"; b.extra = {{"u1", "Celusal"}, {"u2", "2026-11-01"}};
        QVERIFY(productoCoincide(a, "krocante"));
        QVERIFY(!productoCoincide(b, "krocante"));
        QVERIFY(productoCoincide(b, "celusal sal"));                  // junto con las otras palabras
        QVERIFY(productoCoincide(b, "2026-11-01"));                   // (una fecha entera; «2026-11» sería un rango de precios)
    }
};

QTEST_MAIN(TstUtil)
#include "tst_util.moc"
