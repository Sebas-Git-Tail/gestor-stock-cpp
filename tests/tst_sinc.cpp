// Pruebas de la sincronización: dos bases (la «PC» y el «celular») que hablan por TCP en esta misma máquina.
#include "db.h"
#include "sincronizacion.h"
#include "util.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {

Producto producto(const QString& nombre, int cantidad = 0, const QString& codigo = {}, QList<Precio> precios = {})
{
    Producto p;
    p.nombre = nombre;
    p.cantidad = cantidad;
    p.codigo = codigo;
    p.precios = precios;
    p.categoria = QStringLiteral("Almacén");
    return p;
}

qint64 idDe(Stock& s, const QString& nombre)
{
    for (const Producto& p : s.listar()) if (p.nombre == nombre) return p.id;
    return 0;
}

Producto porNombre(Stock& s, const QString& nombre)
{
    for (const Producto& p : s.listar()) if (p.nombre == nombre) return p;
    return {};
}

// Lo que importa de una base, ordenado, para comparar la PC con el celular (los números internos son distintos en cada una)
QStringList resumen(Stock& s)
{
    QStringList l;
    QHash<QString, QString> nombreDeColumna;
    for (const Columna& c : s.columnas()) {
        nombreDeColumna.insert(c.uuid, c.nombre);
        l << QStringLiteral("col|%1|%2|%3|%4|%5").arg(c.clave, c.nombre, textoTipoColumna(c.tipo)).arg(c.orden).arg(c.visible ? 1 : 0);
    }
    for (const Producto& p : s.listar()) {
        QStringList extras;
        for (auto it = p.extra.constBegin(); it != p.extra.constEnd(); ++it)
            extras << nombreDeColumna.value(it.key()) + QLatin1Char('=') + it.value();
        extras.sort();
        l << QStringLiteral("prod|%1|%2|%3|%4|%5|%6").arg(p.nombre, p.codigo).arg(p.cantidad).arg(textoPrecios(p.precios), p.vencimiento, extras.join(','));
    }
    l << QStringLiteral("operaciones|%1").arg(s.operaciones(1000).size());
    l.sort();
    return l;
}

struct ResultadoSinc {
    bool ok = false;
    QString mensaje;
    int recibidas = 0, enviadas = 0;
};

ResultadoSinc esperar(ClienteSinc& cli, const std::function<void()>& accion)
{
    QSignalSpy spy(&cli, &ClienteSinc::terminado);
    accion();
    if (!spy.wait(15000)) return {false, "sin respuesta", 0, 0};
    const QList<QVariant> a = spy.takeFirst();
    return {a[0].toBool(), a[1].toString(), a[2].toInt(), a[3].toInt()};
}

} // namespace

class TstSinc : public QObject
{
    Q_OBJECT

