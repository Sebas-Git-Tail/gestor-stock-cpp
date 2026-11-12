#pragma once
#include <QDateTime>
#include <QString>
#include <QVector>

/** Una celda de la planilla, ya interpretada. */
struct Celda {
    enum Tipo { Vacia, Texto, Numero, Fecha, Logico };
    Tipo tipo = Vacia;
    QString texto;
    double numero = 0;
    QDateTime fecha;
    bool logico = false;

    static Celda deTexto(const QString& t) { Celda c; c.tipo = Texto; c.texto = t; return c; }
    static Celda deNumero(double n) { Celda c; c.tipo = Numero; c.numero = n; return c; }
    static Celda deFecha(const QDateTime& f) { Celda c; c.tipo = Fecha; c.fecha = f; return c; }
};

using FilaXlsx = QVector<Celda>;

struct LecturaXlsx {
    bool ok = false;
    QString error;
    QVector<FilaXlsx> filas;   // filas[0] es la fila 1 de Excel; cada fila tiene exactamente maxColumnas celdas
};

/** Lee la primera hoja de un .xlsx (sin openpyxl: descomprime el zip con zlib e interpreta el XML con Qt). */
LecturaXlsx leerPrimeraHoja(const QString& ruta, int maxColumnas);
