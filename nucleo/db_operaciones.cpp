// Ventas, compras, reportes y alertas (parte de la clase Stock; ver db.h)
#include "db_interno.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
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
            total += l.cantidad * l.precioUnitario;   // compra: el costo, si se cargó
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
                 qMax(0.0, a.linea.precioUnitario)});
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

// ====================================================================== reportes y avisos

namespace {
// Costo promedio (ponderado por cantidad) de cada producto, según las compras con costo cargado
QHash<QString, double> costosPromedio(const QSqlDatabase& db)
{
    QHash<QString, double> costos;
    QSqlQuery q = consulta(db,
        "SELECT i.producto_uuid, SUM(i.cantidad * i.precio_unitario) / SUM(i.cantidad) FROM operacion_items i "
        "JOIN operaciones o ON o.id = i.operacion_id "
        "WHERE o.tipo = 'compra' AND i.precio_unitario > 0 AND i.cantidad > 0 GROUP BY i.producto_uuid");
    while (q.next()) costos.insert(q.value(0).toString(), q.value(1).toDouble());
    return costos;
}
}

double Stock::costoPromedio(qint64 productoId) const
{
    try {
        const QString uuid = valor(db_, "SELECT uuid FROM productos WHERE id = ?", {productoId}).toString();
        return costosPromedio(db_).value(uuid, 0.0);
    } catch (const std::exception&) {
        return 0;
    }
}

Reporte Stock::reporte(const QDate& desde, const QDate& hasta) const
{
    Reporte r;
    r.desde = desde;
    r.hasta = hasta;
    const QString d = desde.toString(Qt::ISODate), h = hasta.toString(Qt::ISODate);
    try {
        // fecha es «2026-11-13T17:05:00»: los primeros 10 caracteres son el día
        QSqlQuery q = consulta(db_,
            "SELECT tipo, COUNT(*), COALESCE(SUM(total), 0) FROM operaciones "
            "WHERE substr(fecha, 1, 10) BETWEEN ? AND ? GROUP BY tipo", {d, h});
        while (q.next()) {
            if (q.value(0).toString() == QLatin1String("venta")) {
                r.cantidadVentas = q.value(1).toInt();
                r.ventas = aCentavos(q.value(2).toDouble());
            } else {
                r.compras = aCentavos(q.value(2).toDouble());
            }
        }

        QSqlQuery dias = consulta(db_,
            "SELECT substr(fecha, 1, 10), SUM(total) FROM operaciones WHERE tipo = 'venta' "
            "AND substr(fecha, 1, 10) BETWEEN ? AND ? GROUP BY 1 ORDER BY 1", {d, h});
        QMap<QString, double> meses;
        while (dias.next()) {
            const QString dia = dias.value(0).toString();
            const double importe = aCentavos(dias.value(1).toDouble());
            r.ventasPorDia.append({QDate::fromString(dia, Qt::ISODate), importe});
            meses[dia.left(7)] += importe;
        }
        for (auto it = meses.cbegin(); it != meses.cend(); ++it) r.ventasPorMes.append({it.key(), aCentavos(it.value())});

        // Lo vendido, producto por producto: para lo más vendido y la ganancia
        const QHash<QString, double> costos = costosPromedio(db_);
        QSqlQuery items = consulta(db_,
            "SELECT i.producto_uuid, MAX(i.nombre), SUM(i.cantidad), SUM(i.cantidad * i.precio_unitario) FROM operacion_items i "
            "JOIN operaciones o ON o.id = i.operacion_id WHERE o.tipo = 'venta' "
            "AND substr(o.fecha, 1, 10) BETWEEN ? AND ? GROUP BY i.producto_uuid", {d, h});
        while (items.next()) {
            const QString uuid = items.value(0).toString();
            ProductoVendido pv{items.value(1).toString(), items.value(2).toInt(), aCentavos(items.value(3).toDouble())};
            // El nombre de ahora, si el producto sigue existiendo (en la venta quedó el de ese momento)
            const QString actual = valor(db_, "SELECT nombre FROM productos WHERE uuid = ? AND borrado = 0", {uuid}).toString();
            if (!actual.isEmpty()) pv.nombre = actual;
            r.unidadesVendidas += pv.cantidad;
            if (costos.contains(uuid)) {
                r.ventasConCosto += pv.importe;
                r.costoDeLoVendido += pv.cantidad * costos.value(uuid);
            } else {
                r.unidadesSinCosto += pv.cantidad;
            }
            r.masVendidos.append(pv);
        }
        std::sort(r.masVendidos.begin(), r.masVendidos.end(), [](const ProductoVendido& a, const ProductoVendido& b) {
            return a.cantidad != b.cantidad ? a.cantidad > b.cantidad : a.importe > b.importe;
        });
        if (r.masVendidos.size() > 10) r.masVendidos.resize(10);
        r.ventasConCosto = aCentavos(r.ventasConCosto);
        r.costoDeLoVendido = aCentavos(r.costoDeLoVendido);
        r.ganancia = aCentavos(r.ventasConCosto - r.costoDeLoVendido);
    } catch (const std::exception&) {
    }
    return r;
}

QList<Alerta> Stock::alertas(int diasVencimiento, const QDate& hoy) const
{
    QList<Alerta> lista;
    try {
        // Poco stock: solo los que tienen un mínimo cargado (y la columna sigue puesta)
        if (columnaActiva(CLAVE_STOCK_MIN)) {
            QSqlQuery q = consulta(db_,
                "SELECT id, nombre, cantidad, stock_minimo FROM productos WHERE borrado = 0 AND stock_minimo > 0 "
                "AND cantidad <= stock_minimo ORDER BY cantidad, nombre COLLATE NOCASE");
            while (q.next()) {
                const int cantidad = q.value(2).toInt();
                lista.append({Alerta::Tipo::StockBajo, q.value(0).toLongLong(), q.value(1).toString(),
                              cantidad == 0 ? QStringLiteral("sin stock (mínimo %1)").arg(q.value(3).toInt())
                                            : QStringLiteral("quedan %1 (mínimo %2)").arg(cantidad).arg(q.value(3).toInt())});
            }
        }
        // Vencidos y por vencer: solo si queda algo de ese producto
        if (columnaActiva(CLAVE_VENCIMIENTO)) {
            QSqlQuery q = consulta(db_,
                "SELECT id, nombre, vencimiento FROM productos WHERE borrado = 0 AND cantidad > 0 "
                "AND vencimiento IS NOT NULL AND vencimiento <> '' AND vencimiento <= ? ORDER BY vencimiento",
                {hoy.addDays(diasVencimiento).toString(Qt::ISODate)});
            while (q.next()) {
                const QDate vence = QDate::fromString(q.value(2).toString(), Qt::ISODate);
                if (!vence.isValid()) continue;
                const qint64 dias = hoy.daysTo(vence);
                if (dias < 0)
                    lista.append({Alerta::Tipo::Vencido, q.value(0).toLongLong(), q.value(1).toString(),
                                  QStringLiteral("venció el %1").arg(vence.toString(QStringLiteral("dd/MM/yyyy")))});
                else
                    lista.append({Alerta::Tipo::PorVencer, q.value(0).toLongLong(), q.value(1).toString(),
                                  dias == 0 ? QStringLiteral("vence hoy")
                                  : dias == 1 ? QStringLiteral("vence mañana")
                                              : QStringLiteral("vence en %1 días").arg(dias)});
            }
        }
    } catch (const std::exception&) {
    }
    return lista;
}
