// Interfaz con Widgets para ModeloApps:
//   PanelApps     una página «Apps» (los Manager la agregan a su lista de secciones)
//   DialogoApps   la misma lista en una ventana (Bodrio, Carina, gestor de stock, ingresos y gastos)
//   avisarActualizaciones()  al abrir la app: si hay una versión nueva, pregunta y la instala
#pragma once

#include "ModeloApps.h"

#include <QDialog>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace apps {

class PanelApps : public QWidget
{
    Q_OBJECT
public:
    explicit PanelApps(const QString &estaApp, QWidget *parent = nullptr);
    ModeloApps *modelo() const { return modelo_; }

protected:
    void showEvent(QShowEvent *e) override;     // la primera vez que se muestra, busca las versiones

private:
    void armar();
    void actualizarFila(int fila);
    void actualizarPropia();

    ModeloApps *modelo_;
    QVBoxLayout *lista_;
    QLabel *mensaje_, *propia_;
    QPushButton *botonPropia_;
    struct Fila { QLabel *nombre, *estado; QPushButton *boton; };
    QList<Fila> filas_;
    bool buscado_ = false;
};

class DialogoApps : public QDialog
{
    Q_OBJECT
public:
    explicit DialogoApps(const QString &estaApp, QWidget *parent = nullptr);
};

// Muestra «Apps y actualizaciones»: en la PC, DialogoApps; en Android, una capa encima de la ventana
// con un botón «Volver» (abrir una ventana aparte con Widgets en Android cerraba la app: error fatal de
// Qt al crear el contexto OpenGL de la ventana nueva).
void mostrarApps(QWidget *ventana, const QString &estaApp);

// Busca (una vez por día) una versión nueva de la app y, si hay, pregunta si se instala (en Android,
// muestra la capa de mostrarApps con el botón «Actualizar», sin abrir diálogos).
void avisarActualizaciones(QWidget *ventana, const QString &estaApp);

} // namespace apps
