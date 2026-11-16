#include "PanelApps.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGuiApplication>
#include <QWindow>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace apps {

PanelApps::PanelApps(const QString &estaApp, QWidget *parent)
    : QWidget(parent), modelo_(new ModeloApps(estaApp, this))
{
    setWindowTitle(tr("Apps"));
    armar();
    connect(modelo_, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &a, const QModelIndex &b) {
        for (int f = a.row(); f <= b.row(); ++f)
            actualizarFila(f);
    });
    connect(modelo_, &ModeloApps::mensajeCambio, this, [this] { mensaje_->setText(modelo_->mensaje()); });
    connect(modelo_, &ModeloApps::versionNuevaCambio, this, &PanelApps::actualizarPropia);
    connect(modelo_, &ModeloApps::actualizandoCambio, this, &PanelApps::actualizarPropia);
    // Al volver a la app (por ejemplo, del instalador de Android) la lista se pone al día
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState estado) {
        if (estado == Qt::ApplicationActive && buscado_ && isVisible())
            modelo_->refrescar();
    });
}

void PanelApps::armar()
{
    auto *principal = new QVBoxLayout(this);

    // Esta app: versión y botón para actualizar
    auto *filaPropia = new QHBoxLayout;
    propia_ = new QLabel(this);
    propia_->setWordWrap(true);
    botonPropia_ = new QPushButton(tr("Buscar actualizaciones"), this);
    filaPropia->addWidget(propia_, 1);
    filaPropia->addWidget(botonPropia_);
    principal->addLayout(filaPropia);
    connect(botonPropia_, &QPushButton::clicked, this, [this] {
        if (modelo_->versionNueva().isEmpty()) {
            propia_->setText(tr("Buscando una versión nueva…"));
            modelo_->buscarActualizacion(true);
            // Si no hay nada nuevo, al rato vuelve el texto de siempre
            QTimer::singleShot(4000, this, &PanelApps::actualizarPropia);
        } else {
            modelo_->actualizarEstaApp();
        }
    });
    actualizarPropia();

    auto *linea = new QFrame(this);
    linea->setFrameShape(QFrame::HLine);
    principal->addWidget(linea);

    auto *titulo = new QLabel(tr("<b>Otras apps</b>"), this);
    principal->addWidget(titulo);

    auto *contenido = new QWidget;
    lista_ = new QVBoxLayout(contenido);
    lista_->setContentsMargins(0, 0, 0, 0);
    for (int f = 0; f < modelo_->rowCount(); ++f) {
        const QModelIndex i = modelo_->index(f);
        auto *caja = new QFrame(contenido);
        caja->setFrameShape(QFrame::StyledPanel);
        auto *fila = new QHBoxLayout(caja);
        auto *textos = new QVBoxLayout;
        Fila w;
        w.nombre = new QLabel(QStringLiteral("<b>%1</b><br>%2").arg(i.data(ModeloApps::Nombre).toString().toHtmlEscaped(),
                                                                  i.data(ModeloApps::Descripcion).toString().toHtmlEscaped()), caja);
        w.nombre->setWordWrap(true);
        w.estado = new QLabel(caja);
        w.estado->setWordWrap(true);
        w.estado->setEnabled(false);       // gris
        textos->addWidget(w.nombre);
        textos->addWidget(w.estado);
        w.boton = new QPushButton(caja);
        fila->addLayout(textos, 1);
        fila->addWidget(w.boton);
        connect(w.boton, &QPushButton::clicked, this, [this, f] { modelo_->instalar(f); });
        lista_->addWidget(caja);
        filas_ << w;
        actualizarFila(f);
    }
    if (modelo_->rowCount() == 0)
        lista_->addWidget(new QLabel(tr("No hay otras apps para este sistema."), contenido));
    lista_->addStretch();

    auto *desplazable = new QScrollArea(this);
    desplazable->setWidgetResizable(true);
    desplazable->setFrameShape(QFrame::NoFrame);
    desplazable->setWidget(contenido);
    principal->addWidget(desplazable, 1);

    mensaje_ = new QLabel(this);
    mensaje_->setWordWrap(true);
    mensaje_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    principal->addWidget(mensaje_);
}

