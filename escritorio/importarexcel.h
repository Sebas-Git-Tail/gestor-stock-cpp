#pragma once
#include "modelos.h"
#include "xlsxlector.h"
#include <QStringList>

/**
 * Lee una lista de precios en Excel (formato "DIETETICA KROCANTE") y la convierte en productos.
 *
 * La planilla no es una tabla plana, así que acá se interpreta su estructura:
 *  - una fila en blanco separa bloques; la primera fila de cada bloque es la CATEGORÍA y sus
 *    columnas de precio traen la presentación ("500 g", "250 g"...) de ese bloque;
 *  - una fila sin precios seguida de filas con marcador (¬¬, **, #) es un grupo/marca: no es un
 *    producto, se antepone al nombre de sus hijos y les pasa el proveedor;
 *  - una fila "(...)" sin datos es una aclaración del producto anterior (va a notas);
 *  - los párrafos largos del final se ignoran.
 * Todo lo que no se pudo interpretar (precios con texto, vencimientos raros) se guarda en las notas
 * del producto y se informa en la lista de avisos, para no perder nada.
 */
struct LecturaPlanilla {
    bool ok = false;
    QString error;
    QList<ProductoImportado> productos;
    QStringList avisos;
};

LecturaPlanilla leerPlanilla(const QString& ruta);

// Expuestos para las pruebas
LecturaPlanilla procesarFilas(const QVector<FilaXlsx>& filas);
QString normalizarEtiqueta(const QString& texto);   // '500grs' -> '500 g' | 'x UNIDAD' -> 'unidad'
QString normalizarMedida(const QString& texto);     // '8X15' -> '8x15'

struct VencimientoLeido {
    QString iso;     // "YYYY-MM-DD" o vacío
    QString nota;    // lo que no se entendió, para las notas
};
VencimientoLeido parsearVencimiento(const Celda& valor);
