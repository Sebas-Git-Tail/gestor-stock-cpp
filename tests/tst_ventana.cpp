#include "db.h"
#include "dialogocolumnas.h"
#include "dialogooperacion.h"
#include "dialogoproducto.h"
#include "dialogosincronizacion.h"
#include "portapapeles.h"
#include "sincronizacion.h"
#include "ventanaprincipal.h"
#include <QAbstractItemView>
#include <QClipboard>
#include <QCompleter>
#include <QMimeData>
#include <QStatusBar>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <functional>

namespace {

constexpr int N = 300;   // productos de prueba: "Producto 000" … "Producto 299"; el precio es 1000 + 10 * número

// Maneja las ventanas modales como lo haría una persona (cada 20 ms mira cuál está abierta)
class Conductor : public QObject
{
public:
    explicit Conductor(std::function<void(QWidget*)> alAbrirse) : alAbrirse_(std::move(alAbrirse))
    {
        connect(&timer_, &QTimer::timeout, this, [this] {
            if (QWidget* w = QApplication::activeModalWidget()) alAbrirse_(w);
        });
        timer_.start(20);
    }
private:
    std::function<void(QWidget*)> alAbrirse_;
    QTimer timer_;
};

void cerrarCualquierMensaje(QWidget* w)
{
    if (auto* m = qobject_cast<QMessageBox*>(w)) m->accept();
}

}

class TstVentana : public QObject
{
    Q_OBJECT
    QTemporaryDir dir_;
    std::unique_ptr<Stock> stock_;
    std::unique_ptr<QSettings> ajustes_;

    // Id del producto de la fila r (están ordenados por nombre y se crearon en orden: id = fila + 1)
    static qint64 id(int fila) { return fila + 1; }

    std::unique_ptr<VentanaPrincipal> abrir()
    {
        auto w = std::make_unique<VentanaPrincipal>(stock_.get(), ajustes_.get());
        w->resize(1200, 600);
        w->show();
        QTest::qWait(60);                     // deja pasar el restaurarPosicion() que se dispara solo
        return w;
    }

    static bool esVisible(VentanaPrincipal& w, qint64 idProducto)
    {
        const int f = w.filaDe(idProducto);
        if (f < 0) return false;
        const QRect r = w.tabla()->visualRect(w.tabla()->model()->index(f, 0));
        return r.isValid() && w.tabla()->viewport()->rect().intersects(r);
    }

private slots:
    void init()
    {
        stock_ = std::make_unique<Stock>(":memory:");
        for (int i = 0; i < N; ++i) {
            Producto p;
            p.nombre = QStringLiteral("Producto %1").arg(i, 3, 10, QLatin1Char('0'));
            p.categoria = i % 2 ? "Almacén" : "Limpieza";
            p.cantidad = 10 + i % 5;
            p.precios = {{"500 g", 1000.0 + 10 * i}};
            QVERIFY(stock_->crear(p).ok);
        }
        ajustes_ = std::make_unique<QSettings>(dir_.filePath(QStringLiteral("ajustes_%1.ini").arg(QTest::currentTestFunction())), QSettings::IniFormat);
    }

    // ---------------------------------------------------------------- tabla
    void columnasEnElOrdenDelUsuario()
    {
        auto w = abrir();
        QStringList titulos;
        for (int c = 0; c < w->tabla()->model()->columnCount(); ++c) titulos << w->tabla()->model()->headerData(c, Qt::Horizontal).toString();
        QCOMPARE(titulos, (QStringList{"Código", "Nombre", "Precios", "Categoría", "Proveedor", "Cantidad", "Medida bolsa", "Vencimiento", "Stock mín."}));
        QCOMPARE(w->cantidadDeFilas(), N);
        QCOMPARE(w->tabla()->model()->index(0, 2).data().toString(), QString("500 g: $ 1.000"));
    }

    // ---------------------------------------------------------------- buscador
    void buscarPorPrecio()
    {
        auto w = abrir();
        w->buscador()->setText("1500");
        QCOMPARE(w->cantidadDeFilas(), 1);                                   // el Producto 050 (1000 + 10 * 50)
        QCOMPARE(w->tabla()->model()->index(0, 1).data().toString(), QString("Producto 050"));
        w->buscador()->setText("1500-1520");
        QCOMPARE(w->cantidadDeFilas(), 3);
        w->buscador()->setText("<1030");
        QCOMPARE(w->cantidadDeFilas(), 3);                                   // 1000, 1010 y 1020
        w->buscador()->setText(">=3980");
        QCOMPARE(w->cantidadDeFilas(), 2);
        w->buscador()->setText("$ 1.500");
        QCOMPARE(w->cantidadDeFilas(), 1);
        w->buscador()->setText("producto 050 1500");                         // texto + precio a la vez
        QCOMPARE(w->cantidadDeFilas(), 1);
        w->buscador()->setText("");
        QCOMPARE(w->cantidadDeFilas(), N);
    }

    // ---------------------------------------------------------------- posición
    void refrescarNoMueveLaLista()
    {
        auto w = abrir();
        w->seleccionar({id(130)}, id(130));
        w->tabla()->scrollTo(w->tabla()->model()->index(120, 0), QAbstractItemView::PositionAtTop);
        const qint64 primera = w->idPrimeraFilaVisible();
        QCOMPARE(primera, id(120));
        w->refrescar();                                                      // por ejemplo, después de editar un producto
        QCOMPARE(w->idPrimeraFilaVisible(), primera);
        QCOMPARE(w->idActual(), id(130));
        QCOMPARE(w->idsSeleccionados(), (QList<qint64>{id(130)}));
    }

    void alTerminarLaBusquedaVuelveAdondeEstaba()
    {
        auto w = abrir();
        w->seleccionar({id(130)}, id(130));
        w->tabla()->scrollTo(w->tabla()->model()->index(120, 0), QAbstractItemView::PositionAtTop);
        const qint64 primera = w->idPrimeraFilaVisible();
        QVERIFY(w->tabla()->verticalScrollBar()->value() > 0);

        w->buscador()->setText("1500");                                      // empieza la búsqueda: queda 1 resultado
        QCOMPARE(w->cantidadDeFilas(), 1);
        w->buscador()->setText("");                                          // termina la búsqueda
        QCOMPARE(w->cantidadDeFilas(), N);
        QCOMPARE(w->idPrimeraFilaVisible(), primera);                        // no volvió al principio
        QCOMPARE(w->idActual(), id(130));                                    // y sigue marcado lo que estaba marcado
    }

    void alTerminarLaBusquedaSeQuedaEnLoQueSeEligio()
    {
        auto w = abrir();
        QCOMPARE(w->idPrimeraFilaVisible(), id(0));                          // empieza arriba de todo
        w->buscador()->setText("2500");                                      // el Producto 150
        QCOMPARE(w->cantidadDeFilas(), 1);
        w->seleccionar({id(150)}, id(150));                                  // lo elige entre los resultados
        w->buscador()->setText("");
        QCOMPARE(w->idActual(), id(150));
        QVERIFY(w->tabla()->verticalScrollBar()->value() > 0);               // no volvió al principio
        QVERIFY2(esVisible(*w, id(150)), "el producto elegido tiene que verse en la lista");
    }

