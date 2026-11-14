#include "db.h"
#include <QDir>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

namespace {
// «Ganancia %», si está puesta
std::optional<Columna> columnaGanancia(const Stock& s)
{
    for (const Columna& c : s.columnas())
        if (c.uuid == QLatin1String(UUID_COLUMNA_GANANCIA)) return c;
    return std::nullopt;
}

Producto producto(const QString& nombre, int cantidad, QList<Precio> precios, const QString& codigo = QString(),
                  const QString& categoria = QStringLiteral("Almacén"))
{
    Producto p;
    p.codigo = codigo;
    p.nombre = nombre;
    p.categoria = categoria;
    p.cantidad = cantidad;
    p.precios = precios;
    return p;
}

ProductoImportado importado(const QString& nombre, const QString& categoria = QStringLiteral("Almacén"))
{
    ProductoImportado p;
    p.nombre = nombre;
    p.categoria = categoria;
    return p;
}

// Una base como la dejaba la 2.1.0 (esquema 3, con la columna «precio» de las versiones muy viejas) y ya con ventas:
// Avena: 8 en stock, con una compra de 5 y una venta de 2 anotadas; Harina: 0; Sal: 5.
void crearBaseDe2_1(const QString& ruta)
{
    const QString nombre = QStringLiteral("v21_%1").arg(ruta);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", nombre);
        db.setDatabaseName(ruta);
        QVERIFY2(db.open(), "no se pudo crear la base de la 2.1.0");
        QSqlQuery q(db);
        const QStringList sentencias = {
            "CREATE TABLE productos (id INTEGER PRIMARY KEY AUTOINCREMENT, codigo TEXT UNIQUE, nombre TEXT NOT NULL,"
            " categoria TEXT NOT NULL DEFAULT '', cantidad INTEGER NOT NULL DEFAULT 0 CHECK (cantidad >= 0),"
            " precio REAL NOT NULL DEFAULT 0, stock_minimo INTEGER NOT NULL DEFAULT 0, proveedor TEXT NOT NULL DEFAULT '',"
            " vencimiento TEXT, medida_bolsa TEXT NOT NULL DEFAULT '', notas TEXT NOT NULL DEFAULT '')",
            "CREATE TABLE precios (id INTEGER PRIMARY KEY AUTOINCREMENT, producto_id INTEGER NOT NULL REFERENCES productos(id) ON DELETE CASCADE,"
            " orden INTEGER NOT NULL DEFAULT 0, presentacion TEXT NOT NULL DEFAULT '', precio REAL NOT NULL)",
            "CREATE INDEX idx_precios_producto ON precios(producto_id)",
            "CREATE TABLE operaciones (id INTEGER PRIMARY KEY AUTOINCREMENT, tipo TEXT NOT NULL CHECK (tipo IN ('venta', 'compra')),"
            " fecha TEXT NOT NULL, total REAL NOT NULL DEFAULT 0)",
            "CREATE TABLE operacion_items (id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " operacion_id INTEGER NOT NULL REFERENCES operaciones(id) ON DELETE CASCADE, producto_id INTEGER, nombre TEXT NOT NULL,"
            " presentacion TEXT NOT NULL DEFAULT '', cantidad INTEGER NOT NULL, precio_unitario REAL NOT NULL DEFAULT 0)",
            "CREATE INDEX idx_items_operacion ON operacion_items(operacion_id)",
            "INSERT INTO productos (id, codigo, nombre, categoria, cantidad, vencimiento) VALUES (1, 'A1', 'Avena', 'Almacén', 8, '2027-03-01')",
            "INSERT INTO productos (id, nombre, categoria, cantidad) VALUES (2, 'Harina', 'Almacén', 0)",
            "INSERT INTO productos (id, nombre, categoria, cantidad, notas) VALUES (3, 'Sal', 'Almacén', 5, 'fina')",
            "INSERT INTO precios (producto_id, orden, presentacion, precio) VALUES (1, 0, '500 g', 1900), (1, 1, '1000 g', 3500), (3, 0, '', 300)",
            "INSERT INTO operaciones (id, tipo, fecha, total) VALUES (1, 'compra', '2026-10-20T10:00:00', 0), (2, 'venta', '2026-10-25T12:30:00', 300)",
            "INSERT INTO operacion_items (operacion_id, producto_id, nombre, presentacion, cantidad, precio_unitario) VALUES"
            " (1, 1, 'Avena', '', 5, 0), (2, 1, 'Avena', '500 g', 2, 100), (2, 99, 'Producto que se borró', '', 1, 100)",
            "PRAGMA user_version = 3",
        };
        for (const QString& s : sentencias) QVERIFY2(q.exec(s), qPrintable(s + " -> " + q.lastError().text()));
        db.close();
    }
    QSqlDatabase::removeDatabase(nombre);
}

