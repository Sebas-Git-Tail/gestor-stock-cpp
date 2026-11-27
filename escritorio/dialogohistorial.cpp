#include "Tema.h"
#include "dialogohistorial.h"
#include "ui_dialogo_historial.h"

#include "db.h"
#include "util.h"

#include <QCalendarWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <tuple>

namespace {
enum ColumnaArbol { ColFecha, ColTipo, ColDetalle, ColTotal, ColEstado };
constexpr int ROL_ID = Qt::UserRole + 1;    // el id de la venta/compra, en las filas de arriba

QString fechaTexto(const QString& iso, const QString& formato)
{
    const QDateTime f = QDateTime::fromString(iso, Qt::ISODate);
    return f.isValid() ? f.toString(formato) : iso;
}
}

DialogoHistorial::DialogoHistorial(Stock& stock, QWidget* parent)
    : QDialog(parent), ui(new Ui::DialogoHistorial), stock_(stock)
{
    ui->setupUi(this);
    armando_ = true;
    ui->comboTipo->addItems({tr("Ventas y compras"), tr("Solo ventas"), tr("Solo compras")});
    for (QDateEdit* fecha : {ui->editDesde, ui->editHasta})
        fecha->calendarWidget()->setFirstDayOfWeek(Qt::Sunday);   // como en el resto del programa
    const QDate hoy = QDate::currentDate();
    ui->editDesde->setDate(hoy.addMonths(-1));
    ui->editHasta->setDate(hoy);
    armando_ = false;

    auto* cabecera = ui->arbol->header();
    cabecera->setStretchLastSection(false);
    cabecera->setSectionResizeMode(ColDetalle, QHeaderView::Stretch);
    ui->arbol->setColumnWidth(ColFecha, 190);
    ui->arbol->setColumnWidth(ColTipo, 90);
    ui->arbol->setColumnWidth(ColTotal, 110);
    ui->arbol->setColumnWidth(ColEstado, 170);
    ui->btnAnular->setIcon(tema::icono(QStringLiteral("deshacer")));
    tema::marcarPeligro(ui->btnAnular);
    ui->buttonBox->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));

    connect(ui->comboTipo, &QComboBox::currentIndexChanged, this, [this] { if (!armando_) cargar(); });
    connect(ui->editDesde, &QDateEdit::dateChanged, this, [this] { if (!armando_) cargar(); });
    connect(ui->editHasta, &QDateEdit::dateChanged, this, [this] { if (!armando_) cargar(); });
    connect(ui->checkAnuladas, &QCheckBox::toggled, this, [this] { cargar(); });
    connect(ui->arbol, &QTreeWidget::currentItemChanged, this, [this] { actualizarBoton(); });
    connect(ui->btnAnular, &QPushButton::clicked, this, &DialogoHistorial::anularElegida);
    cargar();
}

DialogoHistorial::~DialogoHistorial()
{
    delete ui;
}

QTreeWidget* DialogoHistorial::arbol() const { return ui->arbol; }
void DialogoHistorial::elegirTipo(int indice) { ui->comboTipo->setCurrentIndex(indice); }
QString DialogoHistorial::textoResumen() const { return ui->labelResumen->text(); }

void DialogoHistorial::fijarFechas(const QDate& desde, const QDate& hasta)
{
    armando_ = true;
    ui->editDesde->setDate(desde);
    ui->editHasta->setDate(hasta);
    armando_ = false;
    cargar();
}

void DialogoHistorial::elegir(int fila)
{
    if (QTreeWidgetItem* item = ui->arbol->topLevelItem(fila)) ui->arbol->setCurrentItem(item);
}

