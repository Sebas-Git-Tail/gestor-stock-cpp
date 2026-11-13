#include "dialogoreportes.h"
#include "ui_dialogo_reportes.h"

#include "db.h"
#include "util.h"

#include <QHeaderView>
#include <QLocale>
#include <QTableWidget>

namespace {
enum Periodo { Hoy, Semana, EsteMes, MesAnterior, EsteAnio, Personalizado };

void armarTabla(QTableWidget* t, const QStringList& titulos)
{
    t->setColumnCount(int(titulos.size()));
    t->setHorizontalHeaderLabels(titulos);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < titulos.size(); ++c) t->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
}

void ponerFila(QTableWidget* t, int fila, const QStringList& valores)
{
    for (int c = 0; c < valores.size(); ++c) {
        auto* item = new QTableWidgetItem(valores.at(c));
        if (c > 0) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        t->setItem(fila, c, item);
    }
}

QString nombreMes(const QString& anioMes)   // "2026-11" → "Noviembre 2026"
{
    const QDate d = QDate::fromString(anioMes + QStringLiteral("-01"), Qt::ISODate);
    if (!d.isValid()) return anioMes;
    QString mes = QLocale().standaloneMonthName(d.month());
    if (!mes.isEmpty()) mes[0] = mes[0].toUpper();
    return mes + u' ' + QString::number(d.year());
}
}

DialogoReportes::DialogoReportes(Stock& stock, Pestana pestana, QWidget* parent)
    : QDialog(parent), ui(new Ui::DialogoReportes), stock_(stock)
{
    ui->setupUi(this);
    QFont grande = ui->labelVentas->font();
    grande.setPointSizeF(grande.pointSizeF() * 1.6);
    grande.setBold(true);
    for (QLabel* l : {ui->labelVentas, ui->labelCompras, ui->labelGanancia}) l->setFont(grande);

    ui->comboPeriodo->addItems({tr("Hoy"), tr("Últimos 7 días"), tr("Este mes"), tr("Mes anterior"), tr("Este año"), tr("Elegir fechas")});
    armarTabla(ui->tablaMasVendidos, {tr("Producto"), tr("Unidades"), tr("Importe")});
    armarTabla(ui->tablaMeses, {tr("Mes"), tr("Ventas")});
    armarTabla(ui->tablaDias, {tr("Día"), tr("Ventas")});
    armarTabla(ui->tablaAvisos, {tr("Producto"), tr("Aviso")});

    connect(ui->comboPeriodo, &QComboBox::currentIndexChanged, this, &DialogoReportes::periodoElegido);
    connect(ui->editDesde, &QDateEdit::dateChanged, this, [this] { if (!armando_) { ui->comboPeriodo->setCurrentIndex(Personalizado); actualizar(); } });
    connect(ui->editHasta, &QDateEdit::dateChanged, this, [this] { if (!armando_) { ui->comboPeriodo->setCurrentIndex(Personalizado); actualizar(); } });

    ui->comboPeriodo->setCurrentIndex(EsteMes);
    periodoElegido(EsteMes);
    cargarAvisos();
    if (pestana == Pestana::Avisos) ui->pestanas->setCurrentWidget(ui->pestanaAvisos);
}

DialogoReportes::~DialogoReportes()
{
    delete ui;
}

void DialogoReportes::elegirPeriodo(int indice) { ui->comboPeriodo->setCurrentIndex(indice); }
QString DialogoReportes::textoVentas() const { return ui->labelVentas->text(); }
QString DialogoReportes::textoGanancia() const { return ui->labelGanancia->text(); }
int DialogoReportes::cantidadAvisos() const { return ui->tablaAvisos->rowCount(); }

void DialogoReportes::periodoElegido(int indice)
{
    const QDate hoy = QDate::currentDate();
    QDate desde = hoy, hasta = hoy;
    switch (indice) {
    case Hoy: break;
    case Semana: desde = hoy.addDays(-6); break;
    case EsteMes: desde = QDate(hoy.year(), hoy.month(), 1); break;
    case MesAnterior: {
        const QDate inicio = QDate(hoy.year(), hoy.month(), 1).addMonths(-1);
        desde = inicio;
        hasta = inicio.addMonths(1).addDays(-1);
        break;
    }
    case EsteAnio: desde = QDate(hoy.year(), 1, 1); break;
    default:                        // Elegir fechas: quedan las que están
        actualizar();
        return;
    }
    armando_ = true;
    ui->editDesde->setDate(desde);
    ui->editHasta->setDate(hasta);
    armando_ = false;
    actualizar();
}

