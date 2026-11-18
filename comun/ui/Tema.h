// Tema común de las apps con Widgets: oscuro, con los mismos colores que el sitio
// (seb-dev.fyi, ~/Web/recursos/sitio.css) y un color de acento por app.
//
//   tema::aplicar(app);                      // celeste del sitio (#00aae4)
//   tema::aplicar(app, QColor("#e8a838"));   // otro acento (Bodrio: dorado)
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
#include <QString>

class QApplication;
class QLabel;
class QPushButton;
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

// Estilo Fusion + paleta oscura + hoja de estilo. Llamar una vez, antes de crear ventanas.
void aplicar(QApplication &app, const QColor &acento = QColor(0x00, 0xaa, 0xe4));

// Ícono de ui/iconos/<nombre>.svg pintado de un color (vacío = color del texto).
// Incluye la versión deshabilitada en gris.
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
