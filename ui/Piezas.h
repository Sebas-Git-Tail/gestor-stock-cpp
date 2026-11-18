// Piezas de interfaz comunes (Widgets), con el estilo de Tema.h:
//   TarjetaAccion  tarjeta tocable con ícono, título y descripción (menús de opciones)
//   EstadoVacio    lo que se ve cuando una lista está vacía: ícono, texto y un botón
//   avisar()       aviso flotante abajo de la ventana que se va solo (opcionalmente con
//                  un botón, p. ej. «Deshacer»)

#pragma once

#include "Tema.h"

#include <QFrame>
#include <QPointer>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QTimer;

namespace tema {

class TarjetaAccion : public QFrame
{
    Q_OBJECT

public:
    TarjetaAccion(const QString &icono, const QString &titulo, const QString &descripcion,
                  QWidget *parent = nullptr);
    void setIcono(const QString &icono);
    void setTitulo(const QString &titulo);
    void setDescripcion(const QString &descripcion);
    QString titulo() const;

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *evento) override;
    void mouseReleaseEvent(QMouseEvent *evento) override;
    void keyPressEvent(QKeyEvent *evento) override;
    void changeEvent(QEvent *evento) override;

private:
    void apretar(bool apretada);
    void pintarIcono();

    QString nombreIcono;
    QLabel *etiquetaIcono;
    QLabel *etiquetaTitulo;
    QLabel *etiquetaDescripcion;
};

class EstadoVacio : public QWidget
{
    Q_OBJECT

public:
    // textoBoton vacío = sin botón
    EstadoVacio(const QString &icono, const QString &titulo, const QString &texto,
                const QString &textoBoton = QString(), QWidget *parent = nullptr);
    void setTextos(const QString &titulo, const QString &texto);
    void setTextoBoton(const QString &textoBoton);

signals:
    void accion();

private:
    QLabel *etiquetaTitulo;
    QLabel *etiquetaTexto;
    QPushButton *boton;
};

// Aviso flotante en la parte de abajo de «ventana» (se busca la ventana de arriba de todo).
// Si llega otro aviso, el anterior se cierra («alVencer» se llama igual: lo pendiente se confirma).
struct OpcionesAviso
{
    Estado estado = Estado::Neutro;
    int milisegundos = 4000;
    QString textoBoton;                 // p. ej. «Deshacer»
    std::function<void()> alBoton;      // se llamó al tocar el botón
    std::function<void()> alVencer;     // se cerró sin tocar el botón (tiempo o aviso nuevo)
};

void avisar(QWidget *ventana, const QString &texto, Estado estado = Estado::Neutro);
void avisar(QWidget *ventana, const QString &texto, const OpcionesAviso &opciones);

// Confirma ya el aviso que esté abierto (llama a su alVencer): usar al cerrar la ventana
void cerrarAvisos(QWidget *ventana);

} // namespace tema
