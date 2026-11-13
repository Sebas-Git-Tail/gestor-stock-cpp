#pragma once
#include <QDate>
#include <QPair>
#include <QHash>
#include <QList>
#include <QString>

struct Precio {
    QString presentacion;      // "500 g", "250 g"... (puede quedar vacío)
    double precio = 0;
};

// ---------------------------------------------------------------------- columnas

// Qué clase de dato guarda una columna. Las propias pueden ser Texto, Numero o Fecha;
// Entero y Precios son de las que trae el programa (cantidad, stock mínimo y precios).
enum class TipoColumna { Texto, Numero, Fecha, Entero, Precios };

inline QString textoTipoColumna(TipoColumna t)      // así se guarda en la base y viaja al sincronizar
{
    switch (t) {
    case TipoColumna::Numero: return QStringLiteral("numero");
    case TipoColumna::Fecha: return QStringLiteral("fecha");
    case TipoColumna::Entero: return QStringLiteral("entero");
    case TipoColumna::Precios: return QStringLiteral("precios");
    case TipoColumna::Texto: break;
    }
    return QStringLiteral("texto");
}

inline TipoColumna tipoColumnaDeTexto(const QString& s)
{
    if (s == QLatin1String("numero")) return TipoColumna::Numero;
    if (s == QLatin1String("fecha")) return TipoColumna::Fecha;
    if (s == QLatin1String("entero")) return TipoColumna::Entero;
    if (s == QLatin1String("precios")) return TipoColumna::Precios;
    return TipoColumna::Texto;
}

inline QString nombreTipoColumna(TipoColumna t)     // para mostrar
{
    switch (t) {
    case TipoColumna::Numero: return QStringLiteral("Número");
    case TipoColumna::Fecha: return QStringLiteral("Fecha");
    case TipoColumna::Entero: return QStringLiteral("Número entero");
    case TipoColumna::Precios: return QStringLiteral("Precios");
    case TipoColumna::Texto: break;
    }
    return QStringLiteral("Texto");
}

// Claves de las columnas que trae el programa
inline constexpr char CLAVE_CODIGO[] = "codigo";
inline constexpr char CLAVE_NOMBRE[] = "nombre";
inline constexpr char CLAVE_PRECIOS[] = "precios";
inline constexpr char CLAVE_CATEGORIA[] = "categoria";
inline constexpr char CLAVE_PROVEEDOR[] = "proveedor";
inline constexpr char CLAVE_CANTIDAD[] = "cantidad";
inline constexpr char CLAVE_MEDIDA[] = "medida_bolsa";
inline constexpr char CLAVE_VENCIMIENTO[] = "vencimiento";
inline constexpr char CLAVE_STOCK_MIN[] = "stock_minimo";
inline constexpr char CLAVE_NOTAS[] = "notas";

struct Columna {
    qint64 id = 0;
    QString uuid;              // el mismo en todos los dispositivos
    QString clave;             // las que trae el programa: "nombre", "codigo"...; vacío = columna propia
    QString nombre;            // el título que se ve
    TipoColumna tipo = TipoColumna::Texto;
    int orden = 0;
    bool visible = true;       // se ve en la tabla (PC) y en cada tarjeta (celular)

    bool propia() const { return clave.isEmpty(); }
    // Sin estas tres el programa no puede vender, comprar ni buscar: no se pueden eliminar
    bool esencial() const
    {
        return clave == QLatin1String(CLAVE_NOMBRE) || clave == QLatin1String(CLAVE_CANTIDAD) || clave == QLatin1String(CLAVE_PRECIOS);
    }
};

// ---------------------------------------------------------------------- productos

struct Producto {
    qint64 id = 0;
    QString uuid;              // lo completa la base: identifica al producto en todos los dispositivos
    QString codigo;            // vacío = sin código
    QString nombre;
    QString categoria;
    int cantidad = 0;
    int stockMinimo = 0;
    QString proveedor;
    QString vencimiento;       // "YYYY-MM-DD" o vacío
    QString medidaBolsa;       // dimensiones de la bolsa, ej.: "10x20/12x25"
    QString notas;
    QList<Precio> precios;
    // Lo que se cargó en las columnas propias: uuid de la columna -> valor tal como se guarda
    // (números con punto decimal, fechas "YYYY-MM-DD")
    QHash<QString, QString> extra;
};

// Lo que sale de leer la planilla de Excel (ver importarexcel.h)
struct ProductoImportado {
    QString nombre, categoria, proveedor;
    QString vencimiento;       // "YYYY-MM-DD" o vacío
    QString medidaBolsa, notas;
    QList<Precio> precios;
    int fila = 0;              // fila de la planilla, para los avisos
};

enum class TipoOperacion { Venta, Compra };

struct LineaOperacion {
    qint64 productoId = 0;
    int cantidad = 0;
    QString presentacion;      // qué presentación se vendió ("500 g")
    double precioUnitario = 0; // venta: precio de cada uno. Compra: cuánto costó cada uno (0 = no se cargó)
};

struct ItemOperacion {
    qint64 productoId = 0;
    QString nombre, presentacion;
    int cantidad = 0;
    double precioUnitario = 0;
};

struct Operacion {
    qint64 id = 0;
    TipoOperacion tipo = TipoOperacion::Venta;
    QString fecha;
    double total = 0;
    QList<ItemOperacion> items;
};

// Lo más vendido de un período
struct ProductoVendido {
    QString nombre;
    int cantidad = 0;
    double importe = 0;
};

// Reporte de un período (ver Stock::reporte)
struct Reporte {
    QDate desde, hasta;
    double ventas = 0;               // importe vendido
    double compras = 0;              // importe comprado (solo lo que tiene costo cargado)
    int cantidadVentas = 0;          // cuántas ventas (operaciones)
    int unidadesVendidas = 0;
    // Ganancia estimada: lo vendido menos lo que costó, con el costo promedio de las compras de cada
    // producto. Lo vendido de productos sin costo cargado no entra (se avisa con unidadesSinCosto).
    double ventasConCosto = 0;
    double costoDeLoVendido = 0;
    double ganancia = 0;
    int unidadesSinCosto = 0;
    QList<QPair<QDate, double>> ventasPorDia;     // solo los días con ventas, en orden
    QList<QPair<QString, double>> ventasPorMes;   // "2026-11", en orden
    QList<ProductoVendido> masVendidos;           // los 10 con más unidades
};

// Avisos: poco stock, vencido o por vencer (ver Stock::alertas)
struct Alerta {
    enum class Tipo { StockBajo, Vencido, PorVencer };
    Tipo tipo = Tipo::StockBajo;
    qint64 productoId = 0;
    QString nombre;
    QString detalle;                 // "quedan 2 (mínimo 5)", "venció el 01/11/2026", "vence en 12 días"
};
