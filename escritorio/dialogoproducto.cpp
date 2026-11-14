#include "dialogoproducto.h"
#include "ui_dialogo_producto.h"
#include "util.h"

#include <QCalendarWidget>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QTableWidgetItem>

namespace {
QString numeroComoTexto(double v) { return QString::number(v, 'g', QLocale::FloatingPointShortest); }
}

DialogoProducto::DialogoProducto(const QStringList& categorias, const QStringList& proveedores,
                                 const Producto* producto, QWidget* parent, const QList<Columna>& columnas)
    : QDialog(parent), ui(new Ui::DialogoProducto)
{
    ui->setupUi(this);
    ui->dateVencimiento->calendarWidget()->setFirstDayOfWeek(Qt::Sunday);   // pedido de la tía: la semana empieza el domingo
    ui->comboCategoria->addItems(categorias);
    ui->comboProveedor->addItems(proveedores);
    ui->tablaPrecios->setColumnWidth(0, 190);
    ui->dateVencimiento->setDate(QDate::currentDate());

    connect(ui->btnAgregarPrecio, &QPushButton::clicked, this, &DialogoProducto::filaNueva);
    connect(ui->btnQuitarPrecio, &QPushButton::clicked, this, &DialogoProducto::quitarFila);

    if (producto) {
        idProducto_ = producto->id;
        setWindowTitle(tr("Editar producto"));
        ui->inputCodigo->setText(producto->codigo);
        ui->inputNombre->setText(producto->nombre);
        ui->comboCategoria->setCurrentText(producto->categoria);
        ui->comboProveedor->setCurrentText(producto->proveedor);
        ui->spinCantidad->setValue(producto->cantidad);
        ui->spinStockMinimo->setValue(producto->stockMinimo);
        ui->inputMedidaBolsa->setText(producto->medidaBolsa);
        ui->textNotas->setPlainText(producto->notas);
        for (const Precio& p : producto->precios) agregarFila(p.presentacion, numeroComoTexto(p.precio));
        if (!producto->vencimiento.isEmpty()) {
            ui->checkVence->setChecked(true);
            ui->dateVencimiento->setDate(QDate::fromString(producto->vencimiento, Qt::ISODate));
        }
    } else {
        ui->comboCategoria->setCurrentText(QString());
        ui->comboProveedor->setCurrentText(QString());
    }
    armarColumnas(columnas, producto);
}

DialogoProducto::~DialogoProducto() = default;

// Títulos de las columnas, campos de las que se quitaron (escondidos) y un campo por cada columna propia
void DialogoProducto::armarColumnas(const QList<Columna>& columnas, const Producto* producto)
{
    if (columnas.isEmpty()) return;

    // Clave de la columna -> la etiqueta y el campo que la muestran
    struct Fila { const char* clave; QLabel* etiqueta; QWidget* campo; };
    const Fila filas[] = {
        {CLAVE_CODIGO, ui->labelCodigo, ui->inputCodigo},
        {CLAVE_NOMBRE, ui->labelNombre, ui->inputNombre},
        {CLAVE_CATEGORIA, ui->labelCategoria, ui->comboCategoria},
        {CLAVE_PROVEEDOR, ui->labelProveedor, ui->comboProveedor},
        {CLAVE_CANTIDAD, ui->labelCantidad, ui->spinCantidad},
        {CLAVE_MEDIDA, ui->labelMedidaBolsa, ui->inputMedidaBolsa},
        {CLAVE_VENCIMIENTO, nullptr, ui->dateVencimiento},        // en esta fila la etiqueta es el tilde «vence»
        {CLAVE_STOCK_MIN, ui->labelStockMinimo, ui->spinStockMinimo},
        {CLAVE_NOTAS, ui->labelNotas, ui->textNotas},
    };
    for (const Fila& f : filas) {
        const Columna* c = nullptr;
        for (const Columna& col : columnas) if (col.clave == QLatin1String(f.clave)) c = &col;
        ui->formulario->setRowVisible(f.campo, c != nullptr);            // quitada: no se muestra
        if (!c) continue;
        if (f.etiqueta) f.etiqueta->setText(c->nombre + QLatin1Char(':'));
        else ui->checkVence->setText(c->nombre);
    }
    for (const Columna& c : columnas)
        if (c.clave == QLatin1String(CLAVE_PRECIOS)) ui->labelPrecios->setText(c.nombre + QLatin1Char(':'));

    // Una fila por cada columna propia, antes de las notas
    int filaNotas = ui->formulario->rowCount();
    QFormLayout::ItemRole rol;
    ui->formulario->getWidgetPosition(ui->textNotas, &filaNotas, &rol);
    QWidget* anterior = ui->spinStockMinimo;
    for (const Columna& c : columnas) {
        if (!c.propia()) continue;
        auto* campo = new QLineEdit(this);
        campo->setObjectName(QStringLiteral("campoPropio_") + c.uuid);
        if (c.uuid == QLatin1String(UUID_COLUMNA_GANANCIA) && c.tipo == TipoColumna::Numero) {
            campo->setPlaceholderText(tr("Ej.: 30 (lo que le gana sobre el costo)"));
            campo->setToolTip(tr("Porcentaje que se le suma al costo: con 30, lo que cuesta $100 se vende a $130.\n"
                                 "Con esto, Reportes calcula la ganancia de lo vendido."));
        }
        else if (c.tipo == TipoColumna::Numero) campo->setPlaceholderText(tr("Ej.: 12,5"));
        else if (c.tipo == TipoColumna::Fecha) campo->setPlaceholderText(tr("dd/mm/aaaa"));
        if (producto) campo->setText(textoValorColumna(c.tipo, producto->extra.value(c.uuid)));
        ui->formulario->insertRow(filaNotas++, c.nombre + QLatin1Char(':'), campo);
        setTabOrder(anterior, campo);
        anterior = campo;
        propias_.append({c, campo});
    }
    setTabOrder(anterior, ui->textNotas);
}

