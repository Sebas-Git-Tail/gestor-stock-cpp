#include "Piezas.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QTimer>

namespace tema {

// ─── TarjetaAccion ──────────────────────────────────────────────────────────

TarjetaAccion::TarjetaAccion(const QString &icono, const QString &titulo, const QString &descripcion,
                             QWidget *parent)
    : QFrame(parent), nombreIcono(icono)
{
    setProperty("tarjetaAccion", true);
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    etiquetaIcono = new QLabel(this);
    etiquetaIcono->setFixedSize(40, 40);
    etiquetaIcono->setAlignment(Qt::AlignCenter);
    const QColor a = colores().acento;
    etiquetaIcono->setStyleSheet(QStringLiteral("background: rgba(%1, %2, %3, 0.16); border-radius: 8px;")
                                     .arg(a.red()).arg(a.green()).arg(a.blue()));

    etiquetaTitulo = new QLabel(titulo, this);
    QFont negrita = etiquetaTitulo->font();
    negrita.setBold(true);
    etiquetaTitulo->setFont(negrita);

    etiquetaDescripcion = new QLabel(descripcion, this);
    etiquetaDescripcion->setProperty("secundario", true);
    etiquetaDescripcion->setWordWrap(true);
    etiquetaDescripcion->setVisible(!descripcion.isEmpty());

    auto *textos = new QVBoxLayout;
    textos->setSpacing(2);
    textos->addWidget(etiquetaTitulo);
    textos->addWidget(etiquetaDescripcion);
    textos->addStretch();

    auto *fila = new QHBoxLayout(this);
    fila->setContentsMargins(14, 12, 14, 12);
    fila->setSpacing(12);
    fila->addWidget(etiquetaIcono, 0, Qt::AlignTop);
    fila->addLayout(textos, 1);

    setAccessibleName(titulo);
    setAccessibleDescription(descripcion);
    pintarIcono();
}

void TarjetaAccion::setIcono(const QString &icono)
{
    nombreIcono = icono;
    pintarIcono();
}

void TarjetaAccion::setTitulo(const QString &titulo)
{
    etiquetaTitulo->setText(titulo);
    setAccessibleName(titulo);
}

void TarjetaAccion::setDescripcion(const QString &descripcion)
{
    etiquetaDescripcion->setText(descripcion);
    etiquetaDescripcion->setVisible(!descripcion.isEmpty());
    setAccessibleDescription(descripcion);
}

QString TarjetaAccion::titulo() const
{
    return etiquetaTitulo->text();
}

void TarjetaAccion::pintarIcono()
{
    const QColor c = isEnabled() ? colores().acento : colores().texto3;
    etiquetaIcono->setPixmap(pixmap(nombreIcono, 22, c, devicePixelRatioF()));
}

void TarjetaAccion::apretar(bool apretada)
{
    setProperty("apretada", apretada);
    repulir(this);
}

void TarjetaAccion::mousePressEvent(QMouseEvent *evento)
{
    if (evento->button() == Qt::LeftButton)
        apretar(true);
    QFrame::mousePressEvent(evento);
}

void TarjetaAccion::mouseReleaseEvent(QMouseEvent *evento)
{
    const bool estabaApretada = property("apretada").toBool();
    apretar(false);
    if (evento->button() == Qt::LeftButton && estabaApretada && rect().contains(evento->position().toPoint()))
        emit clicked();
    QFrame::mouseReleaseEvent(evento);
}

void TarjetaAccion::keyPressEvent(QKeyEvent *evento)
{
    if (evento->key() == Qt::Key_Return || evento->key() == Qt::Key_Enter || evento->key() == Qt::Key_Space) {
        emit clicked();
        return;
    }
    QFrame::keyPressEvent(evento);
}

void TarjetaAccion::changeEvent(QEvent *evento)
{
    if (evento->type() == QEvent::EnabledChange) {
        setCursor(isEnabled() ? Qt::PointingHandCursor : Qt::ArrowCursor);
        pintarIcono();
    } else if (evento->type() == QEvent::PaletteChange) {
        pintarIcono();     // cambió el modo claro/oscuro
    }
    QFrame::changeEvent(evento);
}

// ─── EstadoVacio ────────────────────────────────────────────────────────────

EstadoVacio::EstadoVacio(const QString &icono, const QString &titulo, const QString &texto,
                         const QString &textoBoton, QWidget *parent)
    : QWidget(parent)
{
    auto *dibujo = new QLabel(this);
    dibujo->setPixmap(pixmap(icono, 56, colores().texto3, devicePixelRatioF()));
    alCambiarModo(dibujo, [dibujo, icono] { dibujo->setPixmap(pixmap(icono, 56, colores().texto3, dibujo->devicePixelRatioF())); });
    dibujo->setAlignment(Qt::AlignCenter);

    etiquetaTitulo = new QLabel(titulo, this);
    QFont grande = etiquetaTitulo->font();
    grande.setPointSizeF(grande.pointSizeF() * 1.25);
    grande.setBold(true);
    etiquetaTitulo->setFont(grande);
    etiquetaTitulo->setAlignment(Qt::AlignCenter);

    etiquetaTexto = new QLabel(texto, this);
    etiquetaTexto->setProperty("secundario", true);
    etiquetaTexto->setWordWrap(true);
    etiquetaTexto->setAlignment(Qt::AlignCenter);
    // Ancho fijo: un QLabel con ajuste de línea centrado en el layout no calcula bien su alto
    etiquetaTexto->setFixedWidth(420);
    etiquetaTexto->setMinimumHeight(etiquetaTexto->heightForWidth(420));

    boton = new QPushButton(textoBoton, this);
    marcarPrimario(boton);
    boton->setVisible(!textoBoton.isEmpty());
    connect(boton, &QPushButton::clicked, this, &EstadoVacio::accion);

    auto *columna = new QVBoxLayout(this);
    columna->addStretch();
    columna->addWidget(dibujo, 0, Qt::AlignHCenter);
    columna->addSpacing(8);
    columna->addWidget(etiquetaTitulo, 0, Qt::AlignHCenter);
    columna->addWidget(etiquetaTexto, 0, Qt::AlignHCenter);
    columna->addSpacing(12);
    columna->addWidget(boton, 0, Qt::AlignHCenter);
    columna->addStretch();
}

void EstadoVacio::setTextos(const QString &titulo, const QString &texto)
{
    etiquetaTitulo->setText(titulo);
    etiquetaTexto->setText(texto);
    etiquetaTexto->setMinimumHeight(etiquetaTexto->heightForWidth(420));
}

void EstadoVacio::setTextoBoton(const QString &textoBoton)
{
    boton->setText(textoBoton);
    boton->setVisible(!textoBoton.isEmpty());
}

// ─── Aviso flotante ─────────────────────────────────────────────────────────

namespace {

const char *const kNombreAviso = "avisoFlotante";

// Vive dentro del recuadro del aviso: lo ubica cuando cambia el tamaño de la ventana y
// llama a lo que corresponda al cerrarse
class ControlAviso : public QObject
{
public:
    ControlAviso(QFrame *marco, QWidget *ventana, OpcionesAviso opciones)
        : QObject(marco), marco(marco), ventana(ventana), opciones(std::move(opciones))
    {
        ventana->installEventFilter(this);
    }