// Crea a mano una base con el esquema viejo (antes de que existieran los precios por presentación)
void crearBaseVieja(const QString& ruta, bool conMedida)
{
    const QString nombre = QStringLiteral("vieja_%1").arg(ruta);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", nombre);
        db.setDatabaseName(ruta);
        QVERIFY2(db.open(), "no se pudo crear la base vieja");
        QSqlQuery q(db);
        QVERIFY(q.exec("CREATE TABLE productos (id INTEGER PRIMARY KEY AUTOINCREMENT, codigo TEXT UNIQUE, nombre TEXT NOT NULL,"
                       " categoria TEXT NOT NULL DEFAULT '', cantidad INTEGER NOT NULL DEFAULT 0 CHECK (cantidad >= 0),"
                       " precio REAL NOT NULL DEFAULT 0, stock_minimo INTEGER NOT NULL DEFAULT 0"
                       + QString(conMedida ? ", proveedor TEXT NOT NULL DEFAULT '', vencimiento TEXT, peso_bolsa REAL NOT NULL DEFAULT 0, unidad TEXT NOT NULL DEFAULT 'kg'" : "") + ")"));
        if (conMedida) {
            QVERIFY(q.exec("INSERT INTO productos (codigo, nombre, cantidad, precio, peso_bolsa, unidad, proveedor) VALUES ('X1', 'Harina', 4, 600, 500, 'g', 'Smile')"));
            QVERIFY(q.exec("INSERT INTO productos (codigo, nombre, cantidad, precio) VALUES ('X2', 'Sal', 2, 300)"));
        } else {
            QVERIFY(q.exec("INSERT INTO productos (codigo, nombre, cantidad, precio) VALUES ('X1', 'Viejo', 4, 100)"));
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(nombre);
}
}

class TstDb : public QObject
{
    Q_OBJECT
private slots:
    // ---------------------------------------------------------------- productos
    void crearYListar()
    {
        Stock s(":memory:");
        QVERIFY(s.abierta());
        Producto p = producto("Yerba", 10, {{"500 g", 1500}, {"1000 g", 2800}}, "A1");
        p.proveedor = "Zucchi";
        p.vencimiento = "2027-01-31";
        p.medidaBolsa = "10x20";
        p.notas = "una nota";
        const Resultado r = s.crear(p);
        QVERIFY(r.ok && r.id > 0);
        const auto lista = s.listar();
        QCOMPARE(lista.size(), 1);
        QCOMPARE(lista[0].nombre, QString("Yerba"));
        QCOMPARE(lista[0].proveedor, QString("Zucchi"));
        QCOMPARE(lista[0].vencimiento, QString("2027-01-31"));
        QCOMPARE(lista[0].medidaBolsa, QString("10x20"));
        QCOMPARE(lista[0].precios.size(), 2);
        QCOMPARE(lista[0].precios[0].presentacion, QString("500 g"));       // varios precios, en orden
        QCOMPARE(lista[0].precios[1].precio, 2800.0);
    }

    void codigoDuplicadoYCodigosVacios()
    {
        Stock s(":memory:");
        QVERIFY(s.crear(producto("Yerba", 1, {}, "A1")).ok);
        const Resultado r = s.crear(producto("Otra", 1, {}, "A1"));
        QVERIFY(!r.ok);
        QCOMPARE(r.error, QString("Ya existe un producto con ese código."));
        QVERIFY(s.crear(producto("Uno", 1, {}, "")).ok);                    // sin código no chocan entre sí
        QVERIFY(s.crear(producto("Dos", 1, {}, "")).ok);
        QCOMPARE(s.listar().size(), 3);
    }

    void actualizarReemplazaPreciosYEliminarLosBorra()
    {
        Stock s(":memory:");
        const qint64 id = s.crear(producto("Yerba", 10, {{"500 g", 1500}, {"1000 g", 2800}}, "A1")).id;
        Producto p = *s.obtener(id);
        p.precios = {{"250 g", 900}};
        p.cantidad = 7;                                  // actualizar() no toca la cantidad: para eso está fijarCantidad()
        QVERIFY(s.actualizar(p).ok);
        QCOMPARE(s.obtener(id)->precios.size(), 1);
        QCOMPARE(s.obtener(id)->cantidad, 10);
        QVERIFY(s.fijarCantidad(id, 7).ok);
        QCOMPARE(s.obtener(id)->cantidad, 7);
        QVERIFY(s.eliminar(id).ok);
        QVERIFY(s.listar().isEmpty());
        QVERIFY(!s.obtener(id));
    }

    void unProductoEliminadoLiberaSuCodigo()
    {
        Stock s(":memory:");
        const qint64 id = s.crear(producto("Yerba", 1, {}, "A1")).id;
        QVERIFY(s.eliminar(id).ok);
        QVERIFY(s.crear(producto("Otra yerba", 1, {}, "A1")).ok);                   // el código se puede volver a usar
        QVERIFY(!s.crear(producto("Repetida", 1, {}, "A1")).ok);                    // pero sigue siendo único entre los vivos
        QVERIFY(s.eliminar(id).ok);                                                  // borrar dos veces no rompe nada
        QCOMPARE(s.listar().size(), 1);
    }