void PanelApps::actualizarFila(int fila)
{
    if (fila < 0 || fila >= filas_.size())
        return;
    const QModelIndex i = modelo_->index(fila);
    const Fila &w = filas_.at(fila);
    QString estado = i.data(ModeloApps::Estado).toString();
    const double progreso = i.data(ModeloApps::Progreso).toDouble();
    if (i.data(ModeloApps::Ocupado).toBool() && progreso >= 0)
        estado += QStringLiteral(" %1 %").arg(qRound(progreso * 100));
    w.estado->setText(estado);
    const QString accion = i.data(ModeloApps::Accion).toString();
    w.boton->setText(accion.isEmpty() ? tr("Al día") : accion);
    w.boton->setEnabled(!accion.isEmpty());
    w.boton->setVisible(!i.data(ModeloApps::Ocupado).toBool());
}

void PanelApps::actualizarPropia()
{
    const QString nombre = QCoreApplication::applicationName();
    if (modelo_->actualizando()) {
        const double p = modelo_->progresoPropio();
        propia_->setText(p >= 0 ? tr("Actualizando… %1 %").arg(qRound(p * 100)) : tr("Actualizando…"));
        botonPropia_->setEnabled(false);
        return;
    }
    botonPropia_->setEnabled(true);
    if (modelo_->versionNueva().isEmpty()) {
        propia_->setText(tr("<b>%1</b> %2").arg(nombre.toHtmlEscaped(), modelo_->versionPropia()));
        botonPropia_->setText(tr("Buscar actualizaciones"));
    } else {
        propia_->setText(tr("<b>%1</b> %2 · hay una versión nueva: <b>%3</b>")
                             .arg(nombre.toHtmlEscaped(), modelo_->versionPropia(), modelo_->versionNueva()));
        botonPropia_->setText(tr("Actualizar"));
    }
}

void PanelApps::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (!buscado_) {
        buscado_ = true;
        modelo_->refrescar();
        modelo_->buscarActualizacion(true);
    }
}

DialogoApps::DialogoApps(const QString &estaApp, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Apps y actualizaciones"));
    setAttribute(Qt::WA_DeleteOnClose);
    auto *capa = new QVBoxLayout(this);
    capa->addWidget(new PanelApps(estaApp, this), 1);
    // Botón propio: con QDialogButtonBox::Close dice «Close» donde Qt no tiene traducciones (Windows, Android)
    auto *botones = new QDialogButtonBox(this);
    botones->addButton(tr("Cerrar"), QDialogButtonBox::RejectRole);
    connect(botones, &QDialogButtonBox::rejected, this, &QDialog::reject);
    capa->addWidget(botones);
    resize(560, 520);
}

namespace {

// Capa con la lista de apps encima de toda la ventana (Android): es un hijo de la ventana, no una
// ventana nueva. Sigue el tamaño de la ventana y se cierra con «Volver».
class CapaApps : public QWidget
{
public:
    CapaApps(QWidget *ventana, const QString &estaApp) : QWidget(ventana)
    {
        setObjectName(QStringLiteral("capaApps"));
        setAttribute(Qt::WA_DeleteOnClose);
        setAutoFillBackground(true);                      // tapa lo que hay abajo (usa la paleta de la app)
        auto *capa = new QVBoxLayout(this);
        auto *arriba = new QHBoxLayout;
        auto *volver = new QPushButton(tr("← Volver"), this);
        auto *titulo = new QLabel(tr("<b>Apps y actualizaciones</b>"), this);
        arriba->addWidget(volver);
        arriba->addWidget(titulo, 1);
        capa->addLayout(arriba);
        capa->addWidget(new PanelApps(estaApp, this), 1);
        connect(volver, &QPushButton::clicked, this, &QWidget::close);
        // La ventana se dibuja detrás de la barra de estado: se respetan sus márgenes seguros
        if (QWindow *w = ventana->window()->windowHandle()) {
            setContentsMargins(w->safeAreaMargins());
            connect(w, &QWindow::safeAreaMarginsChanged, this, [this](const QMargins &m) { setContentsMargins(m); });
        }
        ventana->installEventFilter(this);
        setGeometry(ventana->rect());
        raise();
        show();
    }

protected:
    bool eventFilter(QObject *objeto, QEvent *e) override
    {
        if (objeto == parentWidget() && e->type() == QEvent::Resize)
            setGeometry(parentWidget()->rect());
        return false;
    }
};

} // namespace

