// Lee la salida de rsync en una simulación (--dry-run --itemize-changes --stats) y cuenta qué
// se copiaría y qué se borraría del respaldo, para poder revisarlo antes de la copia real.

#pragma once

#include <QString>
#include <QStringList>

class ResumenSimulacion
{
public:
    // Devuelve verdadero si la línea era parte del resumen (no hay que mostrarla de otra forma)
    bool procesar(const QString &linea);

    qsizetype archivosACopiar() const { return copiar; }
    qsizetype archivosABorrar() const { return borrar; }
    QString tamanoACopiar() const { return tamano; }         // como lo escribe rsync: 1.23G
    QStringList primerosABorrar() const { return listaBorrar; }
    bool borradosOmitidos() const { return omitidos; }       // rsync no llegó a evaluar los borrados

private:
    qsizetype copiar = 0;
    qsizetype borrar = 0;
    QString tamano;
    QStringList listaBorrar;
    bool omitidos = false;
};