    void laBusquedaEscritaPocoAPocoTambienConservaElLugar()
    {
        auto w = abrir();
        w->seleccionar({id(200)}, id(200));
        w->tabla()->scrollTo(w->tabla()->model()->index(190, 0), QAbstractItemView::PositionAtTop);
        const qint64 primera = w->idPrimeraFilaVisible();
        for (const QString& t : {QStringLiteral("p"), QStringLiteral("pr"), QStringLiteral("pro"), QStringLiteral("prod"), QStringLiteral("prodx")})
            w->buscador()->setText(t);                                       // tecla por tecla
        QCOMPARE(w->cantidadDeFilas(), 0);
        w->buscador()->clear();
        QCOMPARE(w->idPrimeraFilaVisible(), primera);
        QCOMPARE(w->idActual(), id(200));
    }

    void laPosicionSeGuardaYSeRestauraAlReabrir()
    {
        qint64 primera = 0;
        {
            auto w = abrir();
            w->seleccionar({id(110)}, id(110));
            w->tabla()->scrollTo(w->tabla()->model()->index(100, 0), QAbstractItemView::PositionAtTop);
            primera = w->idPrimeraFilaVisible();
            QCOMPARE(primera, id(100));
            w->close();                                                      // al cerrar guarda la posición
        }
        auto w2 = abrir();                                                   // programa nuevo, mismos ajustes
        QCOMPARE(w2->idActual(), id(110));
        QCOMPARE(w2->idPrimeraFilaVisible(), primera);                       // no empieza por el principio
    }

    void sePuedeCerrarEnMedioDeUnaBusqueda()
    {
        qint64 primera = 0;
        {
            auto w = abrir();
            w->tabla()->scrollTo(w->tabla()->model()->index(60, 0), QAbstractItemView::PositionAtTop);
            primera = w->idPrimeraFilaVisible();
            w->buscador()->setText("1500");                                  // se cierra con la búsqueda a medias
            w->close();
        }
        auto w2 = abrir();
        QCOMPARE(w2->cantidadDeFilas(), N);                                  // se abre con la lista completa…
        QCOMPARE(w2->idPrimeraFilaVisible(), primera);                       // …en el lugar de antes de buscar
    }

    void ordenYAnchosSeRecuerdan()
    {
        {
            auto w = abrir();
            w->tabla()->horizontalHeader()->setSortIndicator(5, Qt::DescendingOrder);   // por cantidad, de mayor a menor
            w->close();
        }
        auto w2 = abrir();
        QCOMPARE(w2->tabla()->horizontalHeader()->sortIndicatorSection(), 5);
        QVERIFY(w2->tabla()->horizontalHeader()->sortIndicatorOrder() == Qt::DescendingOrder);
        QCOMPARE(w2->tabla()->model()->index(0, 5).data().toString(), QString("14"));
    }

    // ---------------------------------------------------------------- ventas y compras desde la ventana
    void ventaDeVariosProductosDesdeLaVentana()
    {
        auto w = abrir();
        const int antes1 = stock_->obtener(id(0))->cantidad, antes2 = stock_->obtener(id(1))->cantidad;
        w->seleccionar({id(0), id(1)}, id(0));
        int mensajes = 0;
        Conductor c([&](QWidget* ventana) {
            if (auto* d = qobject_cast<DialogoOperacion*>(ventana)) {
                QCOMPARE(d->cantidadDeLineas(), 2);                          // los dos productos elegidos ya están cargados
                d->fijarCantidad(0, 2);
                d->fijarCantidad(1, 3);
                QCOMPARE(d->total(), 2 * 1000.0 + 3 * 1010.0);               // la suma de los importes
                d->accept();
            } else if (auto* m = qobject_cast<QMessageBox*>(ventana)) {
                QVERIFY(m->text().contains("$ 5.030"));                      // el cartel final muestra el total
                ++mensajes;
                m->accept();
            }
        });
        w->venta();
        QCOMPARE(mensajes, 1);
        QCOMPARE(stock_->obtener(id(0))->cantidad, antes1 - 2);              // baja el stock de cada uno
        QCOMPARE(stock_->obtener(id(1))->cantidad, antes2 - 3);
        const auto ops = stock_->operaciones();
        QCOMPARE(ops.size(), 1);
        QCOMPARE(ops[0].total, 5030.0);
        QCOMPARE(w->tabla()->model()->index(0, 5).data().toString(), QString::number(antes1 - 2));   // y la tabla se actualiza
    }

    void compraSumaStockDesdeLaVentana()
    {
        auto w = abrir();
        const int antes = stock_->obtener(id(5))->cantidad;
        w->seleccionar({id(5)}, id(5));
        Conductor c([&](QWidget* ventana) {
            if (auto* d = qobject_cast<DialogoOperacion*>(ventana)) {
                d->fijarCantidad(0, 10);
                d->accept();
            } else {
                cerrarCualquierMensaje(ventana);
            }
        });
        w->compra();
        QCOMPARE(stock_->obtener(id(5))->cantidad, antes + 10);
        QVERIFY(stock_->operaciones().first().tipo == TipoOperacion::Compra);
    }

    void cancelarLaVentaNoCambiaNada()
    {
        auto w = abrir();
        const int antes = stock_->obtener(id(0))->cantidad;
        w->seleccionar({id(0)}, id(0));
        Conductor c([&](QWidget* ventana) { if (auto* d = qobject_cast<DialogoOperacion*>(ventana)) d->reject(); });
        w->venta();
        QCOMPARE(stock_->obtener(id(0))->cantidad, antes);
        QVERIFY(stock_->operaciones().isEmpty());
    }

    void laVentaMantieneLaPosicionDeLaLista()
    {
        auto w = abrir();
        w->seleccionar({id(160)}, id(160));
        w->tabla()->scrollTo(w->tabla()->model()->index(150, 0), QAbstractItemView::PositionAtTop);
        const qint64 primera = w->idPrimeraFilaVisible();
        Conductor c([&](QWidget* ventana) {
            if (auto* d = qobject_cast<DialogoOperacion*>(ventana)) d->accept();
            else cerrarCualquierMensaje(ventana);
        });
        w->venta();
        QCOMPARE(w->idPrimeraFilaVisible(), primera);
        QCOMPARE(w->idActual(), id(160));
    }

