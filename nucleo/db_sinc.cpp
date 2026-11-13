// Sincronización: filas que se intercambian y pares emparejados (parte de la clase Stock; ver db.h)
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

// ====================================================================== sincronización: filas

// Formato de cada fila en el intercambio (JSON):
//   {"t":"col","u":uuid,"mod":ms,"aut":disp,"del":0,"k":clave,"n":nombre,"tp":"texto","o":3,"vis":1}
//   {"t":"prod","u":uuid,"mod":ms,"aut":disp,"del":0,"cod":..,"n":..,"cat":..,"sm":0,"prv":..,"venc":"2026-11-01",
//    "med":..,"nt":..,"pr":[{"p":"500 g","v":4000.0}],"x":{"<uuid de columna>":"valor"}}      (sin la cantidad)
//   {"t":"aj","u":uuid,"mod":ms,"aut":disp,"pu":uuidProducto,"d":-3,"f":"2026-11-01T10:00:00","m":"Corrección manual"}
//   {"t":"op","u":uuid,"mod":ms,"aut":disp,"tp":"venta","f":"...","tot":1234.5,
//    "it":[{"pu":uuidProducto,"n":"Avena","pr":"500 g","c":2,"pv":4000.0}]}

QJsonObject Stock::jsonColumna(qint64 id) const
{
    QSqlQuery q = consulta(db_, "SELECT uuid, modificado, autor, borrado, clave, nombre, tipo, orden, visible FROM columnas WHERE id = ?", {id});
    if (!q.next()) return {};
    return QJsonObject{{"t", "col"}, {"u", q.value(0).toString()}, {"mod", q.value(1).toLongLong()}, {"aut", q.value(2).toString()},
                       {"del", q.value(3).toInt()}, {"k", q.value(4).toString()}, {"n", q.value(5).toString()},
                       {"tp", q.value(6).toString()}, {"o", q.value(7).toInt()}, {"vis", q.value(8).toInt()}};
}

QJsonObject Stock::jsonProducto(qint64 id) const
{
    QSqlQuery q = consulta(db_, "SELECT uuid, modificado, autor, borrado, codigo, nombre, categoria, stock_minimo, proveedor, vencimiento, "
                                "medida_bolsa, notas FROM productos WHERE id = ?", {id});
    if (!q.next()) return {};
    QJsonObject o{{"t", "prod"}, {"u", q.value(0).toString()}, {"mod", q.value(1).toLongLong()}, {"aut", q.value(2).toString()},
                  {"del", q.value(3).toInt()}};
    if (q.value(3).toInt()) return o;                       // borrado: alcanza con que el otro sepa que ya no está
    o.insert(QStringLiteral("cod"), q.value(4).toString());
    o.insert(QStringLiteral("n"), q.value(5).toString());
    o.insert(QStringLiteral("cat"), q.value(6).toString());
    o.insert(QStringLiteral("sm"), q.value(7).toInt());
    o.insert(QStringLiteral("prv"), q.value(8).toString());
    o.insert(QStringLiteral("venc"), q.value(9).toString());
    o.insert(QStringLiteral("med"), q.value(10).toString());
    o.insert(QStringLiteral("nt"), q.value(11).toString());
    QJsonArray precios;
    QSqlQuery r = consulta(db_, "SELECT presentacion, precio FROM precios WHERE producto_id = ? ORDER BY orden, id", {id});
    while (r.next()) precios.append(QJsonObject{{"p", r.value(0).toString()}, {"v", r.value(1).toDouble()}});
    o.insert(QStringLiteral("pr"), precios);
    QJsonObject extra;
    QSqlQuery e = consulta(db_, "SELECT c.uuid, v.valor FROM valores_extra v JOIN columnas c ON c.id = v.columna_id "
                                "WHERE v.producto_id = ? AND c.borrado = 0", {id});
    while (e.next()) extra.insert(e.value(0).toString(), e.value(1).toString());
    o.insert(QStringLiteral("x"), extra);
    return o;
}

