#pragma once
#include "modelos.h"
#include <QDialog>
#include <QList>
#include <memory>

namespace Ui { class DialogoProducto; }
class QLineEdit;

/**
 * Alta y edición de un producto (dialogo_producto.ui).
 * Con `columnas` (las de la tabla) muestra solo los campos de las columnas que están puestas, con los títulos que
 * tengan, y agrega un campo por cada columna propia. Sin ellas (en las pruebas) muestra todos los campos del programa.
 */
class DialogoProducto : public QDialog
{
    Q_OBJECT
public:
    DialogoProducto(const QStringList& categorias, const QStringList& proveedores,
                    const Producto* producto = nullptr, QWidget* parent = nullptr,
                    const QList<Columna>& columnas = {});
    ~DialogoProducto() override;

    Producto resultado() const;       // los datos cargados (usar después de aceptar)
    void accept() override;           // valida antes de cerrar

    // Para las pruebas
    QLineEdit* campoPropio(const QString& nombreDeColumna) const;

private:
    void armarColumnas(const QList<Columna>& columnas, const Producto* producto);
    void agregarFila(const QString& presentacion = QString(), const QString& precio = QString());
    void filaNueva();
    void quitarFila();
    bool leerPrecios(QList<Precio>* precios, QString* error) const;
    bool leerPropias(QHash<QString, QString>* extra, QString* error) const;

    struct CampoPropio {
        Columna columna;
        QLineEdit* campo = nullptr;
    };

    std::unique_ptr<Ui::DialogoProducto> ui;
    qint64 idProducto_ = 0;
    QList<CampoPropio> propias_;
};