    // PC con servidor + celular emparejado, listos para sincronizar
    struct Escenario {
        Stock pc{":memory:"};
        Stock cel{":memory:"};
        ServidorSinc servidor{pc};
        ClienteSinc cliente{cel};
        QString dispPc;
        bool emparejar()
        {
            if (!servidor.iniciar(0, false)) return false;
            const QString cod = servidor.nuevoCodigo();
            QSignalSpy listo(&servidor, &ServidorSinc::sincronizado);
            const ResultadoSinc r = esperar(cliente, [&] { cliente.emparejar("127.0.0.1", servidor.puerto(), cod); });
            dispPc = pc.dispositivo();
            if (r.ok && listo.isEmpty()) listo.wait(5000);       // que la PC también haya terminado
            return r.ok;
        }
        ResultadoSinc sincronizar()
        {
            QSignalSpy listo(&servidor, &ServidorSinc::sincronizado);
            const ResultadoSinc r = esperar(cliente, [&] { cliente.sincronizar(dispPc); });
            if (r.ok && listo.isEmpty()) listo.wait(5000);
            return r;
        }
    };

private slots:
    // ---------------------------------------------------------------- emparejar
    void emparejarConCodigo()
    {
        Stock pc(":memory:"), cel(":memory:");
        ServidorSinc servidor(pc);
        QVERIFY(servidor.iniciar(0, false));
        ClienteSinc cliente(cel);

        // Sin código activo en la PC
        ResultadoSinc r = esperar(cliente, [&] { cliente.emparejar("127.0.0.1", servidor.puerto(), "123456"); });
        QVERIFY(!r.ok);
        QVERIFY2(r.mensaje.contains("código"), qPrintable(r.mensaje));

        // Código equivocado
        const QString cod = servidor.nuevoCodigo();
        QCOMPARE(cod.size(), 6);
        const QString malo = QString::number((cod.toInt() + 1) % 1000000).rightJustified(6, '0');
        r = esperar(cliente, [&] { cliente.emparejar("127.0.0.1", servidor.puerto(), malo); });
        QVERIFY(!r.ok);
        QVERIFY(cel.pares().isEmpty());
        QVERIFY(pc.pares().isEmpty());

        // Bien (con espacios, como se lee en pantalla)
        QSignalSpy emparejado(&servidor, &ServidorSinc::emparejado);
        r = esperar(cliente, [&] { cliente.emparejar("127.0.0.1", servidor.puerto(), cod.left(3) + " " + cod.mid(3)); });
        QVERIFY2(r.ok, qPrintable(r.mensaje));
        QCOMPARE(emparejado.count(), 1);
        QCOMPARE(cel.pares().size(), 1);
        QCOMPARE(pc.pares().size(), 1);
        QCOMPARE(cel.pares().first().secreto, pc.pares().first().secreto);   // la misma clave, sin que viaje
        QCOMPARE(cel.pares().first().dispositivo, pc.dispositivo());
        QVERIFY(servidor.codigo().isEmpty());                                // el código se usa una vez

        // Demasiados intentos con un código: se anula
        servidor.nuevoCodigo();
        ClienteSinc otro(cel);
        for (int i = 0; i < sinc::INTENTOS_CODIGO; ++i)
            esperar(otro, [&] { otro.emparejar("127.0.0.1", servidor.puerto(), "000000"); });
        QVERIFY(servidor.codigo().isEmpty() || servidor.codigo() == "000000");
    }

    void unCelularQuitadoNoSincronizaMas()
    {
        Escenario e;
        QVERIFY(e.emparejar());
        e.pc.eliminarPar(e.cel.dispositivo());                                 // en la PC se quitó el celular
        const ResultadoSinc r = e.sincronizar();
        QVERIFY(!r.ok);
        QVERIFY2(r.mensaje.contains("emparejado") || r.mensaje.contains("Emparej"), qPrintable(r.mensaje));
    }