QJsonObject Stock::jsonAjuste(qint64 id) const
{
    QSqlQuery q = consulta(db_, "SELECT uuid, modificado, autor, producto_uuid, delta, fecha, motivo FROM ajustes_stock WHERE id = ?", {id});
    if (!q.next()) return {};
    return QJsonObject{{"t", "aj"}, {"u", q.value(0).toString()}, {"mod", q.value(1).toLongLong()}, {"aut", q.value(2).toString()},
                       {"pu", q.value(3).toString()}, {"d", q.value(4).toLongLong()}, {"f", q.value(5).toString()}, {"m", q.value(6).toString()}};
}

QJsonObject Stock::jsonOperacion(qint64 id) const
{
    QSqlQuery q = consulta(db_, "SELECT uuid, modificado, autor, tipo, fecha, total FROM operaciones WHERE id = ?", {id});
    if (!q.next()) return {};
    QJsonObject o{{"t", "op"}, {"u", q.value(0).toString()}, {"mod", q.value(1).toLongLong()}, {"aut", q.value(2).toString()},
                  {"tp", q.value(3).toString()}, {"f", q.value(4).toString()}, {"tot", q.value(5).toDouble()}};
    QJsonArray items;
    QSqlQuery i = consulta(db_, "SELECT producto_uuid, nombre, presentacion, cantidad, precio_unitario FROM operacion_items "
                                "WHERE operacion_id = ? ORDER BY id", {id});
    while (i.next())
        items.append(QJsonObject{{"pu", i.value(0).toString()}, {"n", i.value(1).toString()}, {"pr", i.value(2).toString()},
                                 {"c", i.value(3).toInt()}, {"pv", i.value(4).toDouble()}});
    o.insert(QStringLiteral("it"), items);
    return o;
}

QJsonArray Stock::cambiosDesde(qint64 desde) const
{
    QJsonArray filas;
    try {
        const auto ids = [&](const char* tabla) {
            QList<qint64> lista;
            QSqlQuery q = consulta(db_, QStringLiteral("SELECT id FROM %1 WHERE cambio > ? ORDER BY cambio").arg(QLatin1String(tabla)), {desde});
            while (q.next()) lista << q.value(0).toLongLong();
            return lista;
        };
        for (qint64 id : ids("columnas")) filas << jsonColumna(id);
        for (qint64 id : ids("productos")) filas << jsonProducto(id);
        for (qint64 id : ids("ajustes_stock")) filas << jsonAjuste(id);
        for (qint64 id : ids("operaciones")) filas << jsonOperacion(id);
    } catch (const std::exception&) {
    }
    return filas;
}

QJsonArray Stock::filasPorUuid(const QStringList& uuids) const
{
    QJsonArray filas;
    try {
        for (const QString& u : uuids) {
            QVariant id = valor(db_, "SELECT id FROM columnas WHERE uuid = ?", {u});
            if (id.isValid()) { filas << jsonColumna(id.toLongLong()); continue; }
            id = valor(db_, "SELECT id FROM productos WHERE uuid = ?", {u});
            if (id.isValid()) { filas << jsonProducto(id.toLongLong()); continue; }
            id = valor(db_, "SELECT id FROM ajustes_stock WHERE uuid = ?", {u});
            if (id.isValid()) { filas << jsonAjuste(id.toLongLong()); continue; }
            id = valor(db_, "SELECT id FROM operaciones WHERE uuid = ?", {u});
            if (id.isValid()) filas << jsonOperacion(id.toLongLong());
        }
    } catch (const std::exception&) {
    }
    return filas;
}