    void laCantidadEsLaSumaDeLosMovimientos()
    {
        Stock s(":memory:");
        const qint64 id = s.crear(producto("Yerba", 10, {{"", 100}})).id;            // stock inicial: 10
        QCOMPARE(s.obtener(id)->cantidad, 10);
        QVERIFY(s.registrarOperacion(TipoOperacion::Venta, {{id, 4, "", 100}}).ok);   // -4
        QVERIFY(s.registrarOperacion(TipoOperacion::Compra, {{id, 6, "", 0}}).ok);    // +6
        QCOMPARE(s.obtener(id)->cantidad, 12);
        QVERIFY(s.fijarCantidad(id, 20).ok);                                          // corrección manual: +8
        QCOMPARE(s.obtener(id)->cantidad, 20);
        QVERIFY(s.fijarCantidad(id, 20).ok);                                          // sin cambio: no anota nada
        QVERIFY(!s.fijarCantidad(id, -1).ok);
        QVERIFY(s.ajustar(id, -5).ok);
        QCOMPARE(s.obtener(id)->cantidad, 15);
        QVERIFY(!s.ajustar(id, -16).ok);
        QCOMPARE(s.obtener(id)->cantidad, 15);
        // cada cambio quedó anotado como un movimiento: el stock inicial (+10), la corrección (+8) y el ajuste (-5)
        int ajustes = 0, operaciones = 0;
        qint64 suma = 0;
        for (const QJsonValue& v : s.cambiosDesde(0)) {
            const QJsonObject o = v.toObject();
            if (o["t"].toString() == "aj") { ++ajustes; suma += o["d"].toVariant().toLongLong(); }
            if (o["t"].toString() == "op") ++operaciones;
        }
        QCOMPARE(ajustes, 3);
        QCOMPARE(suma, qint64(13));                                                   // 10 + 8 - 5 (lo demás son las ventas y compras)
        QCOMPARE(operaciones, 2);
    }

    // ---------------------------------------------------------------- columnas
    void columnasDeFabrica()
    {
        Stock s(":memory:");
        const QList<Columna> cols = s.columnas();
        QStringList nombres;
        for (const Columna& c : cols) nombres << c.nombre;
        QCOMPARE(nombres, (QStringList{"Código", "Nombre", "Precios", "Categoría", "Proveedor", "Cantidad", "Medida bolsa",
                                       "Vencimiento", "Stock mín.", "Notas", "Ganancia %"}));
        for (const Columna& c : cols) QVERIFY2(!c.propia() || c.uuid == UUID_COLUMNA_GANANCIA, qPrintable(c.nombre));
        QVERIFY(!cols.last().visible);                                               // las notas se ven como ayuda sobre el nombre
        QVERIFY(cols[1].esencial() && cols[2].esencial() && cols[5].esencial());     // nombre, precios y cantidad
        QVERIFY(!cols[0].esencial());
        QVERIFY(s.columnasQuitadas().isEmpty());
        QCOMPARE(cols[1].uuid, QString("col-nombre"));                               // mismo uuid en todos los dispositivos
    }

    void agregarRenombrarYEliminarUnaColumnaPropia()
    {
        Stock s(":memory:");
        const Resultado r = s.agregarColumna("  Marca  ", TipoColumna::Texto);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(s.columna(r.id)->nombre, QString("Marca"));
        QVERIFY(s.columna(r.id)->propia());
        QCOMPARE(s.columnas().last().id, r.id);                                       // se agrega al final
        QVERIFY(!s.agregarColumna("marca", TipoColumna::Texto).ok);                   // repetida (sin importar mayúsculas)
        QVERIFY(!s.agregarColumna("Categoria", TipoColumna::Texto).ok);               // ni sin tilde, ni como una de fábrica
        QVERIFY(!s.agregarColumna("   ", TipoColumna::Texto).ok);
        QVERIFY(!s.agregarColumna("Algo", TipoColumna::Precios).ok);                  // las propias: texto, número o fecha
        QVERIFY(s.renombrarColumna(r.id, "Marca / fabricante").ok);
        QVERIFY(!s.renombrarColumna(r.id, "proveedor").ok);                           // choca con una existente
        QVERIFY(s.renombrarColumna(r.id, "MARCA / FABRICANTE").ok);                   // cambiar solo mayúsculas sí
        QVERIFY(s.renombrarColumna(s.columnaPorClave("proveedor")->id, "Marca del proveedor").ok);   // las de fábrica también
        QCOMPARE(s.columnaPorClave("proveedor")->nombre, QString("Marca del proveedor"));
        QVERIFY(s.cambiarTipoColumna(r.id, TipoColumna::Numero).ok);
        QVERIFY(s.columna(r.id)->tipo == TipoColumna::Numero);
        QVERIFY(!s.cambiarTipoColumna(s.columnaPorClave("codigo")->id, TipoColumna::Numero).ok);   // las de fábrica no cambian de tipo
        QVERIFY(s.eliminarColumna(r.id).ok);
        QVERIFY(!s.columna(r.id));
        QVERIFY(s.columnasQuitadas().isEmpty());                                     // una propia eliminada no se puede restaurar
        QVERIFY(s.agregarColumna("Marca", TipoColumna::Texto).ok);                    // y el nombre se puede volver a usar
    }