void DialogoHistorial::cargar()
{
    FiltroOperaciones f;
    if (ui->comboTipo->currentIndex() == 1) f.tipo = TipoOperacion::Venta;
    else if (ui->comboTipo->currentIndex() == 2) f.tipo = TipoOperacion::Compra;
    f.desde = ui->editDesde->date();
    f.hasta = ui->editHasta->date();
    if (f.desde > f.hasta) std::swap(f.desde, f.hasta);
    f.conAnuladas = ui->checkAnuladas->isChecked();
    f.limite = 2000;
    operaciones_ = stock_.operaciones(f);

    const qint64 elegido = idElegido();
    ui->arbol->clear();
    int ventas = 0, compras = 0, anuladas = 0;
    double totalVentas = 0;
    const QColor apagado = tema::colores().texto3;
    for (const Operacion& o : operaciones_) {
        const bool venta = o.tipo == TipoOperacion::Venta;
        if (o.estaAnulada()) ++anuladas;
        else if (venta) { ++ventas; totalVentas += o.total; }
        else ++compras;

        // Arriba: cuándo, qué, un resumen de los productos y el total; abajo (al abrirla), producto por producto
        QStringList productos;
        for (const ItemOperacion& i : o.items) productos << QStringLiteral("%1 ×%2").arg(i.nombre).arg(i.cantidad);
        QStringList detalle{productos.join(QStringLiteral(", "))};
        if (!o.cliente.isEmpty()) detalle << tr("Cliente: %1").arg(o.cliente);
        if (!o.medioPago.isEmpty()) detalle << o.medioPago;
        auto* fila = new QTreeWidgetItem(ui->arbol);
        fila->setText(ColFecha, fechaTexto(o.fecha, QStringLiteral("ddd dd/MM/yyyy HH:mm")));
        fila->setText(ColTipo, venta ? tr("Venta") : tr("Compra"));
        fila->setText(ColDetalle, detalle.join(QStringLiteral(" · ")));
        fila->setToolTip(ColDetalle, detalle.join(QLatin1Char('\n')));
        fila->setText(ColTotal, venta || o.total > 0 ? formatearPrecio(o.total) : QString());
        fila->setTextAlignment(ColTotal, Qt::AlignRight | Qt::AlignVCenter);
        fila->setData(ColFecha, ROL_ID, o.id);
        if (o.estaAnulada()) {
            fila->setText(ColEstado, tr("Anulada el %1").arg(fechaTexto(o.anulada, QStringLiteral("dd/MM/yyyy"))));
            for (int c = 0; c < ui->arbol->columnCount(); ++c) {
                QFont letra = fila->font(c);
                letra.setStrikeOut(c != ColEstado);
                fila->setFont(c, letra);
                fila->setForeground(c, apagado);
            }
        }
        for (const ItemOperacion& i : o.items) {
            auto* hijo = new QTreeWidgetItem(fila);
            hijo->setText(ColDetalle, (i.presentacion.isEmpty() ? i.nombre : i.nombre + QStringLiteral(" — ") + i.presentacion)
                                          + (i.precioUnitario > 0 ? QStringLiteral("   %1 × %2").arg(i.cantidad).arg(formatearPrecio(i.precioUnitario))
                                                                  : QStringLiteral("   ×%1").arg(i.cantidad)));
            if (i.precioUnitario > 0) hijo->setText(ColTotal, formatearPrecio(i.cantidad * i.precioUnitario));
            hijo->setTextAlignment(ColTotal, Qt::AlignRight | Qt::AlignVCenter);
        }
        for (const auto& [nombre, monto, signo] : {std::tuple{tr("Descuento"), o.descuento, QStringLiteral("− ")},
                                                   std::tuple{tr("Recargo"), o.recargo, QStringLiteral("+ ")}}) {
            if (monto <= 0) continue;
            auto* hijo = new QTreeWidgetItem(fila);
            hijo->setText(ColDetalle, nombre);
            hijo->setText(ColTotal, signo + formatearPrecio(monto));
            hijo->setTextAlignment(ColTotal, Qt::AlignRight | Qt::AlignVCenter);
        }
        if (o.id == elegido) ui->arbol->setCurrentItem(fila);
    }

    QStringList resumen;
    resumen << (ventas == 1 ? tr("1 venta por %1").arg(formatearPrecio(totalVentas))
                            : tr("%1 ventas por %2").arg(ventas).arg(formatearPrecio(totalVentas)));
    resumen << (compras == 1 ? tr("1 compra") : tr("%1 compras").arg(compras));
    if (anuladas) resumen << (anuladas == 1 ? tr("1 anulada (no se cuenta)") : tr("%1 anuladas (no se cuentan)").arg(anuladas));
    ui->labelResumen->setText(operaciones_.isEmpty() ? tr("No hay ventas ni compras en estas fechas.")
                                                     : resumen.join(QStringLiteral(" · ")) + QStringLiteral(". ")
                                                           + tr("Tocá una para ver el detalle."));
    actualizarBoton();
}

qint64 DialogoHistorial::idElegido() const
{
    QTreeWidgetItem* item = ui->arbol->currentItem();
    if (item && item->parent()) item = item->parent();       // eligió un renglón del detalle: vale su venta/compra
    return item ? item->data(ColFecha, ROL_ID).toLongLong() : 0;
}

void DialogoHistorial::actualizarBoton()
{
    const qint64 id = idElegido();
    bool anulable = false;
    for (const Operacion& o : operaciones_) if (o.id == id) anulable = !o.estaAnulada();
    ui->btnAnular->setEnabled(anulable);
}

void DialogoHistorial::anularElegida()
{
    const qint64 id = idElegido();
    const Operacion* o = nullptr;
    for (const Operacion& x : operaciones_) if (x.id == id) o = &x;
    if (!o || o->estaAnulada()) return;
    const bool venta = o->tipo == TipoOperacion::Venta;
    QMessageBox caja(this);
    caja.setIcon(QMessageBox::Question);
    caja.setWindowTitle(venta ? tr("Anular la venta") : tr("Anular la compra"));
    caja.setText(venta ? tr("¿Anular la venta del %1 por %2?").arg(fechaTexto(o->fecha, QStringLiteral("dd/MM/yyyy HH:mm")), formatearPrecio(o->total))
                       : tr("¿Anular la compra del %1?").arg(fechaTexto(o->fecha, QStringLiteral("dd/MM/yyyy HH:mm"))));
    caja.setInformativeText(venta ? tr("Lo vendido vuelve al stock y los reportes ya no la cuentan (sirve también para una devolución). "
                                       "Queda en el historial como anulada. No se puede deshacer.")
                                  : tr("Lo comprado se descuenta del stock y los reportes ya no la cuentan. "
                                       "Queda en el historial como anulada. No se puede deshacer."));
    QPushButton* anular = caja.addButton(tr("Anular"), QMessageBox::DestructiveRole);
    caja.addButton(tr("Cancelar"), QMessageBox::RejectRole);
    caja.exec();
    if (caja.clickedButton() != anular) return;
    const Resultado r = stock_.anularOperacion(id);
    if (!r.ok) {
        QMessageBox::warning(this, caja.windowTitle(), r.error);
        return;
    }
    huboCambios_ = true;
    cargar();
}