void mostrarApps(QWidget *ventana, const QString &estaApp)
{
#ifdef Q_OS_ANDROID
    if (auto *vieja = ventana->findChild<QWidget *>(QStringLiteral("capaApps"), Qt::FindDirectChildrenOnly)) {
        vieja->raise();
        return;
    }
    new CapaApps(ventana, estaApp);
#else
    (new DialogoApps(estaApp, ventana))->show();
#endif
}

void avisarActualizaciones(QWidget *ventana, const QString &estaApp)
{
#ifdef Q_OS_ANDROID
    // Sin diálogos (en Android una ventana nueva cerraba la app): si hay versión nueva se muestra la capa
    // de apps, que arriba ofrece «Actualizar».
    const InfoApp *info = buscar(estaApp);
    if (!info)
        return;
    auto *act = new Actualizador(*info, true, ventana);
    QPointer<QWidget> v(ventana);
    QObject::connect(act, &Actualizador::hayVersionNueva, ventana, [v, estaApp] {
        if (v)
            mostrarApps(v, estaApp);
    });
    act->buscarUnaVezPorDia();
#else
    const InfoApp *app = buscar(estaApp);
    if (!app)
        return;
    auto *act = new Actualizador(*app, true, ventana);
    QPointer<QWidget> v(ventana);
    QObject::connect(act, &Actualizador::hayVersionNueva, ventana, [act, v](const QString &version) {
        if (!v)
            return;
        const auto respuesta = QMessageBox::question(v, QObject::tr("Versión nueva"),
            QObject::tr("Hay una versión nueva de %1: la %2 (tenés la %3).\n\n¿La instalo ahora?")
                .arg(act->app().nombre, version, QCoreApplication::applicationVersion()));
        if (respuesta != QMessageBox::Yes)
            return;
        auto *progreso = new QProgressDialog(QObject::tr("Descargando la versión %1…").arg(version), QString(), 0, 0, v);
        progreso->setWindowTitle(QObject::tr("Actualizando"));
        progreso->setMinimumDuration(0);
        progreso->setAttribute(Qt::WA_DeleteOnClose);
        progreso->show();
        QObject::connect(act, &Actualizador::progreso, progreso, [progreso](qint64 recibido, qint64 total) {
            if (total > 0) {
                progreso->setMaximum(100);
                progreso->setValue(int(recibido * 100 / total));
            }
        });
        QObject::connect(act, &Actualizador::mensaje, progreso, &QProgressDialog::setLabelText);
        QObject::connect(act, &Actualizador::fallo, progreso, [progreso, v](const QString &texto) {
            progreso->close();
            if (v)
                QMessageBox::warning(v, QObject::tr("Actualización"), texto);
        });
        QObject::connect(act, &Actualizador::instalado, progreso, [progreso] {
            // En Android se abrió el instalador del sistema; en Windows/Linux la app se cierra sola
            QTimer::singleShot(1500, progreso, &QWidget::close);
        });
        act->descargarEInstalar();
    });
    act->buscarUnaVezPorDia();
#endif
}

} // namespace apps