    void ocultarMoverYQuitarColumnasDeFabrica()
    {
        Stock s(":memory:");
        const qint64 nombre = s.columnaPorClave("nombre")->id;
        const qint64 proveedor = s.columnaPorClave("proveedor")->id;
        const qint64 cantidad = s.columnaPorClave("cantidad")->id;
        QVERIFY(!s.mostrarColumna(nombre, false).ok);                                 // el nombre siempre se ve
        QVERIFY(s.mostrarColumna(proveedor, false).ok);
        QVERIFY(!s.columna(proveedor)->visible);
        QVERIFY(s.mostrarColumna(proveedor, true).ok);
        // mover: la cantidad (posición 5) al principio
        QVERIFY(s.moverColumna(cantidad, 0).ok);
        QCOMPARE(s.columnas().first().clave, QString("cantidad"));
        QCOMPARE(s.columnas()[1].clave, QString("codigo"));
        for (int i = 0; i < s.columnas().size(); ++i) QCOMPARE(s.columnas()[i].orden, i);   // siempre 0, 1, 2...
        QVERIFY(s.moverColumna(cantidad, 99).ok);                                     // más allá del final: queda última
        QCOMPARE(s.columnas().last().clave, QString("cantidad"));
        QVERIFY(s.moverColumna(cantidad, 5).ok);
        QCOMPARE(s.columnas()[5].clave, QString("cantidad"));
        // eliminar: las esenciales no; las otras se quitan y se pueden volver a poner
        QVERIFY(!s.eliminarColumna(nombre).ok);
        QVERIFY(!s.eliminarColumna(cantidad).ok);
        QVERIFY(!s.eliminarColumna(s.columnaPorClave("precios")->id).ok);
        s.crear(producto("Avena", 3, {}, "A1"));
        Producto p = s.listar().first();
        p.proveedor = "Zucchi";
        QVERIFY(s.actualizar(p).ok);
        QVERIFY(s.eliminarColumna(proveedor).ok);
        QVERIFY(!s.columnaActiva(CLAVE_PROVEEDOR));
        QCOMPARE(s.columnasQuitadas().size(), 1);
        QVERIFY(s.restaurarColumna(proveedor).ok);
        QVERIFY(s.columnaActiva(CLAVE_PROVEEDOR));
        QCOMPARE(s.columnas().last().clave, QString("proveedor"));                    // vuelve al final
        QCOMPARE(s.listar().first().proveedor, QString("Zucchi"));                    // sin perder lo que tenía cargado
        QVERIFY(!s.restaurarColumna(proveedor).ok);                                   // ya estaba puesta
    }

    void ordenarTodasLasColumnasDeUnaVez()
    {
        Stock s(":memory:");
        QList<qint64> ids;
        for (const Columna& c : s.columnas()) ids << c.id;
        std::reverse(ids.begin(), ids.end());
        QVERIFY(s.ordenarColumnas(ids).ok);
        QCOMPARE(s.columnas().first().uuid, QString(UUID_COLUMNA_GANANCIA));
        QCOMPARE(s.columnas().last().clave, QString("codigo"));
        for (int i = 0; i < s.columnas().size(); ++i) QCOMPARE(s.columnas()[i].orden, i);
        // las que no se nombran quedan al final, en el orden que tenían
        QVERIFY(s.ordenarColumnas({s.columnaPorClave("cantidad")->id}).ok);
        QCOMPARE(s.columnas().first().clave, QString("cantidad"));
        QCOMPARE(s.columnas()[1].uuid, QString(UUID_COLUMNA_GANANCIA));
        QCOMPARE(s.columnas()[2].clave, QString("notas"));
        QCOMPARE(s.columnas().size(), 11);                                           // no se pierde ninguna
    }

    void valoresDeColumnasPropias()
    {
        Stock s(":memory:");
        const qint64 marca = s.agregarColumna("Marca", TipoColumna::Texto).id;
        const qint64 peso = s.agregarColumna("Peso (kg)", TipoColumna::Numero).id;
        const qint64 compra = s.agregarColumna("Última compra", TipoColumna::Fecha).id;
        const QString uMarca = s.columna(marca)->uuid, uPeso = s.columna(peso)->uuid, uCompra = s.columna(compra)->uuid;

        Producto p = producto("Avena", 3, {});
        p.extra = {{uMarca, "  Krocante "}, {uPeso, "1.234,5"}, {uCompra, "01/11/2026"}};
        const Resultado r = s.crear(p);
        QVERIFY2(r.ok, qPrintable(r.error));
        const Producto g = *s.obtener(r.id);
        QCOMPARE(g.extra.value(uMarca), QString("Krocante"));
        QCOMPARE(g.extra.value(uPeso), QString("1234.5"));                            // se guarda con punto decimal
        QCOMPARE(g.extra.value(uCompra), QString("2026-11-01"));                       // y la fecha como ISO
        QCOMPARE(s.listar().first().extra.size(), 3);
        QCOMPARE(s.listar("krocante").size(), 1);                                      // la búsqueda mira las columnas propias

        // un valor que no sirve deshace todo (el producto no se crea ni se cambia)
        Producto mala = producto("Sal", 1, {});
        mala.extra = {{uPeso, "mucho"}};
        QVERIFY(!s.crear(mala).ok);
        QCOMPARE(s.listar().size(), 1);
        Producto cambiada = g;
        cambiada.nombre = "Avena integral";
        cambiada.extra = {{uCompra, "ayer"}};
        QVERIFY(!s.actualizar(cambiada).ok);
        QCOMPARE(s.obtener(r.id)->nombre, QString("Avena"));
        QCOMPARE(s.obtener(r.id)->extra.value(uCompra), QString("2026-11-01"));

        // valores vacíos no se guardan; una columna que no existe se ignora
        Producto vacia = g;
        vacia.extra = {{uMarca, ""}, {"no-existe", "x"}, {uPeso, "2,5"}};
        QVERIFY(s.actualizar(vacia).ok);
        QCOMPARE(s.obtener(r.id)->extra.size(), 1);
        QCOMPARE(s.obtener(r.id)->extra.value(uPeso), QString("2.5"));

        // al eliminar una columna propia se va lo que tenía cargado
        QVERIFY(s.eliminarColumna(peso).ok);
        QVERIFY(s.obtener(r.id)->extra.isEmpty());
    }

