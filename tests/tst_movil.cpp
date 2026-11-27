// Pruebas de la app del celular: el controlador y las pantallas QML.
#include "controlador.h"
#include "util.h"
#include "db.h"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "ModeloApps.h"
#include "Novedades.h"
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(GestorStockPlugin)
Q_IMPORT_QML_PLUGIN(Comun_UiPlugin)   // comun/ui/qml

namespace {
// Busca en las pantallas un elemento por objectName
QQuickItem* buscar(QQuickItem* raiz, const QString& nombre)
{
    if (!raiz) return nullptr;
    if (raiz->objectName() == nombre) return raiz;
    for (QQuickItem* hijo : raiz->childItems())
        if (QQuickItem* r = buscar(hijo, nombre)) return r;
    return nullptr;
}

// Los elementos de una clase (por ejemplo «CheckBox») que hay adentro, en orden de arriba a abajo
QList<QQuickItem*> deClase(QQuickItem* raiz, const char* clase)
{
    QList<QQuickItem*> lista;
    if (!raiz) return lista;
    if (QString::fromLatin1(raiz->metaObject()->className()).contains(QLatin1String(clase)) && raiz->isVisible()) lista << raiz;
    for (QQuickItem* hijo : raiz->childItems()) lista << deClase(hijo, clase);
    std::sort(lista.begin(), lista.end(), [](QQuickItem* a, QQuickItem* b) {
        return a->mapToScene(QPointF()).y() < b->mapToScene(QPointF()).y();
    });
    return lista;
}

void tocar(QQuickWindow* ventana, QQuickItem* item)
{
    QTest::mouseClick(ventana, Qt::LeftButton, {}, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    QTest::qWait(50);
}

// Carga Main.qml como main.cpp, pero sin salir a internet
struct Pantallas {
    QQmlApplicationEngine engine;
    apps::ModeloApps modeloApps{QStringLiteral("gestor-stock")};
    soporte::Novedades novedades;
    QStringList advertencias;
    QQuickWindow* ventana = nullptr;
    bool cargar()
    {
        qputenv("COMUN_APPS_SIN_RED", "1");
        engine.rootContext()->setContextProperty(QStringLiteral("Apps"), &modeloApps);
        engine.rootContext()->setContextProperty(QStringLiteral("Novedades"), &novedades);
        QObject::connect(&engine, &QQmlEngine::warnings, [this](const QList<QQmlError>& l) { for (const auto& e : l) advertencias << e.toString(); });
        engine.addImportPath("qrc:/");
        engine.load(QUrl("qrc:/GestorStock/qml/Main.qml"));
        if (engine.rootObjects().isEmpty()) return false;
        ventana = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        return ventana && QTest::qWaitForWindowExposed(ventana);
    }
};
}

class TstMovil : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QQuickStyle::setStyle("Material");
        if (QGuiApplication::platformName() == "offscreen") QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    }

    void productosColumnasYVentas()
    {
        Stock s(":memory:");
        Controlador c(s);
        QCOMPARE(c.productos().size(), 0);
        QVariantMap p = c.producto(0);
        p["nombre"] = "Avena"; p["cantidad"] = "10"; p["categoria"] = "Almacén";
        p["precios"] = QVariantList{QVariantMap{{"presentacion", "500 g"}, {"precio", "1.900,50"}}};
        QCOMPARE(c.guardarProducto(p), QString());
        QCOMPARE(c.productos().size(), 1);
        const qint64 id = c.productos().first().toMap().value("id").toLongLong();
        QVERIFY(c.productos().first().toMap().value("detalle").toString().contains("Almacén"));
        QVERIFY(!c.guardarProducto(QVariantMap{{"nombre", ""}}).isEmpty());
        QVERIFY(!c.guardarProducto(QVariantMap{{"nombre", "X"}, {"cantidad", "mucho"}}).isEmpty());
        QVERIFY(!c.guardarProducto(QVariantMap{{"nombre", "X"}, {"precios", QVariantList{QVariantMap{{"presentacion", "a"}, {"precio", "zzz"}}}}}).isEmpty());
        QCOMPARE(c.productos().size(), 1);

        // columna propia: aparece en el editor y en la lista, y se puede buscar
        QCOMPARE(c.agregarColumna("Marca", 0), QString());
        QVERIFY(!c.agregarColumna("marca", 0).isEmpty());
        QString uuid;
        for (const QVariant& v : c.columnas()) if (v.toMap().value("nombre") == "Marca") uuid = v.toMap().value("uuid").toString();
        QVERIFY(!uuid.isEmpty());
        QVariantMap e = c.producto(id);
        QCOMPARE(e.value("cantidad").toString(), QString("10"));
        e["extra"] = QVariantMap{{uuid, "Krocante"}};
        e["cantidad"] = "12";                                              // corrección de stock
        QCOMPARE(c.guardarProducto(e), QString());
        QVERIFY(c.productos().first().toMap().value("detalle").toString().contains("Marca: Krocante"));
        QCOMPARE(c.productos().first().toMap().value("cantidad").toInt(), 12);
        c.setBusqueda("krocante");
        QCOMPARE(c.productos().size(), 1);
        c.setBusqueda("zzz");
        QCOMPARE(c.productos().size(), 0);
        c.setBusqueda(QString());

        // venta y compra
        const QVariantMap v = c.registrarOperacion(true, {QVariantMap{{"id", id}, {"cantidad", 5}, {"presentacion", "500 g"}, {"precio", "2.000"}}});
        QVERIFY2(v.value("ok").toBool(), qPrintable(v.value("error").toString()));
        QCOMPARE(v.value("total").toString(), QString("$ 10.000"));
        QVERIFY(!c.registrarOperacion(true, {QVariantMap{{"id", id}, {"cantidad", 50}, {"precio", "1"}}}).value("ok").toBool());
        QVERIFY(c.registrarOperacion(false, {QVariantMap{{"id", id}, {"cantidad", 3}}}).value("ok").toBool());
        QCOMPARE(c.productos().first().toMap().value("cantidad").toInt(), 10);

        // columnas: mover, ocultar, quitar, volver a poner
        QCOMPARE(c.moverColumna(c.columnas().last().toMap().value("id").toLongLong(), -1), QString());
        QVERIFY(!c.eliminarColumna(c.columnas().at(1).toMap().value("id").toLongLong()).isEmpty());   // «Nombre»
        QCOMPARE(c.eliminarColumna(c.columnas().at(4).toMap().value("id").toLongLong()), QString());  // «Proveedor»
        QCOMPARE(c.columnasQuitadas().size(), 1);
        QCOMPARE(c.restaurarColumna(c.columnasQuitadas().first().toMap().value("id").toLongLong()), QString());
        QCOMPARE(c.eliminarProducto(id), QString());
        QCOMPARE(c.productos().size(), 0);
    }

    void pantallasQml()
    {
        Stock s(":memory:");
        Controlador c(s);
        Controlador::setInstancia(&c);
        for (const char* n : {"Avena", "Sal", "Yerba"}) {
            QVariantMap p = c.producto(0);
            p["nombre"] = n; p["cantidad"] = "5";
            QCOMPARE(c.guardarProducto(p), QString());
        }
        QStringList advertencias;
        QQmlApplicationEngine engine;
        // «Apps y actualizaciones» como en main.cpp, pero sin salir a internet
        qputenv("COMUN_APPS_SIN_RED", "1");
        apps::ModeloApps modeloApps(QStringLiteral("gestor-stock"));
        engine.rootContext()->setContextProperty(QStringLiteral("Apps"), &modeloApps);
        soporte::Novedades novedades;   // «Novedades» (comun/soporte): el diálogo de lo nuevo y el informe de fallas
        engine.rootContext()->setContextProperty(QStringLiteral("Novedades"), &novedades);
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError>& l) { for (const auto& e : l) advertencias << e.toString(); });
        engine.addImportPath("qrc:/");
        engine.load(QUrl("qrc:/GestorStock/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(advertencias.join('\n')));
        auto* ventana = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QVERIFY(QTest::qWaitForWindowExposed(ventana));
        QTest::qWait(100);
        // Un producto con poco stock: aparece el aviso y la página de avisos tiene algo
        QVariantMap conMinimo = c.producto(0);
        conMinimo["nombre"] = "Harina"; conMinimo["cantidad"] = "1"; conMinimo["stockMinimo"] = "4";
        c.guardarProducto(conMinimo);
        for (const char* accion : {"columnas", "venta", "compra", "sinc", "reportes", "avisos", "historial", "apps"}) {
            QMetaObject::invokeMethod(ventana, "abrir", Q_ARG(QVariant, QVariant(accion)));
            QTest::qWait(200);
        }
        QMetaObject::invokeMethod(ventana, "editar", Q_ARG(QVariant, QVariant(c.productos().first().toMap().value("id"))));
        QTest::qWait(200);
        QMetaObject::invokeMethod(ventana, "editar", Q_ARG(QVariant, QVariant(0)));
        QTest::qWait(200);
        QMetaObject::invokeMethod(ventana, "crearConElCodigo", Q_ARG(QVariant, QVariant("7790001")));   // código escaneado que no existía
        QTest::qWait(200);
        QMetaObject::invokeMethod(ventana, "avisarEliminado", Q_ARG(QVariant, QVariant("Sal")));
        QTest::qWait(100);
        QVERIFY2(advertencias.isEmpty(), qPrintable(advertencias.join('\n')));
        Controlador::setInstancia(nullptr);
    }

    void reportesYAvisos()
    {
        Stock s(":memory:");
        Controlador c(s);
        QVariantMap p = c.producto(0);
        p["nombre"] = "Avena"; p["cantidad"] = "0"; p["stockMinimo"] = "2";
        QCOMPARE(c.guardarProducto(p), QString());
        const qint64 id = c.productos().first().toMap().value("id").toLongLong();
        QCOMPARE(c.resumenAlertas(), QString("1 con poco stock"));
        // Compra con costo y una venta
        QVERIFY(c.registrarOperacion(false, {QVariantMap{{"id", id}, {"cantidad", 10}, {"precio", "1.000,50"}}}).value("ok").toBool());
        QVERIFY(!c.registrarOperacion(false, {QVariantMap{{"id", id}, {"cantidad", 1}, {"precio", "mucho"}}}).value("ok").toBool());
        QVERIFY(c.registrarOperacion(true, {QVariantMap{{"id", id}, {"cantidad", 2}, {"precio", "1500"}}}).value("ok").toBool());
        QCOMPARE(c.resumenAlertas(), QString());              // ya hay stock (8 > 2)
        const QVariantMap r = c.reporte(0);
        QCOMPARE(r.value("ventas").toString(), formatearPrecio(3000));
        QCOMPARE(r.value("compras").toString(), formatearPrecio(10005));
        QCOMPARE(r.value("ganancia").toString(), formatearPrecio(3000 - 2 * 1000.5));
        QCOMPARE(r.value("masVendidos").toList().size(), 1);
    }

    // ---------------------------------------------------------------- 2.10.0
    void historialAnularDeshacerYArchivos()
    {
        QTemporaryDir dir;
        Stock s(":memory:");
        Controlador c(s);
        QVariantMap p = c.producto(0);
        p["nombre"] = "Avena"; p["cantidad"] = "10"; p["codigo"] = "779123";
        p["precios"] = QVariantList{QVariantMap{{"presentacion", ""}, {"precio", "1000"}}};
        QCOMPARE(c.guardarProducto(p), QString());
        const qint64 id = c.idPorCodigo("779123");
        QVERIFY(id > 0);
        QCOMPARE(c.idPorCodigo("000"), qint64(0));
        const auto cantidad = [&] { return c.productos().first().toMap().value("cantidad").toInt(); };

        // venta con medio de pago, cliente y descuento
        const QVariantList linea{QVariantMap{{"id", id}, {"cantidad", 3}, {"precio", "1000"}}};
        QVERIFY(!c.registrarOperacion(true, linea, {{"descuento", "mucho"}}).value("ok").toBool());
        const QVariantMap v = c.registrarOperacion(true, linea, {{"medioPago", "Efectivo"}, {"cliente", "Juana"}, {"descuento", "500"}});
        QVERIFY2(v.value("ok").toBool(), qPrintable(v.value("error").toString()));
        QCOMPARE(v.value("total").toString(), formatearPrecio(2500));
        QVERIFY(c.registrarOperacion(false, {QVariantMap{{"id", id}, {"cantidad", 2}}}).value("ok").toBool());
        QCOMPARE(cantidad(), 9);

        // historial
        QCOMPARE(c.historial(0, c.fechaIso(1), c.hoyIso(), true).size(), 2);
        QCOMPARE(c.historial(0, c.fechaIso(-1), c.fechaIso(-2), true).size(), 0);   // mañana y pasado: nada
        QCOMPARE(c.historial(2, "", "", true).size(), 1);
        const QVariantMap venta = c.historial(1, "", "", true).first().toMap();
        QVERIFY(venta.value("datos").toString().contains("Juana"));
        QVERIFY(venta.value("datos").toString().contains("Efectivo"));
        QCOMPARE(venta.value("items").toList().size(), 2);                           // el producto y el descuento
        QCOMPARE(c.reporteFechas(c.hoyIso(), c.hoyIso()).value("ventas").toString(), formatearPrecio(2500));

        // anular
        QCOMPARE(c.anularOperacion(venta.value("id").toLongLong()), QString());
        QCOMPARE(cantidad(), 12);
        QVERIFY(c.historial(1, "", "", true).first().toMap().value("anulada").toBool());
        QCOMPARE(c.historial(1, "", "", false).size(), 0);
        QVERIFY(!c.anularOperacion(venta.value("id").toLongLong()).isEmpty());
        QCOMPARE(c.reporteFechas(c.hoyIso(), c.hoyIso()).value("ventas").toString(), formatearPrecio(0));
        QCOMPARE(c.rangoPeriodo(0).value("desde").toString(), c.hoyIso());
        QCOMPARE(c.fechaCorta("2026-11-26"), QString("26/11/2026"));

        // deshacer al eliminar
        QVERIFY(!c.deshacerEliminar().isEmpty());                                    // no hay nada
        QCOMPARE(c.eliminarProducto(id), QString());
        QCOMPARE(c.productos().size(), 0);
        QCOMPARE(c.deshacerEliminar(), QString());
        QCOMPARE(c.productos().size(), 1);
        QCOMPARE(cantidad(), 12);
        QCOMPARE(c.idPorCodigo("779123"), id);
        QVERIFY(!c.deshacerEliminar().isEmpty());                                    // una sola vez

        // exportar CSV y copia de seguridad
        QCOMPARE(c.exportar(QUrl::fromLocalFile(dir.filePath("stock.csv"))), QString());
        QFile csv(dir.filePath("stock.csv"));
        QVERIFY(csv.open(QIODevice::ReadOnly));
        const QByteArray contenido = csv.readAll();
        QVERIFY(contenido.startsWith("\xEF\xBB\xBF"));                             // con BOM, para Excel
        QVERIFY(contenido.contains("Avena;"));
        QVERIFY(contenido.contains("779123"));
        QCOMPARE(c.backup(QUrl::fromLocalFile(dir.filePath("copia.db"))), QString());
        Stock copia(dir.filePath("copia.db"));
        QVERIFY(copia.abierta());
        QCOMPARE(copia.listar().size(), 1);
        QCOMPARE(copia.operaciones().size(), 2);
    }

    void paginaDeColumnas()
    {
        Stock s(":memory:");
        Controlador c(s);
        Controlador::setInstancia(&c);
        Pantallas pantallas;
        QVERIFY2(pantallas.cargar(), qPrintable(pantallas.advertencias.join('\n')));
        QQuickWindow* ventana = pantallas.ventana;
        QMetaObject::invokeMethod(ventana, "abrir", Q_ARG(QVariant, QVariant("columnas")));
        QTest::qWait(400);                                                            // la animación del StackView
        QQuickItem* lista = buscar(ventana->contentItem(), "listaColumnas");
        QVERIFY(lista);
        QCOMPARE(lista->property("count").toInt(), int(c.columnas().size()));

        // Una columna nueva aparece sola en la lista
        QCOMPARE(c.agregarColumna("Marca", 0), QString());
        QTest::qWait(50);
        QCOMPARE(lista->property("count").toInt(), int(c.columnas().size()));

        // El tilde de la primera («Código») la oculta
        QList<QQuickItem*> tildes = deClase(lista, "CheckBox");
        QVERIFY(!tildes.isEmpty());
        QCOMPARE(c.columnas().first().toMap().value("nombre").toString(), QString("Código"));
        QVERIFY(c.columnas().first().toMap().value("visible").toBool());
        tocar(ventana, tildes.first());
        QVERIFY(!c.columnas().first().toMap().value("visible").toBool());
        QCOMPARE(s.columnaPorClave("codigo")->visible, false);

        // «Bajar» en la primera la cambia de lugar con la segunda; la primera no se puede subir
        QList<QQuickItem*> botones;
        for (QQuickItem* b : deClase(lista, "Button")) if (b->property("text").toString() == "Bajar") botones << b;
        QVERIFY(!botones.isEmpty());
        tocar(ventana, botones.first());
        QCOMPARE(c.columnas().at(0).toMap().value("nombre").toString(), QString("Nombre"));
        QCOMPARE(c.columnas().at(1).toMap().value("nombre").toString(), QString("Código"));
        for (QQuickItem* b : deClase(lista, "Button"))
            if (b->property("text").toString() == "Subir") { QVERIFY(!b->property("enabled").toBool()); break; }
        QVERIFY2(pantallas.advertencias.isEmpty(), qPrintable(pantallas.advertencias.join('\n')));
        Controlador::setInstancia(nullptr);
    }

};

QTEST_MAIN(TstMovil)
#include "tst_movil.moc"