QLineEdit* DialogoProducto::campoPropio(const QString& nombreDeColumna) const
{
    for (const CampoPropio& p : propias_) if (p.columna.nombre == nombreDeColumna) return p.campo;
    return nullptr;
}

void DialogoProducto::agregarFila(const QString& presentacion, const QString& precio)
{
    const int fila = ui->tablaPrecios->rowCount();
    ui->tablaPrecios->insertRow(fila);
    ui->tablaPrecios->setItem(fila, 0, new QTableWidgetItem(presentacion));
    ui->tablaPrecios->setItem(fila, 1, new QTableWidgetItem(precio));
}

void DialogoProducto::filaNueva()
{
    agregarFila();
    const int fila = ui->tablaPrecios->rowCount() - 1;
    ui->tablaPrecios->setCurrentCell(fila, 0);
    ui->tablaPrecios->editItem(ui->tablaPrecios->item(fila, 0));
}

void DialogoProducto::quitarFila()
{
    if (ui->tablaPrecios->currentRow() >= 0) ui->tablaPrecios->removeRow(ui->tablaPrecios->currentRow());
}

bool DialogoProducto::leerPrecios(QList<Precio>* precios, QString* error) const
{
    precios->clear();
    for (int fila = 0; fila < ui->tablaPrecios->rowCount(); ++fila) {
        const auto* itemPres = ui->tablaPrecios->item(fila, 0);
        const auto* itemPrecio = ui->tablaPrecios->item(fila, 1);
        const QString pres = itemPres ? itemPres->text().trimmed() : QString();
        const QString texto = itemPrecio ? itemPrecio->text().trimmed() : QString();
        if (pres.isEmpty() && texto.isEmpty()) continue;
        double valor = 0;
        if (!parsearPrecio(texto, &valor)) {
            *error = tr("El precio de la fila %1 no es un número válido: «%2».").arg(fila + 1).arg(texto);
            return false;
        }
        precios->append({pres, valor});
    }
    return true;
}

bool DialogoProducto::leerPropias(QHash<QString, QString>* extra, QString* error) const
{
    extra->clear();
    for (const CampoPropio& p : propias_) {
        QString guardado;
        const QString escrito = p.campo->text();
        if (!normalizarValorColumna(p.columna.tipo, escrito, &guardado)) {
            *error = p.columna.tipo == TipoColumna::Fecha
                         ? tr("«%1» tiene que ser una fecha, por ejemplo 01/11/2026 (escribiste «%2»).").arg(p.columna.nombre, escrito.trimmed())
                         : tr("«%1» tiene que ser un número, por ejemplo 12,5 (escribiste «%2»).").arg(p.columna.nombre, escrito.trimmed());
            return false;
        }
        if (!guardado.isEmpty()) extra->insert(p.columna.uuid, guardado);
    }
    return true;
}

void DialogoProducto::accept()
{
    if (ui->inputNombre->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Falta el nombre"), tr("El producto necesita un nombre."));
        return;
    }
    QList<Precio> precios;
    QString error;
    if (!leerPrecios(&precios, &error)) {
        QMessageBox::warning(this, tr("Precio inválido"), error);
        return;
    }
    QHash<QString, QString> extra;
    if (!leerPropias(&extra, &error)) {
        QMessageBox::warning(this, tr("Dato inválido"), error);
        return;
    }
    QDialog::accept();
}

Producto DialogoProducto::resultado() const
{
    Producto p;
    p.id = idProducto_;
    p.codigo = ui->inputCodigo->text().trimmed();
    p.nombre = ui->inputNombre->text().trimmed();
    p.categoria = ui->comboCategoria->currentText().trimmed();
    p.proveedor = ui->comboProveedor->currentText().trimmed();
    p.cantidad = ui->spinCantidad->value();
    p.stockMinimo = ui->spinStockMinimo->value();
    p.medidaBolsa = ui->inputMedidaBolsa->text().trimmed();
    p.notas = ui->textNotas->toPlainText().trimmed();
    if (ui->checkVence->isChecked()) p.vencimiento = ui->dateVencimiento->date().toString(Qt::ISODate);
    QString error;
    leerPrecios(&p.precios, &error);
    leerPropias(&p.extra, &error);
    return p;
}
