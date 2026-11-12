#pragma once
#include "modelos.h"
#include <QDialog>
#include <QHash>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
namespace Ui { class DialogoOperacion; }

/**
 * Ventana de venta o compra (dialogo_operacion.ui).
 *  - Venta: cada producto lleva presentación/precio y cantidad; muestra el subtotal de cada uno y el TOTAL.
 *    La cantidad no puede pasar del stock.
 *  - Compra: solo cantidades (suma stock).
 * Los productos se cargan con los que estaban seleccionados en la tabla y se pueden agregar más con el buscador.
 */
class DialogoOperacion : public QDialog
{
    Q_OBJECT
public:
    DialogoOperacion(TipoOperacion tipo, const QList<Producto>& productos, const QList<qint64>& preseleccion,
                     QWidget* parent = nullptr);
    ~DialogoOperacion() override;

    QList<LineaOperacion> lineas() const;
    double total() const;

    void accept() override;

    // Para las pruebas
    int cantidadDeLineas() const;
    void agregarProducto(qint64 id);             // si ya está, suma 1 a la cantidad
    void fijarCantidad(int fila, int cantidad);
    void fijarPrecio(int fila, double precio);
    void elegirPresentacion(int fila, int indice);
    void agregarDesdeTexto(const QString& texto);

private:
    int filaDe(qint64 id) const;
    QSpinBox* spinCantidad(int fila) const;
    QDoubleSpinBox* spinPrecio(int fila) const;
    QComboBox* comboPresentacion(int fila) const;
    void quitarLinea();
    void recalcular();
    void agregarDesdeBuscador();

    std::unique_ptr<Ui::DialogoOperacion> ui;
    bool venta_ = true;
    bool recienElegido_ = false;      // true entre que se elige de la lista y el Enter que llega después
    QList<Producto> productos_;
    QHash<qint64, int> posicion_;     // id -> índice en productos_
};
