// Lo que la sincronización necesita de la base de datos de cada programa. Stock (gestor de stock)
// y Cuentas (ingresos y gastos) heredan de BaseSinc.

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QString>
#include <QStringList>

#include <optional>

/** Otro dispositivo con el que se sincroniza (en la PC: los celulares; en el celular: la PC). */
struct Par {
    QString dispositivo;            // identificador del otro dispositivo
    QString nombre;                 // "PC de Sebastián", "Celular"
    QString secreto;                // clave compartida que se acordó al emparejar («v2:» + 64 hex)
    QString host;                   // (celular) dirección de la PC
    int puerto = 0;
    qint64 enviado = 0;             // (celular) hasta qué cambio local ya le mandé
    qint64 recibido = 0;            // (celular) hasta qué cambio de la PC ya recibí
    QDateTime ultimaSinc;
};

struct ResultadoAplicar {
    bool ok = true;
    QString error;
    int aplicadas = 0;              // filas del otro que cambiaron algo acá
    int ignoradas = 0;              // filas que no se pudieron usar (dato roto)
    QStringList aceptadas;          // uuids que se tomaron del otro (no hace falta devolvérselos)
    QStringList ganadorasLocales;   // filas donde la versión de acá es más nueva, o que acá se tuvieron
                                    // que cambiar (nombre repetido...): hay que mandarlas
};

class BaseSinc
{
public:
    virtual ~BaseSinc() = default;
    virtual QString dispositivo() const = 0;                              // identificador de este equipo
    virtual QString nombreDispositivo() const = 0;
    virtual qint64 secuencia() const = 0;                                 // último número de cambio local
    virtual QJsonArray cambiosDesde(qint64 desde) const = 0;              // lo que cambió después de ese número
    virtual QJsonArray filasPorUuid(const QStringList &uuids) const = 0;
    virtual ResultadoAplicar aplicarCambios(const QJsonArray &filas) = 0; // lo que mandó el otro
    virtual std::optional<Par> par(const QString &dispositivo) const = 0;
    virtual void guardarPar(const Par &p) = 0;
};

/** Lo que cambia entre un programa y otro. */
struct PerfilSinc {
    QString programa;               // "Gestor de Stock": para los mensajes («¿Está abierto … en la PC?»)
    QByteArray prefijoBusqueda;     // "GS": la búsqueda por UDP manda «GS-BUSCAR 2»
    QString claveBusqueda;          // "gs": la PC contesta {"gs": 2, ...}
    quint16 puerto = 0;
    quint16 puertoBusqueda = 0;
};