ResultadoAplicar Stock::aplicarCambios(const QJsonArray& filas)
{
    ResultadoAplicar r;
    try {
        Transaccion t(db_);
        QSet<QString> tocados;       // productos cuya cantidad hay que volver a calcular
        // Primero las columnas (los productos las nombran por uuid), después productos, ajustes y ventas/compras
        for (const char* tipoFila : {"col", "prod", "aj", "op"}) {
            for (const QJsonValue& v : filas) {
                const QJsonObject o = v.toObject();
                if (o.value(QLatin1String("t")).toString() != QLatin1String(tipoFila)) continue;
                const QString uuid = o.value(QLatin1String("u")).toString();
                const qint64 mod = o.value(QLatin1String("mod")).toVariant().toLongLong();
                const QString aut = o.value(QLatin1String("aut")).toString();
                const int del = o.value(QLatin1String("del")).toInt() ? 1 : 0;
                if (uuid.isEmpty()) { ++r.ignoradas; continue; }

                if (qstrcmp(tipoFila, "col") == 0) {
                    const QString clave = o.value(QLatin1String("k")).toString();
                    const QString nombre = o.value(QLatin1String("n")).toString().simplified();
                    const QString tipo = textoTipoColumna(tipoColumnaDeTexto(o.value(QLatin1String("tp")).toString()));
                    const int orden = o.value(QLatin1String("o")).toInt();
                    const bool esencial = clave == QLatin1String(CLAVE_NOMBRE) || clave == QLatin1String(CLAVE_CANTIDAD) || clave == QLatin1String(CLAVE_PRECIOS);
                    const int visible = (o.value(QLatin1String("vis")).toInt() || clave == QLatin1String(CLAVE_NOMBRE)) ? 1 : 0;
                    if (nombre.isEmpty() || (esencial && del)) { ++r.ignoradas; continue; }
                    QSqlQuery q = consulta(db_, "SELECT id, modificado, autor FROM columnas WHERE uuid = ?", {uuid});
                    qint64 id = 0;
                    if (q.next()) {
                        id = q.value(0).toLongLong();
                        if (!ganaRemota(mod, aut, q.value(1).toLongLong(), q.value(2).toString())) {
                            if (mod != q.value(1).toLongLong() || aut != q.value(2).toString()) r.ganadorasLocales << uuid;
                            continue;
                        }
                        consulta(db_, "UPDATE columnas SET nombre = ?, tipo = ?, orden = ?, visible = ?, borrado = ?, modificado = ?, autor = ?, cambio = ? "
                                      "WHERE id = ?", {nombre, tipo, orden, visible, del, mod, aut, siguienteCambio(), id});
                    } else {
                        q = consulta(db_, "INSERT INTO columnas (uuid, clave, nombre, tipo, orden, visible, borrado, modificado, autor, cambio) "
                                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                                     {uuid, clave, nombre, tipo, orden, visible, del, mod, aut, siguienteCambio()});
                        id = q.lastInsertId().toLongLong();
                    }
                    if (del && clave.isEmpty()) consulta(db_, "DELETE FROM valores_extra WHERE columna_id = ?", {id});   // se va con lo cargado
                    r.aceptadas << uuid;
                    ++r.aplicadas;
                } else if (qstrcmp(tipoFila, "prod") == 0) {
                    const QString nombre = o.value(QLatin1String("n")).toString();
                    if (!del && nombre.trimmed().isEmpty()) { ++r.ignoradas; continue; }
                    QSqlQuery q = consulta(db_, "SELECT id, modificado, autor FROM productos WHERE uuid = ?", {uuid});
                    qint64 id = 0;
                    if (q.next()) {
                        id = q.value(0).toLongLong();
                        if (!ganaRemota(mod, aut, q.value(1).toLongLong(), q.value(2).toString())) {
                            if (mod != q.value(1).toLongLong() || aut != q.value(2).toString()) r.ganadorasLocales << uuid;
                            continue;
                        }
                    }
                    // El código es único: si otro producto de acá ya lo tiene, se queda con él el de uuid más chico
                    // (los dos dispositivos deciden lo mismo) y el otro queda sin código
                    QString codigo = del ? QString() : o.value(QLatin1String("cod")).toString().trimmed();
                    bool codigoQuitado = false;
                    if (!codigo.isEmpty()) {
                        QSqlQuery c = consulta(db_, "SELECT id, uuid FROM productos WHERE codigo = ? AND borrado = 0 AND uuid != ?", {codigo, uuid});
                        if (c.next()) {
                            const qint64 otroId = c.value(0).toLongLong();
                            const QString otroUuid = c.value(1).toString();
                            if (uuid < otroUuid) {
                                consulta(db_, "UPDATE productos SET codigo = NULL WHERE id = ?", {otroId});
                                marcar("productos", otroId);
                                r.ganadorasLocales << otroUuid;
                            } else {
                                codigo.clear();
                                codigoQuitado = true;
                            }
                        }
                    }
                    const QVariantList datos = {textoONulo(codigo), texto(nombre), texto(o.value(QLatin1String("cat")).toString()),
                                                o.value(QLatin1String("sm")).toInt(), texto(o.value(QLatin1String("prv")).toString()),
                                                textoONulo(o.value(QLatin1String("venc")).toString()),
                                                texto(o.value(QLatin1String("med")).toString()), texto(o.value(QLatin1String("nt")).toString()),
                                                del, mod, aut, siguienteCambio()};
                    if (id) {
                        QVariantList p = datos;
                        p << id;
                        consulta(db_, "UPDATE productos SET codigo = ?, nombre = ?, categoria = ?, stock_minimo = ?, proveedor = ?, vencimiento = ?, "
                                      "medida_bolsa = ?, notas = ?, borrado = ?, modificado = ?, autor = ?, cambio = ? WHERE id = ?", p);
                    } else {
                        QVariantList p = {uuid};
                        p << datos;
                        q = consulta(db_, "INSERT INTO productos (uuid, codigo, nombre, categoria, stock_minimo, proveedor, vencimiento, medida_bolsa, "
                                          "notas, borrado, modificado, autor, cambio) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", p);
                        id = q.lastInsertId().toLongLong();
                    }
                    consulta(db_, "DELETE FROM precios WHERE producto_id = ?", {id});
                    consulta(db_, "DELETE FROM valores_extra WHERE producto_id = ?", {id});
                    if (!del) {
                        int orden = 0;
                        for (const QJsonValue& pv : o.value(QLatin1String("pr")).toArray()) {
                            const QJsonObject po = pv.toObject();
                            consulta(db_, "INSERT INTO precios (producto_id, orden, presentacion, precio) VALUES (?, ?, ?, ?)",
                                     {id, orden++, texto(po.value(QLatin1String("p")).toString()), po.value(QLatin1String("v")).toDouble()});
                        }
                        const QJsonObject extra = o.value(QLatin1String("x")).toObject();
                        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
                            const QVariant col = valor(db_, "SELECT id FROM columnas WHERE uuid = ? AND borrado = 0 AND clave = ''", {it.key()});
                            const QString val = it.value().toString();
                            if (col.isValid() && !val.isEmpty())
                                consulta(db_, "INSERT INTO valores_extra (producto_id, columna_id, valor) VALUES (?, ?, ?)", {id, col, val});
                        }
                    }
                    if (codigoQuitado) {                    // lo que quedó distinto acá tiene que volver al otro dispositivo
                        marcar("productos", id);
                        r.ganadorasLocales << uuid;
                    } else {
                        r.aceptadas << uuid;
                    }
                    tocados.insert(uuid);
                    ++r.aplicadas;
                } else if (qstrcmp(tipoFila, "aj") == 0) {
                    // Los ajustes no se modifican nunca: si ya está, no hay nada que hacer
                    const QString pu = o.value(QLatin1String("pu")).toString();
                    if (pu.isEmpty()) { ++r.ignoradas; continue; }
                    if (!valor(db_, "SELECT id FROM ajustes_stock WHERE uuid = ?", {uuid}).isValid()) {
                        consulta(db_, "INSERT INTO ajustes_stock (uuid, producto_uuid, delta, fecha, motivo, modificado, autor, cambio) "
                                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                                 {uuid, pu, o.value(QLatin1String("d")).toVariant().toLongLong(), texto(o.value(QLatin1String("f")).toString()),
                                  texto(o.value(QLatin1String("m")).toString()), mod, aut, siguienteCambio()});
                        tocados.insert(pu);
                        ++r.aplicadas;
                    }
                    r.aceptadas << uuid;
                } else {
                    if (!valor(db_, "SELECT id FROM operaciones WHERE uuid = ?", {uuid}).isValid()) {
                        const QString tipo = o.value(QLatin1String("tp")).toString() == QLatin1String("compra") ? QStringLiteral("compra") : QStringLiteral("venta");
                        QSqlQuery q = consulta(db_, "INSERT INTO operaciones (uuid, tipo, fecha, total, modificado, autor, cambio) VALUES (?, ?, ?, ?, ?, ?, ?)",
                                               {uuid, tipo, texto(o.value(QLatin1String("f")).toString()), o.value(QLatin1String("tot")).toDouble(),
                                                mod, aut, siguienteCambio()});
                        const qint64 idOperacion = q.lastInsertId().toLongLong();
                        for (const QJsonValue& iv : o.value(QLatin1String("it")).toArray()) {
                            const QJsonObject io = iv.toObject();
                            const QString pu = io.value(QLatin1String("pu")).toString();
                            const QVariant idProducto = valor(db_, "SELECT id FROM productos WHERE uuid = ?", {pu});
                            consulta(db_, "INSERT INTO operacion_items (operacion_id, producto_id, producto_uuid, nombre, presentacion, cantidad, precio_unitario) "
                                          "VALUES (?, ?, ?, ?, ?, ?, ?)",
                                     {idOperacion, idProducto.isValid() ? idProducto : QVariant(QMetaType(QMetaType::LongLong)), pu,
                                      texto(io.value(QLatin1String("n")).toString()), texto(io.value(QLatin1String("pr")).toString()),
                                      io.value(QLatin1String("c")).toInt(), io.value(QLatin1String("pv")).toDouble()});
                            if (!pu.isEmpty()) tocados.insert(pu);
                        }
                        ++r.aplicadas;
                    }
                    r.aceptadas << uuid;
                }
            }
        }
        for (const QString& u : tocados) recalcularCantidad(u);
        t.confirmar();
    } catch (const std::exception& e) {
        r = ResultadoAplicar{};
        r.ok = false;
        r.error = QString::fromUtf8(e.what());
    }
    return r;
}