    // ---------------------------------------------------------------- el diálogo de venta/compra
    void noSePuedeVenderMasQueElStock()
    {
        Producto p; p.nombre = "Poco"; p.cantidad = 3; p.precios = {{"", 100}};
        p.id = stock_->crear(p).id;
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {p.id});
        d.fijarCantidad(0, 99);
        QCOMPARE(d.lineas().first().cantidad, 3);                            // se frena en lo que hay
    }

    void productoSinStockBloqueaLaVenta()
    {
        Producto p; p.nombre = "Agotado"; p.cantidad = 0; p.precios = {{"", 100}};
        p.id = stock_->crear(p).id;
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {p.id});
        QCOMPARE(d.lineas().first().cantidad, 0);
        QVERIFY(!d.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
        Conductor c(cerrarCualquierMensaje);
        d.accept();                                                          // avisa y no se cierra
        QVERIFY(d.result() != QDialog::Accepted);
    }

    void presentacionYPrecioEnLaVenta()
    {
        Producto p; p.nombre = "Avena"; p.cantidad = 20; p.precios = {{"500 g", 1900}, {"1000 g", 3600}};
        p.id = stock_->crear(p).id;
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {p.id});
        d.fijarCantidad(0, 2);
        QCOMPARE(d.total(), 3800.0);                                         // 2 × 1900
        d.elegirPresentacion(0, 1);                                          // cambia a la de 1000 g
        QCOMPARE(d.total(), 7200.0);                                         // 2 × 3600
        QCOMPARE(d.lineas().first().presentacion, QString("1000 g"));
        d.fijarPrecio(0, 3500);                                              // descuento a mano
        QCOMPARE(d.total(), 7000.0);
        QCOMPARE(d.lineas().first().precioUnitario, 3500.0);
    }

    void unProductoSinPrecioSePuedeVenderConPrecioManual()
    {
        Producto p; p.nombre = "Suelto"; p.cantidad = 5;
        p.id = stock_->crear(p).id;
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {p.id});
        d.fijarPrecio(0, 250);
        d.fijarCantidad(0, 4);
        QCOMPARE(d.total(), 1000.0);
    }

    void agregarProductosConElBuscadorDelDialogo()
    {
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {});
        QCOMPARE(d.cantidadDeLineas(), 0);
        d.agregarDesdeTexto("producto 007");                                 // un único resultado: se agrega
        QCOMPARE(d.cantidadDeLineas(), 1);
        QCOMPARE(d.lineas().first().productoId, id(7));
        d.agregarProducto(id(7));                                            // el mismo otra vez: suma una unidad
        QCOMPARE(d.cantidadDeLineas(), 1);
        QCOMPARE(d.lineas().first().cantidad, 2);
        d.agregarDesdeTexto("1500");                                         // se puede buscar por precio
        QCOMPARE(d.cantidadDeLineas(), 2);
        d.agregarProducto(id(9));
        QCOMPARE(d.cantidadDeLineas(), 3);
    }

    void laCompraNoPideNiMuestraPrecios()
    {
        DialogoOperacion d(TipoOperacion::Compra, stock_->listar(), {id(1)});
        QCOMPARE(d.total(), 0.0);
        QCOMPARE(d.lineas().first().cantidad, 1);
        QCOMPARE(d.lineas().first().precioUnitario, 0.0);
    }

    // ---------------------------------------------------------------- diálogo de producto
    void dialogoDeProductoDevuelveLoQueSeCargo()
    {
        Producto p;
        p.id = 42; p.codigo = "A1"; p.nombre = "Yerba"; p.categoria = "Almacén"; p.proveedor = "Zucchi";
        p.cantidad = 6; p.stockMinimo = 2; p.medidaBolsa = "10x20"; p.notas = "nota"; p.vencimiento = "2027-01-31";
        p.precios = {{"500 g", 1900}, {"1000 g", 3700.5}};
        DialogoProducto d({"Almacén"}, {"Zucchi"}, &p);
        const Producto r = d.resultado();
        QCOMPARE(r.id, qint64(42));
        QCOMPARE(r.nombre, p.nombre);
        QCOMPARE(r.vencimiento, p.vencimiento);
        QCOMPARE(r.medidaBolsa, p.medidaBolsa);
        QCOMPARE(r.precios.size(), 2);
        QCOMPARE(r.precios[1].precio, 3700.5);
        QCOMPARE(r.cantidad, 6);
    }

    void dialogoDeProductoNoAceptaPrecioInvalido()
    {
        Producto p; p.nombre = "Yerba";
        DialogoProducto d({}, {}, &p);
        auto* tabla = d.findChild<QTableWidget*>("tablaPrecios");
        tabla->insertRow(0);
        tabla->setItem(0, 0, new QTableWidgetItem("500 g"));
        tabla->setItem(0, 1, new QTableWidgetItem("mil pesos"));
        int avisos = 0;
        Conductor c([&](QWidget* w) { if (qobject_cast<QMessageBox*>(w)) { ++avisos; cerrarCualquierMensaje(w); } });
        d.accept();
        QCOMPARE(avisos, 1);
        QVERIFY(d.result() != QDialog::Accepted);
        tabla->item(0, 1)->setText("1.900,50");
        d.accept();
        QVERIFY(d.result() == QDialog::Accepted);
        QCOMPARE(d.resultado().precios.first().precio, 1900.5);
    }

    // ---------------------------------------------------------------- columnas: la tabla sigue a la base
    static QStringList titulosDe(VentanaPrincipal& w)
    {
        QStringList l;
        for (int c = 0; c < w.tabla()->model()->columnCount(); ++c) l << w.tabla()->model()->headerData(c, Qt::Horizontal).toString();
        return l;
    }

    static int columnaConTitulo(VentanaPrincipal& w, const QString& titulo) { return titulosDe(w).indexOf(titulo); }

    void laTablaSigueALaEstructuraDeLaBase()
    {
        auto w = abrir();
        QCOMPARE(titulosDe(*w).size(), 9);
        QVERIFY(stock_->agregarColumna("Marca", TipoColumna::Texto).ok);
        w->refrescar();
        QCOMPARE(titulosDe(*w).size(), 10);
        QCOMPARE(titulosDe(*w).last(), QString("Marca"));                          // una columna nueva se ve al final

        QVERIFY(stock_->mostrarColumna(stock_->columnaPorClave("proveedor")->id, false).ok);
        w->refrescar();
        QVERIFY(!titulosDe(*w).contains("Proveedor"));                              // oculta: no se ve (los datos quedan)
        QCOMPARE(w->cantidadDeFilas(), N);

        QVERIFY(stock_->renombrarColumna(stock_->columnaPorClave("categoria")->id, "Rubro").ok);
        w->refrescar();
        QVERIFY(titulosDe(*w).contains("Rubro") && !titulosDe(*w).contains("Categoría"));

        QVERIFY(stock_->moverColumna(stock_->columnaPorClave("cantidad")->id, 0).ok);
        w->refrescar();
        QCOMPARE(titulosDe(*w).first(), QString("Cantidad"));
        QCOMPARE(w->tabla()->model()->index(0, 0).data().toString(), QString("10"));  // y la fila trae el dato de esa columna

        QVERIFY(stock_->eliminarColumna(stock_->columnaPorClave("medida_bolsa")->id).ok);
        QVERIFY(stock_->eliminarColumna(stock_->columnas().last().id).ok);          // «Marca»
        w->refrescar();
        QVERIFY(!titulosDe(*w).contains("Medida bolsa") && !titulosDe(*w).contains("Marca"));
        QCOMPARE(w->cantidadDeFilas(), N);
        QCOMPARE(w->columnasVisibles().size(), 7);
    }

    void losValoresDeLasColumnasPropiasSeVenYSeOrdenan()
    {
        const qint64 peso = stock_->agregarColumna("Peso (kg)", TipoColumna::Numero).id;
        const qint64 compra = stock_->agregarColumna("Última compra", TipoColumna::Fecha).id;
        const QString uPeso = stock_->columna(peso)->uuid, uCompra = stock_->columna(compra)->uuid;
        const QStringList pesos = {"10", "2,5", "1.234,5"};
        for (int i = 0; i < 3; ++i) {
            Producto p = *stock_->obtener(id(i));
            p.extra = {{uPeso, pesos[i]}, {uCompra, "01/11/2026"}};
            QVERIFY(stock_->actualizar(p).ok);
        }
        auto w = abrir();
        const int cPeso = columnaConTitulo(*w, "Peso (kg)"), cCompra = columnaConTitulo(*w, "Última compra");
        QVERIFY(cPeso >= 0 && cCompra >= 0);
        QCOMPARE(w->tabla()->model()->index(w->filaDe(id(1)), cPeso).data().toString(), QString("2,5"));        // se ve formateado
        QCOMPARE(w->tabla()->model()->index(w->filaDe(id(2)), cPeso).data().toString(), QString("1.234,5"));
        QCOMPARE(w->tabla()->model()->index(w->filaDe(id(0)), cCompra).data().toString(), QString("01/11/2026"));
        QCOMPARE(w->tabla()->model()->index(w->filaDe(id(5)), cPeso).data().toString(), QString());              // sin valor, vacío
        // ordena como número (2,5 < 10 < 1234,5) y no como texto, y lo que no tiene valor va al final
        w->tabla()->horizontalHeader()->setSortIndicator(cPeso, Qt::AscendingOrder);
        QCOMPARE(w->tabla()->model()->index(0, cPeso).data().toString(), QString("2,5"));
        QCOMPARE(w->tabla()->model()->index(1, cPeso).data().toString(), QString("10"));
        QCOMPARE(w->tabla()->model()->index(2, cPeso).data().toString(), QString("1.234,5"));
        QCOMPARE(w->tabla()->model()->index(3, cPeso).data().toString(), QString());
        // y se puede buscar por lo que se cargó ahí, escrito como se acostumbra
        w->buscador()->setText("1.234,5");
        QCOMPARE(w->cantidadDeFilas(), 1);
        w->buscador()->setText("1234.5");
        QCOMPARE(w->cantidadDeFilas(), 1);
    }

    void arrastrarUnTituloGuardaElNuevoOrden()
    {
        auto w = abrir();
        auto* cabecera = w->tabla()->horizontalHeader();
        const int cantidad = columnaConTitulo(*w, "Cantidad");
        cabecera->moveSection(cabecera->visualIndex(cantidad), 0);                  // lo mismo que arrastrarla al principio
        QCOMPARE(stock_->columnas().first().clave, QString("cantidad"));            // quedó guardado en la base
        QCOMPARE(titulosDe(*w).first(), QString("Cantidad"));
        QCOMPARE(w->cantidadDeFilas(), N);
        // una columna oculta conserva su lugar
        QVERIFY(stock_->mostrarColumna(stock_->columnaPorClave("proveedor")->id, false).ok);
        w->refrescar();
        cabecera->moveSection(cabecera->visualIndex(columnaConTitulo(*w, "Medida bolsa")), 0);
        QCOMPARE(titulosDe(*w).first(), QString("Medida bolsa"));
        QCOMPARE(titulosDe(*w).at(1), QString("Cantidad"));                          // las demás siguen en su orden
        QVERIFY(!stock_->columnaPorClave("proveedor")->visible);                      // la oculta sigue oculta…
        QCOMPARE(stock_->columnas().size(), 10);                                       // …y no se perdió ninguna
    }

    void elOrdenYElAnchoSeRecuerdanPorColumnaAunqueCambienLasDemas()
    {
        const qint64 peso = stock_->agregarColumna("Peso (kg)", TipoColumna::Numero).id;
        Q_UNUSED(peso);
        {
            auto w = abrir();
            const int c = columnaConTitulo(*w, "Peso (kg)");
            w->tabla()->horizontalHeader()->setSortIndicator(c, Qt::DescendingOrder);
            w->tabla()->setColumnWidth(c, 222);
            w->close();
        }
        QVERIFY(stock_->mostrarColumna(stock_->columnaPorClave("codigo")->id, false).ok);   // cambian los números de columna
        auto w2 = abrir();
        const int c2 = columnaConTitulo(*w2, "Peso (kg)");
        QCOMPARE(c2, 8);                                                             // (era la 9)
        QCOMPARE(w2->tabla()->horizontalHeader()->sortIndicatorSection(), c2);
        QVERIFY(w2->tabla()->horizontalHeader()->sortIndicatorOrder() == Qt::DescendingOrder);
        QCOMPARE(w2->tabla()->columnWidth(c2), 222);
    }

    void sinLaColumnaDeVencimientoNoHayAvisosDeVencimiento()
    {
        Producto p;
        p.nombre = "Vencido";
        p.vencimiento = QDate::currentDate().addDays(-3).toString(Qt::ISODate);
        const qint64 idVencido = stock_->crear(p).id;
        auto w = abrir();
        const QModelIndex celda = w->tabla()->model()->index(w->filaDe(idVencido), 1);
        QVERIFY(celda.data(Qt::BackgroundRole).isValid());                          // fila marcada como vencida
        QVERIFY(w->statusBar()->currentMessage().contains("1 vencidos"));
        QVERIFY(stock_->eliminarColumna(stock_->columnaPorClave("vencimiento")->id).ok);
        w->refrescar();
        QVERIFY(!w->tabla()->model()->index(w->filaDe(idVencido), 1).data(Qt::BackgroundRole).isValid());
        QVERIFY(!w->statusBar()->currentMessage().contains("vencidos"));
        QVERIFY(stock_->restaurarColumna(stock_->columnasQuitadas().first().id).ok);   // al volver a ponerla, vuelven los avisos
        w->refrescar();
        QVERIFY(w->tabla()->model()->index(w->filaDe(idVencido), 1).data(Qt::BackgroundRole).isValid());
    }

    // ---------------------------------------------------------------- columnas: diálogo de producto
    void elDialogoDeProductoSigueALasColumnas()
    {
        const qint64 marca = stock_->agregarColumna("Marca", TipoColumna::Texto).id;
        const qint64 peso = stock_->agregarColumna("Peso (kg)", TipoColumna::Numero).id;
        QVERIFY(stock_->renombrarColumna(stock_->columnaPorClave("proveedor")->id, "Distribuidor").ok);
        QVERIFY(stock_->eliminarColumna(stock_->columnaPorClave("medida_bolsa")->id).ok);
        QVERIFY(stock_->eliminarColumna(stock_->columnaPorClave("vencimiento")->id).ok);

        Producto p = *stock_->obtener(id(0));
        DialogoProducto d({}, {}, &p, nullptr, stock_->columnas());
        d.show();
        QTest::qWait(20);
        QCOMPARE(d.findChild<QLabel*>("labelProveedor")->text(), QString("Distribuidor:"));        // con el título nuevo
        QVERIFY(d.findChild<QLineEdit*>("inputMedidaBolsa")->isHidden());                        // quitadas: no aparecen
        QVERIFY(d.findChild<QCheckBox*>("checkVence")->isHidden());
        QVERIFY(!d.findChild<QLineEdit*>("inputNombre")->isHidden());
        QVERIFY(d.campoPropio("Marca") && !d.campoPropio("Marca")->isHidden());                   // y las propias, sí
        QVERIFY(d.campoPropio("Peso (kg)"));
        QVERIFY(!d.campoPropio("Peso (kg)")->placeholderText().isEmpty());

        d.campoPropio("Marca")->setText("  Krocante ");
        d.campoPropio("Peso (kg)")->setText("1.234,5");
        QCOMPARE(d.resultado().extra.value(stock_->columna(marca)->uuid), QString("Krocante"));
        QCOMPARE(d.resultado().extra.value(stock_->columna(peso)->uuid), QString("1234.5"));
        // un número que no es número: avisa y no cierra
        d.campoPropio("Peso (kg)")->setText("mucho");
        int avisos = 0;
        Conductor c([&](QWidget* w) { if (qobject_cast<QMessageBox*>(w)) { ++avisos; cerrarCualquierMensaje(w); } });
        d.accept();
        QCOMPARE(avisos, 1);
        QVERIFY(d.result() != QDialog::Accepted);
        d.campoPropio("Peso (kg)")->setText("");                                                  // vacío vale (queda sin dato)
        d.accept();
        QVERIFY(d.result() == QDialog::Accepted);
        QVERIFY(!d.resultado().extra.contains(stock_->columna(peso)->uuid));
    }

    void editarUnaFilaDesdeLaVentanaGuardaLasColumnasPropiasYLaCantidad()
    {
        const qint64 marca = stock_->agregarColumna("Marca", TipoColumna::Texto).id;
        auto w = abrir();
        const qint64 elegido = id(3);
        w->seleccionar({elegido}, elegido);
        const int cantidadInicial = stock_->obtener(elegido)->cantidad;
        bool hecho = false;
        Conductor c([&](QWidget* ventana) {
            auto* d = qobject_cast<DialogoProducto*>(ventana);
            if (!d || hecho) return;
            hecho = true;
            d->campoPropio("Marca")->setText("Krocante");
            d->findChild<QSpinBox*>("spinCantidad")->setValue(cantidadInicial + 7);
            d->accept();
        });
        w->editar();
        QVERIFY(hecho);
        const Producto g = *stock_->obtener(elegido);
        QCOMPARE(g.extra.value(stock_->columna(marca)->uuid), QString("Krocante"));
        QCOMPARE(g.cantidad, cantidadInicial + 7);
        const int col = columnaConTitulo(*w, "Marca");
        QCOMPARE(w->tabla()->model()->index(w->filaDe(elegido), col).data().toString(), QString("Krocante"));   // y la tabla se actualizó
    }

    void guardarOtrosCambiosNoPisaLasVentasHechasMientrasLaVentanaEstabaAbierta()
    {
        auto w = abrir();
        const qint64 elegido = id(3);
        w->seleccionar({elegido}, elegido);
        const int cantidadInicial = stock_->obtener(elegido)->cantidad;
        bool hecho = false;
        Conductor c([&](QWidget* ventana) {
            auto* d = qobject_cast<DialogoProducto*>(ventana);
            if (!d || hecho) return;
            hecho = true;
            // mientras la ventana de edición está abierta, llega una venta (por ejemplo, desde el celular)
            QVERIFY(stock_->registrarOperacion(TipoOperacion::Venta, {{elegido, 2, "", 100}}).ok);
            d->findChild<QLineEdit*>("inputMedidaBolsa")->setText("12x30");           // y la persona guarda otro dato, sin tocar la cantidad
            d->accept();
        });
        w->editar();
        QVERIFY(hecho);
        QCOMPARE(stock_->obtener(elegido)->medidaBolsa, QString("12x30"));
        QCOMPARE(stock_->obtener(elegido)->cantidad, cantidadInicial - 2);              // la venta no se perdió
    }

    void agregarUnaFilaDesdeLaVentanaConUnaColumnaPropia()
    {
        const qint64 marca = stock_->agregarColumna("Marca", TipoColumna::Texto).id;
        auto w = abrir();
        bool hecho = false;
        Conductor c([&](QWidget* ventana) {
            auto* d = qobject_cast<DialogoProducto*>(ventana);
            if (!d || hecho) return;
            hecho = true;
            d->findChild<QLineEdit*>("inputNombre")->setText("Producto nuevo");
            d->findChild<QSpinBox*>("spinCantidad")->setValue(9);
            d->campoPropio("Marca")->setText("Sol");
            d->accept();
        });
        w->agregar();
        QVERIFY(hecho);
        QCOMPARE(w->cantidadDeFilas(), N + 1);
        const Producto nuevo = stock_->listar("producto nuevo").first();
        QCOMPARE(nuevo.cantidad, 9);                                                  // el stock inicial
        QCOMPARE(nuevo.extra.value(stock_->columna(marca)->uuid), QString("Sol"));
    }

    // ---------------------------------------------------------------- columnas: ventana para administrarlas
    void laVentanaDeColumnasAgregaRenombraYOrdena()
    {
        DialogoColumnas d(*stock_);
        d.show();
        auto* lista = d.findChild<QListWidget*>("listaColumnas");
        QCOMPARE(lista->count(), 10);
        QVERIFY(lista->item(9)->text().startsWith("Notas"));
        QCOMPARE(lista->item(9)->checkState(), Qt::Unchecked);                        // las notas no se ven en la tabla
        QCOMPARE(lista->item(1)->checkState(), Qt::Checked);

        const Resultado r = d.agregar("Marca", TipoColumna::Texto);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(lista->count(), 11);
        QCOMPARE(d.seleccionada(), r.id);                                              // queda elegida la nueva
        QVERIFY(lista->currentItem()->text().contains("propia"));
        QVERIFY(!d.agregar("marca", TipoColumna::Texto).ok);                           // repetida
        QVERIFY(d.renombrar("Marca comercial").ok);
        QVERIFY(lista->currentItem()->text().startsWith("Marca comercial"));
        QVERIFY(d.cambiarTipo(TipoColumna::Numero).ok);
        QVERIFY(lista->currentItem()->text().contains("número"));
        QVERIFY(d.mover(-1).ok);                                                       // sube un lugar
        QCOMPARE(d.seleccionada(), r.id);
        QCOMPARE(lista->currentRow(), 9);
        QVERIFY(d.mover(+1).ok);
        QCOMPARE(lista->currentRow(), 10);
        // el tilde es «se ve en la tabla»
        lista->item(4)->setCheckState(Qt::Unchecked);                                   // Proveedor
        QVERIFY(!stock_->columnaPorClave("proveedor")->visible);
        QVERIFY(d.huboCambios());
        // el nombre siempre se ve
        const qint64 idNombre = stock_->columnaPorClave("nombre")->id;
        QVERIFY(!d.mostrar(idNombre, false).ok);
        // eliminar: las esenciales no (el botón ni se habilita); las propias, sí
        d.seleccionar(idNombre);
        QVERIFY(!d.findChild<QPushButton*>("btnEliminar")->isEnabled());
        d.seleccionar(stock_->columnaPorClave("cantidad")->id);
        QVERIFY(!d.findChild<QPushButton*>("btnEliminar")->isEnabled());
        QVERIFY(!d.eliminar().ok);
        d.seleccionar(r.id);
        QVERIFY(d.findChild<QPushButton*>("btnEliminar")->isEnabled());
        QVERIFY(d.findChild<QPushButton*>("btnTipo")->isEnabled());
        QVERIFY(d.eliminar().ok);
        QCOMPARE(lista->count(), 10);
        // una del programa se quita y se puede volver a poner
        d.seleccionar(stock_->columnaPorClave("categoria")->id);
        QVERIFY(!d.findChild<QPushButton*>("btnTipo")->isEnabled());                    // las del programa no cambian de tipo
        QVERIFY(!d.findChild<QPushButton*>("btnRestaurar")->isEnabled());
        QVERIFY(d.eliminar().ok);
        QCOMPARE(lista->count(), 9);
        QVERIFY(d.findChild<QPushButton*>("btnRestaurar")->isEnabled());
        QVERIFY(d.restaurar(stock_->columnasQuitadas().first().id).ok);
        QCOMPARE(lista->count(), 10);
        QCOMPARE(lista->item(9)->text().left(9), QString("Categoría"));                 // vuelve al final
    }

    void lasPreguntasDeColumnasFuncionanConSusVentanitas()
    {
        // agregar: primero el nombre y después el tipo
        int paso = 0;
        qint64 nueva = 0;
        {
            Conductor c([&](QWidget* w) {
                auto* q = qobject_cast<QInputDialog*>(w);
                if (!q) return;
                if (paso == 0) { q->setTextValue("Origen"); ++paso; q->accept(); }
                else if (paso == 1) { q->setTextValue("Fecha"); ++paso; q->accept(); }
            });
            QVERIFY(DialogoColumnas::pedirAgregar(*stock_, nullptr, &nueva));
            QCOMPARE(stock_->columna(nueva)->nombre, QString("Origen"));
            QVERIFY(stock_->columna(nueva)->tipo == TipoColumna::Fecha);
        }
        // renombrar
        const Columna origen = *stock_->columna(nueva);
        {
            bool hecho = false;
            Conductor c([&](QWidget* w) { if (auto* q = qobject_cast<QInputDialog*>(w); q && !hecho) { hecho = true; q->setTextValue("Procedencia"); q->accept(); } });
            QVERIFY(DialogoColumnas::pedirRenombrar(*stock_, origen, nullptr));
            QCOMPARE(stock_->columna(origen.id)->nombre, QString("Procedencia"));
        }
        // eliminar: pide confirmación; diciendo que no, no pasa nada
        {
            bool hecho = false;
            Conductor c([&](QWidget* w) { if (auto* m = qobject_cast<QMessageBox*>(w); m && !hecho) { hecho = true; m->button(QMessageBox::No)->click(); } });
            QVERIFY(!DialogoColumnas::pedirEliminar(*stock_, origen, nullptr));
            QVERIFY(stock_->columna(origen.id).has_value());
        }
        {
            bool hecho = false;
            Conductor c([&](QWidget* w) { if (auto* m = qobject_cast<QMessageBox*>(w); m && !hecho) { hecho = true; m->button(QMessageBox::Yes)->click(); } });
            QVERIFY(DialogoColumnas::pedirEliminar(*stock_, origen, nullptr));
            QVERIFY(!stock_->columna(origen.id).has_value());
        }
        // una esencial: lo explica y no la elimina
        {
            int avisos = 0;
            Conductor c([&](QWidget* w) { if (qobject_cast<QMessageBox*>(w)) { ++avisos; cerrarCualquierMensaje(w); } });
            QVERIFY(!DialogoColumnas::pedirEliminar(*stock_, *stock_->columnaPorClave("nombre"), nullptr));
            QCOMPARE(avisos, 1);
            QVERIFY(stock_->columnaActiva(CLAVE_NOMBRE));
        }
    }

    // ---------------------------------------------------------------- copiar y pegar con columnas propias
    void copiarYPegarIncluyenLasColumnasPropias()
    {
        const qint64 marca = stock_->agregarColumna("Marca", TipoColumna::Texto).id;
        const qint64 peso = stock_->agregarColumna("Peso (kg)", TipoColumna::Numero).id;
        const QString uMarca = stock_->columna(marca)->uuid, uPeso = stock_->columna(peso)->uuid;
        Producto p = *stock_->obtener(id(2));
        p.extra = {{uMarca, "Krocante"}, {uPeso, "2,5"}};
        QVERIFY(stock_->actualizar(p).ok);
        auto w = abrir();
        w->seleccionar({id(2)}, id(2));
        w->copiar();
        const QMimeData* datos = QApplication::clipboard()->mimeData();
        const QStringList filas = datos->text().split('\n', Qt::SkipEmptyParts);
        QVERIFY(filas.first().endsWith("\tMarca\tPeso (kg)"));                           // los títulos de las propias van al final
        QVERIFY2(filas.at(1).endsWith("\tKrocante\t2,5"), qPrintable(filas.at(1)));
        bool hecho = false;
        Conductor c([&](QWidget* ventana) {
            auto* m = qobject_cast<QMessageBox*>(ventana);
            if (!m || hecho) return;
            hecho = true;
            m->button(QMessageBox::Yes)->click();
        });
        w->pegar();
        QVERIFY(hecho);
        QCOMPARE(w->cantidadDeFilas(), N + 1);
        int copias = 0;
        for (const Producto& q : stock_->listar("Producto 002")) {
            if (q.id == id(2)) continue;
            ++copias;
            QCOMPARE(q.extra.value(uMarca), QString("Krocante"));                       // la copia trae lo cargado en las columnas propias
            QCOMPARE(q.extra.value(uPeso), QString("2.5"));
        }
        QCOMPARE(copias, 1);
    }

    // ---------------------------------------------------------------- sincronización con el celular
    void laVentanaSincronizaConUnCelularYSeActualizaSola()
    {
        ajustes_->setValue("sinc/puerto", 0);                                           // cualquier puerto libre (no choca con el programa real)
        auto w = abrir();
        QString error;
        QVERIFY2(DialogoSincronizacion::activar(w->servidor(), true, ajustes_.get(), &error), qPrintable(error));
        QVERIFY(ajustes_->value("sinc/activa").toBool());

        Stock cel(":memory:");
        Producto nuevo;
        nuevo.nombre = "Cargado en el celular";
        nuevo.cantidad = 7;
        QVERIFY(cel.crear(nuevo).ok);
        const qint64 marcaCel = cel.agregarColumna("Marca", TipoColumna::Texto).id;
        Q_UNUSED(marcaCel);
        ClienteSinc cliente(cel);
        const QString codigo = w->servidor().nuevoCodigo();
        QSignalSpy fin(&cliente, &ClienteSinc::terminado);
        cliente.emparejar("127.0.0.1", w->servidor().puerto(), codigo);
        QVERIFY(fin.wait(15000));
        QVERIFY2(fin.first().at(0).toBool(), qPrintable(fin.first().at(1).toString()));
        QTRY_COMPARE(w->cantidadDeFilas(), N + 1);                                     // la tabla se actualiza sola
        QTRY_VERIFY(titulosDe(*w).contains("Marca"));                                  // y también la estructura
        QCOMPARE(stock_->listar("cargado en el celular").first().cantidad, 7);
        QCOMPARE(cel.listar().size(), N + 1);                                          // y el celular recibió los de la PC

        // apagarla se recuerda
        QVERIFY(DialogoSincronizacion::activar(w->servidor(), false, ajustes_.get()));
        QVERIFY(!w->servidor().activo());
        QVERIFY(!ajustes_->value("sinc/activa").toBool());
    }

    void laVentanaDeSincronizacionMuestraElCodigoYLosCelulares()
    {
        ajustes_->setValue("sinc/puerto", 0);
        auto w = abrir();
        DialogoSincronizacion d(*stock_, w->servidor(), ajustes_.get());
        d.show();
        QVERIFY(!d.findChild<QCheckBox*>("chkActivar")->isChecked());
        QVERIFY(d.findChild<QLabel*>("labelCodigo")->text().isEmpty());
        d.findChild<QPushButton*>("btnEmparejar")->click();                              // activa y muestra el código
        QVERIFY(w->servidor().activo());
        QCOMPARE(d.findChild<QLabel*>("labelCodigo")->text().remove(' ').size(), 6);
        d.findChild<QPushButton*>("btnEmparejar")->click();                              // cancelar el código
        QVERIFY(d.findChild<QLabel*>("labelCodigo")->text().isEmpty());
        QCOMPARE(d.findChild<QListWidget*>("listaPares")->count(), 1);                   // «Ninguno todavía»
        QVERIFY(!d.findChild<QPushButton*>("btnQuitar")->isEnabled());
    }

    // ---------------------------------------------------------------- buscador de la venta (error de la cantidad en 2)
    // Escribe en el buscador como una persona y devuelve el buscador y su lista desplegable
    static QLineEdit* escribirEnBuscador(DialogoOperacion& d, const QString& texto)
    {
        d.show();
        QTest::qWait(30);
        auto* buscador = d.findChild<QLineEdit*>("inputBuscarProducto");
        buscador->setFocus();
        QTest::keyClicks(buscador, texto);
        QTest::qWait(30);
        return buscador;
    }

    void elegirDeLaListaConFlechaYEnterSumaUno()
    {
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {});
        auto* buscador = escribirEnBuscador(d, "producto 007");
        auto* lista = buscador->completer()->popup();
        QVERIFY(lista->isVisible());
        QTest::keyClick(lista, Qt::Key_Down);
        QTest::keyClick(lista, Qt::Key_Return);
        QTest::qWait(30);
        QCOMPARE(d.cantidadDeLineas(), 1);
        QCOMPARE(d.lineas().first().productoId, id(7));
        QCOMPARE(d.lineas().first().cantidad, 1);                         // antes arrancaba en 2
        QVERIFY(buscador->text().isEmpty());
    }

    void elegirLaSegundaOpcionConElMouseAgregaEsa()
    {
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {});
        auto* buscador = escribirEnBuscador(d, "producto 01");             // Producto 010 … 019
        auto* lista = buscador->completer()->popup();
        QVERIFY(lista->isVisible());
        const QModelIndex segunda = lista->model()->index(1, 0);
        QVERIFY(segunda.data().toString().startsWith("Producto 011"));
        QTest::mouseClick(lista->viewport(), Qt::LeftButton, {}, lista->visualRect(segunda).center());
        QTest::qWait(30);
        QCOMPARE(d.cantidadDeLineas(), 1);
        QCOMPARE(d.lineas().first().productoId, id(11));                  // antes entraba otro producto
        QCOMPARE(d.lineas().first().cantidad, 1);
    }

    void elegirElMismoDosVecesSumaDeAUno()
    {
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {});
        for (int vez = 0; vez < 2; ++vez) {
            auto* buscador = escribirEnBuscador(d, "producto 007");
            QTest::keyClick(buscador->completer()->popup(), Qt::Key_Down);
            QTest::keyClick(buscador->completer()->popup(), Qt::Key_Return);
            QTest::qWait(30);
        }
        QCOMPARE(d.cantidadDeLineas(), 1);
        QCOMPARE(d.lineas().first().cantidad, 2);                         // antes llegaba a 4
    }

    void enterSinElegirDeLaListaTambienAgrega()
    {
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {});
        auto* buscador = escribirEnBuscador(d, "producto 007");
        QTest::keyClick(buscador, Qt::Key_Return);                        // lista abierta pero sin nada marcado
        QTest::qWait(30);
        QCOMPARE(d.cantidadDeLineas(), 1);
        QCOMPARE(d.lineas().first().cantidad, 1);
    }

    void enterConVariosResultadosDejaLaListaAbierta()
    {
        DialogoOperacion d(TipoOperacion::Venta, stock_->listar(), {});
        auto* buscador = escribirEnBuscador(d, "producto 01");
        QTest::keyClick(buscador, Qt::Key_Return);
        QTest::qWait(50);
        QCOMPARE(d.cantidadDeLineas(), 0);                                // no adivina cuál
        QVERIFY(buscador->completer()->popup()->isVisible());             // deja elegir de la lista
    }

    // ---------------------------------------------------------------- copiar y pegar renglones
    static void aceptarPegado(QWidget* w)
    {
        if (auto* m = qobject_cast<QMessageBox*>(w)) {
            if (auto* si = m->button(QMessageBox::Yes)) si->click();
            else m->accept();
        }
    }

    void copiarPoneTextoYVersionCompleta()
    {
        auto w = abrir();
        w->seleccionar({id(5), id(3)}, id(3));
        w->copiar();
        const QMimeData* datos = QApplication::clipboard()->mimeData();
        QVERIFY(datos->hasFormat(MIME_PRODUCTOS));
        const QStringList filas = datos->text().trimmed().split('\n');
        QCOMPARE(filas.size(), 3);                                        // títulos + 2 renglones
        QVERIFY(filas[1].contains("Producto 003"));                       // en el orden de la tabla
        QVERIFY(filas[2].contains("Producto 005"));
        QVERIFY(filas[1].contains("500 g: $ 1.030"));
    }

    void pegarLoCopiadoCreaCopiasSinStock()
    {
        Producto conCodigo = *stock_->obtener(id(3));
        conCodigo.codigo = "X-3";
        conCodigo.notas = "nota que solo viaja en la versión completa";
        QVERIFY(stock_->actualizar(conCodigo).ok);
        auto w = abrir();
        w->seleccionar({id(3), id(5)}, id(3));
        w->copiar();
        int preguntas = 0;
        Conductor c([&](QWidget* v) { if (qobject_cast<QMessageBox*>(v)) ++preguntas; aceptarPegado(v); });
        w->pegar();
        QCOMPARE(preguntas, 1);
        const QList<Producto> todos = stock_->listar();
        QCOMPARE(todos.size(), N + 2);
        QList<Producto> copias;
        for (const Producto& p : todos) if (p.id > N) copias << p;
        QCOMPARE(copias.size(), 2);
        for (const Producto& p : copias) {
            QCOMPARE(p.cantidad, 0);                                      // copia nueva: sin stock
            QVERIFY(p.codigo.isEmpty());                                  // el código es único: la copia queda sin código
        }
        const Producto& copia3 = copias[0].nombre == "Producto 003" ? copias[0] : copias[1];
        QCOMPARE(copia3.notas, conCodigo.notas);
        QCOMPARE(copia3.precios.first().precio, 1030.0);
        QCOMPARE(stock_->obtener(id(3))->codigo, QString("X-3"));          // el original no se toca
        QCOMPARE(w->idsSeleccionados().size(), 2);                        // quedan marcadas las copias
        for (qint64 i : w->idsSeleccionados()) QVERIFY(i > N);
    }

    void pegarTextoDeUnaPlanilla()
    {
        auto w = abrir();
        QApplication::clipboard()->setText("Nombre\tCantidad\tPrecios\tCódigo\nChía premium\t5\t100 g: $ 1.600\tC-1\nLino\t\t$ 800\t\n");
        Conductor c(aceptarPegado);
        w->pegar();
        QCOMPARE(stock_->listar().size(), N + 2);
        const auto chia = stock_->listar("chia");
        QCOMPARE(chia.size(), 1);
        QCOMPARE(chia[0].cantidad, 5);                                    // desde una planilla, la cantidad se respeta
        QCOMPARE(chia[0].codigo, QString("C-1"));
        QCOMPARE(chia[0].precios.first().presentacion, QString("100 g"));
    }

    void pegarConCodigoRepetidoLoDejaSinCodigo()
    {
        Producto p = *stock_->obtener(id(0));
        p.codigo = "A1";
        stock_->actualizar(p);
        auto w = abrir();
        QApplication::clipboard()->setText("Código\tNombre\nA1\tOtro producto\n");
        Conductor c(aceptarPegado);
        w->pegar();
        const auto otro = stock_->listar("otro producto");
        QCOMPARE(otro.size(), 1);
        QVERIFY(otro[0].codigo.isEmpty());
        QVERIFY(w->statusBar()->currentMessage().contains("sin código"));
    }

    void cancelarElPegadoNoAgregaNada()
    {
        auto w = abrir();
        QApplication::clipboard()->setText("Nombre\nYerba\n");
        Conductor c([](QWidget* v) { if (auto* m = qobject_cast<QMessageBox*>(v)) m->button(QMessageBox::No)->click(); });
        w->pegar();
        QCOMPARE(stock_->listar().size(), N);
    }

    void pegarSinNadaUtilAvisa()
    {
        auto w = abrir();
        QApplication::clipboard()->setText("Nombre\tCantidad\n\t3\n");     // sin nombre
        int avisos = 0;
        Conductor c([&](QWidget* v) { if (qobject_cast<QMessageBox*>(v)) { ++avisos; cerrarCualquierMensaje(v); } });
        w->pegar();
        QCOMPARE(avisos, 1);
        QCOMPARE(stock_->listar().size(), N);
    }

    void atajosCtrlCYCtrlVEnLaTabla()
    {
        auto w = abrir();
        w->tabla()->setFocus();
        w->seleccionar({id(8)}, id(8));
        QApplication::clipboard()->clear();
        QTest::keyClick(w->tabla(), Qt::Key_C, Qt::ControlModifier);
        QVERIFY(QApplication::clipboard()->mimeData()->hasFormat(MIME_PRODUCTOS));
        QVERIFY(QApplication::clipboard()->text().contains("Producto 008"));
        Conductor c(aceptarPegado);
        QTest::keyClick(w->tabla(), Qt::Key_V, Qt::ControlModifier);
        QTest::qWait(30);
        QCOMPARE(stock_->listar("producto 008").size(), 2);
    }

    void ctrlCEnElBuscadorSigueCopiandoTexto()
    {
        auto w = abrir();
        w->buscador()->setText("yerba");
        w->buscador()->selectAll();
        w->buscador()->setFocus();
        QTest::keyClick(w->buscador(), Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("yerba"));    // no copia renglones
        QVERIFY(!QApplication::clipboard()->mimeData()->hasFormat(MIME_PRODUCTOS));
    }

    void pegarConservaLaPosicionDeLaLista()
    {
        auto w = abrir();
        w->tabla()->scrollTo(w->tabla()->model()->index(150, 0), QAbstractItemView::PositionAtTop);
        w->seleccionar({id(155)}, id(155));
        w->copiar();
        Conductor c(aceptarPegado);
        w->pegar();
        // la copia se llama igual y queda al lado del original: tiene que verse sin saltar al principio
        const qint64 copia = w->idsSeleccionados().first();
        QVERIFY(copia > N);
        QVERIFY(esVisible(*w, copia));
        QVERIFY(w->tabla()->verticalScrollBar()->value() > 0);
    }
};

QTEST_MAIN(TstVentana)
#include "tst_ventana.moc"
