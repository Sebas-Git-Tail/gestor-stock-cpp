#include "dialogooperacion.h"
#include "ui_dialogo_operacion.h"
#include "util.h"

#include <QComboBox>
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTimer>
#include <cmath>

namespace {
enum Columna { ColProducto, ColStock, ColVence, ColPresentacion, ColPrecio, ColCantidad, ColSubtotal };
constexpr int ROL_ID = Qt::UserRole;
constexpr int ROL_COMPLETAR = Qt::UserRole + 1;    // texto contra el que busca el autocompletado
constexpr int ROL_PRESENTACION = Qt::UserRole + 1; // en el combo de presentaciones: el texto real de la presentación
}

DialogoOperacion::DialogoOperacion(TipoOperacion tipo, const QList<Producto>& productos,
                                   const QList<qint64>& preseleccion, QWidget* parent)
    : QDialog(parent), ui(new Ui::DialogoOperacion), venta_(tipo == TipoOperacion::Venta), productos_(productos)
{
    ui->setupUi(this);
    for (int i = 0; i < productos_.size(); ++i) posicion_.insert(productos_[i].id, i);

    const QString nombreOperacion = venta_ ? tr("Venta") : tr("Compra");
    setWindowTitle(nombreOperacion);
    ui->labelTitulo->setText(venta_ ? tr("Venta: baja el stock y suma el importe") : tr("Compra: suma stock"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(venta_ ? tr("Confirmar venta") : tr("Confirmar compra"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));

    auto* tabla = ui->tablaLineas;
    tabla->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabla->verticalHeader()->setVisible(false);
    const int anchos[] = {330, 70, 110, 150, 130, 100, 130};
    for (int c = 0; c < 7; ++c) tabla->setColumnWidth(c, anchos[c]);
    tabla->horizontalHeader()->setSectionResizeMode(ColProducto, QHeaderView::Stretch);
    if (!venta_) {
        // Compra: sin presentación, pero con el costo de cada uno (opcional: sirve para calcular la
        // ganancia en ☰ → Reportes)
        tabla->setColumnHidden(ColPresentacion, true);
        if (auto* titulo = tabla->horizontalHeaderItem(ColPrecio)) {
            titulo->setText(tr("Costo c/u"));
            titulo->setToolTip(tr("Opcional: cuánto pagaste cada uno. Con esto, Reportes calcula la ganancia."));
        }
    }

    // Buscador de productos: escribir para filtrar; acepta mayúsculas, tildes y un pedazo del nombre
    auto* modelo = new QStandardItemModel(this);
    for (const Producto& p : productos_) {
        // En la lista se ve el precio (con su presentación) en vez de la categoría: así se distinguen los
        // tamaños de un mismo producto sin tener que probar. La categoría se sigue pudiendo buscar.
        const QString precios = textoPrecios(p.precios);
        auto* item = new QStandardItem(p.nombre + (precios.isEmpty() ? QString() : QStringLiteral(" — ") + precios)
                                       + QStringLiteral("  (stock %1)").arg(p.cantidad));
        item->setData(p.id, ROL_ID);
        item->setData(normalizarBusqueda(p.nombre + QLatin1Char(' ') + p.codigo + QLatin1Char(' ') + p.categoria + QLatin1Char(' ') + p.proveedor)
                          + QLatin1Char(' ') + (p.nombre + QLatin1Char(' ') + p.categoria).toLower(), ROL_COMPLETAR);
        modelo->appendRow(item);
    }
    auto* completer = new QCompleter(modelo, this);
    completer->setCompletionRole(ROL_COMPLETAR);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setMaxVisibleItems(12);
    // El buscador es un QLineEdit, no un QComboBox: un QComboBox con este autocompletado (que usa una lista
    // propia) intentaba seleccionar la fila elegida en SU lista vacía, vaciaba el buscador y el producto
    // que llegaba era el de esa posición en la lista completa (se agregaba otro producto).
    ui->inputBuscarProducto->setCompleter(completer);
    connect(completer, QOverload<const QModelIndex&>::of(&QCompleter::activated), this, [this](const QModelIndex& idx) {
        agregarProducto(idx.data(ROL_ID).toLongLong());
        // Si el Enter que eligió de la lista le llega al buscador después de esto (depende de la versión
        // de Qt), se ignora: si no, buscaba el mismo producto otra vez y la cantidad arrancaba en 2.
        recienElegido_ = true;
        QTimer::singleShot(0, this, [this] {
            ui->inputBuscarProducto->clear();
            recienElegido_ = false;
        });
    });
    connect(ui->btnAgregarLinea, &QPushButton::clicked, this, &DialogoOperacion::agregarDesdeBuscador);
    connect(ui->inputBuscarProducto, &QLineEdit::returnPressed, this, &DialogoOperacion::agregarDesdeBuscador);
    connect(ui->btnQuitarLinea, &QPushButton::clicked, this, &DialogoOperacion::quitarLinea);

    for (qint64 id : preseleccion) agregarProducto(id);
    recalcular();
}

DialogoOperacion::~DialogoOperacion() = default;

int DialogoOperacion::filaDe(qint64 id) const
{
    for (int f = 0; f < ui->tablaLineas->rowCount(); ++f) {
        if (ui->tablaLineas->item(f, ColProducto)->data(ROL_ID).toLongLong() == id) return f;
    }
    return -1;
}

QSpinBox* DialogoOperacion::spinCantidad(int fila) const
{
    return qobject_cast<QSpinBox*>(ui->tablaLineas->cellWidget(fila, ColCantidad));
}
QDoubleSpinBox* DialogoOperacion::spinPrecio(int fila) const
{
    return qobject_cast<QDoubleSpinBox*>(ui->tablaLineas->cellWidget(fila, ColPrecio));
}
QComboBox* DialogoOperacion::comboPresentacion(int fila) const
{
    return qobject_cast<QComboBox*>(ui->tablaLineas->cellWidget(fila, ColPresentacion));
}

int DialogoOperacion::cantidadDeLineas() const { return ui->tablaLineas->rowCount(); }

void DialogoOperacion::agregarProducto(qint64 id)
{
    const auto it = posicion_.constFind(id);
    if (it == posicion_.constEnd()) return;
    const Producto& p = productos_[*it];

    if (const int existente = filaDe(id); existente >= 0) {       // ya estaba: una unidad más
        QSpinBox* s = spinCantidad(existente);
        s->setValue(s->value() + 1);
        ui->tablaLineas->selectRow(existente);
        return;
    }

    auto* tabla = ui->tablaLineas;
    const int fila = tabla->rowCount();
    tabla->insertRow(fila);

    auto* nombre = new QTableWidgetItem(p.nombre);
    nombre->setData(ROL_ID, p.id);
    QStringList detalle;
    if (!p.categoria.isEmpty()) detalle << p.categoria;
    if (!p.proveedor.isEmpty()) detalle << tr("Proveedor: %1").arg(p.proveedor);
    nombre->setToolTip(detalle.join(QStringLiteral("\n")));
    tabla->setItem(fila, ColProducto, nombre);

    auto* stock = new QTableWidgetItem(QString::number(p.cantidad));
    stock->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tabla->setItem(fila, ColStock, stock);

    auto* vence = new QTableWidgetItem;
    if (!p.vencimiento.isEmpty()) {
        const QDate f = QDate::fromString(p.vencimiento, Qt::ISODate);
        vence->setText(f.toString(QStringLiteral("dd/MM/yyyy")));
        switch (estadoVencimiento(p.vencimiento)) {
        case EstadoVencimiento::Vencido:
            vence->setForeground(QColor(190, 30, 30));
            vence->setToolTip(tr("¡Vencido!"));
            break;
        case EstadoVencimiento::PorVencer:
            vence->setForeground(QColor(170, 110, 0));
            vence->setToolTip(tr("Vence pronto"));
            break;
        default: break;
        }
    }
    tabla->setItem(fila, ColVence, vence);

    if (!venta_) {
        auto* costo = new QDoubleSpinBox;
        costo->setDecimals(2);
        costo->setRange(0, 1e9);
        costo->setPrefix(QStringLiteral("$ "));
        costo->setSpecialValueText(tr("(sin cargar)"));
        costo->setToolTip(tr("Opcional: cuánto pagaste cada uno"));
        tabla->setCellWidget(fila, ColPrecio, costo);
        connect(costo, &QDoubleSpinBox::valueChanged, this, [this](double) { recalcular(); });
    }
    if (venta_) {
        auto* presentaciones = new QComboBox;
        if (p.precios.isEmpty()) {
            presentaciones->addItem(tr("Precio manual"), 0.0);
            presentaciones->setItemData(0, QString(), ROL_PRESENTACION);
        }
        for (int i = 0; i < p.precios.size(); ++i) {
            presentaciones->addItem(p.precios[i].presentacion.isEmpty() ? tr("Precio") : p.precios[i].presentacion,
                                    p.precios[i].precio);
            presentaciones->setItemData(i, p.precios[i].presentacion, ROL_PRESENTACION);
        }
        tabla->setCellWidget(fila, ColPresentacion, presentaciones);

        auto* precio = new QDoubleSpinBox;           // se puede corregir a mano (descuentos, etc.)
        precio->setDecimals(2);
        precio->setRange(0, 1e9);
        precio->setPrefix(QStringLiteral("$ "));
        precio->setValue(presentaciones->currentData().toDouble());
        tabla->setCellWidget(fila, ColPrecio, precio);
        connect(presentaciones, &QComboBox::currentIndexChanged, this, [presentaciones, precio](int) {
            precio->setValue(presentaciones->currentData().toDouble());
        });
        connect(precio, &QDoubleSpinBox::valueChanged, this, [this](double) { recalcular(); });
    }

    auto* cantidad = new QSpinBox;
    if (venta_) {
        cantidad->setRange(p.cantidad > 0 ? 1 : 0, p.cantidad);      // no se puede vender más de lo que hay
        cantidad->setValue(p.cantidad > 0 ? 1 : 0);
    } else {
        cantidad->setRange(1, 1000000);
        cantidad->setValue(1);
    }
    tabla->setCellWidget(fila, ColCantidad, cantidad);
    connect(cantidad, &QSpinBox::valueChanged, this, [this](int) { recalcular(); });

    auto* subtotal = new QTableWidgetItem;
    subtotal->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tabla->setItem(fila, ColSubtotal, subtotal);

    tabla->selectRow(fila);
    recalcular();
}

void DialogoOperacion::fijarCantidad(int fila, int cantidad) { spinCantidad(fila)->setValue(cantidad); }
void DialogoOperacion::fijarPrecio(int fila, double precio) { spinPrecio(fila)->setValue(precio); }
void DialogoOperacion::elegirPresentacion(int fila, int indice) { comboPresentacion(fila)->setCurrentIndex(indice); }

void DialogoOperacion::quitarLinea()
{
    if (ui->tablaLineas->currentRow() >= 0) ui->tablaLineas->removeRow(ui->tablaLineas->currentRow());
    recalcular();
}

void DialogoOperacion::recalcular()
{
    double total = 0;
    bool todoBien = ui->tablaLineas->rowCount() > 0;
    for (int f = 0; f < ui->tablaLineas->rowCount(); ++f) {
        const int cantidad = spinCantidad(f)->value();
        auto* nombre = ui->tablaLineas->item(f, ColProducto);
        const double subtotal = cantidad * spinPrecio(f)->value();
        total += subtotal;
        ui->tablaLineas->item(f, ColSubtotal)->setText(venta_ || subtotal > 0 ? formatearPrecio(subtotal) : QString());
        const bool sinStock = cantidad <= 0;
        nombre->setForeground(sinStock ? QBrush(QColor(190, 30, 30)) : QBrush());
        if (sinStock) todoBien = false;
    }
    total = std::round(total * 100.0) / 100.0;
    ui->labelTotal->setText(venta_ ? tr("Total: %1").arg(formatearPrecio(total))
                                   : tr("Total de la compra: %1").arg(formatearPrecio(total)));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(todoBien);
}

double DialogoOperacion::total() const
{
    double total = 0;
    for (int f = 0; f < ui->tablaLineas->rowCount(); ++f) total += spinCantidad(f)->value() * spinPrecio(f)->value();
    return std::round(total * 100.0) / 100.0;
}

QList<LineaOperacion> DialogoOperacion::lineas() const
{
    QList<LineaOperacion> res;
    for (int f = 0; f < ui->tablaLineas->rowCount(); ++f) {
        LineaOperacion l;
        l.productoId = ui->tablaLineas->item(f, ColProducto)->data(ROL_ID).toLongLong();
        l.cantidad = spinCantidad(f)->value();
        l.precioUnitario = spinPrecio(f)->value();      // compra: el costo (0 = no se cargó)
        if (venta_)
            l.presentacion = comboPresentacion(f)->currentData(ROL_PRESENTACION).toString();
        res.append(l);
    }
    return res;
}

void DialogoOperacion::agregarDesdeTexto(const QString& texto)
{
    if (texto.trimmed().isEmpty()) return;
    const QList<Producto> coincidencias = filtrarProductos(productos_, texto);
    if (coincidencias.size() == 1) {
        agregarProducto(coincidencias.first().id);
        ui->inputBuscarProducto->clear();
    } else if (coincidencias.isEmpty()) {
        QMessageBox::information(this, windowTitle(), tr("No encontré productos que coincidan con «%1».").arg(texto));
    } else if (auto* c = ui->inputBuscarProducto->completer()) {
        // Varios: que elija de la lista. Se abre en el próximo ciclo porque Qt la cierra al terminar el Enter.
        QTimer::singleShot(0, c, [c] { c->complete(); });
    }
}

void DialogoOperacion::agregarDesdeBuscador()
{
    // Un Enter con la lista abierta y una opción marcada es para la lista (ella agrega el producto):
    // en Qt 6.4 ese Enter le llega ANTES al buscador, y si no se ignora el producto se suma dos veces.
    if (const QCompleter* c = ui->inputBuscarProducto->completer();
        c && c->popup()->isVisible() && c->popup()->currentIndex().isValid())
        return;
    if (recienElegido_) return;                   // el mismo caso, cuando el Enter llega después
    agregarDesdeTexto(ui->inputBuscarProducto->text());
}

void DialogoOperacion::accept()
{
    if (ui->tablaLineas->rowCount() == 0) {
        QMessageBox::warning(this, windowTitle(), tr("Agregá al menos un producto."));
        return;
    }
    QStringList sinStock;
    for (int f = 0; f < ui->tablaLineas->rowCount(); ++f) {
        if (spinCantidad(f)->value() <= 0) sinStock << ui->tablaLineas->item(f, ColProducto)->text();
    }
    if (!sinStock.isEmpty()) {
        QMessageBox::warning(this, windowTitle(),
                             tr("No hay stock de:\n• %1\n\nQuitalos de la lista para continuar.").arg(sinStock.join(QStringLiteral("\n• "))));
        return;
    }
    QDialog::accept();
}