void DialogoReportes::actualizar()
{
    QDate desde = ui->editDesde->date(), hasta = ui->editHasta->date();
    if (desde > hasta) std::swap(desde, hasta);
    const Reporte r = stock_.reporte(desde, hasta);

    ui->labelVentas->setText(formatearPrecio(r.ventas));
    ui->labelDetalleVentas->setText(r.cantidadVentas == 1 ? tr("1 venta, %1 unidades").arg(r.unidadesVendidas)
                                                          : tr("%1 ventas, %2 unidades").arg(r.cantidadVentas).arg(r.unidadesVendidas));
    ui->labelCompras->setText(formatearPrecio(r.compras));
    ui->labelDetalleCompras->setText(tr("(lo que tiene el costo cargado)"));
    ui->labelGanancia->setText(r.ventasConCosto > 0 ? formatearPrecio(r.ganancia) : QStringLiteral("—"));
    QString detalle;
    if (r.ventasConCosto > 0)
        detalle = tr("Vendido %1 − costo %2").arg(formatearPrecio(r.ventasConCosto), formatearPrecio(r.costoDeLoVendido));
    if (r.unidadesSinCosto > 0)
        detalle += (detalle.isEmpty() ? QString() : QStringLiteral(". "))
                   + tr("%n unidad(es) vendida(s) sin costo cargado no entran: cargá el costo en las compras.", nullptr, r.unidadesSinCosto);
    ui->labelDetalleGanancia->setText(detalle);

    ui->tablaMasVendidos->setRowCount(int(r.masVendidos.size()));
    for (int i = 0; i < r.masVendidos.size(); ++i)
        ponerFila(ui->tablaMasVendidos, i, {r.masVendidos[i].nombre, QString::number(r.masVendidos[i].cantidad),
                                            formatearPrecio(r.masVendidos[i].importe)});
    ui->tablaMeses->setRowCount(int(r.ventasPorMes.size()));
    for (int i = 0; i < r.ventasPorMes.size(); ++i)
        ponerFila(ui->tablaMeses, i, {nombreMes(r.ventasPorMes[i].first), formatearPrecio(r.ventasPorMes[i].second)});
    ui->tablaDias->setRowCount(int(r.ventasPorDia.size()));
    for (int i = 0; i < r.ventasPorDia.size(); ++i)
        ponerFila(ui->tablaDias, i, {QLocale().toString(r.ventasPorDia[i].first, QStringLiteral("dddd dd/MM/yyyy")),
                                     formatearPrecio(r.ventasPorDia[i].second)});
}

void DialogoReportes::cargarAvisos()
{
    const QList<Alerta> avisos = stock_.alertas();
    ui->tablaAvisos->setRowCount(int(avisos.size()));
    for (int i = 0; i < avisos.size(); ++i) {
        ponerFila(ui->tablaAvisos, i, {avisos[i].nombre, avisos[i].detalle});
        const QColor color = avisos[i].tipo == Alerta::Tipo::PorVencer ? QColor(170, 110, 0) : QColor(190, 30, 30);
        ui->tablaAvisos->item(i, 1)->setForeground(color);
        ui->tablaAvisos->item(i, 1)->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    ui->labelAvisos->setText(avisos.isEmpty()
        ? tr("No hay avisos: ningún producto está por debajo de su stock mínimo ni vence en los próximos 30 días.")
        : tr("Productos por debajo de su stock mínimo, vencidos o que vencen en los próximos 30 días."));
    ui->pestanas->setTabText(ui->pestanas->indexOf(ui->pestanaAvisos),
                             avisos.isEmpty() ? tr("Avisos") : tr("Avisos (%1)").arg(avisos.size()));
}
