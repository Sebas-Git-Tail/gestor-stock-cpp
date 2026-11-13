// Ventas, compras, reportes y alertas (parte de la clase Stock; ver db.h)
#include "db_interno.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <algorithm>

using namespace dbi;

// ====================================================================== ventas y compras

ResultadoOperacion Stock::registrarOperacion(TipoOperacion tipo, const QList<LineaOperacion>& lineas)
{
    ResultadoOperacion r;
    if (lineas.isEmpty()) {
        r.error = QStringLiteral("No hay productos en la operación.");
        return r;
    }
    for (const LineaOperacion& l : lineas) {
        if (l.cantidad <= 0) {
            r.error = QStringLiteral("Las cantidades tienen que ser mayores que cero.");
            return r;
        }
    }
    const bool venta = tipo == TipoOperacion::Venta;
    try {
        Transaccion t(db_);                        // si algo falla, no se toca el stock de ningún producto
        double total = 0;
        struct Anotada { LineaOperacion linea; QString uuid, nombre; };
        QList<Anotada> anotadas;
        QHash<qint64, int> pedido;                 // lo que ya se pidió de cada producto en esta misma operación

        for (const LineaOperacion& l : lineas) {
            QSqlQuery q = consulta(db_, "SELECT uuid, nombre, cantidad FROM productos WHERE id = ? AND borrado = 0", {l.productoId});
            if (!q.next()) {
                r.error = QStringLiteral("Uno de los productos ya no existe.");
                return r;
            }
            const QString nombre = q.value(1).toString();
            const int hay = q.value(2).toInt();
            pedido[l.productoId] += l.cantidad;
            if (venta && pedido[l.productoId] > hay) {
                r.error = QStringLiteral("No hay stock suficiente de «%1» (hay %2, se piden %3).").arg(nombre).arg(hay).arg(pedido[l.productoId]);
                return r;
            }
            if (venta) total += l.cantidad * l.precioUnitario;
            anotadas.append({l, q.value(0).toString(), nombre});
        }
        total = aCentavos(total);

        QSqlQuery op = consulta(db_, "INSERT INTO operaciones (uuid, tipo, fecha, total) VALUES (?, ?, ?, ?)",
                                {uuidNuevo(), venta ? "venta" : "compra", QDateTime::currentDateTime().toString(Qt::ISODate), total});
        const qint64 idOperacion = op.lastInsertId().toLongLong();
        marcar("operaciones", idOperacion);
        QSet<QString> tocados;
        for (const Anotada& a : anotadas) {
            consulta(db_,
                "INSERT INTO operacion_items (operacion_id, producto_id, producto_uuid, nombre, presentacion, cantidad, precio_unitario) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)",
                {idOperacion, a.linea.productoId, a.uuid, texto(a.nombre), texto(a.linea.presentacion), a.linea.cantidad,
                 venta ? a.linea.precioUnitario : 0.0});
            tocados.insert(a.uuid);
        }
        for (const QString& u : tocados) recalcularCantidad(u);
        t.confirmar();
        r.ok = true;
        r.id = idOperacion;
        r.total = total;
        return r;
    } catch (const std::exception& e) {
        r.error = QString::fromUtf8(e.what());
        return r;
    }
}

QList<Operacion> Stock::operaciones(int limite) const
{
    QList<Operacion> lista;
    try {
        QSqlQuery q = consulta(db_, "SELECT id, tipo, fecha, total FROM operaciones ORDER BY id DESC LIMIT ?", {limite});
        while (q.next()) {
            Operacion o;
            o.id = q.value(0).toLongLong();
            o.tipo = q.value(1).toString() == QLatin1String("venta") ? TipoOperacion::Venta : TipoOperacion::Compra;
            o.fecha = q.value(2).toString();
            o.total = q.value(3).toDouble();
            lista.append(o);
        }
        for (Operacion& o : lista) {
            QSqlQuery i = consulta(db_,
                "SELECT producto_id, nombre, presentacion, cantidad, precio_unitario FROM operacion_items WHERE operacion_id = ? ORDER BY id",
                {o.id});
            while (i.next()) {
                o.items.append({i.value(0).toLongLong(), i.value(1).toString(), i.value(2).toString(),
                                i.value(3).toInt(), i.value(4).toDouble()});
            }
        }
    } catch (const std::exception&) {
    }
    return lista;
}
