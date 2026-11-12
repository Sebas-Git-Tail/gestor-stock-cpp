#pragma once
#include "modelos.h"
#include <QByteArray>
#include <QStringList>

/**
 * Copiar y pegar renglones de productos.
 *
 * Al copiar se guardan dos versiones en el portapapeles:
 *  - texto separado por tabulaciones (con una fila de títulos), que se pega bien en Excel, LibreOffice,
 *    WhatsApp, etc.;
 *  - una versión completa propia (MIME_PRODUCTOS), con todos los datos, para pegar dentro del programa
 *    sin perder nada (notas, todos los precios...).
 *
 * Al pegar se usa la versión completa si está; si no, se interpreta el texto: reconoce los títulos de las
 * columnas en cualquier orden, y si no hay títulos asume el orden de la tabla del programa.
 */

inline constexpr char MIME_PRODUCTOS[] = "application/x-gestor-stock-productos";

// Columnas del texto: las del programa en el mismo orden que la tabla principal y después las propias (`propias`)
QStringList columnasPortapapeles(const QList<Columna>& propias = {});

// Las columnas propias viajan por nombre, no por uuid: así se pueden pegar también en otra base o en otra planilla
QString productosATexto(const QList<Producto>& productos, const QList<Columna>& propias = {});
QByteArray productosAJson(const QList<Producto>& productos, const QList<Columna>& propias = {});
QList<Producto> productosDesdeJson(const QByteArray& datos, const QList<Columna>& propias = {});

struct LecturaPortapapeles {
    QList<Producto> productos;
    QStringList avisos;        // renglones que no se pudieron usar
};
LecturaPortapapeles productosDesdeTexto(const QString& texto, const QList<Columna>& propias = {});

// Expuestas para las pruebas
QList<QStringList> partirTabulado(const QString& texto);    // filas y celdas, respetando comillas como Excel
QList<Precio> preciosDesdeTexto(const QString& texto);       // "500 g: $ 4.000 · 250 g: $ 2.200"
