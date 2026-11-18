#include "Tema.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QSvgRenderer>

#include <cmath>

// Fuera de cualquier namespace: Q_INIT_RESOURCE declara una función global
static void iniciarRecursosIconos() { Q_INIT_RESOURCE(iconos); }   // el .qrc está en una biblioteca estática

namespace tema {

namespace {

Colores &coloresActuales()
{
    static Colores c{
        QColor(0x11, 0x14, 0x18),   // fondo
        QColor(0x17, 0x1a, 0x20),   // superficie
        QColor(0x1e, 0x22, 0x2a),   // superficie2
        QColor(0x2a, 0x30, 0x39),   // borde
        QColor(0x36, 0x3d, 0x48),   // bordeFuerte
        QColor(0xe7, 0xe9, 0xec),   // texto
        QColor(0x9a, 0xa3, 0xae),   // texto2
        QColor(0x6b, 0x74, 0x80),   // texto3
        QColor(0x00, 0xaa, 0xe4),   // acento
        QColor(0x0b, 0x0d, 0x10),   // sobreAcento
        QColor(0x3f, 0xb9, 0x7f),   // ok
        QColor(0xe0, 0xa4, 0x3a),   // aviso
        QColor(0xef, 0x64, 0x61),   // error
    };
    return c;
}

void asegurarRecursos()
{
    static const bool listo = [] { iniciarRecursosIconos(); return true; }();
    Q_UNUSED(listo)
}

// Luminancia relativa (WCAG) para decidir si el texto sobre el acento va claro u oscuro
double luminancia(const QColor &color)
{
    const auto canal = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * canal(color.redF()) + 0.7152 * canal(color.greenF()) + 0.0722 * canal(color.blueF());
}

QString rgba(const QColor &color, double alfa)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alfa);
}

