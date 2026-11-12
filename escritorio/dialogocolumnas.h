#pragma once
#include "db.h"
#include <QDialog>
#include <memory>

namespace Ui { class DialogoColumnas; }

/**
 * La estructura de la tabla: agregar, renombrar, cambiar el tipo, mostrar u ocultar, ordenar y eliminar columnas
 * (dialogo_columnas.ui). Las columnas propias (las que agrega la persona) se completan en cada producto.
 * Nombre, Precios y Cantidad no se pueden eliminar; las demás que trae el programa se pueden quitar y volver a poner.
 */
class DialogoColumnas : public QDialog
{
    Q_OBJECT
public:
    explicit DialogoColumnas(Stock& stock, QWidget* parent = nullptr);
    ~DialogoColumnas() override;

    bool huboCambios() const { return cambios_; }

    // Lo que hacen los botones, sin las ventanitas de preguntas (lo usan los botones y las pruebas)
    qint64 seleccionada() const;                      // 0 si no hay
    void seleccionar(qint64 id);
    Resultado agregar(const QString& nombre, TipoColumna tipo);
    Resultado renombrar(const QString& nombre);
    Resultado cambiarTipo(TipoColumna tipo);
    Resultado mostrar(qint64 id, bool visible);
    Resultado mover(int delta);                       // -1 la sube un lugar, +1 la baja
    Resultado eliminar();
    Resultado restaurar(qint64 id);

    // Las mismas preguntas con sus ventanitas, para usarlas también desde el menú de los títulos de la tabla.
    // Devuelven true si cambió algo.
    static bool pedirAgregar(Stock& stock, QWidget* padre, qint64* nueva = nullptr);
    static bool pedirRenombrar(Stock& stock, const Columna& columna, QWidget* padre);
    static bool pedirEliminar(Stock& stock, const Columna& columna, QWidget* padre);

private:
    void cargar(qint64 elegir = 0);
    void actualizarBotones();
    void mostrarError(const Resultado& r);
    void pedirTipo();
    void pedirRestaurar();

    std::unique_ptr<Ui::DialogoColumnas> ui;
    Stock& stock_;
    bool cambios_ = false;
    bool cargando_ = false;
};
