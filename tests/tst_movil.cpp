// Pruebas de la app del celular: el controlador y las pantallas QML.
#include "controlador.h"
#include "db.h"
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QtQml/qqmlextensionplugin.h>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(GestorStockPlugin)

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
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError>& l) { for (const auto& e : l) advertencias << e.toString(); });
        engine.addImportPath("qrc:/");
        engine.load(QUrl("qrc:/GestorStock/qml/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(advertencias.join('\n')));
        auto* ventana = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QVERIFY(QTest::qWaitForWindowExposed(ventana));
        QTest::qWait(100);
        for (const char* accion : {"columnas", "venta", "compra", "sinc"}) {
            QMetaObject::invokeMethod(ventana, "abrir", Q_ARG(QVariant, QVariant(accion)));
            QTest::qWait(200);
        }
        QMetaObject::invokeMethod(ventana, "editar", Q_ARG(QVariant, QVariant(c.productos().first().toMap().value("id"))));
        QTest::qWait(200);
        QMetaObject::invokeMethod(ventana, "editar", Q_ARG(QVariant, QVariant(0)));
        QTest::qWait(200);
        QVERIFY2(advertencias.isEmpty(), qPrintable(advertencias.join('\n')));
        Controlador::setInstancia(nullptr);
    }
};

QTEST_MAIN(TstMovil)
#include "tst_movil.moc"
