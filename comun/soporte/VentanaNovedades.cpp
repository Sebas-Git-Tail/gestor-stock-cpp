#include "VentanaNovedades.h"

#include "Fallas.h"
#include "Novedades.h"
#include "Piezas.h"
#include "Tema.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace soporte {

Novedades *novedades()
{
    static Novedades *n = new Novedades(qApp);
    return n;
}

VentanaNovedades::VentanaNovedades(bool soloNuevas, QWidget *parent) : QDialog(parent)
{
    Novedades *n = novedades();
    const QString nombre = QApplication::applicationDisplayName().isEmpty() ? QApplication::applicationName()
                                                                            : QApplication::applicationDisplayName();
    setWindowTitle(soloNuevas ? tr("Novedades") : tr("Novedades de %1").arg(nombre));
    resize(520, 460);

    auto *capa = new QVBoxLayout(this);
    capa->setContentsMargins(20, 18, 20, 16);
    capa->setSpacing(12);

    auto *titulo = new QLabel(soloNuevas ? tr("Qué hay de nuevo en %1").arg(nombre)
                                         : tr("%1 %2").arg(nombre, n->version()), this);
    titulo->setStyleSheet(QStringLiteral("font-size: 17px; font-weight: 600;"));
    capa->addWidget(titulo);

    auto *texto = new QTextBrowser(this);
    texto->setOpenExternalLinks(true);
    texto->setFrameShape(QFrame::NoFrame);
    texto->setFocusPolicy(Qt::NoFocus);
    texto->setStyleSheet(QStringLiteral("QTextBrowser { background: transparent; border: none; }"));
    const QString markdown = soloNuevas ? n->textoNuevas() : n->textoTodas();
    texto->setMarkdown(markdown.isEmpty() ? tr("Todavía no hay novedades anotadas para esta versión.") : markdown);
    capa->addWidget(texto, 1);

    if (n->desarrollador()) {
        auto *nota = new QLabel(tr("Este equipo es tuyo: también ves lo marcado «solo vos»."), this);
        nota->setProperty("secundario", true);
        nota->setWordWrap(true);
        capa->addWidget(nota);
    }

    auto *abajo = new QHBoxLayout;
    auto *informe = new QPushButton(tema::icono(QStringLiteral("falla")), tr("Enviar informe de fallas"), this);
    informe->setToolTip(tr("Si algo anda mal, manda el registro de la app (sin tus datos) para poder arreglarlo"));
    connect(informe, &QPushButton::clicked, this, [this] { enviarInformeFallas(this); });
    abajo->addWidget(informe);
    abajo->addStretch(1);
    auto *cerrar = new QPushButton(soloNuevas ? tr("Entendido") : tr("Cerrar"), this);
    tema::marcarPrimario(cerrar);
    cerrar->setDefault(true);
    connect(cerrar, &QPushButton::clicked, this, &QDialog::accept);
    abajo->addWidget(cerrar);
    capa->addLayout(abajo);

    if (soloNuevas)
        connect(this, &QDialog::finished, n, &Novedades::marcarVistas);
}

void mostrarNovedades(QWidget *ventana, bool soloNuevas)
{
    auto *v = new VentanaNovedades(soloNuevas, ventana);
    v->setAttribute(Qt::WA_DeleteOnClose);
    v->open();
}

QToolButton *botonNovedades(QWidget *ventana)
{
    auto *boton = new QToolButton(ventana);
    boton->setObjectName(QStringLiteral("botonNovedades"));
    boton->setAutoRaise(true);
    boton->setIconSize(QSize(20, 20));
    boton->setCursor(Qt::PointingHandCursor);
    boton->setIcon(tema::icono(QStringLiteral("regalo")));
    boton->setToolTip(QObject::tr("Novedades e informe de fallas"));
    boton->setAccessibleName(boton->toolTip());
    QObject::connect(boton, &QToolButton::clicked, boton, [boton] { mostrarNovedades(boton->window(), false); });
    return boton;
}

void novedadesAlAbrir(QWidget *ventana)
{
    if (!novedades()->hayNuevas())
        return;
    QPointer<QWidget> v(ventana);
    QTimer::singleShot(1200, ventana, [v] {
        if (v && novedades()->hayNuevas())
            mostrarNovedades(v, true);
    });
}

void enviarInformeFallas(QWidget *ventana)
{
    bool ok = false;
    const QString comentario = QInputDialog::getMultiLineText(ventana, QObject::tr("Enviar informe de fallas"),
        QObject::tr("Contá qué pasó (opcional). Se manda junto con el registro de la app, sin tus datos:"),
        QString(), &ok);
    if (!ok)
        return;
    Fallas &f = Fallas::instancia();
    QPointer<QWidget> v(ventana);
    auto *conexion = new QMetaObject::Connection;
    *conexion = QObject::connect(&f, &Fallas::enviadoManual, ventana, [v, conexion](bool enviado) {
        QObject::disconnect(*conexion);
        delete conexion;
        if (!v)
            return;
        if (enviado)
            tema::avisar(v, QObject::tr("Informe enviado. ¡Gracias!"), tema::Estado::Ok);
        else
            tema::avisar(v, QObject::tr("No se pudo enviar ahora: se manda solo la próxima vez que haya conexión."),
                         tema::Estado::Aviso);
    });
    f.enviarManual(comentario);
}

} // namespace soporte