    void resolver(bool porBoton)
    {
        if (resuelto)
            return;
        resuelto = true;
        if (ventana)
            ventana->removeEventFilter(this);
        const std::function<void()> funcion = porBoton ? opciones.alBoton : opciones.alVencer;
        marco->setObjectName(QString());    // que avisoAbierto() no lo encuentre mientras se borra
        marco->hide();
        marco->deleteLater();
        if (funcion)
            funcion();
    }

    void ubicar()
    {
        if (!ventana)
            return;
        const int ancho = qBound(260, marco->sizeHint().width(), qMin(620, ventana->width() - 32));
        const int alto = marco->hasHeightForWidth() ? marco->heightForWidth(ancho) : marco->sizeHint().height();
        marco->setGeometry((ventana->width() - ancho) / 2, ventana->height() - alto - 20, ancho, alto);
        marco->raise();
    }

protected:
    bool eventFilter(QObject *objeto, QEvent *evento) override
    {
        if (objeto == ventana && evento->type() == QEvent::Resize)
            ubicar();
        return false;
    }

private:
    QFrame *marco;
    QPointer<QWidget> ventana;
    OpcionesAviso opciones;
    bool resuelto = false;
};

QWidget *ventanaDe(QWidget *widget)
{
    return widget ? widget->window() : nullptr;
}

ControlAviso *avisoAbierto(QWidget *ventana)
{
    auto *marco = ventana->findChild<QFrame *>(QLatin1String(kNombreAviso), Qt::FindDirectChildrenOnly);
    if (!marco)
        return nullptr;
    for (QObject *hijo : marco->children()) {
        if (auto *control = dynamic_cast<ControlAviso *>(hijo))
            return control;
    }
    return nullptr;
}

} // namespace

void avisar(QWidget *ventana, const QString &texto, Estado estado)
{
    OpcionesAviso opciones;
    opciones.estado = estado;
    avisar(ventana, texto, opciones);
}

void avisar(QWidget *widget, const QString &texto, const OpcionesAviso &opciones)
{
    QWidget *ventana = ventanaDe(widget);
    if (!ventana)
        return;
    cerrarAvisos(ventana);

    auto *marco = new QFrame(ventana);
    marco->setObjectName(QLatin1String(kNombreAviso));

    auto *punto = new QLabel(marco);
    const QString nombreIcono = opciones.estado == Estado::Ok      ? QStringLiteral("listo")
                                : opciones.estado == Estado::Error ? QStringLiteral("error")
                                : opciones.estado == Estado::Aviso ? QStringLiteral("aviso")
                                                                   : QStringLiteral("info");
    const QColor colorIcono = opciones.estado == Estado::Neutro ? colores().acento : color(opciones.estado);
    punto->setPixmap(pixmap(nombreIcono, 18, colorIcono, ventana->devicePixelRatioF()));

    auto *etiqueta = new QLabel(texto, marco);
    etiqueta->setWordWrap(true);

    auto *fila = new QHBoxLayout(marco);
    fila->setContentsMargins(14, 10, 10, 10);
    fila->setSpacing(10);
    fila->addWidget(punto, 0, Qt::AlignVCenter);
    fila->addWidget(etiqueta, 1);

    auto *control = new ControlAviso(marco, ventana, opciones);
    if (!opciones.textoBoton.isEmpty()) {
        auto *boton = new QPushButton(opciones.textoBoton, marco);
        boton->setProperty("plano", true);
        boton->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;").arg(colores().acento.name()));
        boton->setCursor(Qt::PointingHandCursor);
        QObject::connect(boton, &QPushButton::clicked, control, [control] { control->resolver(true); });
        fila->addWidget(boton, 0, Qt::AlignVCenter);
    } else {
        fila->addSpacing(4);
    }

    auto *reloj = new QTimer(control);
    reloj->setSingleShot(true);
    QObject::connect(reloj, &QTimer::timeout, control, [control] { control->resolver(false); });
    reloj->start(qMax(1500, opciones.milisegundos));

    control->ubicar();
    marco->show();
    control->ubicar();
}

void cerrarAvisos(QWidget *widget)
{
    QWidget *ventana = ventanaDe(widget);
    if (!ventana)
        return;
    if (ControlAviso *control = avisoAbierto(ventana))
        control->resolver(false);
}

// ─── recordarVentana ────────────────────────────────────────────────────────

namespace {

// Vive como hijo de la ventana: guarda su geometría al cerrarla (o al salir de la app)
class Recordador : public QObject
{
public:
    Recordador(QWidget *ventana, QSettings *ajustes, const QString &clave)
        : QObject(ventana), ventana_(ventana), ajustes_(ajustes), clave_(clave)
    {
        ventana->installEventFilter(this);
        connect(qApp, &QCoreApplication::aboutToQuit, this, &Recordador::guardar);
    }

    void guardar()
    {
        // saveGeometry() incluye el tamaño «normal» y si está maximizada o en pantalla completa
        const QByteArray geometria = ventana_->saveGeometry();
        if (ajustes_)
            ajustes_->setValue(clave_, geometria);
        else
            QSettings().setValue(clave_, geometria);
    }

protected:
    bool eventFilter(QObject *objeto, QEvent *evento) override
    {
        if (objeto == ventana_ && evento->type() == QEvent::Close)
            guardar();
        return false;
    }

private:
    QWidget *ventana_;
    QPointer<QSettings> ajustes_;
    QString clave_;
};

} // namespace

void recordarVentana(QWidget *ventana, QSettings *ajustes, const QString &clave)
{
#ifdef Q_OS_ANDROID
    Q_UNUSED(ventana)
    Q_UNUSED(ajustes)
    Q_UNUSED(clave)
#else
    const QByteArray geometria = ajustes ? ajustes->value(clave).toByteArray() : QSettings().value(clave).toByteArray();
    if (!geometria.isEmpty())
        ventana->restoreGeometry(geometria);
    new Recordador(ventana, ajustes, clave);
#endif
}

} // namespace tema
