#include "ventanaprincipal.h"
#include "ui_main.h"
#include "dialogocolumnas.h"
#include "dialogooperacion.h"
#include "dialogoproducto.h"
#include "dialogoreportes.h"
#include "dialogosincronizacion.h"
#include "importarexcel.h"
#include "portapapeles.h"
#include "sincronizacion.h"
#include "util.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QStandardItemModel>
#include <QTextStream>
#include <QTimer>

namespace {

// Ancho con el que arranca cada columna (después se recuerda el que le puso la persona)
int anchoPorDefecto(const Columna& c)
{
    const QString k = c.clave;
    if (k == QLatin1String(CLAVE_CODIGO)) return 60;
    if (k == QLatin1String(CLAVE_NOMBRE)) return 420;
    if (k == QLatin1String(CLAVE_PRECIOS)) return 210;
    if (k == QLatin1String(CLAVE_CATEGORIA)) return 150;
    if (k == QLatin1String(CLAVE_PROVEEDOR)) return 100;
    if (k == QLatin1String(CLAVE_CANTIDAD)) return 80;
    if (k == QLatin1String(CLAVE_MEDIDA)) return 110;
    if (k == QLatin1String(CLAVE_VENCIMIENTO)) return 110;
    if (k == QLatin1String(CLAVE_STOCK_MIN)) return 60;
    if (k == QLatin1String(CLAVE_NOTAS)) return 250;
    return 130;                                   // las propias
}

constexpr int ROL_ID = Qt::UserRole + 1;      // el id del producto va en cada celda de la fila

// (fondo, texto) para resaltar filas
const QColor FONDO_VENCIDO(240, 128, 128), TEXTO_VENCIDO(60, 0, 0);
const QColor FONDO_POR_VENCER(255, 241, 170), TEXTO_POR_VENCER(80, 60, 0);
const QColor FONDO_STOCK_BAJO(255, 214, 214), TEXTO_STOCK_BAJO(120, 0, 0);

QString campoCsv(const QString& s)
{
    if (!s.contains(QLatin1Char(';')) && !s.contains(QLatin1Char('"')) && !s.contains(QLatin1Char('\n'))) return s;
    QString c = s;
    c.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + c + QLatin1Char('"');
}

} // namespace

VentanaPrincipal::VentanaPrincipal(Stock* stock, QSettings* ajustes, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), stock_(stock), ajustes_(ajustes)
{
    ui->setupUi(this);
    uuidOrden_ = ajustes_->value("tabla/ordenColumna").toString();
    sentidoOrden_ = ajustes_->value("tabla/ordenSentido", int(Qt::AscendingOrder)).toInt() == int(Qt::DescendingOrder)
                        ? Qt::DescendingOrder : Qt::AscendingOrder;
    armarTabla();
    armarMenu();
    conectar();

    restoreGeometry(ajustes_->value("ventana/geometria").toByteArray());
    refrescar();
    QTimer::singleShot(0, this, &VentanaPrincipal::restaurarPosicion);   // cuando la ventana ya tiene su tamaño final
}

VentanaPrincipal::~VentanaPrincipal() { armando_ = true; }   // al cerrar, los cambios de ancho de la tabla ya no se guardan

// ====================================================================== armado

void VentanaPrincipal::armarTabla()
{
    modelo_ = new QStandardItemModel(0, 0, this);
    modelo_->setSortRole(Qt::UserRole);           // ordena por el valor real, no por el texto

    auto* tabla = ui->tablaProductos;
    tabla->setModel(modelo_);
    auto* cabecera = tabla->horizontalHeader();
    cabecera->setStretchLastSection(true);
    cabecera->setSectionsMovable(true);           // arrastrar un título cambia el orden de las columnas
    cabecera->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(cabecera, &QHeaderView::customContextMenuRequested, this, &VentanaPrincipal::menuEncabezado);
    connect(cabecera, &QHeaderView::sectionMoved, this, &VentanaPrincipal::columnaMovida);
    connect(cabecera, &QHeaderView::sectionResized, this, [this](int seccion, int, int ancho) {
        if (!armando_ && seccion >= 0 && seccion < cols_.size())
            ajustes_->setValue(QStringLiteral("tabla/ancho/") + cols_[seccion].uuid, ancho);
    });
    connect(cabecera, &QHeaderView::sortIndicatorChanged, this, [this](int seccion, Qt::SortOrder sentido) {
        if (armando_ || seccion < 0 || seccion >= cols_.size()) return;
        uuidOrden_ = cols_[seccion].uuid;
        sentidoOrden_ = sentido;
    });
    actualizarColumnas();                         // arma las columnas que dice la base
}

void VentanaPrincipal::armarMenu()
{
    // Menú hamburguesa: un QToolButton con un QMenu
    const QIcon icono = QIcon::fromTheme(QStringLiteral("open-menu-symbolic"), QIcon::fromTheme(QStringLiteral("application-menu")));
    if (!icono.isNull()) {
        ui->btnMenu->setIcon(icono);
        ui->btnMenu->setText(QString());
    }
    auto* menu = new QMenu(this);
    menu->addAction(ui->actionColumnas);
    menu->addAction(ui->actionReportes);
    menu->addAction(ui->actionSincronizar);
    menu->addSeparator();
    menu->addAction(ui->actionImportar);
    menu->addAction(ui->actionExportar);
    menu->addAction(ui->actionBackup);
    menu->addSeparator();
    menu->addAction(ui->actionAcerca);
    ui->btnMenu->setMenu(menu);
}

