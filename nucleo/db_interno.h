// Ayudantes internos de la base de datos: los comparten db.cpp, db_operaciones.cpp,
// db_importacion.cpp y db_sinc.cpp. No es parte de la interfaz (eso es db.h).
#pragma once

#include "db.h"
#include "util.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <cmath>
#include <stdexcept>

namespace dbi {

struct ErrorBD : std::runtime_error {
    using std::runtime_error::runtime_error;
};

inline QSqlQuery consulta(const QSqlDatabase& db, const QString& sql, const QVariantList& params = {})
{
    QSqlQuery q(db);
    if (!q.prepare(sql)) throw ErrorBD(q.lastError().text().toStdString());
    for (const QVariant& v : params) q.addBindValue(v);
    if (!q.exec()) throw ErrorBD(q.lastError().text().toStdString());
    return q;
}

inline QVariant valor(const QSqlDatabase& db, const QString& sql, const QVariantList& params = {})
{
    QSqlQuery q = consulta(db, sql, params);
    return q.next() ? q.value(0) : QVariant();
}

// Transacción: si no se llama a confirmar() antes de salir, se deshace todo
class Transaccion
{
public:
    explicit Transaccion(QSqlDatabase db) : db_(db)
    {
        if (!db_.transaction()) throw ErrorBD(db_.lastError().text().toStdString());
    }
    ~Transaccion() { if (!confirmada_) db_.rollback(); }
    void confirmar()
    {
        if (!db_.commit()) throw ErrorBD(db_.lastError().text().toStdString());
        confirmada_ = true;
    }
private:
    QSqlDatabase db_;
    bool confirmada_ = false;
};

// codigo y vencimiento: vacío = NULL (así los códigos vacíos no chocan entre sí)
inline QVariant textoONulo(const QString& s) { return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s); }

// El resto de las columnas de texto son NOT NULL: un QString "nulo" (sin asignar) se enviaría como NULL
inline QString texto(const QString& s) { return s.isNull() ? QString(QLatin1String("")) : s; }

inline qint64 ahora() { return QDateTime::currentMSecsSinceEpoch(); }
inline QString uuidNuevo() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
inline double aCentavos(double x) { return std::round(x * 100.0) / 100.0; }

// ¿Gana la versión remota? La más nueva; si son del mismo milisegundo, decide el autor (igual en los dos lados)
inline bool ganaRemota(qint64 modRemoto, const QString& autorRemoto, qint64 modLocal, const QString& autorLocal)
{
    if (modRemoto != modLocal) return modRemoto > modLocal;
    return autorRemoto > autorLocal;
}

inline constexpr const char* SELECT_PRODUCTO =
    "SELECT id, uuid, codigo, nombre, categoria, cantidad, stock_minimo, proveedor, vencimiento, medida_bolsa, notas FROM productos ";

inline Producto leerProducto(const QSqlQuery& q)
{
    Producto p;
    p.id = q.value(0).toLongLong();
    p.uuid = q.value(1).toString();
    p.codigo = q.value(2).toString();
    p.nombre = q.value(3).toString();
    p.categoria = q.value(4).toString();
    p.cantidad = q.value(5).toInt();
    p.stockMinimo = q.value(6).toInt();
    p.proveedor = q.value(7).toString();
    p.vencimiento = q.value(8).toString();
    p.medidaBolsa = q.value(9).toString();
    p.notas = q.value(10).toString();
    return p;
}

inline constexpr const char* SELECT_COLUMNA = "SELECT id, uuid, clave, nombre, tipo, orden, visible FROM columnas ";

inline Columna leerColumna(const QSqlQuery& q)
{
    Columna c;
    c.id = q.value(0).toLongLong();
    c.uuid = q.value(1).toString();
    c.clave = q.value(2).toString();
    c.nombre = q.value(3).toString();
    c.tipo = tipoColumnaDeTexto(q.value(4).toString());
    c.orden = q.value(5).toInt();
    c.visible = q.value(6).toInt() != 0;
    return c;
}

inline void guardarPrecios(const QSqlDatabase& db, qint64 id, const QList<Precio>& precios)
{
    consulta(db, "DELETE FROM precios WHERE producto_id = ?", {id});
    int orden = 0;
    for (const Precio& p : precios) {
        consulta(db, "INSERT INTO precios (producto_id, orden, presentacion, precio) VALUES (?, ?, ?, ?)",
                 {id, orden++, texto(p.presentacion), p.precio});
    }
}

// Guarda lo cargado en las columnas propias. Un valor que no encaja con el tipo de su columna es un error
// (se deshace todo); el de una columna que ya no existe se ignora.
inline void guardarExtras(const QSqlDatabase& db, qint64 productoId, const QHash<QString, QString>& extra)
{
    consulta(db, "DELETE FROM valores_extra WHERE producto_id = ?", {productoId});
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        QSqlQuery q = consulta(db, "SELECT id, tipo, nombre FROM columnas WHERE uuid = ? AND borrado = 0 AND clave = ''", {it.key()});
        if (!q.next()) continue;
        const TipoColumna tipo = tipoColumnaDeTexto(q.value(1).toString());
        QString guardado;
        if (!normalizarValorColumna(tipo, it.value(), &guardado))
            throw ErrorBD(QStringLiteral("El valor de «%1» no sirve para una columna de tipo «%2»: «%3».")
                              .arg(q.value(2).toString(), nombreTipoColumna(tipo), it.value()).toStdString());
        if (guardado.isEmpty()) continue;
        consulta(db, "INSERT INTO valores_extra (producto_id, columna_id, valor) VALUES (?, ?, ?)", {productoId, q.value(0), guardado});
    }
}

inline QString mensajeDeError(const std::exception& e)
{
    const QString msg = QString::fromUtf8(e.what());
    if (msg.contains(QLatin1String("UNIQUE"), Qt::CaseInsensitive)) return QStringLiteral("Ya existe un producto con ese código.");
    return msg;
}

} // namespace dbi
