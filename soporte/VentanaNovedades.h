// Novedades e informe de fallas con Widgets (las apps de escritorio):
//   botonNovedades(ventana)   botón con un regalito para la barra (al lado del de modo claro/oscuro):
//                             abre las novedades de todas las versiones y «Enviar informe de fallas»
//   novedadesAlAbrir(ventana) la primera vez que se abre una versión nueva, muestra lo nuevo (si hay
//                             algo para este equipo; ver Novedades.h)
#pragma once

#include <QDialog>

class QToolButton;

namespace soporte {

class Novedades;

Novedades *novedades();     // la de la app (se crea la primera vez)

class VentanaNovedades : public QDialog
{
    Q_OBJECT
public:
    // soloNuevas: lo de las versiones que todavía no se vieron; si no, todas
    VentanaNovedades(bool soloNuevas, QWidget *parent = nullptr);
};

QToolButton *botonNovedades(QWidget *ventana);
void novedadesAlAbrir(QWidget *ventana);
void mostrarNovedades(QWidget *ventana, bool soloNuevas = false);
// Pide un comentario (opcional) y manda el informe; avisa cómo salió
void enviarInformeFallas(QWidget *ventana);

} // namespace soporte