void VentanaPrincipal::conectar()
{
    connect(ui->inputBuscar, &QLineEdit::textChanged, this, &VentanaPrincipal::refrescar);
    connect(ui->btnAgregar, &QPushButton::clicked, this, &VentanaPrincipal::agregar);
    connect(ui->btnEditar, &QPushButton::clicked, this, &VentanaPrincipal::editar);
    connect(ui->btnEliminar, &QPushButton::clicked, this, &VentanaPrincipal::eliminar);
    connect(ui->btnVenta, &QPushButton::clicked, this, &VentanaPrincipal::venta);
    connect(ui->btnCompra, &QPushButton::clicked, this, &VentanaPrincipal::compra);
    connect(ui->tablaProductos, &QTableView::doubleClicked, this, &VentanaPrincipal::editar);
    connect(ui->tablaProductos, &QWidget::customContextMenuRequested, this, &VentanaPrincipal::menuContextual);
    connect(ui->actionImportar, &QAction::triggered, this, &VentanaPrincipal::importarExcel);
    connect(ui->actionExportar, &QAction::triggered, this, &VentanaPrincipal::exportarCsv);
    connect(ui->actionBackup, &QAction::triggered, this, &VentanaPrincipal::hacerBackup);
    connect(ui->actionAcerca, &QAction::triggered, this, &VentanaPrincipal::acercaDe);
    connect(ui->actionColumnas, &QAction::triggered, this, &VentanaPrincipal::columnas);
    connect(ui->actionSincronizar, &QAction::triggered, this, &VentanaPrincipal::abrirSincronizacion);
    connect(ui->actionReportes, &QAction::triggered, this, [this] { abrirReportes(false); });
    connect(ui->labelAvisos, &QLabel::linkActivated, this, [this] { abrirReportes(true); });
    ui->tablaProductos->installEventFilter(this);   // Ctrl+C y Ctrl+V cuando la tabla tiene el foco

    // Sincronización con el celular: si se dejó activada, arranca sola
    servidor_ = new ServidorSinc(*stock_, this);
    connect(servidor_, &ServidorSinc::sincronizado, this, [this](const QString& nombre, int recibidas, int enviadas) {
        refrescar();
        statusBar()->showMessage(tr("Sincronizado con %1: recibidos %2, enviados %3").arg(nombre).arg(recibidas).arg(enviadas), 10000);
    });
    connect(servidor_, &ServidorSinc::emparejado, this, [this](const QString& nombre) {
        statusBar()->showMessage(tr("«%1» quedó emparejado").arg(nombre), 10000);
    });
    connect(servidor_, &ServidorSinc::fallo, this, [this](const QString& mensaje) {
        statusBar()->showMessage(tr("Sincronización: %1").arg(mensaje), 10000);
    });
    if (ajustes_->value(QStringLiteral("sinc/activa"), false).toBool()) {
        QString error;
        if (!DialogoSincronizacion::activar(*servidor_, true, ajustes_, &error))
            statusBar()->showMessage(tr("No se pudo activar la sincronización: %1").arg(error), 10000);
    }
}

bool VentanaPrincipal::eventFilter(QObject* objeto, QEvent* evento)
{
    if (objeto == ui->tablaProductos && evento->type() == QEvent::KeyPress) {
        auto* tecla = static_cast<QKeyEvent*>(evento);
        if (tecla->matches(QKeySequence::Copy)) { copiar(); return true; }
        if (tecla->matches(QKeySequence::Paste)) { pegar(); return true; }
    }
    return QMainWindow::eventFilter(objeto, evento);
}

void VentanaPrincipal::menuContextual(const QPoint& pos)
{
    const QModelIndex idx = ui->tablaProductos->indexAt(pos);
    if (idx.isValid() && !ui->tablaProductos->selectionModel()->isRowSelected(idx.row(), QModelIndex()))
        seleccionar({modelo_->item(idx.row(), 0)->data(ROL_ID).toLongLong()}, modelo_->item(idx.row(), 0)->data(ROL_ID).toLongLong());
    QMenu menu(this);
    menu.addAction(tr("Venta…"), this, &VentanaPrincipal::venta);
    menu.addAction(tr("Compra…"), this, &VentanaPrincipal::compra);
    menu.addSeparator();
    menu.addAction(tr("Editar"), this, &VentanaPrincipal::editar);
    menu.addAction(tr("Eliminar"), this, &VentanaPrincipal::eliminar);
    menu.addSeparator();
    // El texto después de la tabulación se muestra a la derecha, como atajo
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-copy")), tr("Copiar renglones\tCtrl+C"), this, &VentanaPrincipal::copiar)
        ->setEnabled(!idsSeleccionados().isEmpty());
    const QMimeData* datos = QApplication::clipboard()->mimeData();
    menu.addAction(QIcon::fromTheme(QStringLiteral("edit-paste")), tr("Pegar como productos nuevos\tCtrl+V"), this, &VentanaPrincipal::pegar)
        ->setEnabled(datos && (datos->hasFormat(QLatin1String(MIME_PRODUCTOS)) || datos->hasText()));
    menu.exec(ui->tablaProductos->viewport()->mapToGlobal(pos));
}

// ====================================================================== posición y selección

QTableView* VentanaPrincipal::tabla() const { return ui->tablaProductos; }
QLineEdit* VentanaPrincipal::buscador() const { return ui->inputBuscar; }
int VentanaPrincipal::cantidadDeFilas() const { return modelo_->rowCount(); }

