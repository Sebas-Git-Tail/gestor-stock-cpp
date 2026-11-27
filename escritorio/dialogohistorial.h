#pragma once
// Historial de ventas y compras (botón «Historial»): la lista por fecha, con el detalle de cada una, filtrada por
// tipo y fechas. «Anular…» devuelve el stock y la deja marcada (Stock::anularOperacion); sirve para una devolución.
// La pantalla está en dialogo_historial.ui.
#include "modelos.h"
#include <QDialog>

class QTreeWidget;
class Stock;
namespace Ui { class DialogoHistorial; }

class DialogoHistorial : public QDialog
{
    Q_OBJECT
public:
    explicit DialogoHistorial(Stock& stock, QWidget* parent = nullptr);
    ~DialogoHistorial() override;

    bool huboCambios() const { return huboCambios_; }   // se anuló algo: hay que refrescar la tabla

    // Para las pruebas
    QTreeWidget* arbol() const;
    void elegirTipo(int indice);                         // 0 ventas y compras, 1 solo ventas, 2 solo compras
    void fijarFechas(const QDate& desde, const QDate& hasta);
    void elegir(int fila);                               // la venta/compra de esa fila (la de arriba es la más nueva)
    QString textoResumen() const;

public slots:
    void anularElegida();                                // pregunta antes

private:
    void cargar();
    void actualizarBoton();
    qint64 idElegido() const;

    Ui::DialogoHistorial* ui;
    Stock& stock_;
    QList<Operacion> operaciones_;
    bool huboCambios_ = false;
    bool armando_ = false;
};