    // ---------------------------------------------------------------- bases de versiones anteriores
    void migraBaseDeLaVersion2_1ConVentasYStock()
    {
        QTemporaryDir dir;
        const QString ruta = dir.filePath("v21.db");
        crearBaseDe2_1(ruta);
        {
            Stock s(ruta);
            QVERIFY2(s.abierta(), qPrintable(s.errorApertura()));
            QCOMPARE(s.listar().size(), 3);
            const auto lista = s.listar();
            const Producto avena = lista[0].nombre == "Avena" ? lista[0] : (lista[1].nombre == "Avena" ? lista[1] : lista[2]);
            QCOMPARE(avena.cantidad, 8);                                              // ya tenía ventas anotadas: la cantidad no cambia
            QCOMPARE(avena.codigo, QString("A1"));
            QCOMPARE(avena.precios.size(), 2);
            QCOMPARE(avena.vencimiento, QString("2027-03-01"));
            QVERIFY(!avena.uuid.isEmpty());
            QCOMPARE(s.obtener(avena.id)->id, avena.id);                              // los números de siempre se conservan
            int sumaStock = 0;
            for (const Producto& p : lista) sumaStock += p.cantidad;
            QCOMPARE(sumaStock, 8 + 0 + 5);
            // el historial sigue y conserva el nombre
            const auto ops = s.operaciones();
            QCOMPARE(ops.size(), 2);
            QCOMPARE(ops.first().items.first().nombre, QString("Avena"));
            // las columnas de fábrica están y el programa funciona como siempre
            QCOMPARE(s.columnas().size(), 11);      // las 10 de fábrica y «Ganancia %»
            QVERIFY(s.registrarOperacion(TipoOperacion::Venta, {{avena.id, 3, "", 100}}).ok);
            QCOMPARE(s.obtener(avena.id)->cantidad, 5);
            QVERIFY(s.fijarCantidad(avena.id, 9).ok);
            QCOMPARE(s.obtener(avena.id)->cantidad, 9);
        }
        // la copia de seguridad previa quedó al lado
        QVERIFY2(QDir(dir.filePath("backups")).entryList({"antes_de_actualizar_*.db"}).size() == 1, "falta la copia previa");
        {
            Stock otraVez(ruta);                                                      // abrir de nuevo no migra otra vez
            QVERIFY(otraVez.abierta());
            QCOMPARE(otraVez.listar().size(), 3);
            QCOMPARE(otraVez.columnas().size(), 11);
        }
        QCOMPARE(QDir(dir.filePath("backups")).entryList({"antes_de_actualizar_*.db"}).size(), 1);
    }