QList<qint64> VentanaPrincipal::idsSeleccionados() const
{
    QList<qint64> ids;
    const auto filas = ui->tablaProductos->selectionModel()->selectedRows();
    for (const QModelIndex& i : filas) ids << modelo_->item(i.row(), 0)->data(ROL_ID).toLongLong();
    return ids;
}

QList<qint64> VentanaPrincipal::idsSeleccionadosEnOrden() const
{
    QList<int> filas;
    for (const QModelIndex& i : ui->tablaProductos->selectionModel()->selectedRows()) filas << i.row();
    std::sort(filas.begin(), filas.end());
    QList<qint64> ids;
    for (int f : filas) ids << modelo_->item(f, 0)->data(ROL_ID).toLongLong();
    return ids;
}

qint64 VentanaPrincipal::idActual() const
{
    const QModelIndex i = ui->tablaProductos->currentIndex();
    return i.isValid() ? modelo_->item(i.row(), 0)->data(ROL_ID).toLongLong() : 0;
}

qint64 VentanaPrincipal::idPrimeraFilaVisible() const
{
    const int fila = ui->tablaProductos->rowAt(0);
    return fila >= 0 ? modelo_->item(fila, 0)->data(ROL_ID).toLongLong() : 0;
}

int VentanaPrincipal::filaDe(qint64 id) const
{
    if (id == 0) return -1;
    for (int f = 0; f < modelo_->rowCount(); ++f) {
        if (modelo_->item(f, 0)->data(ROL_ID).toLongLong() == id) return f;
    }
    return -1;
}

