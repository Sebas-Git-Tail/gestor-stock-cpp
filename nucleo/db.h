#pragma once
#include "base.h"          // Par, ResultadoAplicar y BaseSinc (comun/sinc)
#include "modelos.h"
#include <QDateTime>
#include <QJsonArray>
#include <QPair>
#include <QSqlDatabase>
#include <QStringList>
#include <optional>

struct Resultado {
    bool ok = true;
    QString error;
    qint64 id = 0;
};

struct ResultadoOperacion {
    bool ok = false;
    QString error;
    qint64 id = 0;
    double total = 0;          // suma de las ventas (en compras queda en 0)
};

/**
 * Capa de datos (SQLite). La usan la versión de escritorio y la del celular.
 *
 * Esquema 4 (la 2.2.0). Para poder sincronizar, cada producto, columna, venta/compra y ajuste de stock tiene:
 *   uuid        el mismo en todos los dispositivos
 *   modificado  cuándo se cambió por última vez (milisegundos) y autor = qué dispositivo
 *   cambio      número de cambio local (1, 2, 3...): «mandame lo que cambió después del 120»
 *   borrado     (productos y columnas) no se borra de verdad: se marca, así el otro dispositivo se entera
 * Si el mismo producto o columna se cambió en los dos lados, gana el cambio más nuevo.
 *
 * La CANTIDAD de cada producto no se sincroniza como un dato más (si se vende en el celular y se compra en la PC
 * antes de sincronizar, «gana el último» perdería una de las dos): se calcula sumando todos los movimientos de
 * stock (ajustes manuales + compras - ventas), que sí viajan completos y nunca se pisan.
 */
class Stock : public BaseSinc
{
public:
    static QString rutaPorDefecto();                 // Linux: ~/.local/share/gestor-stock/stock.db
    static constexpr int VERSION_ESQUEMA = 4;

    explicit Stock(const QString& ruta);             // ":memory:" para pruebas
    ~Stock() override;
    Stock(const Stock&) = delete;
    Stock& operator=(const Stock&) = delete;

    QString ruta() const { return ruta_; }
    bool abierta() const { return abierta_; }
    QString errorApertura() const { return errorApertura_; }

    // ---- columnas (la estructura de la tabla)
    QList<Columna> columnas() const;                         // las que están puestas, en orden (visibles u ocultas)
    QList<Columna> columnasQuitadas() const;                 // las que trae el programa y se quitaron (se pueden volver a poner)
    std::optional<Columna> columna(qint64 id) const;
    std::optional<Columna> columnaPorClave(const QString& clave) const;
    bool columnaActiva(const char* clave) const;             // ¿una de las que trae el programa sigue puesta?
    Resultado agregarColumna(const QString& nombre, TipoColumna tipo);        // propia: Texto, Numero o Fecha
    Resultado renombrarColumna(qint64 id, const QString& nombre);
    Resultado cambiarTipoColumna(qint64 id, TipoColumna tipo);                // solo las propias
    Resultado mostrarColumna(qint64 id, bool visible);
    Resultado moverColumna(qint64 id, int posicion);                          // posición 0, 1, 2... entre las puestas
    Resultado ordenarColumnas(const QList<qint64>& ids);                      // el orden completo (las que falten van al final)
    Resultado eliminarColumna(qint64 id);                    // propia: borra también lo cargado; las demás: se quitan
    Resultado restaurarColumna(qint64 id);                   // vuelve a poner una que trae el programa

    // ---- productos (las filas)
    QList<Producto> listar(const QString& filtro = QString()) const;   // el filtro busca también por precio
    std::optional<Producto> obtener(qint64 id) const;
    std::optional<Producto> obtenerPorUuid(const QString& uuid) const;
    QStringList categorias() const;
    QStringList proveedores() const;

    Resultado crear(const Producto& p);              // p.cantidad es el stock inicial; error si el código ya existe
    Resultado actualizar(const Producto& p);         // todo menos la cantidad (para eso: fijarCantidad)
    Resultado eliminar(qint64 id);
    Resultado fijarCantidad(qint64 id, int cantidad);  // corrección manual: queda anotada como un movimiento de stock
    Resultado ajustar(qint64 id, int delta);         // lo mismo, sumando o restando

    // ---- ventas y compras: todo o nada
    ResultadoOperacion registrarOperacion(TipoOperacion tipo, const QList<LineaOperacion>& lineas);
    QList<Operacion> operaciones(int limite = 100) const;

    // ---- reportes y avisos
    Reporte reporte(const QDate& desde, const QDate& hasta) const;     // ventas, compras y ganancia del período
    QList<Alerta> alertas(int diasVencimiento = 30, const QDate& hoy = QDate::currentDate()) const;
    double costoPromedio(qint64 productoId) const;                     // de las compras con costo; 0 si no hay

    // ---- importación desde Excel
    QPair<int, int> previsualizarImportacion(const QList<ProductoImportado>& productos) const;  // (nuevos, existentes)
    QPair<int, int> importar(const QList<ProductoImportado>& productos);                         // (nuevos, actualizados)

    Resultado backup(const QString& destino);

    // ---- sincronización
    QString dispositivo() const override { return dispositivo_; }
    QString nombreDispositivo() const override;
    void setNombreDispositivo(const QString& nombre);
    qint64 secuencia() const override;                                     // último número de cambio local
    QJsonArray cambiosDesde(qint64 desde) const override;                  // todo lo que cambió después de ese número
    QJsonArray filasPorUuid(const QStringList& uuids) const override;
    ResultadoAplicar aplicarCambios(const QJsonArray& filas) override;     // lo que mandó el otro (gana lo más nuevo)

    QList<Par> pares() const;
    std::optional<Par> par(const QString& dispositivo) const override;
    void guardarPar(const Par& p) override;
    void eliminarPar(const QString& dispositivo);

private:
    void crearEsquema();
    void llevarAEsquema3();                                       // bases de versiones muy viejas
    void migrarDesdeEsquema3();                                   // 2.1.0 -> 2.2.0: uuid, marcas de cambio, columnas, movimientos
    void crearColumnasBase();
    bool existeTabla(const QString& nombre) const;
    QString config(const QString& clave, const QString& porDefecto = QString()) const;
    void fijarConfigLocal(const QString& clave, const QString& valor);
    qint64 siguienteCambio();
    void marcar(const char* tabla, qint64 id);                    // cambio local: fecha, autor y número de cambio
    qint64 insertarProducto(const Producto& p);                   // sin cantidad: el stock inicial va como movimiento
    void anotarAjuste(const QString& productoUuid, qint64 delta, const QString& motivo);
    qint64 sumaMovimientos(const QString& productoUuid) const;    // ajustes + compras - ventas (puede dar negativo)
    void recalcularCantidad(const QString& productoUuid);        // cantidad = max(0, suma de movimientos)
    QJsonObject jsonProducto(qint64 id) const;
    QJsonObject jsonColumna(qint64 id) const;
    QJsonObject jsonAjuste(qint64 id) const;
    QJsonObject jsonOperacion(qint64 id) const;
    QHash<QString, qint64> indiceExistentes() const;
    static QString claveProducto(const QString& nombre, const QString& categoria);
    QStringList distintos(const QString& columna) const;

    QString ruta_;
    QString conexion_;
    mutable QSqlDatabase db_;
    bool abierta_ = false;
    QString errorApertura_;
    QString dispositivo_;
};