    // ---------------------------------------------------------------- filas: productos y columnas
    void primeraSincronizacionYCambios()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 10, "A1", {{"500 g", 1900}, {"1000 g", 3500}}));
        e.pc.crear(producto("Sal", 5));
        e.cel.crear(producto("Yerba", 3, "Y1"));
        QVERIFY(e.emparejar());                    // emparejar ya sincroniza
        QCOMPARE(resumen(e.pc), resumen(e.cel));
        QCOMPARE(e.cel.listar().size(), 3);
        QCOMPARE(e.pc.listar().size(), 3);
        QCOMPARE(e.cel.columnas().size(), 10);     // las de fábrica no se duplican
        QCOMPARE(porNombre(e.cel, "Avena").cantidad, 10);
        QCOMPARE(porNombre(e.cel, "Avena").precios.size(), 2);
        QCOMPARE(porNombre(e.pc, "Yerba").codigo, QString("Y1"));

        // Sin cambios: no viaja nada
        ResultadoSinc r = e.sincronizar();
        QVERIFY2(r.ok, qPrintable(r.mensaje));
        QCOMPARE(r.recibidas, 0);
        QCOMPARE(r.enviadas, 0);

        // Cambios en los dos lados
        Producto avena = porNombre(e.pc, "Avena");
        avena.precios = {{"500 g", 2100}};
        avena.vencimiento = "2027-05-01";
        QVERIFY(e.pc.actualizar(avena).ok);
        Producto sal = porNombre(e.cel, "Sal");
        sal.nombre = "Sal fina";
        QVERIFY(e.cel.actualizar(sal).ok);
        QVERIFY(e.cel.eliminar(idDe(e.cel, "Yerba")).ok);
        e.pc.crear(producto("Café", 4));
        r = e.sincronizar();
        QVERIFY2(r.ok, qPrintable(r.mensaje));
        QCOMPARE(resumen(e.pc), resumen(e.cel));
        QCOMPARE(e.pc.listar().size(), 3);         // Avena, Sal fina y Café; Yerba se borró en los dos
        QCOMPARE(porNombre(e.cel, "Avena").precios.first().precio, 2100.0);
        QCOMPARE(porNombre(e.cel, "Avena").vencimiento, QString("2027-05-01"));
        QVERIFY(e.pc.listar("sal fina").size() == 1);
        QVERIFY(e.pc.obtenerPorUuid(porNombre(e.cel, "Café").uuid).has_value());
    }

    void conflictoGanaElCambioMasNuevo()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 10));
        QVERIFY(e.emparejar());

        Producto a = porNombre(e.pc, "Avena");
        a.nombre = "Avena (PC)";
        QVERIFY(e.pc.actualizar(a).ok);
        QTest::qWait(5);                           // el del celular es unos milisegundos más nuevo
        Producto b = porNombre(e.cel, "Avena");
        b.nombre = "Avena (celular)";
        QVERIFY(e.cel.actualizar(b).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(porNombre(e.pc, "Avena (celular)").nombre, QString("Avena (celular)"));
        QCOMPARE(resumen(e.pc), resumen(e.cel));

        // y al revés: ahora el más nuevo es el de la PC
        a = porNombre(e.pc, "Avena (celular)");
        b = porNombre(e.cel, "Avena (celular)");
        b.nombre = "Avena B";
        QVERIFY(e.cel.actualizar(b).ok);
        QTest::qWait(5);
        a.nombre = "Avena A";
        QVERIFY(e.pc.actualizar(a).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(porNombre(e.cel, "Avena A").nombre, QString("Avena A"));
        QCOMPARE(resumen(e.pc), resumen(e.cel));
    }

    void unProductoEliminadoSeVaEnLosDosLadosYLiberaElCodigo()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 10, "A1"));
        QVERIFY(e.emparejar());
        QVERIFY(e.pc.eliminar(idDe(e.pc, "Avena")).ok);
        QVERIFY(e.sincronizar().ok);
        QVERIFY(e.cel.listar().isEmpty());
        QVERIFY(e.cel.crear(producto("Otra avena", 1, "A1")).ok);              // el código quedó libre en el celular también
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(porNombre(e.pc, "Otra avena").codigo, QString("A1"));
        QCOMPARE(resumen(e.pc), resumen(e.cel));
    }

    void elMismoCodigoCargadoEnLosDosLadosQuedaEnUnSoloProducto()
    {
        Escenario e;
        QVERIFY(e.emparejar());
        QVERIFY(e.pc.crear(producto("Yerba de la PC", 1, "A1")).ok);
        QVERIFY(e.cel.crear(producto("Yerba del celular", 1, "A1")).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(e.pc.listar().size(), 2);
        QCOMPARE(resumen(e.pc), resumen(e.cel));                                // los dos ven lo mismo
        // se queda con el código el de uuid más chico (lo decide cualquiera de los dos lados igual); el otro queda sin código
        const Producto x = porNombre(e.pc, "Yerba de la PC"), y = porNombre(e.pc, "Yerba del celular");
        const Producto& conCodigo = x.uuid < y.uuid ? x : y;
        const Producto& sinCodigo = x.uuid < y.uuid ? y : x;
        QCOMPARE(conCodigo.codigo, QString("A1"));
        QVERIFY(sinCodigo.codigo.isEmpty());
        QVERIFY(e.sincronizar().ok);                                            // y no sigue cambiando
        QCOMPARE(resumen(e.pc), resumen(e.cel));
        QCOMPARE(e.sincronizar().enviadas, 0);
    }

    // ---------------------------------------------------------------- columnas
    void lasColumnasPropiasYLoCargadoEnEllasViajan()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 10));
        QVERIFY(e.emparejar());

        const qint64 marca = e.pc.agregarColumna("Marca", TipoColumna::Texto).id;
        const qint64 peso = e.pc.agregarColumna("Peso (kg)", TipoColumna::Numero).id;
        Producto avena = porNombre(e.pc, "Avena");
        avena.extra = {{e.pc.columna(marca)->uuid, "Krocante"}, {e.pc.columna(peso)->uuid, "2,5"}};
        QVERIFY(e.pc.actualizar(avena).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(e.cel.columnas().size(), 12);
        QCOMPARE(resumen(e.pc), resumen(e.cel));
        QCOMPARE(porNombre(e.cel, "Avena").extra.size(), 2);
        QVERIFY(e.cel.listar("krocante").size() == 1);

        // el celular renombra la columna, cambia su orden y oculta otra
        qint64 idMarcaCel = 0;
        for (const Columna& c : e.cel.columnas()) if (c.nombre == "Marca") idMarcaCel = c.id;
        QVERIFY(idMarcaCel);
        QVERIFY(e.cel.renombrarColumna(idMarcaCel, "Marca comercial").ok);
        QVERIFY(e.cel.moverColumna(idMarcaCel, 0).ok);
        QVERIFY(e.cel.mostrarColumna(e.cel.columnaPorClave("proveedor")->id, false).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(resumen(e.pc), resumen(e.cel));
        QCOMPARE(e.pc.columnas().first().nombre, QString("Marca comercial"));
        QVERIFY(!e.pc.columnaPorClave("proveedor")->visible);

        // la PC elimina una columna propia: se va de los dos lados, con lo que tenía cargado
        QVERIFY(e.pc.eliminarColumna(peso).ok);
        // y quita una de fábrica opcional
        QVERIFY(e.pc.eliminarColumna(e.pc.columnaPorClave("medida_bolsa")->id).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(resumen(e.pc), resumen(e.cel));
        QCOMPARE(e.cel.columnas().size(), 10);
        QCOMPARE(porNombre(e.cel, "Avena").extra.size(), 1);
        QVERIFY(!e.cel.columnaActiva(CLAVE_MEDIDA));
        QCOMPARE(e.cel.columnasQuitadas().size(), 1);

        // volverla a poner en el celular también llega a la PC
        QVERIFY(e.cel.restaurarColumna(e.cel.columnasQuitadas().first().id).ok);
        QVERIFY(e.sincronizar().ok);
        QVERIFY(e.pc.columnaActiva(CLAVE_MEDIDA));
        QCOMPARE(resumen(e.pc), resumen(e.cel));
    }

    void lasColumnasEsencialesNoSeBorranPorSincronizacion()
    {
        Escenario e;
        QVERIFY(e.emparejar());
        // un dispositivo «malo» (o de otra versión) manda la columna «Nombre» como borrada: se ignora
        QJsonArray filas;
        filas << QJsonObject{{"t", "col"}, {"u", "col-nombre"}, {"mod", qint64(9999999999999)}, {"aut", "zzz"}, {"del", 1},
                             {"k", "nombre"}, {"n", "Nombre"}, {"tp", "texto"}, {"o", 1}, {"vis", 0}};
        const ResultadoAplicar r = e.pc.aplicarCambios(filas);
        QVERIFY(r.ok);
        QCOMPARE(r.ignoradas, 1);
        QVERIFY(e.pc.columnaActiva(CLAVE_NOMBRE));
    }

    // ---------------------------------------------------------------- stock
    void lasVentasYComprasDeLosDosLadosSeSuman()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 10, {}, {{"", 100}}));
        QVERIFY(e.emparejar());
        const qint64 idPc = idDe(e.pc, "Avena"), idCel = idDe(e.cel, "Avena");
        QCOMPARE(e.cel.obtener(idCel)->cantidad, 10);

        // sin sincronizar: cada lado hace lo suyo
        QVERIFY(e.pc.registrarOperacion(TipoOperacion::Venta, {{idPc, 3, "", 100}}).ok);       // -3
        QVERIFY(e.cel.registrarOperacion(TipoOperacion::Venta, {{idCel, 4, "", 100}}).ok);     // -4
        QVERIFY(e.pc.registrarOperacion(TipoOperacion::Compra, {{idPc, 5, "", 0}}).ok);        // +5
        QCOMPARE(e.pc.obtener(idPc)->cantidad, 12);
        QCOMPARE(e.cel.obtener(idCel)->cantidad, 6);

        QVERIFY(e.sincronizar().ok);
        QCOMPARE(e.pc.obtener(idPc)->cantidad, 8);                                              // 10 - 3 - 4 + 5 en los dos
        QCOMPARE(e.cel.obtener(idCel)->cantidad, 8);
        QCOMPARE(e.pc.operaciones().size(), 3);
        QCOMPARE(e.cel.operaciones().size(), 3);
        QCOMPARE(resumen(e.pc), resumen(e.cel));

        // sincronizar de nuevo no repite nada
        const ResultadoSinc otra = e.sincronizar();
        QVERIFY(otra.ok);
        QCOMPARE(otra.recibidas, 0);
        QCOMPARE(e.pc.obtener(idPc)->cantidad, 8);
        QCOMPARE(e.cel.operaciones().size(), 3);
        // el historial llegó completo, con sus renglones
        const auto ops = e.cel.operaciones();
        int unidades = 0;
        for (const Operacion& o : ops) for (const ItemOperacion& i : o.items) unidades += i.cantidad;
        QCOMPARE(unidades, 3 + 4 + 5);
    }

    void venderLoMismoEnLosDosLadosNoDejaStockNegativoYSeRecuerda()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 1, {}, {{"", 100}}));
        QVERIFY(e.emparejar());
        const qint64 idPc = idDe(e.pc, "Avena"), idCel = idDe(e.cel, "Avena");
        QVERIFY(e.pc.registrarOperacion(TipoOperacion::Venta, {{idPc, 1, "", 100}}).ok);       // la última unidad, en los dos lados
        QVERIFY(e.cel.registrarOperacion(TipoOperacion::Venta, {{idCel, 1, "", 100}}).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(e.pc.obtener(idPc)->cantidad, 0);                                              // se vendió una de más: se ve 0, no -1
        QCOMPARE(e.cel.obtener(idCel)->cantidad, 0);
        QVERIFY(!e.pc.registrarOperacion(TipoOperacion::Venta, {{idPc, 1, "", 100}}).ok);       // y no se puede vender más
        // pero se recuerda: una compra de 5 deja 4 (no 5) en los dos
        QVERIFY(e.pc.registrarOperacion(TipoOperacion::Compra, {{idPc, 5, "", 0}}).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(e.pc.obtener(idPc)->cantidad, 4);
        QCOMPARE(e.cel.obtener(idCel)->cantidad, 4);
    }

    void lasCorreccionesManualesTambienSeSincronizan()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 8));
        QVERIFY(e.emparejar());
        QVERIFY(e.pc.fijarCantidad(idDe(e.pc, "Avena"), 20).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(porNombre(e.cel, "Avena").cantidad, 20);
        QVERIFY(e.cel.ajustar(idDe(e.cel, "Avena"), -2).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(porNombre(e.pc, "Avena").cantidad, 18);
    }

    void editarUnProductoNoPisaLasVentasHechasMientrasTanto()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 10, {}, {{"", 100}}));
        QVERIFY(e.emparejar());
        const Producto abierto = porNombre(e.pc, "Avena");                       // la ventana de la PC lo abre: «Cantidad: 10»
        QVERIFY(e.cel.registrarOperacion(TipoOperacion::Venta, {{idDe(e.cel, "Avena"), 2, "", 100}}).ok);
        QVERIFY(e.sincronizar().ok);                                              // mientras tanto llega la venta del celular
        QCOMPARE(porNombre(e.pc, "Avena").cantidad, 8);
        Producto guardado = abierto;                                              // la persona guarda otros cambios, con «10» a la vista
        guardado.notas = "una nota";
        QVERIFY(e.pc.actualizar(guardado).ok);
        QCOMPARE(porNombre(e.pc, "Avena").cantidad, 8);                           // la venta no se pierde
        QCOMPARE(porNombre(e.pc, "Avena").notas, QString("una nota"));
    }

    void vendidoEnUnLadoYEliminadoEnElOtro()
    {
        Escenario e;
        e.pc.crear(producto("Avena", 10, {}, {{"", 100}}));
        e.pc.crear(producto("Sal", 10, {}, {{"", 50}}));
        QVERIFY(e.emparejar());
        QVERIFY(e.pc.eliminar(idDe(e.pc, "Avena")).ok);
        QVERIFY(e.cel.registrarOperacion(TipoOperacion::Venta, {{idDe(e.cel, "Avena"), 1, "", 100}, {idDe(e.cel, "Sal"), 1, "", 50}}).ok);
        QVERIFY(e.sincronizar().ok);
        QCOMPARE(resumen(e.pc), resumen(e.cel));
        QCOMPARE(e.cel.listar().size(), 1);                                       // Avena se eliminó
        QCOMPARE(e.pc.operaciones().size(), 1);                                   // la venta quedó en el historial, con los dos renglones
        QCOMPARE(e.pc.operaciones().first().items.size(), 2);
        QCOMPARE(porNombre(e.pc, "Sal").cantidad, 9);
    }

    void aplicarDosVecesLoMismoNoDuplicaNada()
    {
        Stock origen(":memory:"), destino(":memory:");
        origen.crear(producto("Avena", 10, "A1", {{"500 g", 1900}}));
        const qint64 id = idDe(origen, "Avena");
        QVERIFY(origen.registrarOperacion(TipoOperacion::Venta, {{id, 3, "", 1900}}).ok);
        const QJsonArray todo = origen.cambiosDesde(0);
        QVERIFY(destino.aplicarCambios(todo).ok);
        QVERIFY(destino.aplicarCambios(todo).ok);
        QCOMPARE(destino.listar().size(), 1);
        QCOMPARE(destino.listar().first().cantidad, 7);
        QCOMPARE(destino.operaciones().size(), 1);
        QCOMPARE(destino.columnas().size(), 10);
        QCOMPARE(destino.listar().first().precios.size(), 1);
    }

    void unaFilaRotaSeIgnoraSinDanarElResto()
    {
        Stock s(":memory:");
        QJsonArray filas;
        filas << QJsonObject{{"t", "prod"}, {"u", ""}, {"mod", 1}, {"aut", "x"}, {"n", "Sin uuid"}};
        filas << QJsonObject{{"t", "prod"}, {"u", "p-1"}, {"mod", 5}, {"aut", "x"}, {"del", 0}, {"n", "  "}};      // sin nombre
        filas << QJsonObject{{"t", "prod"}, {"u", "p-2"}, {"mod", 5}, {"aut", "x"}, {"del", 0}, {"n", "Bueno"}, {"pr", QJsonArray{QJsonObject{{"p", "1 L"}, {"v", 900.0}}}}};
        filas << QJsonObject{{"t", "aj"}, {"u", "a-1"}, {"mod", 5}, {"aut", "x"}, {"pu", ""}, {"d", 3}};            // sin producto
        filas << QJsonObject{{"t", "algo-raro"}, {"u", "z"}};
        const ResultadoAplicar r = s.aplicarCambios(filas);
        QVERIFY(r.ok);
        QCOMPARE(r.ignoradas, 3);
        QCOMPARE(s.listar().size(), 1);
        QCOMPARE(s.listar().first().nombre, QString("Bueno"));
    }
};

QTEST_MAIN(TstSinc)
#include "tst_sinc.moc"
