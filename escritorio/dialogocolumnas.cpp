#include "dialogocolumnas.h"
#include "ui_dialogo_columnas.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>

namespace {

// Los tipos que se pueden elegir para una columna propia, tal como se ven
const TipoColumna TIPOS_PROPIOS[] = {TipoColumna::Texto, TipoColumna::Numero, TipoColumna::Fecha};

QStringList nombresDeTipos()
{
    QStringList l;
    for (TipoColumna t : TIPOS_PROPIOS) l << nombreTipoColumna(t);
    return l;
}

QString descripcion(const Columna& c)
{
    return c.propia() ? QObject::tr("propia, %1").arg(nombreTipoColumna(c.tipo).toLower()) : QObject::tr("del programa");
}

} // namespace

DialogoColumnas::DialogoColumnas(Stock& stock, QWidget* parent)
    : QDialog(parent), ui(std::make_unique<Ui::DialogoColumnas>()), stock_(stock)
{
    ui->setupUi(this);
    ui->botones->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));

    connect(ui->btnAgregar, &QPushButton::clicked, this, [this] {
        qint64 nueva = 0;
        if (pedirAgregar(stock_, this, &nueva)) { cambios_ = true; cargar(nueva); }
    });
    connect(ui->btnRenombrar, &QPushButton::clicked, this, [this] {
        if (const auto c = stock_.columna(seleccionada()); c && pedirRenombrar(stock_, *c, this)) { cambios_ = true; cargar(c->id); }
    });
    connect(ui->btnTipo, &QPushButton::clicked, this, &DialogoColumnas::pedirTipo);
    connect(ui->btnSubir, &QPushButton::clicked, this, [this] { mostrarError(mover(-1)); });
    connect(ui->btnBajar, &QPushButton::clicked, this, [this] { mostrarError(mover(+1)); });
    connect(ui->btnEliminar, &QPushButton::clicked, this, [this] {
        if (const auto c = stock_.columna(seleccionada()); c && pedirEliminar(stock_, *c, this)) { cambios_ = true; cargar(); }
    });
    connect(ui->btnRestaurar, &QPushButton::clicked, this, &DialogoColumnas::pedirRestaurar);
    connect(ui->listaColumnas, &QListWidget::currentRowChanged, this, &DialogoColumnas::actualizarBotones);
    connect(ui->listaColumnas, &QListWidget::itemDoubleClicked, this, [this] { ui->btnRenombrar->click(); });
    connect(ui->listaColumnas, &QListWidget::itemChanged, this, [this](QListWidgetItem* it) {
        if (cargando_) return;                                    // el tilde es «se ve en la tabla»
        const Resultado r = mostrar(it->data(Qt::UserRole).toLongLong(), it->checkState() == Qt::Checked);
        if (!r.ok) { mostrarError(r); cargar(r.id); }
    });
    cargar();
}

DialogoColumnas::~DialogoColumnas() = default;

qint64 DialogoColumnas::seleccionada() const
{
    const QListWidgetItem* it = ui->listaColumnas->currentItem();
    return it ? it->data(Qt::UserRole).toLongLong() : 0;
}

void DialogoColumnas::seleccionar(qint64 id)
{
    for (int i = 0; i < ui->listaColumnas->count(); ++i)
        if (ui->listaColumnas->item(i)->data(Qt::UserRole).toLongLong() == id) ui->listaColumnas->setCurrentRow(i);
}

void DialogoColumnas::cargar(qint64 elegir)
{
    cargando_ = true;
    const qint64 antes = elegir ? elegir : seleccionada();
    const int fila = ui->listaColumnas->currentRow();
    ui->listaColumnas->clear();
    for (const Columna& c : stock_.columnas()) {
        auto* it = new QListWidgetItem(QStringLiteral("%1   (%2)").arg(c.nombre, descripcion(c)), ui->listaColumnas);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(c.visible ? Qt::Checked : Qt::Unchecked);
        it->setData(Qt::UserRole, c.id);
    }
    if (ui->listaColumnas->count()) ui->listaColumnas->setCurrentRow(std::clamp(fila, 0, ui->listaColumnas->count() - 1));
    if (antes) seleccionar(antes);
    cargando_ = false;
    actualizarBotones();
}

void DialogoColumnas::actualizarBotones()
{
    const auto c = stock_.columna(seleccionada());
    const int fila = ui->listaColumnas->currentRow();
    ui->btnRenombrar->setEnabled(c.has_value());
    ui->btnTipo->setEnabled(c && c->propia());
    ui->btnSubir->setEnabled(c && fila > 0);
    ui->btnBajar->setEnabled(c && fila >= 0 && fila < ui->listaColumnas->count() - 1);
    ui->btnEliminar->setEnabled(c && !c->esencial());
    ui->btnRestaurar->setEnabled(!stock_.columnasQuitadas().isEmpty());
}

void DialogoColumnas::mostrarError(const Resultado& r)
{
    if (!r.ok && !r.error.isEmpty()) QMessageBox::warning(this, windowTitle(), r.error);
}

// ---------------------------------------------------------------- acciones

Resultado DialogoColumnas::agregar(const QString& nombre, TipoColumna tipo)
{
    const Resultado r = stock_.agregarColumna(nombre, tipo);
    if (r.ok) { cambios_ = true; cargar(r.id); }
    return r;
}

Resultado DialogoColumnas::renombrar(const QString& nombre)
{
    const qint64 id = seleccionada();
    if (!id) return {false, tr("Elegí una columna."), 0};
    const Resultado r = stock_.renombrarColumna(id, nombre);
    if (r.ok) { cambios_ = true; cargar(id); }
    return r;
}

