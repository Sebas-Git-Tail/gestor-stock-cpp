#pragma once
#include "modelos.h"
#include <QDate>
#include <QVariant>
#include <optional>

constexpr int DIAS_AVISO_VENCIMIENTO = 30;   // desde cuántos días antes se marca "por vencer"

enum class EstadoVencimiento { Ninguno, Ok, PorVencer, Vencido };

// ---- precios
bool parsearPrecio(const QString& texto, double* valor);   // '4000', '$ 4.000', '1.500,50', '1500.5'
QString formatearPrecio(double valor);                      // 4000 -> "$ 4.000" | 1500.5 -> "$ 1.500,50"
QString textoPrecios(const QList<Precio>& precios);         // "500 g: $ 4.000 · 250 g: $ 2.200"

// ---- valores de las columnas propias
bool parsearNumero(const QString& texto, double* valor);   // como parsearPrecio pero acepta negativos: '12', '1.234,5', '-3,5'
QString formatearNumero(double valor);                      // 12 -> "12" | 1234.5 -> "1.234,5"
// Pasa lo que escribió la persona a como se guarda: números con punto decimal ("1234.5"), fechas "YYYY-MM-DD".
// Un texto vacío es válido (queda sin valor). Si no se entiende, devuelve false.
bool normalizarValorColumna(TipoColumna tipo, const QString& texto, QString* guardado);
// Lo contrario: cómo se muestra lo guardado ("1.234,5", "01/11/2026"). Si no encaja con el tipo, se muestra tal cual.
QString textoValorColumna(TipoColumna tipo, const QString& guardado);
// Valor para ordenar una columna (los números como números, las fechas como "YYYY-MM-DD", el texto en minúsculas)
QVariant valorOrdenColumna(TipoColumna tipo, const QString& guardado);

// ---- vencimientos
std::optional<int> diasParaVencer(const QString& vencimiento, const QDate& hoy = QDate::currentDate());
EstadoVencimiento estadoVencimiento(const QString& vencimiento, const QDate& hoy = QDate::currentDate());

// ---- búsqueda
QString normalizarBusqueda(const QString& texto);          // sin mayúsculas ni tildes: "AZÚCAR" -> "azucar"

struct FiltroPrecio {
    double minimo = 0, maximo = 0;
    bool minimoInclusivo = true, maximoInclusivo = true;
    bool soloPrecio = false;    // true en rangos y comparaciones; false en un número suelto (que también busca como texto)
    bool coincide(double precio) const;
};
// '4000' | '2000-5000' | '<3000' | '>=2000'  ->  filtro; si no es una expresión de precio, nullopt
std::optional<FiltroPrecio> interpretarFiltroPrecio(const QString& token);

// Cada palabra de la consulta tiene que coincidir (con el nombre, código, categoría, proveedor, lo cargado en las
// columnas propias o algún precio)
bool productoCoincide(const Producto& p, const QString& consulta);
QList<Producto> filtrarProductos(const QList<Producto>& todos, const QString& consulta);