QString hojaDeEstilo(const Colores &c)
{
    const QString acentoClaro = c.acento.lighter(115).name();
    QString hoja = QStringLiteral(R"(
QToolTip { background: @sup2; color: @texto; border: 1px solid @borde; padding: 4px 8px; border-radius: 6px; }

QPushButton { background: @sup2; color: @texto; border: 1px solid @bordeF; border-radius: 6px; padding: 6px 14px; min-height: 20px; }
QPushButton:hover { border-color: @acento; }
QPushButton:pressed { background: @borde; }
QPushButton:checked { background: @acentoSuave; border-color: @acento; }
QPushButton:disabled { color: @texto3; border-color: @borde; background: @sup; }
QPushButton:focus { outline: none; }
QPushButton[primario="true"] { background: @acento; border-color: @acento; color: @sobreAcento; font-weight: 600; }
QPushButton[primario="true"]:hover { background: @acentoClaro; border-color: @acentoClaro; }
QPushButton[primario="true"]:disabled { background: @borde; border-color: @borde; color: @texto3; }
QPushButton[peligro="true"] { color: @error; }
QPushButton[peligro="true"]:hover { border-color: @error; }
QPushButton[plano="true"] { background: transparent; border-color: transparent; color: @texto2; }
QPushButton[plano="true"]:hover { background: @sup2; color: @texto; }
QPushButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 6px; }

QToolButton { border: 1px solid transparent; border-radius: 6px; padding: 4px; color: @texto; }
QToolButton:hover { background: @sup2; border-color: @borde; }
QToolButton:pressed, QToolButton:checked { background: @acentoSuave; border-color: @acento; }

QLineEdit, QPlainTextEdit, QTextEdit { background: @sup; color: @texto; border: 1px solid @bordeF; border-radius: 6px; padding: 5px 8px; selection-background-color: @acento; selection-color: @sobreAcento; }
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus { border-color: @acento; }
QLineEdit:disabled { color: @texto3; border-color: @borde; }
QTextBrowser { padding: 8px; }
QCheckBox::indicator:unchecked { background: @sup; border: 1px solid @bordeF; border-radius: 3px; width: 14px; height: 14px; }
QCheckBox::indicator:unchecked:hover { border-color: @acento; }

QListView[lateral="true"] { background: @sup; border: 1px solid @borde; border-radius: 10px; padding: 6px; outline: none; }
QListView[lateral="true"]::item { padding: 6px 8px; border-radius: 6px; color: @texto2; margin: 1px 0; }
QListView[lateral="true"]::item:hover { background: @sup2; color: @texto; }
QListView[lateral="true"]::item:selected { background: @acentoSuave; color: @texto; }

QTableView, QTreeView, QListView { background: @sup; alternate-background-color: @sup2; border: 1px solid @borde; border-radius: 8px; gridline-color: @borde; selection-background-color: @acentoSel; selection-color: @texto; outline: none; }
QTableView::item, QTreeView::item { padding: 2px 4px; }
QHeaderView { background: transparent; border: none; }
QHeaderView::section { background: @sup2; color: @texto2; border: none; border-bottom: 1px solid @borde; border-right: 1px solid @borde; padding: 5px 4px; font-weight: 600; }
QHeaderView::section:last { border-right: none; }
QTableCornerButton::section { background: @sup2; border: none; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle { background: @bordeF; border-radius: 3px; }
QScrollBar::handle:vertical { min-height: 30px; }
QScrollBar::handle:horizontal { min-width: 30px; }
QScrollBar::handle:hover { background: @texto3; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QProgressBar { background: @sup2; border: 1px solid @borde; border-radius: 6px; text-align: center; color: @texto; min-height: 14px; }
QProgressBar::chunk { background: @acento; border-radius: 5px; }

QGroupBox { border: 1px solid @borde; border-radius: 10px; margin-top: 14px; padding: 14px 10px 10px 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; color: @texto2; font-weight: 600; }

QTabWidget::pane { border: 1px solid @borde; border-radius: 8px; top: -1px; }
QTabBar::tab { background: transparent; color: @texto2; padding: 8px 14px; border: none; border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: @texto; border-bottom-color: @acento; }
QTabBar::tab:hover { color: @texto; }

QMenu { background: @sup2; color: @texto; border: 1px solid @borde; border-radius: 8px; padding: 4px; }
QMenu::item { padding: 6px 22px 6px 28px; border-radius: 4px; }
QMenu::item:selected { background: @acentoSuave; }
QMenu::item:disabled { color: @texto3; }
QMenu::separator { height: 1px; background: @borde; margin: 4px 6px; }

QLabel[secundario="true"] { color: @texto2; }
QLabel[estado="neutro"] { color: @texto2; }
QLabel[estado="acento"] { color: @acento; }
QLabel[estado="ok"] { color: @ok; }
QLabel[estado="aviso"] { color: @aviso; }
QLabel[estado="error"] { color: @error; }
QLabel[titulo="true"] { font-size: 16pt; font-weight: 600; }

QFrame[tarjeta="true"] { background: @sup; border: 1px solid @borde; border-radius: 10px; }
QFrame[tarjetaAccion="true"] { background: @sup; border: 1px solid @borde; border-radius: 10px; }
QFrame[tarjetaAccion="true"]:hover { background: @sup2; border-color: @acento; }
QFrame[tarjetaAccion="true"]:focus { border-color: @acento; }
QFrame[tarjetaAccion="true"][apretada="true"] { background: @acentoSuave; }
QFrame[tarjetaAccion="true"]:disabled { border-color: @borde; background: @fondo; }
QFrame[tarjetaAccion="true"] QLabel { background: transparent; border: none; }
QFrame#avisoFlotante { background: @sup2; border: 1px solid @bordeF; border-radius: 10px; }
QFrame#avisoFlotante QLabel { background: transparent; }
)");
    // Los nombres largos primero (si no, «@sup» pisaría «@sup2»)
    const QList<QPair<QString, QString>> valores{
        {QStringLiteral("@acentoClaro"), acentoClaro},
        {QStringLiteral("@acentoSuave"), rgba(c.acento, 0.16)},
        {QStringLiteral("@acentoSel"), rgba(c.acento, 0.28)},
        {QStringLiteral("@acento"), c.acento.name()},
        {QStringLiteral("@sobreAcento"), c.sobreAcento.name()},
        {QStringLiteral("@sup2"), c.superficie2.name()},
        {QStringLiteral("@sup"), c.superficie.name()},
        {QStringLiteral("@bordeF"), c.bordeFuerte.name()},
        {QStringLiteral("@borde"), c.borde.name()},
        {QStringLiteral("@fondo"), c.fondo.name()},
        {QStringLiteral("@texto3"), c.texto3.name()},
        {QStringLiteral("@texto2"), c.texto2.name()},
        {QStringLiteral("@texto"), c.texto.name()},
        {QStringLiteral("@okSuave"), rgba(c.ok, 0.16)},
        {QStringLiteral("@ok"), c.ok.name()},
        {QStringLiteral("@avisoSuave"), rgba(c.aviso, 0.16)},
        {QStringLiteral("@aviso"), c.aviso.name()},
        {QStringLiteral("@errorSuave"), rgba(c.error, 0.16)},
        {QStringLiteral("@error"), c.error.name()},
    };
    for (const auto &[clave, valor] : valores)
        hoja.replace(clave, valor);
    return hoja;
}

QString nombreEstado(Estado estado)
{
    switch (estado) {
    case Estado::Acento: return QStringLiteral("acento");
    case Estado::Ok: return QStringLiteral("ok");
    case Estado::Aviso: return QStringLiteral("aviso");
    case Estado::Error: return QStringLiteral("error");
    case Estado::Neutro: break;
    }
    return QStringLiteral("neutro");
}

} // namespace

const Colores &colores()
{
    return coloresActuales();
}