// ====================================================================== sincronización: pares

QList<Par> Stock::pares() const
{
    QList<Par> lista;
    try {
        QSqlQuery q = consulta(db_, "SELECT dispositivo, nombre, secreto, host, puerto, enviado, recibido, ultima_sinc FROM pares ORDER BY nombre");
        while (q.next()) {
            Par p;
            p.dispositivo = q.value(0).toString();
            p.nombre = q.value(1).toString();
            p.secreto = q.value(2).toString();
            p.host = q.value(3).toString();
            p.puerto = q.value(4).toInt();
            p.enviado = q.value(5).toLongLong();
            p.recibido = q.value(6).toLongLong();
            p.ultimaSinc = QDateTime::fromString(q.value(7).toString(), Qt::ISODate);
            lista << p;
        }
    } catch (const std::exception&) {
    }
    return lista;
}

std::optional<Par> Stock::par(const QString& dispositivo) const
{
    for (const Par& p : pares()) if (p.dispositivo == dispositivo) return p;
    return std::nullopt;
}

void Stock::guardarPar(const Par& p)
{
    try {
        consulta(db_, "INSERT INTO pares (dispositivo, nombre, secreto, host, puerto, enviado, recibido, ultima_sinc) VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(dispositivo) DO UPDATE SET nombre = excluded.nombre, secreto = excluded.secreto, host = excluded.host, "
                      "puerto = excluded.puerto, enviado = excluded.enviado, recibido = excluded.recibido, ultima_sinc = excluded.ultima_sinc",
                 {p.dispositivo, texto(p.nombre), texto(p.secreto), texto(p.host), p.puerto, p.enviado, p.recibido,
                  texto(p.ultimaSinc.isValid() ? p.ultimaSinc.toString(Qt::ISODate) : QString())});
    } catch (const std::exception&) {
    }
}

void Stock::eliminarPar(const QString& dispositivo)
{
    try {
        consulta(db_, "DELETE FROM pares WHERE dispositivo = ?", {dispositivo});
    } catch (const std::exception&) {
    }
}