Resultado DialogoColumnas::cambiarTipo(TipoColumna tipo)
{
    const qint64 id = seleccionada();
    if (!id) return {false, tr("Elegí una columna."), 0};
    const Resultado r = stock_.cambiarTipoColumna(id, tipo);
    if (r.ok) { cambios_ = true; cargar(id); }
    return r;
}

Resultado DialogoColumnas::mostrar(qint64 id, bool visible)
{
    const Resultado r = stock_.mostrarColumna(id, visible);
    if (r.ok) cambios_ = true;
    return r;
}

Resultado DialogoColumnas::mover(int delta)
{
    const qint64 id = seleccionada();
    if (!id) return {false, tr("Elegí una columna."), 0};
    const int fila = ui->listaColumnas->currentRow();
    const Resultado r = stock_.moverColumna(id, fila + delta);
    if (r.ok) { cambios_ = true; cargar(id); }
    return r;
}

Resultado DialogoColumnas::eliminar()
{
    const qint64 id = seleccionada();
    if (!id) return {false, tr("Elegí una columna."), 0};
    const Resultado r = stock_.eliminarColumna(id);
    if (r.ok) { cambios_ = true; cargar(); }
    return r;
}

Resultado DialogoColumnas::restaurar(qint64 id)
{
    const Resultado r = stock_.restaurarColumna(id);
    if (r.ok) { cambios_ = true; cargar(id); }
    return r;
}

void DialogoColumnas::pedirTipo()
{
    const auto c = stock_.columna(seleccionada());
    if (!c || !c->propia()) return;
    bool ok = false;
    const QStringList tipos = nombresDeTipos();
    const QString elegido = QInputDialog::getItem(this, tr("Cambiar tipo"),
        tr("Tipo de la columna «%1».\nLo que ya está cargado no se borra: si no encaja con el tipo nuevo, se sigue viendo tal cual.").arg(c->nombre),
        tipos, tipos.indexOf(nombreTipoColumna(c->tipo)), false, &ok);
    if (ok) mostrarError(cambiarTipo(TIPOS_PROPIOS[tipos.indexOf(elegido)]));
}

void DialogoColumnas::pedirRestaurar()
{
    const QList<Columna> quitadas = stock_.columnasQuitadas();
    if (quitadas.isEmpty()) return;
    QStringList nombres;
    for (const Columna& c : quitadas) nombres << c.nombre;
    bool ok = false;
    const QString elegido = QInputDialog::getItem(this, tr("Volver a poner una columna"),
        tr("¿Cuál querés volver a poner? Vuelve con todo lo que tenía cargado."), nombres, 0, false, &ok);
    if (ok) mostrarError(restaurar(quitadas[nombres.indexOf(elegido)].id));
}

// ---------------------------------------------------------------- las preguntas, también para el menú de los títulos

bool DialogoColumnas::pedirAgregar(Stock& stock, QWidget* padre, qint64* nueva)
{
    bool ok = false;
    const QString nombre = QInputDialog::getText(padre, tr("Agregar columna"), tr("Nombre de la columna nueva:"),
                                                 QLineEdit::Normal, QString(), &ok).simplified();
    if (!ok || nombre.isEmpty()) return false;
    const QStringList tipos = nombresDeTipos();
    const QString elegido = QInputDialog::getItem(padre, tr("Agregar columna"),
        tr("¿Qué se va a cargar en «%1»?").arg(nombre), tipos, 0, false, &ok);
    if (!ok) return false;
    const Resultado r = stock.agregarColumna(nombre, TIPOS_PROPIOS[tipos.indexOf(elegido)]);
    if (!r.ok) {
        QMessageBox::warning(padre, tr("Agregar columna"), r.error);
        return false;
    }
    if (nueva) *nueva = r.id;
    return true;
}

bool DialogoColumnas::pedirRenombrar(Stock& stock, const Columna& columna, QWidget* padre)
{
    bool ok = false;
    const QString nombre = QInputDialog::getText(padre, tr("Renombrar columna"), tr("Nuevo nombre:"),
                                                 QLineEdit::Normal, columna.nombre, &ok).simplified();
    if (!ok || nombre.isEmpty() || nombre == columna.nombre) return false;
    const Resultado r = stock.renombrarColumna(columna.id, nombre);
    if (!r.ok) {
        QMessageBox::warning(padre, tr("Renombrar columna"), r.error);
        return false;
    }
    return true;
}

bool DialogoColumnas::pedirEliminar(Stock& stock, const Columna& columna, QWidget* padre)
{
    if (columna.esencial()) {
        QMessageBox::information(padre, tr("Eliminar columna"),
            tr("La columna «%1» no se puede eliminar: el programa la necesita para vender, comprar y buscar.\n\nSi no querés verla, podés ocultarla.")
                .arg(columna.nombre));
        return false;
    }
    const QString pregunta = columna.propia()
        ? tr("¿Eliminar la columna «%1»?\n\nSe borra también lo que cargaste en ella en todos los productos. No se puede deshacer.").arg(columna.nombre)
        : tr("¿Quitar la columna «%1»?\n\nNo se pierde nada: lo que tenían cargado los productos queda guardado y se recupera con «Volver a poner una quitada».")
              .arg(columna.nombre);
    if (QMessageBox::question(padre, tr("Eliminar columna"), pregunta) != QMessageBox::Yes) return false;
    const Resultado r = stock.eliminarColumna(columna.id);
    if (!r.ok) {
        QMessageBox::warning(padre, tr("Eliminar columna"), r.error);
        return false;
    }
    return true;
}