void aplicar(QApplication &app, const QColor &acento)
{
    asegurarRecursos();
    Colores &c = coloresActuales();
    c.acento = acento;
    // 0,18: desde ahí el texto oscuro contrasta más que el blanco (WCAG)
    c.sobreAcento = luminancia(acento) > 0.18 ? QColor(0x0b, 0x0d, 0x10) : QColor(Qt::white);

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    p.setColor(QPalette::Window, c.fondo);
    p.setColor(QPalette::WindowText, c.texto);
    p.setColor(QPalette::Base, c.superficie);
    p.setColor(QPalette::AlternateBase, c.superficie2);
    p.setColor(QPalette::Text, c.texto);
    p.setColor(QPalette::Button, c.superficie2);
    p.setColor(QPalette::ButtonText, c.texto);
    p.setColor(QPalette::BrightText, c.error);
    p.setColor(QPalette::Highlight, c.acento);
    p.setColor(QPalette::HighlightedText, c.sobreAcento);
    p.setColor(QPalette::ToolTipBase, c.superficie2);
    p.setColor(QPalette::ToolTipText, c.texto);
    p.setColor(QPalette::PlaceholderText, c.texto3);
    p.setColor(QPalette::Link, c.acento);
    p.setColor(QPalette::LinkVisited, c.acento.darker(120));
    p.setColor(QPalette::Light, c.bordeFuerte);
    p.setColor(QPalette::Midlight, c.borde);
    p.setColor(QPalette::Mid, c.borde);
    p.setColor(QPalette::Dark, c.fondo);
    p.setColor(QPalette::Shadow, Qt::black);
    for (const auto rol : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, rol, c.texto3);
    p.setColor(QPalette::Disabled, QPalette::Highlight, c.borde);
    p.setColor(QPalette::Disabled, QPalette::Base, c.fondo);
    app.setPalette(p);
    app.setStyleSheet(hojaDeEstilo(c));
}

QPixmap pixmap(const QString &nombre, int lado, const QColor &color, qreal escala)
{
    asegurarRecursos();
    const QColor pintura = color.isValid() ? color : coloresActuales().texto;
    const QString clave = QStringLiteral("%1|%2|%3|%4").arg(nombre).arg(lado).arg(pintura.name(QColor::HexArgb)).arg(escala);
    static QHash<QString, QPixmap> cache;
    if (const auto it = cache.constFind(clave); it != cache.constEnd())
        return *it;

    QFile archivo(QStringLiteral(":/comun/iconos/%1.svg").arg(nombre));
    if (!archivo.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = archivo.readAll();
    const QByteArray hex = pintura.name().toLatin1();
    svg.replace("#000000", hex);
    QSvgRenderer dibujante(svg);
    const int real = qRound(lado * escala);
    QPixmap imagen(real, real);
    imagen.fill(Qt::transparent);
    QPainter pintor(&imagen);
    pintor.setRenderHint(QPainter::Antialiasing);
    if (pintura.alpha() < 255)
        pintor.setOpacity(pintura.alphaF());
    dibujante.render(&pintor, QRectF(0, 0, real, real));
    pintor.end();
    imagen.setDevicePixelRatio(escala);
    cache.insert(clave, imagen);
    return imagen;
}

QIcon icono(const QString &nombre, const QColor &color)
{
    QIcon resultado;
    for (const int lado : {16, 20, 24, 32, 48}) {
        for (const qreal escala : {1.0, 2.0}) {
            resultado.addPixmap(pixmap(nombre, lado, color, escala), QIcon::Normal);
            resultado.addPixmap(pixmap(nombre, lado, coloresActuales().texto3, escala), QIcon::Disabled);
        }
    }
    return resultado;
}

QColor color(Estado estado)
{
    const Colores &c = coloresActuales();
    switch (estado) {
    case Estado::Acento: return c.acento;
    case Estado::Ok: return c.ok;
    case Estado::Aviso: return c.aviso;
    case Estado::Error: return c.error;
    case Estado::Neutro: break;
    }
    return c.texto2;
}

void repulir(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void marcarPrimario(QPushButton *boton, bool primario)
{
    boton->setProperty("primario", primario);
    repulir(boton);
}

void marcarPeligro(QPushButton *boton, bool peligro)
{
    boton->setProperty("peligro", peligro);
    repulir(boton);
}

void marcarChip(QLabel *etiqueta, Estado estado)
{
    // Estilo propio de la etiqueta (no en la hoja de la app): así su tamaño incluye el relleno y la negrita
    const Colores &c = coloresActuales();
    const QColor tinta = estado == Estado::Neutro ? c.texto2 : color(estado);
    const QString fondo = estado == Estado::Neutro ? c.superficie2.name() : rgba(tinta, 0.16);
    const QString borde = estado == Estado::Neutro ? c.borde.name() : rgba(tinta, 0.35);
    etiqueta->setProperty("chip", nombreEstado(estado));
    etiqueta->setStyleSheet(QStringLiteral("QLabel { background: %1; color: %2; border: 1px solid %3; border-radius: 11px;"
                                           " padding: 2px 10px; font-weight: 600; }")
                                .arg(fondo, tinta.name(), borde));
    etiqueta->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
}

void mostrarEstado(QLabel *etiqueta, const QString &texto, Estado estado)
{
    etiqueta->setText(texto);
    etiqueta->setProperty("estado", nombreEstado(estado));
    repulir(etiqueta);
}

} // namespace tema
