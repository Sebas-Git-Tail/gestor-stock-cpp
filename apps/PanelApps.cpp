#include "PanelApps.h"
#include "Piezas.h"
#include "Tema.h"

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
#include <QProgressBar>
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

namespace {

// Recuadro con borde redondeado (tema: propiedad «tarjeta»)
QFrame *nuevaTarjeta(QWidget *padre)
{
    auto *caja = new QFrame(padre);
    caja->setProperty("tarjeta", true);
    return caja;
}

QLabel *nuevoIcono(const QString &nombre, QWidget *padre)
{
    auto *icono = new QLabel(padre);
    icono->setFixedSize(40, 40);
    icono->setAlignment(Qt::AlignCenter);
    const QColor a = tema::colores().acento;
    icono->setStyleSheet(QStringLiteral("background: rgba(%1, %2, %3, 0.16); border-radius: 8px;")
                             .arg(a.red()).arg(a.green()).arg(a.blue()));
    icono->setPixmap(tema::pixmap(nombre, 22, a, padre->devicePixelRatioF()));
    return icono;
}

QProgressBar *nuevoProgreso(QWidget *padre)
{
    auto *barra = new QProgressBar(padre);
    barra->setTextVisible(false);
    barra->setMaximumHeight(8);
    barra->setMinimumWidth(120);
    barra->hide();
    return barra;
}

} // namespace

void PanelApps::armar()
{
    auto *principal = new QVBoxLayout(this);
    principal->setSpacing(10);

    // Esta app: versión, estado y botón para actualizar
    auto *cajaPropia = nuevaTarjeta(this);
    auto *filaPropia = new QHBoxLayout(cajaPropia);
    filaPropia->setContentsMargins(14, 12, 14, 12);
    filaPropia->setSpacing(12);
    filaPropia->addWidget(nuevoIcono(QStringLiteral("actualizar"), cajaPropia), 0, Qt::AlignTop);
    auto *textosPropia = new QVBoxLayout;
    textosPropia->setSpacing(4);
    propia_ = new QLabel(cajaPropia);
    propia_->setWordWrap(true);
    chipPropia_ = new QLabel(cajaPropia);
    progresoPropia_ = nuevoProgreso(cajaPropia);
    textosPropia->addWidget(propia_);
    auto *filaChip = new QHBoxLayout;
    filaChip->addWidget(chipPropia_);
    filaChip->addWidget(progresoPropia_, 1);
    filaChip->addStretch();
    textosPropia->addLayout(filaChip);
    botonPropia_ = new QPushButton(tr("Buscar actualizaciones"), cajaPropia);
    filaPropia->addLayout(textosPropia, 1);
    filaPropia->addWidget(botonPropia_, 0, Qt::AlignVCenter);
    principal->addWidget(cajaPropia);
    connect(botonPropia_, &QPushButton::clicked, this, [this] {
        if (modelo_->versionNueva().isEmpty()) {
            chipPropia_->setVisible(true);
            chipPropia_->setText(tr("Buscando una versión nueva…"));
            tema::marcarChip(chipPropia_, tema::Estado::Neutro);
            modelo_->buscarActualizacion(true);
            // Si no hay nada nuevo, al rato vuelve el texto de siempre
            QTimer::singleShot(4000, this, &PanelApps::actualizarPropia);
        } else {
            modelo_->actualizarEstaApp();
        }
    });
    actualizarPropia();

    auto *titulo = new QLabel(tr("Otras apps"), this);
    titulo->setProperty("secundario", true);
    QFont negrita = titulo->font();
    negrita.setBold(true);
    titulo->setFont(negrita);
    principal->addSpacing(4);
    principal->addWidget(titulo);

    auto *contenido = new QWidget;
    lista_ = new QVBoxLayout(contenido);
    lista_->setContentsMargins(0, 0, 0, 0);
    lista_->setSpacing(8);
    for (int f = 0; f < modelo_->rowCount(); ++f) {
        const QModelIndex i = modelo_->index(f);
        auto *caja = nuevaTarjeta(contenido);
        auto *fila = new QHBoxLayout(caja);
        fila->setContentsMargins(14, 12, 14, 12);
        fila->setSpacing(12);
        fila->addWidget(nuevoIcono(QStringLiteral("paquete"), caja), 0, Qt::AlignTop);
        auto *textos = new QVBoxLayout;
        textos->setSpacing(2);
        Fila w;
        w.nombre = new QLabel(QStringLiteral("<b>%1</b>").arg(i.data(ModeloApps::Nombre).toString().toHtmlEscaped()), caja);
        auto *descripcion = new QLabel(i.data(ModeloApps::Descripcion).toString(), caja);
        descripcion->setWordWrap(true);
        descripcion->setProperty("secundario", true);
        w.estado = new QLabel(caja);
        w.estado->setWordWrap(true);
        w.estado->setProperty("secundario", true);
        w.progreso = nuevoProgreso(caja);
        textos->addWidget(w.nombre);
        textos->addWidget(descripcion);
        textos->addSpacing(2);
        textos->addWidget(w.estado);
        textos->addWidget(w.progreso);
        w.boton = new QPushButton(caja);
        fila->addLayout(textos, 1);
        fila->addWidget(w.boton, 0, Qt::AlignVCenter);
        connect(w.boton, &QPushButton::clicked, this, [this, f] { modelo_->instalar(f); });
        lista_->addWidget(caja);
        filas_ << w;
        actualizarFila(f);
    }
    if (modelo_->rowCount() == 0)
        lista_->addWidget(new tema::EstadoVacio(QStringLiteral("paquete"), tr("No hay otras apps"),
                                                tr("Para este sistema no hay otros programas para instalar."),
                                                QString(), contenido));
    lista_->addStretch();

    auto *desplazable = new QScrollArea(this);
    desplazable->setWidgetResizable(true);
    desplazable->setFrameShape(QFrame::NoFrame);
    desplazable->setWidget(contenido);
    desplazable->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; } QScrollArea > QWidget > QWidget { background: transparent; }"));
    principal->addWidget(desplazable, 1);

    mensaje_ = new QLabel(this);
    mensaje_->setWordWrap(true);
    mensaje_->setProperty("secundario", true);
    mensaje_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    principal->addWidget(mensaje_);
}