void VentanaPrincipal::seleccionar(const QList<qint64>& ids, qint64 actual)
{
    auto* sel = ui->tablaProductos->selectionModel();
    sel->clearSelection();
    for (qint64 id : ids) {
        const int f = filaDe(id);
        if (f >= 0) sel->select(modelo_->index(f, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    const int fa = filaDe(actual);
    if (fa >= 0) sel->setCurrentIndex(modelo_->index(fa, 0), QItemSelectionModel::NoUpdate);
}

void VentanaPrincipal::irAFila(int fila, QAbstractItemView::ScrollHint donde)
{
    if (fila < 0) return;
    ui->tablaProductos->doItemsLayout();          // que el rango de la barra ya esté al día
    ui->tablaProductos->scrollTo(modelo_->index(fila, 0), donde);
}

void VentanaPrincipal::guardarPosicion()
{
    ajustes_->setValue("ventana/geometria", saveGeometry());
    ajustes_->setValue("tabla/ordenColumna", uuidOrden_);
    ajustes_->setValue("tabla/ordenSentido", int(sentidoOrden_));
    // Si se cierra en medio de una búsqueda, lo que importa es el lugar de la lista completa
    const bool enBusqueda = buscando_;
    const qint64 actual = idActual() ? idActual() : (enBusqueda ? idActualAntesDeBuscar_ : 0);
    ajustes_->setValue("posicion/idActual", actual);
    ajustes_->setValue("posicion/idPrimeraFila", enBusqueda ? idPrimeraAntesDeBuscar_ : idPrimeraFilaVisible());
    ajustes_->setValue("posicion/scroll", ui->tablaProductos->verticalScrollBar()->value());
    ajustes_->sync();
}

void VentanaPrincipal::restaurarPosicion()
{
    const qint64 primera = ajustes_->value("posicion/idPrimeraFila", 0).toLongLong();
    const qint64 actual = ajustes_->value("posicion/idActual", 0).toLongLong();
    if (actual) seleccionar({actual}, actual);
    if (filaDe(primera) >= 0) irAFila(filaDe(primera), QAbstractItemView::PositionAtTop);
    else if (filaDe(actual) >= 0) irAFila(filaDe(actual), QAbstractItemView::PositionAtCenter);
    else ui->tablaProductos->verticalScrollBar()->setValue(ajustes_->value("posicion/scroll", 0).toInt());
}

void VentanaPrincipal::closeEvent(QCloseEvent* evento)
{
    guardarPosicion();
    evento->accept();
}

// ====================================================================== tabla

void VentanaPrincipal::refrescar()
{
    const QString filtro = ui->inputBuscar->text().simplified();
    const bool buscandoAhora = !filtro.isEmpty();
    auto* barra = ui->tablaProductos->verticalScrollBar();

    // 1) dónde estaba parado
    const QList<qint64> seleccionados = idsSeleccionados();
    const qint64 actual = idActual();
    const qint64 primeraPrevia = idPrimeraFilaVisible();
    const int scrollPrevio = barra->value();
    const bool empezoBusqueda = buscandoAhora && !buscando_;
    const bool terminoBusqueda = !buscandoAhora && buscando_;
    if (empezoBusqueda) {                       // recordar el lugar de la lista completa para volver a él
        idPrimeraAntesDeBuscar_ = primeraPrevia;
        idsAntesDeBuscar_ = seleccionados;
        idActualAntesDeBuscar_ = actual;
    }

    // 2) volver a llenar la tabla
    actualizarColumnas();                         // por si cambió la estructura (se agregó, ocultó o movió una columna)
    modelo_->removeRows(0, modelo_->rowCount());
    const QList<Producto> productos = stock_->listar(filtro);
    const bool conVence = stock_->columnaActiva(CLAVE_VENCIMIENTO);     // sin esas columnas no hay avisos de vencimiento ni de stock bajo
    const bool conMinimo = stock_->columnaActiva(CLAVE_STOCK_MIN);
    const int colNombre = columnaDe(CLAVE_NOMBRE), colVence = columnaDe(CLAVE_VENCIMIENTO);
    int bajos = 0, vencidos = 0, porVencer = 0;
    for (const Producto& p : productos) {
        const auto dias = conVence ? diasParaVencer(p.vencimiento) : std::nullopt;
        const EstadoVencimiento estado = conVence ? estadoVencimiento(p.vencimiento) : EstadoVencimiento::Ninguno;

        QList<QStandardItem*> fila;
        for (const Columna& c : cols_) {
            QString textoCelda;
            QVariant orden;
            valorDeCelda(p, c, &textoCelda, &orden);
            auto* item = new QStandardItem(textoCelda);
            item->setEditable(false);
            item->setData(orden, Qt::UserRole);
            item->setData(p.id, ROL_ID);
            if (c.tipo == TipoColumna::Entero || c.tipo == TipoColumna::Numero) item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            fila << item;
        }
        if (colNombre >= 0 && !p.notas.isEmpty()) fila[colNombre]->setToolTip(p.notas);

        const bool bajo = conMinimo && p.stockMinimo > 0 && p.cantidad <= p.stockMinimo;
        const QColor *fondo = nullptr, *colorTexto = nullptr;
        if (estado == EstadoVencimiento::Vencido) {
            ++vencidos;
            fondo = &FONDO_VENCIDO; colorTexto = &TEXTO_VENCIDO;
            if (colVence >= 0) fila[colVence]->setToolTip(*dias == 0 ? tr("Vence hoy") : tr("Vencido hace %1 días").arg(-*dias));
        } else if (estado == EstadoVencimiento::PorVencer) {
            ++porVencer;
            fondo = &FONDO_POR_VENCER; colorTexto = &TEXTO_POR_VENCER;
            if (colVence >= 0) fila[colVence]->setToolTip(*dias == 0 ? tr("Vence hoy") : tr("Vence en %1 días").arg(*dias));
        } else if (bajo) {
            fondo = &FONDO_STOCK_BAJO; colorTexto = &TEXTO_STOCK_BAJO;
        }
        if (bajo) ++bajos;
        if (fondo) {
            for (auto* item : fila) { item->setBackground(*fondo); item->setForeground(*colorTexto); }
        }
        modelo_->appendRow(fila);
    }
    auto* cabecera = ui->tablaProductos->horizontalHeader();
    modelo_->sort(cabecera->sortIndicatorSection(), cabecera->sortIndicatorOrder());
    QStringList resumen{tr("%1 productos").arg(productos.size())};
    if (conMinimo) resumen << tr("%1 con stock bajo").arg(bajos);
    if (conVence) resumen << tr("%1 vencidos").arg(vencidos) << tr("%1 por vencer").arg(porVencer);
    statusBar()->showMessage(resumen.join(QStringLiteral(" · ")));

    // 3) volver a donde estaba
    if (terminoBusqueda) {
        // Terminó la búsqueda: si eligió algo entre los resultados, se queda ahí; si no, vuelve a donde estaba antes
        if (filaDe(actual) >= 0) {
            seleccionar(seleccionados, actual);
            irAFila(filaDe(actual), QAbstractItemView::PositionAtCenter);
        } else {
            seleccionar(idsAntesDeBuscar_, idActualAntesDeBuscar_);
            if (filaDe(idPrimeraAntesDeBuscar_) >= 0) irAFila(filaDe(idPrimeraAntesDeBuscar_), QAbstractItemView::PositionAtTop);
            else if (filaDe(idActualAntesDeBuscar_) >= 0) irAFila(filaDe(idActualAntesDeBuscar_), QAbstractItemView::PositionAtCenter);
            else barra->setValue(0);
        }
    } else if (empezoBusqueda) {
        seleccionar(seleccionados, actual);
        barra->setValue(0);                       // los resultados se miran desde arriba
    } else if (filtro == filtroAnterior_) {
        // Un refresco común (después de editar, vender...): la lista queda exactamente donde estaba
        seleccionar(seleccionados, actual);
        if (filaDe(primeraPrevia) >= 0) irAFila(filaDe(primeraPrevia), QAbstractItemView::PositionAtTop);
        else barra->setValue(scrollPrevio);
    } else {
        // Se está escribiendo la búsqueda: si lo elegido sigue en los resultados, se lo muestra
        seleccionar(seleccionados, actual);
        if (filaDe(actual) >= 0) irAFila(filaDe(actual), QAbstractItemView::PositionAtCenter);
        else barra->setValue(0);
    }
    buscando_ = buscandoAhora;
    filtroAnterior_ = filtro;
    actualizarAvisos();
}

// ====================================================================== acciones

void VentanaPrincipal::agregar()
{
    DialogoProducto dlg(stock_->categorias(), stock_->proveedores(), nullptr, this, stock_->columnas());
    if (dlg.exec() != QDialog::Accepted) return;
    const Resultado r = stock_->crear(dlg.resultado());          // la cantidad es el stock inicial
    if (!r.ok) {
        QMessageBox::warning(this, tr("No se pudo guardar"), r.error);
        return;
    }
    refrescar();
    seleccionar({r.id}, r.id);                    // que quede marcado el producto nuevo
    irAFila(filaDe(r.id), QAbstractItemView::EnsureVisible);
}

void VentanaPrincipal::editar()
{
    const qint64 id = idActual();
    if (id == 0) {
        QMessageBox::information(this, tr("Elegí un producto"), tr("Seleccioná una fila de la tabla primero."));
        return;
    }
    const auto producto = stock_->obtener(id);
    if (!producto) return;
    DialogoProducto dlg(stock_->categorias(), stock_->proveedores(), &*producto, this, stock_->columnas());
    if (dlg.exec() != QDialog::Accepted) return;
    const Producto nuevo = dlg.resultado();
    Resultado r = stock_->actualizar(nuevo);
    if (r.ok && nuevo.cantidad != producto->cantidad)
        r = stock_->fijarCantidad(id, nuevo.cantidad);   // solo si la persona la cambió: así no pisa ventas hechas mientras tanto
    if (!r.ok) {
        QMessageBox::warning(this, tr("No se pudo guardar"), r.error);
        refrescar();
        return;
    }
    refrescar();
}

void VentanaPrincipal::eliminar()
{
    QList<qint64> ids = idsSeleccionados();
    if (ids.isEmpty() && idActual()) ids << idActual();
    if (ids.isEmpty()) {
        QMessageBox::information(this, tr("Elegí un producto"), tr("Seleccioná una fila de la tabla primero."));
        return;
    }
    QString pregunta;
    if (ids.size() == 1) {
        const auto p = stock_->obtener(ids.first());
        pregunta = tr("¿Eliminar «%1»? No se puede deshacer.").arg(p ? p->nombre : QString());
    } else {
        pregunta = tr("¿Eliminar los %1 productos seleccionados? No se puede deshacer.").arg(ids.size());
    }
    if (QMessageBox::question(this, tr("Eliminar"), pregunta) != QMessageBox::Yes) return;
    for (qint64 id : ids) stock_->eliminar(id);
    refrescar();
}

void VentanaPrincipal::venta() { operacion(TipoOperacion::Venta); }
void VentanaPrincipal::compra() { operacion(TipoOperacion::Compra); }

void VentanaPrincipal::operacion(TipoOperacion tipo)
{
    QList<qint64> ids = idsSeleccionados();
    if (ids.isEmpty() && idActual()) ids << idActual();

    DialogoOperacion dlg(tipo, stock_->listar(), ids, this);    // dentro se pueden agregar más productos con el buscador
    if (dlg.exec() != QDialog::Accepted) return;

    const ResultadoOperacion r = stock_->registrarOperacion(tipo, dlg.lineas());
    if (!r.ok) {
        QMessageBox::warning(this, tipo == TipoOperacion::Venta ? tr("No se pudo registrar la venta") : tr("No se pudo registrar la compra"),
                             r.error + QStringLiteral("\n\n") + tr("No se modificó el stock de ningún producto."));
        return;
    }
    refrescar();
    if (tipo == TipoOperacion::Venta) {
        statusBar()->showMessage(tr("Venta registrada: %1").arg(formatearPrecio(r.total)), 8000);
        QMessageBox::information(this, tr("Venta registrada"), tr("Total de la venta: %1").arg(formatearPrecio(r.total)));
    } else {
        statusBar()->showMessage(tr("Compra registrada: el stock ya está actualizado."), 8000);
    }
}

void VentanaPrincipal::importarExcel()
{
    const QString ruta = QFileDialog::getOpenFileName(this, tr("Importar desde Excel"), QString(), tr("Planillas de Excel (*.xlsx *.xlsm)"));
    if (ruta.isEmpty()) return;
    const LecturaPlanilla lectura = leerPlanilla(ruta);
    if (!lectura.ok) {
        QMessageBox::critical(this, tr("No se pudo leer el archivo"), lectura.error);
        return;
    }
    if (lectura.productos.isEmpty()) {
        QMessageBox::information(this, tr("Importar"), tr("No encontré productos en la planilla."));
        return;
    }
    const auto [nuevos, existentes] = stock_->previsualizarImportacion(lectura.productos);
    int sinPrecio = 0;
    for (const ProductoImportado& p : lectura.productos) if (p.precios.isEmpty()) ++sinPrecio;
    QStringList detalle = lectura.avisos;
    if (sinPrecio) detalle << tr("%1 productos no tienen ningún precio en la planilla.").arg(sinPrecio);

    QMessageBox caja(this);
    caja.setIcon(QMessageBox::Question);
    caja.setWindowTitle(tr("Importar desde Excel"));
    caja.setText(tr("Se encontraron %1 productos:\n\n  • %2 nuevos (se agregan con cantidad 0)\n"
                    "  • %3 que ya existen (se actualizan precios, proveedor, vencimiento y medida; la cantidad no se toca)\n\n¿Importar?")
                     .arg(lectura.productos.size()).arg(nuevos).arg(existentes));
    if (!detalle.isEmpty()) {
        caja.setInformativeText(tr("Hay %1 avisos: tocá «Mostrar detalles» para verlos.").arg(detalle.size()));
        caja.setDetailedText(detalle.join(QLatin1Char('\n')));
    }
    caja.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    caja.setDefaultButton(QMessageBox::No);
    if (caja.exec() != QMessageBox::Yes) return;

    const QString copia = QFileInfo(stock_->ruta()).absolutePath() + QStringLiteral("/backups/antes_de_importar_%1.db")
                              .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    if (stock_->ruta() != QLatin1String(":memory:")) stock_->backup(copia);    // copia automática, por las dudas
    const auto [agregados, actualizados] = stock_->importar(lectura.productos);
    refrescar();
    QMessageBox::information(this, tr("Importación terminada"),
                             tr("Se agregaron %1 productos y se actualizaron %2.").arg(agregados).arg(actualizados));
}

void VentanaPrincipal::exportarCsv()
{
    const QString ruta = QFileDialog::getSaveFileName(this, tr("Exportar stock"), QStringLiteral("stock.csv"), tr("CSV (*.csv)"));
    if (ruta.isEmpty()) return;
    QFile archivo(ruta);
    if (!archivo.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("No se pudo exportar"), archivo.errorString());
        return;
    }
    QTextStream out(&archivo);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);           // para que Excel respete las tildes
    const QList<Columna> propias = columnasPropias();
    QStringList titulos = {QStringLiteral("Código"), QStringLiteral("Nombre"), QStringLiteral("Categoría"), QStringLiteral("Proveedor"),
                           QStringLiteral("Cantidad"), QStringLiteral("Precios"), QStringLiteral("Medida bolsa"), QStringLiteral("Vencimiento"),
                           QStringLiteral("Stock mínimo"), QStringLiteral("Notas")};
    for (const Columna& c : propias) titulos << c.nombre;
    QStringList titulosCsv;
    for (const QString& tt : titulos) titulosCsv << campoCsv(tt);
    out << titulosCsv.join(QLatin1Char(';')) << "\n";
    for (const Producto& p : stock_->listar()) {
        QStringList precios;
        for (const Precio& pr : p.precios)
            precios << (pr.presentacion.isEmpty() ? QString::number(pr.precio, 'g', 15)
                                                  : pr.presentacion + QStringLiteral(": ") + QString::number(pr.precio, 'g', 15));
        QStringList campos = {p.codigo, p.nombre, p.categoria, p.proveedor, QString::number(p.cantidad),
                              precios.join(QStringLiteral(" | ")), p.medidaBolsa, p.vencimiento,
                              QString::number(p.stockMinimo), p.notas};
        for (const Columna& c : propias) campos << textoValorColumna(c.tipo, p.extra.value(c.uuid));
        QStringList salida;
        for (const QString& c : campos) salida << campoCsv(c);
        out << salida.join(QLatin1Char(';')) << "\n";
    }
    statusBar()->showMessage(tr("Exportado en %1").arg(ruta), 5000);
}

void VentanaPrincipal::hacerBackup()
{
    const QString destino = QFileInfo(stock_->ruta()).absolutePath() + QStringLiteral("/backups/stock_%1.db")
                                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const Resultado r = stock_->backup(destino);
    if (!r.ok) QMessageBox::warning(this, tr("Copia de seguridad"), r.error);
    else QMessageBox::information(this, tr("Copia de seguridad"), tr("Copia guardada en:\n%1").arg(destino));
}

void VentanaPrincipal::acercaDe()
{
    QMessageBox::about(this, tr("Acerca de"),
        tr("<h3>Gestor de Stock %1</h3><p>Hecho con Qt6 y SQLite.</p><p>Los datos se guardan en:<br><code>%2</code></p>")
            .arg(QStringLiteral(GS_VERSION), stock_->ruta().toHtmlEscaped()));
}

// ====================================================================== copiar y pegar renglones

void VentanaPrincipal::copiar()
{
    QList<qint64> ids = idsSeleccionadosEnOrden();
    if (ids.isEmpty() && idActual()) ids << idActual();
    if (ids.isEmpty()) {
        statusBar()->showMessage(tr("Seleccioná uno o más renglones para copiar."), 5000);
        return;
    }
    QList<Producto> productos;
    for (qint64 id : ids) {
        if (const auto p = stock_->obtener(id)) productos << *p;
    }
    // Dos versiones: texto para Excel/WhatsApp/etc. y una completa para pegar en este mismo programa
    auto* datos = new QMimeData;
    const QList<Columna> propias = columnasPropias();
    datos->setText(productosATexto(productos, propias));
    datos->setData(QLatin1String(MIME_PRODUCTOS), productosAJson(productos, propias));
    QApplication::clipboard()->setMimeData(datos);
    statusBar()->showMessage(productos.size() == 1 ? tr("Se copió 1 renglón.")
                                                   : tr("Se copiaron %1 renglones.").arg(productos.size()), 5000);
}

void VentanaPrincipal::pegar()
{
    const QMimeData* datos = QApplication::clipboard()->mimeData();
    QList<Producto> productos;
    QStringList avisos;
    const bool copiadosDeAca = datos && datos->hasFormat(QLatin1String(MIME_PRODUCTOS));
    if (copiadosDeAca) {
        productos = productosDesdeJson(datos->data(QLatin1String(MIME_PRODUCTOS)), columnasPropias());
        for (Producto& p : productos) p.cantidad = 0;     // es una copia: un producto nuevo, todavía sin stock
    } else if (datos && datos->hasText()) {
        const LecturaPortapapeles lectura = productosDesdeTexto(datos->text(), columnasPropias());
        productos = lectura.productos;
        avisos = lectura.avisos;
    }
    if (productos.isEmpty()) {
        QMessageBox::information(this, tr("Pegar"),
                                 tr("En el portapapeles no hay renglones de productos para pegar.\n\n"
                                    "Copiá renglones de esta tabla, o de una planilla que tenga la columna «Nombre»."));
        return;
    }

    QStringList nombres;
    for (int i = 0; i < productos.size() && i < 10; ++i) nombres << QStringLiteral("• ") + productos[i].nombre;
    if (productos.size() > 10) nombres << tr("… y %1 más").arg(productos.size() - 10);

    QMessageBox caja(this);
    caja.setIcon(QMessageBox::Question);
    caja.setWindowTitle(tr("Pegar renglones"));
    caja.setText((productos.size() == 1 ? tr("¿Agregar este producto nuevo?")
                                        : tr("¿Agregar estos %1 productos nuevos?").arg(productos.size()))
                 + QStringLiteral("\n\n") + nombres.join(QLatin1Char('\n')));
    caja.setInformativeText(copiadosDeAca
                                ? tr("Se agregan como copias, con cantidad 0. Si un código ya existe, la copia queda sin código.")
                                : tr("Si un código ya existe, el producto nuevo queda sin código."));
    if (!avisos.isEmpty()) caja.setDetailedText(avisos.join(QLatin1Char('\n')));
    caja.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    caja.setDefaultButton(QMessageBox::Yes);
    if (caja.exec() != QMessageBox::Yes) return;

    QList<qint64> nuevos;
    QStringList errores;
    int sinCodigo = 0;
    for (Producto p : productos) {
        p.id = 0;
        Resultado r = stock_->crear(p);
        if (!r.ok && !p.codigo.isEmpty()) {           // el código ya existe (es único): se pega sin código
            p.codigo.clear();
            r = stock_->crear(p);
            if (r.ok) ++sinCodigo;
        }
        if (r.ok) nuevos << r.id;
        else errores << QStringLiteral("%1: %2").arg(p.nombre, r.error);
    }

    refrescar();
    if (!nuevos.isEmpty()) {                          // quedan marcados los productos recién pegados
        seleccionar(nuevos, nuevos.first());
        irAFila(filaDe(nuevos.first()), QAbstractItemView::EnsureVisible);
    }
    QString mensaje = nuevos.size() == 1 ? tr("Se pegó 1 producto.") : tr("Se pegaron %1 productos.").arg(nuevos.size());
    if (sinCodigo) mensaje += QLatin1Char(' ') + tr("%1 quedaron sin código porque ya existía.").arg(sinCodigo);
    statusBar()->showMessage(mensaje, 8000);
    if (!errores.isEmpty())
        QMessageBox::warning(this, tr("Pegar renglones"), tr("No se pudieron pegar:\n%1").arg(errores.join(QLatin1Char('\n'))));
}

// ====================================================================== columnas

int VentanaPrincipal::columnaDe(const char* clave) const
{
    for (int i = 0; i < cols_.size(); ++i) if (cols_[i].clave == QLatin1String(clave)) return i;
    return -1;
}

int VentanaPrincipal::columnaDeUuid(const QString& uuid) const
{
    if (uuid.isEmpty()) return -1;
    for (int i = 0; i < cols_.size(); ++i) if (cols_[i].uuid == uuid) return i;
    return -1;
}

QList<Columna> VentanaPrincipal::columnasPropias() const
{
    QList<Columna> propias;
    for (const Columna& c : stock_->columnas()) if (c.propia()) propias << c;
    return propias;
}

// Lo que se ve en cada celda y el valor con el que se ordena la columna
void VentanaPrincipal::valorDeCelda(const Producto& p, const Columna& c, QString* texto, QVariant* orden) const
{
    const QString k = c.clave;
    if (k == QLatin1String(CLAVE_CODIGO)) {
        *texto = p.codigo;
        *orden = p.codigo.toLower();
    } else if (k == QLatin1String(CLAVE_NOMBRE)) {
        *texto = p.nombre;
        *orden = p.nombre.toLower();
    } else if (k == QLatin1String(CLAVE_PRECIOS)) {
        double menorPrecio = 0;
        if (!p.precios.isEmpty()) {
            menorPrecio = p.precios.first().precio;
            for (const Precio& pr : p.precios) menorPrecio = qMin(menorPrecio, pr.precio);
        }
        *texto = textoPrecios(p.precios);
        *orden = menorPrecio;
    } else if (k == QLatin1String(CLAVE_CATEGORIA)) {
        *texto = p.categoria;
        *orden = p.categoria.toLower();
    } else if (k == QLatin1String(CLAVE_PROVEEDOR)) {
        *texto = p.proveedor;
        *orden = p.proveedor.toLower();
    } else if (k == QLatin1String(CLAVE_CANTIDAD)) {
        *texto = QString::number(p.cantidad);
        *orden = p.cantidad;
    } else if (k == QLatin1String(CLAVE_MEDIDA)) {
        *texto = p.medidaBolsa;
        *orden = p.medidaBolsa.toLower();
    } else if (k == QLatin1String(CLAVE_VENCIMIENTO)) {
        *texto = p.vencimiento.isEmpty() ? QString() : QDate::fromString(p.vencimiento, Qt::ISODate).toString(QStringLiteral("dd/MM/yyyy"));
        *orden = p.vencimiento.isEmpty() ? QStringLiteral("9999-12-31") : p.vencimiento;   // sin vencimiento queda al final
    } else if (k == QLatin1String(CLAVE_STOCK_MIN)) {
        *texto = QString::number(p.stockMinimo);
        *orden = p.stockMinimo;
    } else if (k == QLatin1String(CLAVE_NOTAS)) {
        *texto = p.notas.simplified();
        *orden = p.notas.toLower();
    } else {                                             // una columna propia
        const QString valor = p.extra.value(c.uuid);
        *texto = textoValorColumna(c.tipo, valor);
        *orden = valorOrdenColumna(c.tipo, valor);
    }
}

// Si lo que dice la base ya no es lo que muestra la tabla (otra columna, otro título, otro orden), se rearma.
// Devuelve true si hubo que rearmar.
bool VentanaPrincipal::actualizarColumnas()
{
    QVector<Columna> nuevas;
    for (const Columna& c : stock_->columnas()) if (c.visible) nuevas << c;
    bool igual = nuevas.size() == cols_.size();
    for (int i = 0; igual && i < nuevas.size(); ++i)
        igual = nuevas[i].uuid == cols_[i].uuid && nuevas[i].nombre == cols_[i].nombre && nuevas[i].tipo == cols_[i].tipo;
    if (igual) return false;
    reconstruirColumnas(nuevas);
    return true;
}

void VentanaPrincipal::reconstruirColumnas(const QVector<Columna>& nuevas)
{
    auto* tabla = ui->tablaProductos;
    auto* cabecera = tabla->horizontalHeader();
    armando_ = true;
    modelo_->removeRows(0, modelo_->rowCount());
    cols_ = nuevas;
    modelo_->setColumnCount(cols_.size());
    QStringList titulos;
    for (const Columna& c : cols_) titulos << c.nombre;
    modelo_->setHorizontalHeaderLabels(titulos);
    for (int v = 0; v < cabecera->count(); ++v) {         // los títulos vuelven a su lugar: el orden ya lo da la base
        const int logica = cabecera->logicalIndex(v);
        if (logica != v) cabecera->moveSection(cabecera->visualIndex(v), v);
    }
    for (int i = 0; i < cols_.size(); ++i)
        tabla->setColumnWidth(i, ajustes_->value(QStringLiteral("tabla/ancho/") + cols_[i].uuid, anchoPorDefecto(cols_[i])).toInt());
    // La tabla sigue ordenada por la misma columna (por cuál es, no por el número de columna)
    int seccion = columnaDeUuid(uuidOrden_);
    if (seccion < 0) seccion = columnaDe(CLAVE_NOMBRE);
    if (seccion < 0) seccion = 0;
    cabecera->setSortIndicator(seccion, sentidoOrden_);
    armando_ = false;
}

// Se arrastró un título: se guarda el nuevo orden (las columnas ocultas conservan su lugar)
void VentanaPrincipal::columnaMovida()
{
    if (armando_) return;
    auto* cabecera = ui->tablaProductos->horizontalHeader();
    QList<qint64> enOrden;
    for (int v = 0; v < cabecera->count(); ++v) {
        const int logica = cabecera->logicalIndex(v);
        if (logica >= 0 && logica < cols_.size()) enOrden << cols_[logica].id;
    }
    QList<qint64> completa;
    int k = 0;
    for (const Columna& c : stock_->columnas()) completa << (c.visible && k < enOrden.size() ? enOrden[k++] : c.id);
    stock_->ordenarColumnas(completa);
    refrescar();
}

void VentanaPrincipal::menuEncabezado(const QPoint& pos)
{
    auto* cabecera = ui->tablaProductos->horizontalHeader();
    const int seccion = cabecera->logicalIndexAt(pos);
    QMenu menu(this);
    if (seccion >= 0 && seccion < cols_.size()) {
        const Columna c = cols_[seccion];
        connect(menu.addAction(tr("Renombrar «%1»…").arg(c.nombre)), &QAction::triggered, this, [this, c] {
            if (DialogoColumnas::pedirRenombrar(*stock_, c, this)) refrescar();
        });
        QAction* ocultar = menu.addAction(tr("Ocultar esta columna"));
        ocultar->setEnabled(c.clave != QLatin1String(CLAVE_NOMBRE));            // el nombre siempre se ve
        connect(ocultar, &QAction::triggered, this, [this, c] {
            const Resultado r = stock_->mostrarColumna(c.id, false);
            if (!r.ok) QMessageBox::warning(this, tr("Ocultar columna"), r.error);
            refrescar();
        });
        connect(menu.addAction(tr("Eliminar esta columna…")), &QAction::triggered, this, [this, c] {
            if (DialogoColumnas::pedirEliminar(*stock_, c, this)) refrescar();
        });
        menu.addSeparator();
    }
    connect(menu.addAction(tr("Agregar columna…")), &QAction::triggered, this, [this] {
        if (DialogoColumnas::pedirAgregar(*stock_, this)) refrescar();
    });
    connect(menu.addAction(tr("Columnas de la tabla…")), &QAction::triggered, this, &VentanaPrincipal::columnas);
    menu.exec(cabecera->mapToGlobal(pos));
}

void VentanaPrincipal::columnas()
{
    DialogoColumnas dlg(*stock_, this);
    dlg.exec();
    if (dlg.huboCambios()) refrescar();
}

void VentanaPrincipal::abrirSincronizacion()
{
    DialogoSincronizacion dlg(*stock_, *servidor_, ajustes_, this);
    dlg.exec();
}

void VentanaPrincipal::abrirReportes(bool avisos)
{
    DialogoReportes dialogo(*stock_, avisos ? DialogoReportes::Pestana::Avisos : DialogoReportes::Pestana::Reportes, this);
    dialogo.exec();
}

void VentanaPrincipal::actualizarAvisos()
{
    const QList<Alerta> avisos = stock_->alertas();
    if (avisos.isEmpty()) {
        ui->labelAvisos->hide();
        return;
    }
    int poco = 0, vencidos = 0, porVencer = 0;
    for (const Alerta& a : avisos) {
        if (a.tipo == Alerta::Tipo::StockBajo) ++poco;
        else if (a.tipo == Alerta::Tipo::Vencido) ++vencidos;
        else ++porVencer;
    }
    QStringList partes;
    if (poco) partes << tr("%n con poco stock", nullptr, poco);
    if (vencidos) partes << tr("%n vencido(s)", nullptr, vencidos);
    if (porVencer) partes << tr("%n por vencer", nullptr, porVencer);
    ui->labelAvisos->setText(QStringLiteral("⚠ %1 · <a href=\"avisos\">Ver</a>").arg(partes.join(QStringLiteral(" · "))));
    ui->labelAvisos->show();
}