    void unaBaseDeUnaVersionMasNuevaNoSeAbre()
    {
        QTemporaryDir dir;
        const QString ruta = dir.filePath("futura.db");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "futura");
            db.setDatabaseName(ruta);
            QVERIFY(db.open());
            QSqlQuery(db).exec("PRAGMA user_version = 99");
            db.close();
        }
        QSqlDatabase::removeDatabase("futura");
        Stock s(ruta);
        QVERIFY(!s.abierta());
        QVERIFY2(s.errorApertura().contains("más nueva"), qPrintable(s.errorApertura()));
    }

    void ajustarNoDejaStockNegativo()
    {
        Stock s(":memory:");
        const qint64 id = s.crear(producto("Yerba", 10, {})).id;
        QVERIFY(s.ajustar(id, +5).ok);
        QCOMPARE(s.obtener(id)->cantidad, 15);
        QVERIFY(!s.ajustar(id, -16).ok);
        QCOMPARE(s.obtener(id)->cantidad, 15);
    }

    void listarBuscaPorTextoYPorPrecio()
    {
        Stock s(":memory:");
        s.crear(producto("Yerba", 1, {{"500 g", 1500}}, "", "Almacén"));
        s.crear(producto("Lavandina", 1, {{"1 L", 900}}, "", "Limpieza"));
        QCOMPARE(s.listar("limp").size(), 1);
        QCOMPARE(s.listar("1500").first().nombre, QString("Yerba"));
        QCOMPARE(s.listar("800-1000").first().nombre, QString("Lavandina"));
        QCOMPARE(s.listar(">1000").first().nombre, QString("Yerba"));
        QCOMPARE(s.listar("almacen").size(), 1);                            // sin tilde
    }

    // ---------------------------------------------------------------- ventas y compras
    void ventaDeVariosProductosSumaElImporteYBajaElStock()
    {
        Stock s(":memory:");
        const qint64 a = s.crear(producto("Yerba", 10, {{"500 g", 1500}})).id;
        const qint64 b = s.crear(producto("Avena", 5, {{"1000 g", 2800.5}})).id;
        const qint64 c = s.crear(producto("Sal", 8, {})).id;                  // este no entra en la venta

        const ResultadoOperacion r = s.registrarOperacion(TipoOperacion::Venta, {
            {a, 2, "500 g", 1500},        // 3000
            {b, 3, "1000 g", 2800.5},     // 8401,50
        });
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.total, 11401.5);                                          // suma de los importes de cada producto
        QCOMPARE(s.obtener(a)->cantidad, 8);                                 // y baja el stock de cada uno
        QCOMPARE(s.obtener(b)->cantidad, 2);
        QCOMPARE(s.obtener(c)->cantidad, 8);

        const auto ops = s.operaciones();
        QCOMPARE(ops.size(), 1);
        QVERIFY(ops[0].tipo == TipoOperacion::Venta);
        QCOMPARE(ops[0].total, 11401.5);
        QCOMPARE(ops[0].items.size(), 2);
        QCOMPARE(ops[0].items[0].nombre, QString("Yerba"));
        QCOMPARE(ops[0].items[1].cantidad, 3);
    }

    void compraSumaStock()
    {
        Stock s(":memory:");
        const qint64 a = s.crear(producto("Yerba", 10, {{"500 g", 1500}})).id;
        const qint64 b = s.crear(producto("Avena", 0, {})).id;
        const ResultadoOperacion r = s.registrarOperacion(TipoOperacion::Compra, {{a, 5, "", 0}, {b, 20, "", 0}});
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(s.obtener(a)->cantidad, 15);
        QCOMPARE(s.obtener(b)->cantidad, 20);
        QVERIFY(s.operaciones().first().tipo == TipoOperacion::Compra);
    }

    void ventaSinStockNoTocaNada()
    {
        Stock s(":memory:");
        const qint64 a = s.crear(producto("Yerba", 10, {{"", 1500}})).id;
        const qint64 b = s.crear(producto("Avena", 1, {{"", 2800}})).id;
        // la primera línea alcanzaría, la segunda no: no se tiene que modificar NINGÚN producto
        const ResultadoOperacion r = s.registrarOperacion(TipoOperacion::Venta, {{a, 3, "", 1500}, {b, 2, "", 2800}});
        QVERIFY(!r.ok);
        QVERIFY2(r.error.contains("Avena") && r.error.contains("hay 1"), qPrintable(r.error));
        QCOMPARE(s.obtener(a)->cantidad, 10);
        QCOMPARE(s.obtener(b)->cantidad, 1);
        QVERIFY(s.operaciones().isEmpty());
    }

    void mismoProductoDosVecesEnLaVenta()
    {
        Stock s(":memory:");
        const qint64 a = s.crear(producto("Yerba", 5, {{"", 100}})).id;
        QVERIFY(!s.registrarOperacion(TipoOperacion::Venta, {{a, 3, "", 100}, {a, 3, "", 100}}).ok);   // 6 > 5
        QCOMPARE(s.obtener(a)->cantidad, 5);
        QVERIFY(s.registrarOperacion(TipoOperacion::Venta, {{a, 3, "", 100}, {a, 2, "", 100}}).ok);
        QCOMPARE(s.obtener(a)->cantidad, 0);
    }

    void operacionesInvalidas()
    {
        Stock s(":memory:");
        const qint64 a = s.crear(producto("Yerba", 5, {})).id;
        QVERIFY(!s.registrarOperacion(TipoOperacion::Venta, {}).ok);
        QVERIFY(!s.registrarOperacion(TipoOperacion::Venta, {{a, 0, "", 1}}).ok);
        QVERIFY(!s.registrarOperacion(TipoOperacion::Compra, {{a, -1, "", 0}}).ok);
        QVERIFY(!s.registrarOperacion(TipoOperacion::Venta, {{9999, 1, "", 1}}).ok);
        QCOMPARE(s.obtener(a)->cantidad, 5);
    }

    void elHistorialConservaElNombreSiSeBorraElProducto()
    {
        Stock s(":memory:");
        const qint64 a = s.crear(producto("Yerba", 5, {{"", 100}})).id;
        QVERIFY(s.registrarOperacion(TipoOperacion::Venta, {{a, 1, "", 100}}).ok);
        s.eliminar(a);
        QCOMPARE(s.operaciones().first().items.first().nombre, QString("Yerba"));
    }

    // ---------------------------------------------------------------- importación
    void importarAgregaConCantidadCero()
    {
        Stock s(":memory:");
        ProductoImportado p = importado("Avena");
        p.proveedor = "ZUCCHI";
        p.precios = {{"500 g", 1900}};
        QCOMPARE(s.importar({p}), qMakePair(1, 0));
        const Producto g = s.listar().first();
        QCOMPARE(g.cantidad, 0);
        QCOMPARE(g.precios.first().precio, 1900.0);
        QCOMPARE(g.proveedor, QString("ZUCCHI"));
    }

    void reimportarActualizaSinTocarElStock()
    {
        Stock s(":memory:");
        ProductoImportado p = importado("Avena");
        p.proveedor = "ZUCCHI";
        p.medidaBolsa = "12x30";
        p.precios = {{"500 g", 1900}};
        s.importar({p});
        const qint64 id = s.listar().first().id;
        s.ajustar(id, +7);                                                   // stock cargado a mano
        ProductoImportado nueva = importado("  AVENA ");                     // mismo producto, con otras mayúsculas
        nueva.precios = {{"500 g", 2100}};                                   // sin proveedor ni medida (celdas vacías)
        QCOMPARE(s.previsualizarImportacion({nueva}), qMakePair(0, 1));
        QCOMPARE(s.importar({nueva}), qMakePair(0, 1));
        const Producto g = *s.obtener(id);
        QCOMPARE(g.cantidad, 7);                                             // no se pisó
        QCOMPARE(g.precios.first().precio, 2100.0);                          // precio actualizado
        QCOMPARE(g.proveedor, QString("ZUCCHI"));                            // lo vacío no borra
        QCOMPARE(g.medidaBolsa, QString("12x30"));
        QCOMPARE(s.listar().size(), 1);
    }

    void mismoNombreEnOtraCategoriaEsOtroProducto()
    {
        Stock s(":memory:");
        QCOMPARE(s.importar({importado("ENELDO", "Condimentos"), importado("ENELDO", "Semillas")}), qMakePair(2, 0));
    }

    // ---------------------------------------------------------------- bases de versiones anteriores
    void migraBaseDeLaVersion1_1()
    {
        QTemporaryDir dir;
        const QString ruta = dir.filePath("vieja.db");
        crearBaseVieja(ruta, true);
        {
            Stock s(ruta);
            QVERIFY2(s.abierta(), qPrintable(s.errorApertura()));
            const auto lista = s.listar();
            QCOMPARE(lista.size(), 2);
            const Producto harina = lista[0].nombre == "Harina" ? lista[0] : lista[1];
            const Producto sal = lista[0].nombre == "Sal" ? lista[0] : lista[1];
            QCOMPARE(harina.cantidad, 4);                                     // no se pierde nada
            QCOMPARE(harina.proveedor, QString("Smile"));
            QCOMPARE(harina.precios.size(), 1);                               // el precio único pasó a la tabla de precios
            QCOMPARE(harina.precios[0].presentacion, QString("500 g"));
            QCOMPARE(harina.precios[0].precio, 600.0);
            QCOMPARE(sal.precios[0].presentacion, QString(""));
            QCOMPARE(sal.precios[0].precio, 300.0);
            QVERIFY(s.registrarOperacion(TipoOperacion::Venta, {{harina.id, 1, "500 g", 600}}).ok);   // y ya se puede vender
        }
        Stock otraVez(ruta);                                                  // abrir de nuevo no duplica los precios
        for (const Producto& p : otraVez.listar()) QCOMPARE(p.precios.size(), 1);
    }

    void migraBaseDeLaVersion1_0()
    {
        QTemporaryDir dir;
        const QString ruta = dir.filePath("v10.db");
        crearBaseVieja(ruta, false);
        Stock s(ruta);
        QVERIFY2(s.abierta(), qPrintable(s.errorApertura()));
        const Producto p = s.listar().first();
        QCOMPARE(p.nombre, QString("Viejo"));
        QCOMPARE(p.precios.first().precio, 100.0);
        QCOMPARE(p.proveedor, QString(""));
    }

    void backupGeneraUnaCopiaQueSeAbre()
    {
        QTemporaryDir dir;
        {
            Stock s(dir.filePath("stock.db"));
            s.crear(producto("Yerba", 3, {{"", 100}}));
            QVERIFY(s.backup(dir.filePath("copias/copia.db")).ok);
        }
        Stock copia(dir.filePath("copias/copia.db"));
        QCOMPARE(copia.listar().first().nombre, QString("Yerba"));
    }

    // ---- 2.3.0: costo en las compras, reportes y avisos

    void compraGuardaElCostoYElTotal()
    {
        Stock s(":memory:");
        const qint64 id = s.crear(producto("Avena", 0, {{"500 g", 4000}})).id;
        const ResultadoOperacion r = s.registrarOperacion(TipoOperacion::Compra, {{id, 10, QString(), 2500}});
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.total, 25000.0);
        QCOMPARE(s.obtener(id)->cantidad, 10);
        QCOMPARE(s.operaciones(1).first().items.first().precioUnitario, 2500.0);
        // Una compra sin costo no cambia el promedio; otra con costo distinto sí (ponderado por cantidad)
        QVERIFY(s.registrarOperacion(TipoOperacion::Compra, {{id, 5, QString(), 0}}).ok);
        QVERIFY(s.registrarOperacion(TipoOperacion::Compra, {{id, 10, QString(), 3500}}).ok);
        QCOMPARE(s.costoPromedio(id), 3000.0);
    }

    void reporteDeVentasYGanancia()
    {
        Stock s(":memory:");
        const qint64 avena = s.crear(producto("Avena", 0, {{"500 g", 4000}})).id;
        const qint64 sal = s.crear(producto("Sal", 20, {{"", 300}})).id;      // sin compras: sin costo
        QVERIFY(s.registrarOperacion(TipoOperacion::Compra, {{avena, 10, QString(), 2500}}).ok);
        QVERIFY(s.registrarOperacion(TipoOperacion::Venta, {{avena, 3, "500 g", 4000}, {sal, 2, QString(), 300}}).ok);
        QVERIFY(s.registrarOperacion(TipoOperacion::Venta, {{avena, 1, "500 g", 3800}}).ok);

        const QDate hoy = QDate::currentDate();
        const Reporte r = s.reporte(hoy.addDays(-1), hoy);
        QCOMPARE(r.cantidadVentas, 2);
        QCOMPARE(r.ventas, 3.0 * 4000 + 2 * 300 + 3800);
        QCOMPARE(r.compras, 25000.0);
        QCOMPARE(r.unidadesVendidas, 6);
        QCOMPARE(r.unidadesSinCosto, 2);                       // las 2 de sal
        QCOMPARE(r.ventasConCosto, 3.0 * 4000 + 3800);
        QCOMPARE(r.costoDeLoVendido, 4.0 * 2500);
        QCOMPARE(r.ganancia, 3.0 * 4000 + 3800 - 4 * 2500);
        QCOMPARE(r.masVendidos.first().nombre, QStringLiteral("Avena"));
        QCOMPARE(r.masVendidos.first().cantidad, 4);
        QCOMPARE(r.ventasPorDia.size(), 1);
        QCOMPARE(r.ventasPorDia.first().first, hoy);
        QCOMPARE(r.ventasPorMes.size(), 1);
        QCOMPARE(r.ventasPorMes.first().first, hoy.toString("yyyy-MM"));

        // Otro período: nada
        const Reporte vacio = s.reporte(hoy.addDays(-30), hoy.addDays(-2));
        QCOMPARE(vacio.cantidadVentas, 0);
        QCOMPARE(vacio.ventas, 0.0);
        QVERIFY(vacio.masVendidos.isEmpty());
    }

    void gananciaConPorcentajeDelProducto()
    {
        Stock s(":memory:");
        const std::optional<Columna> col = columnaGanancia(s);
        QVERIFY(col);
        QVERIFY(col->propia());                                 // así la entienden las versiones anteriores
        QCOMPARE(col->tipo, TipoColumna::Numero);
        QVERIFY(!col->visible);

        Producto yerba = producto("Yerba", 10, {{"1 kg", 1300}});
        yerba.extra.insert(UUID_COLUMNA_GANANCIA, "30");        // costo 1000 + 30 % = 1300
        const qint64 idYerba = s.crear(yerba).id;
        Producto arroz = producto("Arroz", 10, {{"", 500}});
        arroz.extra.insert(UUID_COLUMNA_GANANCIA, "25");        // tiene compras, pero manda el porcentaje
        const qint64 idArroz = s.crear(arroz).id;
        QVERIFY(s.registrarOperacion(TipoOperacion::Compra, {{idArroz, 5, QString(), 450}}).ok);
        const qint64 idSal = s.crear(producto("Sal", 10, {{"", 300}})).id;   // sin porcentaje ni compras
        QVERIFY(s.registrarOperacion(TipoOperacion::Venta, {{idYerba, 2, "1 kg", 1300}, {idArroz, 1, QString(), 500},
                                                            {idSal, 1, QString(), 300}}).ok);

        const QDate hoy = QDate::currentDate();
        Reporte r = s.reporte(hoy, hoy);
        QCOMPARE(r.ventasConCosto, 2600.0 + 500);
        QCOMPARE(r.costoDeLoVendido, 2000.0 + 400);
        QCOMPARE(r.ganancia, 600.0 + 100);
        QCOMPARE(r.unidadesSinCosto, 1);                       // la sal

        // Sin porcentaje, el arroz vuelve al costo de las compras (450)
        arroz = *s.obtener(idArroz);
        arroz.extra.remove(UUID_COLUMNA_GANANCIA);
        QVERIFY(s.actualizar(arroz).ok);
        r = s.reporte(hoy, hoy);
        QCOMPARE(r.ganancia, 600.0 + 50);

        // Si se elimina la columna, el porcentaje ya no cuenta
        QVERIFY(s.eliminarColumna(col->id).ok);
        r = s.reporte(hoy, hoy);
        QCOMPARE(r.unidadesSinCosto, 3);
    }

    void columnaGananciaSeCreaUnaSolaVez()
    {
        QTemporaryDir dir;
        const QString ruta = dir.filePath("ganancia.db");
        {
            Stock s(ruta);
            QVERIFY(s.eliminarColumna(columnaGanancia(s)->id).ok);
        }
        Stock s(ruta);
        QVERIFY(!columnaGanancia(s));                           // eliminada: no vuelve sola
        QCOMPARE(s.columnas().size(), 10);
    }

    void alertasDeStockYVencimiento()
    {
        Stock s(":memory:");
        const QDate hoy(2026, 11, 13);
        Producto poco = producto("Avena", 2, {});
        poco.stockMinimo = 5;
        Producto nada = producto("Harina", 0, {});
        nada.stockMinimo = 1;
        Producto bien = producto("Sal", 10, {});
        bien.stockMinimo = 3;
        Producto vencido = producto("Yogur", 4, {});
        vencido.vencimiento = "2026-11-01";
        Producto porVencer = producto("Queso", 4, {});
        porVencer.vencimiento = "2026-11-20";
        Producto lejos = producto("Arroz", 4, {});
        lejos.vencimiento = "2027-06-01";
        Producto vencidoSinStock = producto("Leche", 0, {});
        vencidoSinStock.vencimiento = "2026-10-01";
        for (const Producto& p : {poco, nada, bien, vencido, porVencer, lejos, vencidoSinStock}) QVERIFY(s.crear(p).ok);

        const QList<Alerta> a = s.alertas(30, hoy);
        QStringList textos;
        for (const Alerta& x : a) textos << x.nombre + ": " + x.detalle;
        QCOMPARE(textos, (QStringList{"Harina: sin stock (mínimo 1)", "Avena: quedan 2 (mínimo 5)",
                                      "Yogur: venció el 01/11/2026", "Queso: vence en 7 días"}));
        QCOMPARE(a.at(2).tipo, Alerta::Tipo::Vencido);
        QCOMPARE(a.at(3).tipo, Alerta::Tipo::PorVencer);
        QCOMPARE(s.alertas(3, hoy).size(), 3);                // el queso todavía no
    }
};

QTEST_MAIN(TstDb)
#include "tst_db.moc"