void PanelApps::actualizarFila(int fila)
{
    if (fila < 0 || fila >= filas_.size())
        return;
    const QModelIndex i = modelo_->index(fila);
    const Fila &w = filas_.at(fila);
    const bool ocupado = i.data(ModeloApps::Ocupado).toBool();
    const double progreso = i.data(ModeloApps::Progreso).toDouble();
    w.estado->setText(i.data(ModeloApps::Estado).toString());
    w.progreso->setVisible(ocupado);
    if (ocupado) {
        w.progreso->setRange(0, progreso >= 0 ? 100 : 0);    // 0..0 = sin saber cuánto falta
        if (progreso >= 0)
            w.progreso->setValue(qRound(progreso * 100));
    }
    const QString accion = i.data(ModeloApps::Accion).toString();
    w.boton->setText(accion.isEmpty() ? tr("Al día") : accion);
    w.boton->setEnabled(!accion.isEmpty());
    w.boton->setVisible(!ocupado);
    tema::marcarPrimario(w.boton, !accion.isEmpty());
}

void PanelApps::actualizarPropia()
{
    const QString nombre = QCoreApplication::applicationName();
    propia_->setText(tr("<b>%1</b> %2").arg(nombre.toHtmlEscaped(), modelo_->versionPropia()));
    if (modelo_->actualizando()) {
        const double p = modelo_->progresoPropio();
        chipPropia_->setVisible(true);
        chipPropia_->setText(tr("Actualizando…"));
        tema::marcarChip(chipPropia_, tema::Estado::Acento);
        progresoPropia_->show();
        progresoPropia_->setRange(0, p >= 0 ? 100 : 0);
        if (p >= 0)
            progresoPropia_->setValue(qRound(p * 100));
        botonPropia_->setEnabled(false);
        return;
    }
    progresoPropia_->hide();
    botonPropia_->setEnabled(true);
    if (modelo_->versionNueva().isEmpty()) {
        chipPropia_->setText(tr("Al día"));
        tema::marcarChip(chipPropia_, tema::Estado::Ok);
        chipPropia_->setVisible(comprobado_);
        botonPropia_->setText(tr("Buscar actualizaciones"));
        tema::marcarPrimario(botonPropia_, false);
    } else {
        chipPropia_->setVisible(true);
        chipPropia_->setText(tr("Versión nueva: %1").arg(modelo_->versionNueva()));
        tema::marcarChip(chipPropia_, tema::Estado::Aviso);
        botonPropia_->setText(tr("Actualizar"));
        tema::marcarPrimario(botonPropia_, true);
    }
}

void PanelApps::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    if (!buscado_) {
        buscado_ = true;
        modelo_->refrescar();
        modelo_->buscarActualizacion(true);
        QTimer::singleShot(5000, this, [this] { comprobado_ = true; actualizarPropia(); });
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
        auto *volver = new QPushButton(tema::icono(QStringLiteral("volver")), tr("Volver"), this);
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
    QObject::connect(act, &Actualizador::hayVersionNueva, ventana, [act, v, estaApp] {
        if (v && act->tocaAvisar())
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
        if (!v || !act->tocaAvisar())     // se pregunta como mucho una vez por día
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
