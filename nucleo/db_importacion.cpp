// Importación desde Excel y copia de seguridad (parte de la clase Stock; ver db.h)
#include "db_interno.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

using namespace dbi;

// ====================================================================== importación

QString Stock::claveProducto(const QString& nombre, const QString& categoria)
{
    static const QRegularExpression espacios(QStringLiteral(R"(\s+)"));
    auto limpio = [](QString s) { return s.replace(espacios, QStringLiteral(" ")).trimmed().toCaseFolded(); };
    return limpio(nombre) + QChar(0x1F) + limpio(categoria);
}

QHash<QString, qint64> Stock::indiceExistentes() const
{
    QHash<QString, qint64> indice;
    QSqlQuery q = consulta(db_, "SELECT id, nombre, categoria FROM productos WHERE borrado = 0");
    while (q.next()) indice.insert(claveProducto(q.value(1).toString(), q.value(2).toString()), q.value(0).toLongLong());
    return indice;
}

QPair<int, int> Stock::previsualizarImportacion(const QList<ProductoImportado>& productos) const
{
    QSet<QString> vistos;
    int nuevos = 0, actualizados = 0;
    try {
        const auto existentes = indiceExistentes();
        for (const ProductoImportado& p : productos) {
            const QString k = claveProducto(p.nombre, p.categoria);
            if (existentes.contains(k) || vistos.contains(k)) ++actualizados;
            else ++nuevos;
            vistos.insert(k);
        }
    } catch (const std::exception&) {
    }
    return {nuevos, actualizados};
}

QPair<int, int> Stock::importar(const QList<ProductoImportado>& productos)
{
    int nuevos = 0, actualizados = 0;
    try {
        Transaccion t(db_);
        auto existentes = indiceExistentes();
        for (const ProductoImportado& p : productos) {
            const QString k = claveProducto(p.nombre, p.categoria);
            const auto it = existentes.constFind(k);
            if (it == existentes.constEnd()) {
                Producto nuevo;
                nuevo.nombre = p.nombre;
                nuevo.categoria = p.categoria;
                nuevo.proveedor = p.proveedor;
                nuevo.vencimiento = p.vencimiento;
                nuevo.medidaBolsa = p.medidaBolsa;
                nuevo.notas = p.notas;
                nuevo.precios = p.precios;
                const qint64 id = insertarProducto(nuevo);        // sin código y con cantidad 0
                existentes.insert(k, id);
                ++nuevos;
            } else {
                const qint64 id = *it;
                // Solo se pisa lo que la planilla trae con valor; la cantidad y el stock mínimo no se tocan
                const QList<QPair<QString, QString>> campos = {
                    {"proveedor", p.proveedor}, {"vencimiento", p.vencimiento},
                    {"medida_bolsa", p.medidaBolsa}, {"notas", p.notas}};
                for (const auto& [campo, valor] : campos) {
                    if (!valor.isEmpty()) consulta(db_, QStringLiteral("UPDATE productos SET %1 = ? WHERE id = ?").arg(campo), {valor, id});
                }
                if (!p.precios.isEmpty()) guardarPrecios(db_, id, p.precios);
                marcar("productos", id);
                ++actualizados;
            }
        }
        t.confirmar();
    } catch (const std::exception&) {
        return {0, 0};
    }
    return {nuevos, actualizados};
}

Resultado Stock::backup(const QString& destino)
{
    try {
        QDir().mkpath(QFileInfo(destino).absolutePath());
        QFile::remove(destino);                      // VACUUM INTO no sobrescribe
        QString literal = destino;
        literal.replace(QLatin1Char('\''), QStringLiteral("''"));
        consulta(db_, QStringLiteral("VACUUM INTO '%1'").arg(literal));
        return {true, QString(), 0};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), 0};
    }
}

QString Stock::carpetaCopias() const
{
    return QFileInfo(ruta_).absolutePath() + QStringLiteral("/backups");
}

Resultado Stock::copiaDiaria(const QDate& hoy, int conservar)
{
    if (ruta_ == QLatin1String(":memory:") || !abierta_) return {true, QString(), 0};
    const QDir carpeta(carpetaCopias());
    const QString nombre = QStringLiteral("automatica_%1.db").arg(hoy.toString(Qt::ISODate));
    if (carpeta.exists(nombre)) return {true, QString(), 0};
    // Primero con otro nombre: una copia que quedó a medias (se cortó la luz) no cuenta como la del día
    const QString temporal = carpeta.filePath(nombre + QStringLiteral(".tmp"));
    const Resultado r = backup(temporal);
    if (!r.ok || !QFile::rename(temporal, carpeta.filePath(nombre))) {
        QFile::remove(temporal);
        return {false, r.ok ? QStringLiteral("No se pudo guardar la copia del día.") : r.error, 0};
    }
    // Quedan las últimas: solo se borran las automáticas (las de «Copia de seguridad» y las de antes de importar no)
    static const QRegularExpression patron(QStringLiteral(R"(^automatica_\d{4}-\d{2}-\d{2}\.db$)"));
    QStringList automaticas;
    for (const QString& f : carpeta.entryList({QStringLiteral("automatica_*.db")}, QDir::Files, QDir::Name))
        if (patron.match(f).hasMatch()) automaticas << f;              // por nombre = de la más vieja a la más nueva
    while (automaticas.size() > qMax(1, conservar)) QFile::remove(carpeta.filePath(automaticas.takeFirst()));
    return {true, QString(), 1};
}
