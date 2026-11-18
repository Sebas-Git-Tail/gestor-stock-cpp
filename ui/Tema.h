// Tema común de las apps con Widgets: oscuro (los mismos colores que el sitio, seb-dev.fyi,
// ~/Web/recursos/sitio.css) o claro, y un color de acento por app.
//
//   tema::aplicar(app);                      // celeste del sitio (#00aae4)
//   tema::aplicar(app, QColor("#e8a838"));   // otro acento (Bodrio: dorado)
//
// Modo claro / oscuro: se cambia en vivo y se recuerda (QSettings «tema/oscuro»; la primera vez, oscuro).
//   layout->addWidget(tema::botonModo(this));           botón de sol/luna que lo alterna
//   tema::agregarEstilo([](const tema::Colores &c) { return QStringLiteral("..."); });   estilo propio de la app
//   tema::alCambiarModo(this, [this] { armarEstilo(); });   rehacer lo que se pintó con colores fijos
// Los íconos de tema::icono() se repintan solos con los colores del modo nuevo.
//
// Además de la paleta deja una hoja de estilo con «marcas» que se ponen como propiedades:
//   tema::marcarPrimario(boton)         botón principal, relleno con el acento
//   tema::marcarPeligro(boton)          botón que borra algo (texto rojo)
//   tema::marcarChip(etiqueta, estado)  pastilla de estado (verde, amarillo, rojo...)
//   tema::mostrarEstado(etiqueta, texto, estado)  texto de color según cómo salió algo
//   etiqueta->setProperty("secundario", true)   texto gris
//   etiqueta->setProperty("titulo", true)       título de página
//   frame->setProperty("tarjeta", true)         recuadro con borde redondeado
//   lista->setProperty("lateral", true)         menú lateral de secciones
// Los íconos son SVG de trazo (ui/iconos) y se pintan del color que se pida: tema::icono("buscar").

#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QObject>
#include <QString>

#include <functional>

class QApplication;
class QLabel;
class QPushButton;
class QToolButton;
class QWidget;

namespace tema {

struct Colores
{
    QColor fondo, superficie, superficie2, borde, bordeFuerte;
    QColor texto, texto2, texto3;
    QColor acento, sobreAcento;    // sobreAcento: texto encima del acento
    QColor ok, aviso, error;
};

enum class Estado { Neutro, Acento, Ok, Aviso, Error };

// Colores actuales (después de aplicar(), con el acento elegido)
const Colores &colores();

// Estilo Fusion + paleta (del modo guardado) + hoja de estilo. Llamar una vez, antes de crear ventanas.
void aplicar(QApplication &app, const QColor &acento = QColor(0x00, 0xaa, 0xe4));

// Modo claro u oscuro
bool oscuro();
void ponerModo(bool oscuro);    // lo cambia en vivo, lo guarda y avisa (avisos()->modoCambiado)
void alternarModo();
// Botón (sol en modo oscuro, luna en modo claro) que alterna el modo
QToolButton *botonModo(QWidget *padre = nullptr);

// Hoja de estilo propia de la app, que se agrega a la común y se rehace al cambiar de modo
void agregarEstilo(std::function<QString(const Colores &)> estilo);

// Avisa cuando cambia el modo (después de cambiar la paleta y la hoja de estilo)
class AvisosTema : public QObject
{
    Q_OBJECT
signals:
    void modoCambiado(bool oscuro);
};
AvisosTema *avisos();
// Llama a «accion» cada vez que cambia el modo, mientras viva «contexto»
void alCambiarModo(QObject *contexto, std::function<void()> accion);

// Ícono de ui/iconos/<nombre>.svg pintado de un color (vacío = color del texto).
// Incluye la versión deshabilitada en gris. Si el color es uno del tema (texto, texto2, texto3, acento,
// ok, aviso, error...), el ícono sigue a ese color cuando cambia el modo.
QIcon icono(const QString &nombre, const QColor &color = QColor());
QPixmap pixmap(const QString &nombre, int lado, const QColor &color = QColor(), qreal escala = 1.0);

// Color de un estado (para chips, avisos, textos)
QColor color(Estado estado);

void marcarPrimario(QPushButton *boton, bool primario = true);
void marcarPeligro(QPushButton *boton, bool peligro = true);
void marcarChip(QLabel *etiqueta, Estado estado);
// Texto de estado con color (verde si salió bien, rojo si falló...), sin ✓/✗ delante
void mostrarEstado(QLabel *etiqueta, const QString &texto, Estado estado = Estado::Neutro);
// Vuelve a aplicar la hoja de estilo a un widget después de cambiarle una propiedad
void repulir(QWidget *widget);

} // namespace tema
