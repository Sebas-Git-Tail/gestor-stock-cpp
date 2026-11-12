#include "db.h"
#include "util.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace {

// Columnas que tienen las tablas que se sincronizan
#define COLUMNAS_SINC \
    " modificado INTEGER NOT NULL DEFAULT 0," \
    " autor TEXT NOT NULL DEFAULT ''," \
    " cambio INTEGER NOT NULL DEFAULT 0"

const char* const ESQUEMA[] = {
    "CREATE TABLE IF NOT EXISTS config ("
    " clave TEXT PRIMARY KEY,"
    " valor TEXT NOT NULL)",

    // La cantidad es un valor calculado (ver la explicación en db.h): se guarda acá solo para listar rápido
    "CREATE TABLE IF NOT EXISTS productos ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " uuid TEXT NOT NULL UNIQUE,"
    " codigo TEXT,"
    " nombre TEXT NOT NULL,"
    " categoria TEXT NOT NULL DEFAULT '',"
    " cantidad INTEGER NOT NULL DEFAULT 0 CHECK (cantidad >= 0),"
    " stock_minimo INTEGER NOT NULL DEFAULT 0,"
    " proveedor TEXT NOT NULL DEFAULT '',"
    " vencimiento TEXT,"
    " medida_bolsa TEXT NOT NULL DEFAULT '',"
    " notas TEXT NOT NULL DEFAULT ''," COLUMNAS_SINC ","
    " borrado INTEGER NOT NULL DEFAULT 0)",

    // El código es único entre los productos que no están borrados (vacío = NULL: no choca con otros vacíos)
    "CREATE UNIQUE INDEX IF NOT EXISTS idx_productos_codigo ON productos(codigo) WHERE codigo IS NOT NULL AND borrado = 0",
    "CREATE INDEX IF NOT EXISTS idx_productos_cambio ON productos(cambio)",

    "CREATE TABLE IF NOT EXISTS precios ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " producto_id INTEGER NOT NULL REFERENCES productos(id) ON DELETE CASCADE,"
    " orden INTEGER NOT NULL DEFAULT 0,"
    " presentacion TEXT NOT NULL DEFAULT '',"
    " precio REAL NOT NULL)",

    "CREATE INDEX IF NOT EXISTS idx_precios_producto ON precios(producto_id)",

    // Estructura de la tabla: las columnas que trae el programa (clave no vacía) y las propias
    "CREATE TABLE IF NOT EXISTS columnas ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " uuid TEXT NOT NULL UNIQUE,"
    " clave TEXT NOT NULL DEFAULT '',"
    " nombre TEXT NOT NULL,"
    " tipo TEXT NOT NULL DEFAULT 'texto',"
    " orden INTEGER NOT NULL DEFAULT 0,"
    " visible INTEGER NOT NULL DEFAULT 1," COLUMNAS_SINC ","
    " borrado INTEGER NOT NULL DEFAULT 0)",

    "CREATE INDEX IF NOT EXISTS idx_columnas_cambio ON columnas(cambio)",

    // Lo cargado en las columnas propias
    "CREATE TABLE IF NOT EXISTS valores_extra ("
    " producto_id INTEGER NOT NULL REFERENCES productos(id) ON DELETE CASCADE,"
    " columna_id INTEGER NOT NULL REFERENCES columnas(id) ON DELETE CASCADE,"
    " valor TEXT NOT NULL,"
    " PRIMARY KEY (producto_id, columna_id))",

    // Historial de ventas y compras. producto_id no es clave foránea a propósito: si se borra un producto,
    // el historial conserva su nombre. producto_uuid es lo que une cada renglón con su producto entre dispositivos.
    "CREATE TABLE IF NOT EXISTS operaciones ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " uuid TEXT NOT NULL UNIQUE,"
    " tipo TEXT NOT NULL CHECK (tipo IN ('venta', 'compra')),"
    " fecha TEXT NOT NULL,"
    " total REAL NOT NULL DEFAULT 0," COLUMNAS_SINC ")",

    "CREATE INDEX IF NOT EXISTS idx_operaciones_cambio ON operaciones(cambio)",

    "CREATE TABLE IF NOT EXISTS operacion_items ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " operacion_id INTEGER NOT NULL REFERENCES operaciones(id) ON DELETE CASCADE,"
    " producto_id INTEGER,"
    " producto_uuid TEXT NOT NULL DEFAULT '',"
    " nombre TEXT NOT NULL,"
    " presentacion TEXT NOT NULL DEFAULT '',"
    " cantidad INTEGER NOT NULL,"
    " precio_unitario REAL NOT NULL DEFAULT 0)",

    "CREATE INDEX IF NOT EXISTS idx_items_operacion ON operacion_items(operacion_id)",
    "CREATE INDEX IF NOT EXISTS idx_items_producto ON operacion_items(producto_uuid)",

    // Correcciones manuales de stock y stock inicial: junto con las compras y las ventas, dan la cantidad
    "CREATE TABLE IF NOT EXISTS ajustes_stock ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " uuid TEXT NOT NULL UNIQUE,"
    " producto_uuid TEXT NOT NULL,"
    " delta INTEGER NOT NULL,"
    " fecha TEXT NOT NULL,"
    " motivo TEXT NOT NULL DEFAULT ''," COLUMNAS_SINC ")",

    "CREATE INDEX IF NOT EXISTS idx_ajustes_producto ON ajustes_stock(producto_uuid)",
    "CREATE INDEX IF NOT EXISTS idx_ajustes_cambio ON ajustes_stock(cambio)",

    // Dispositivos emparejados
    "CREATE TABLE IF NOT EXISTS pares ("
    " dispositivo TEXT PRIMARY KEY,"
    " nombre TEXT NOT NULL DEFAULT '',"
    " secreto TEXT NOT NULL,"
    " host TEXT NOT NULL DEFAULT '',"
    " puerto INTEGER NOT NULL DEFAULT 0,"
    " enviado INTEGER NOT NULL DEFAULT 0,"
    " recibido INTEGER NOT NULL DEFAULT 0,"
    " ultima_sinc TEXT NOT NULL DEFAULT '')",
};

// El esquema de la 2.1.0 y anteriores: solo para poder llevar bases viejas hasta él antes de rehacerlas
const char* const ESQUEMA_ANTERIOR[] = {
    "CREATE TABLE IF NOT EXISTS productos ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " codigo TEXT UNIQUE,"
    " nombre TEXT NOT NULL,"
    " categoria TEXT NOT NULL DEFAULT '',"
    " cantidad INTEGER NOT NULL DEFAULT 0 CHECK (cantidad >= 0),"
    " stock_minimo INTEGER NOT NULL DEFAULT 0,"
    " proveedor TEXT NOT NULL DEFAULT '',"
    " vencimiento TEXT,"
    " medida_bolsa TEXT NOT NULL DEFAULT '',"
    " notas TEXT NOT NULL DEFAULT '')",
    "CREATE TABLE IF NOT EXISTS precios ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " producto_id INTEGER NOT NULL REFERENCES productos(id) ON DELETE CASCADE,"
    " orden INTEGER NOT NULL DEFAULT 0,"
    " presentacion TEXT NOT NULL DEFAULT '',"
    " precio REAL NOT NULL)",
    "CREATE TABLE IF NOT EXISTS operaciones ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " tipo TEXT NOT NULL CHECK (tipo IN ('venta', 'compra')),"
    " fecha TEXT NOT NULL,"
    " total REAL NOT NULL DEFAULT 0)",
    "CREATE TABLE IF NOT EXISTS operacion_items ("
    " id INTEGER PRIMARY KEY AUTOINCREMENT,"
    " operacion_id INTEGER NOT NULL REFERENCES operaciones(id) ON DELETE CASCADE,"
    " producto_id INTEGER,"
    " nombre TEXT NOT NULL,"
    " presentacion TEXT NOT NULL DEFAULT '',"
    " cantidad INTEGER NOT NULL,"
    " precio_unitario REAL NOT NULL DEFAULT 0)",
};

// Las columnas con las que arranca una base (en este orden; «Notas» queda oculta: se ve como ayuda sobre el nombre)
struct ColumnaBase { const char* clave; const char* nombre; TipoColumna tipo; bool visible; };
const ColumnaBase COLUMNAS_BASE[] = {
    {CLAVE_CODIGO, "Código", TipoColumna::Texto, true},
    {CLAVE_NOMBRE, "Nombre", TipoColumna::Texto, true},
    {CLAVE_PRECIOS, "Precios", TipoColumna::Precios, true},
    {CLAVE_CATEGORIA, "Categoría", TipoColumna::Texto, true},
    {CLAVE_PROVEEDOR, "Proveedor", TipoColumna::Texto, true},
    {CLAVE_CANTIDAD, "Cantidad", TipoColumna::Entero, true},
    {CLAVE_MEDIDA, "Medida bolsa", TipoColumna::Texto, true},
    {CLAVE_VENCIMIENTO, "Vencimiento", TipoColumna::Fecha, true},
    {CLAVE_STOCK_MIN, "Stock mín.", TipoColumna::Entero, true},
    {CLAVE_NOTAS, "Notas", TipoColumna::Texto, false},
};

struct ErrorBD : std::runtime_error {
    using std::runtime_error::runtime_error;
};

QSqlQuery consulta(const QSqlDatabase& db, const QString& sql, const QVariantList& params = {})
{
    QSqlQuery q(db);
    if (!q.prepare(sql)) throw ErrorBD(q.lastError().text().toStdString());
    for (const QVariant& v : params) q.addBindValue(v);
    if (!q.exec()) throw ErrorBD(q.lastError().text().toStdString());
    return q;
}

QVariant valor(const QSqlDatabase& db, const QString& sql, const QVariantList& params = {})
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
QVariant textoONulo(const QString& s) { return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s); }

// El resto de las columnas de texto son NOT NULL: un QString "nulo" (sin asignar) se enviaría como NULL
QString texto(const QString& s) { return s.isNull() ? QString(QLatin1String("")) : s; }

qint64 ahora() { return QDateTime::currentMSecsSinceEpoch(); }
QString uuidNuevo() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
double aCentavos(double x) { return std::round(x * 100.0) / 100.0; }

// ¿Gana la versión remota? La más nueva; si son del mismo milisegundo, decide el autor (igual en los dos lados)
bool ganaRemota(qint64 modRemoto, const QString& autorRemoto, qint64 modLocal, const QString& autorLocal)
{
    if (modRemoto != modLocal) return modRemoto > modLocal;
    return autorRemoto > autorLocal;
}

const char* const SELECT_PRODUCTO =
    "SELECT id, uuid, codigo, nombre, categoria, cantidad, stock_minimo, proveedor, vencimiento, medida_bolsa, notas FROM productos ";

Producto leerProducto(const QSqlQuery& q)
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

const char* const SELECT_COLUMNA = "SELECT id, uuid, clave, nombre, tipo, orden, visible FROM columnas ";

Columna leerColumna(const QSqlQuery& q)
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

void guardarPrecios(const QSqlDatabase& db, qint64 id, const QList<Precio>& precios)
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
void guardarExtras(const QSqlDatabase& db, qint64 productoId, const QHash<QString, QString>& extra)
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

QString mensajeDeError(const std::exception& e)
{
    const QString msg = QString::fromUtf8(e.what());
    if (msg.contains(QLatin1String("UNIQUE"), Qt::CaseInsensitive)) return QStringLiteral("Ya existe un producto con ese código.");
    return msg;
}

} // namespace

// ====================================================================== apertura

QString Stock::rutaPorDefecto()
{
#ifdef Q_OS_ANDROID
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/stock.db");
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/gestor-stock/stock.db");
#endif
}

Stock::Stock(const QString& ruta)
    : ruta_(ruta)
{
    static std::atomic<int> contador{0};
    conexion_ = QStringLiteral("stock_%1").arg(contador++);
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conexion_);
    if (ruta != QLatin1String(":memory:")) QDir().mkpath(QFileInfo(ruta).absolutePath());
    db_.setDatabaseName(ruta);
    if (!db_.open()) {
        errorApertura_ = db_.lastError().text();
        return;
    }
    try {
        consulta(db_, "PRAGMA foreign_keys = ON");
        const int version = valor(db_, "PRAGMA user_version").toInt();
        if (version > VERSION_ESQUEMA)
            throw ErrorBD("La base es de una versión más nueva del programa. Actualizá Gestor de Stock.");
        if (existeTabla(QStringLiteral("productos")) && version < 4) {      // base de la 2.1.0 o anterior
            if (ruta != QLatin1String(":memory:")) {
                // Por las dudas: copia de la base tal como estaba, antes de rehacerla
                const QString copia = QFileInfo(ruta).absolutePath() + QStringLiteral("/backups/antes_de_actualizar_%1.db")
                                          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
                const Resultado r = backup(copia);
                if (!r.ok) throw ErrorBD(("No se pudo hacer la copia de seguridad antes de actualizar la base: " + r.error).toStdString());
            }
            llevarAEsquema3();
            migrarDesdeEsquema3();
        } else {
            crearEsquema();
        }
        consulta(db_, "PRAGMA foreign_keys = ON");

        dispositivo_ = config(QStringLiteral("dispositivo"));
        if (dispositivo_.isEmpty()) {
            dispositivo_ = uuidNuevo();
            fijarConfigLocal(QStringLiteral("dispositivo"), dispositivo_);
        }
        if (valor(db_, "SELECT COUNT(*) FROM columnas").toInt() == 0) {
            Transaccion t(db_);
            crearColumnasBase();
            t.confirmar();
        }
        abierta_ = true;
    } catch (const std::exception& e) {
        errorApertura_ = QString::fromUtf8(e.what());
    }
}

Stock::~Stock()
{
    if (db_.isOpen()) db_.close();
    db_ = QSqlDatabase();
    QSqlDatabase::removeDatabase(conexion_);
}

bool Stock::existeTabla(const QString& nombre) const
{
    return valor(db_, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?", {nombre}).toInt() > 0;
}

void Stock::crearEsquema()
{
    for (const char* sql : ESQUEMA) consulta(db_, QString::fromLatin1(sql));
    consulta(db_, QStringLiteral("PRAGMA user_version = %1").arg(VERSION_ESQUEMA));
}

// Bases de versiones muy viejas (de antes de los vencimientos, las notas o los varios precios): se llevan al
// esquema de la 2.1.0 para después rehacerlas. Es lo mismo que hacía el programa antes.
void Stock::llevarAEsquema3()
{
    for (const char* sql : ESQUEMA_ANTERIOR) consulta(db_, QString::fromLatin1(sql));   // (si faltaba alguna tabla)
    QSet<QString> cols;
    {
        QSqlQuery q = consulta(db_, "PRAGMA table_info(productos)");
        while (q.next()) cols.insert(q.value("name").toString());
    }
    const int version = valor(db_, "PRAGMA user_version").toInt();
    Transaccion t(db_);
    static const QList<QPair<QString, QString>> columnasNuevas = {
        {"proveedor", "TEXT NOT NULL DEFAULT ''"},
        {"vencimiento", "TEXT"},
        {"medida_bolsa", "TEXT NOT NULL DEFAULT ''"},
        {"notas", "TEXT NOT NULL DEFAULT ''"},
    };
    for (const auto& [nombre, definicion] : columnasNuevas) {
        if (!cols.contains(nombre)) consulta(db_, QStringLiteral("ALTER TABLE productos ADD COLUMN %1 %2").arg(nombre, definicion));
    }
    if (version < 2 && cols.contains("precio")) {   // antes había un único precio por producto
        const bool conMedida = cols.contains("peso_bolsa") && cols.contains("unidad");
        const QString sql = QStringLiteral("SELECT id, precio, %1 FROM productos WHERE precio > 0")
                                .arg(conMedida ? "peso_bolsa, unidad" : "0, ''");
        QSqlQuery q = consulta(db_, sql);
        QList<std::tuple<qint64, double, QString>> filas;
        while (q.next()) {
            const double peso = q.value(2).toDouble();
            const QString etiqueta = peso > 0 ? QString::number(peso, 'g') + QLatin1Char(' ') + q.value(3).toString() : QString();
            filas.append({q.value(0).toLongLong(), q.value(1).toDouble(), etiqueta});
        }
        for (const auto& [id, precio, etiqueta] : filas) {
            consulta(db_, "INSERT INTO precios (producto_id, orden, presentacion, precio) VALUES (?, 0, ?, ?)",
                     {id, texto(etiqueta), precio});
        }
    }
    // Hecho: si el paso siguiente fallara, al volver a abrir no se repite (los precios se duplicarían)
    if (version < 3) consulta(db_, "PRAGMA user_version = 3");
    t.confirmar();
}

// 2.1.0 -> 2.2.0: las tablas se rehacen con uuid, marcas de cambio y borrado lógico, y la cantidad pasa a ser la
// suma de los movimientos de stock. Todo en una sola transacción: si algo falla, la base queda como estaba.
void Stock::migrarDesdeEsquema3()
{
    consulta(db_, "PRAGMA foreign_keys = OFF");
    {
        Transaccion t(db_);
        for (const char* tabla : {"productos", "precios", "operaciones", "operacion_items"})
            consulta(db_, QStringLiteral("ALTER TABLE %1 RENAME TO %1_v3").arg(QLatin1String(tabla)));
        consulta(db_, "DROP INDEX IF EXISTS idx_precios_producto");
        consulta(db_, "DROP INDEX IF EXISTS idx_items_operacion");
        for (const char* sql : ESQUEMA) consulta(db_, QString::fromLatin1(sql));

        dispositivo_ = uuidNuevo();
        fijarConfigLocal(QStringLiteral("dispositivo"), dispositivo_);
        const qint64 mod = ahora();

        // Productos (con los mismos números de siempre) y sus precios
        QHash<qint64, QString> uuidDe;
        QSqlQuery q = consulta(db_, "SELECT id, codigo, nombre, categoria, cantidad, stock_minimo, proveedor, vencimiento, medida_bolsa, notas "
                                    "FROM productos_v3 ORDER BY id");
        while (q.next()) {
            const qint64 id = q.value(0).toLongLong();
            const QString uuid = uuidNuevo();
            uuidDe.insert(id, uuid);
            consulta(db_, "INSERT INTO productos (id, uuid, codigo, nombre, categoria, cantidad, stock_minimo, proveedor, vencimiento, "
                          "medida_bolsa, notas, modificado, autor, cambio) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                     {id, uuid, textoONulo(q.value(1).toString()), texto(q.value(2).toString()), texto(q.value(3).toString()),
                      std::max(0, q.value(4).toInt()), q.value(5).toInt(), texto(q.value(6).toString()),
                      textoONulo(q.value(7).toString()), texto(q.value(8).toString()), texto(q.value(9).toString()),
                      mod, dispositivo_, siguienteCambio()});
        }
        consulta(db_, "INSERT INTO precios (id, producto_id, orden, presentacion, precio) "
                      "SELECT id, producto_id, orden, presentacion, precio FROM precios_v3 WHERE producto_id IN (SELECT id FROM productos)");

        // Ventas y compras
        q = consulta(db_, "SELECT id, tipo, fecha, total FROM operaciones_v3 ORDER BY id");
        while (q.next()) {
            consulta(db_, "INSERT INTO operaciones (id, uuid, tipo, fecha, total, modificado, autor, cambio) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                     {q.value(0), uuidNuevo(), q.value(1), q.value(2), q.value(3), mod, dispositivo_, siguienteCambio()});
        }
        consulta(db_, "INSERT INTO operacion_items (id, operacion_id, producto_id, producto_uuid, nombre, presentacion, cantidad, precio_unitario) "
                      "SELECT i.id, i.operacion_id, i.producto_id, COALESCE(p.uuid, ''), i.nombre, i.presentacion, i.cantidad, i.precio_unitario "
                      "FROM operacion_items_v3 i LEFT JOIN productos p ON p.id = i.producto_id "
                      "WHERE i.operacion_id IN (SELECT id FROM operaciones)");

        // La cantidad de cada producto pasa a ser la suma de sus movimientos. Lo que ya estaba cargado y no se
        // explica con las ventas y compras anotadas, queda como «stock inicial»: así el resultado es el mismo número.
        const QString fecha = QDateTime::currentDateTime().toString(Qt::ISODate);
        for (auto it = uuidDe.constBegin(); it != uuidDe.constEnd(); ++it) {
            const qint64 operado = valor(db_, "SELECT COALESCE(SUM(CASE o.tipo WHEN 'compra' THEN i.cantidad ELSE -i.cantidad END), 0) "
                                              "FROM operacion_items i JOIN operaciones o ON o.id = i.operacion_id WHERE i.producto_uuid = ?",
                                         {it.value()}).toLongLong();
            const qint64 actual = valor(db_, "SELECT cantidad FROM productos WHERE id = ?", {it.key()}).toLongLong();
            if (actual - operado != 0) anotarAjuste(it.value(), actual - operado, QStringLiteral("Stock inicial"));
        }

        crearColumnasBase();
        for (const char* tabla : {"operacion_items_v3", "operaciones_v3", "precios_v3", "productos_v3"})
            consulta(db_, QStringLiteral("DROP TABLE %1").arg(QLatin1String(tabla)));
        consulta(db_, QStringLiteral("PRAGMA user_version = %1").arg(VERSION_ESQUEMA));
        t.confirmar();
    }
    consulta(db_, "PRAGMA foreign_keys = ON");
}

// Sin transacción propia: la llaman el constructor y la migración, que ya tienen la suya
void Stock::crearColumnasBase()
{
    int orden = 0;
    for (const ColumnaBase& c : COLUMNAS_BASE) {
        // Mismo uuid, misma fecha (1) y sin autor en todos los dispositivos: al sincronizar por primera vez
        // las columnas de fábrica de la PC y del celular son idénticas y no se duplican
        consulta(db_, "INSERT INTO columnas (uuid, clave, nombre, tipo, orden, visible, modificado, autor, cambio) VALUES (?, ?, ?, ?, ?, ?, 1, '', ?)",
                 {QStringLiteral("col-") + QLatin1String(c.clave), QLatin1String(c.clave), QString::fromUtf8(c.nombre),
                  textoTipoColumna(c.tipo), orden++, c.visible ? 1 : 0, siguienteCambio()});
    }
}

QString Stock::config(const QString& clave, const QString& porDefecto) const
{
    const QVariant v = valor(db_, "SELECT valor FROM config WHERE clave = ?", {clave});
    return v.isValid() ? v.toString() : porDefecto;
}

void Stock::fijarConfigLocal(const QString& clave, const QString& v)
{
    consulta(db_, "INSERT INTO config (clave, valor) VALUES (?, ?) ON CONFLICT(clave) DO UPDATE SET valor = excluded.valor",
             {clave, texto(v)});
}

qint64 Stock::siguienteCambio()
{
    const qint64 n = config(QStringLiteral("secuencia"), QStringLiteral("0")).toLongLong() + 1;
    fijarConfigLocal(QStringLiteral("secuencia"), QString::number(n));
    return n;
}

qint64 Stock::secuencia() const
{
    try {
        return config(QStringLiteral("secuencia"), QStringLiteral("0")).toLongLong();
    } catch (const std::exception&) {
        return 0;
    }
}

void Stock::marcar(const char* tabla, qint64 id)
{
    // Nunca más viejo que la versión que reemplaza (por si el reloj del otro dispositivo estaba adelantado)
    consulta(db_, QStringLiteral("UPDATE %1 SET modificado = MAX(?, modificado + 1), autor = ?, cambio = ? WHERE id = ?")
                      .arg(QLatin1String(tabla)),
             {ahora(), dispositivo_, siguienteCambio(), id});
}

QString Stock::nombreDispositivo() const
{
    try {
        QString porDefecto;
#ifdef Q_OS_ANDROID
        porDefecto = QStringLiteral("Celular");
#else
        porDefecto = QStringLiteral("PC (%1)").arg(QSysInfo::machineHostName());
#endif
        return config(QStringLiteral("nombre_dispositivo"), porDefecto);
    } catch (const std::exception&) {
        return QString();
    }
}

void Stock::setNombreDispositivo(const QString& nombre)
{
    try {
        fijarConfigLocal(QStringLiteral("nombre_dispositivo"), nombre.simplified());
    } catch (const std::exception&) {
    }
}

// ====================================================================== columnas

QList<Columna> Stock::columnas() const
{
    QList<Columna> lista;
    try {
        QSqlQuery q = consulta(db_, QString::fromLatin1(SELECT_COLUMNA) + QStringLiteral("WHERE borrado = 0 ORDER BY orden, id"));
        while (q.next()) lista << leerColumna(q);
    } catch (const std::exception&) {
    }
    return lista;
}

QList<Columna> Stock::columnasQuitadas() const
{
    QList<Columna> lista;
    try {
        QSqlQuery q = consulta(db_, QString::fromLatin1(SELECT_COLUMNA) + QStringLiteral("WHERE borrado = 1 AND clave != '' ORDER BY id"));
        while (q.next()) lista << leerColumna(q);
    } catch (const std::exception&) {
    }
    return lista;
}

std::optional<Columna> Stock::columna(qint64 id) const
{
    try {
        QSqlQuery q = consulta(db_, QString::fromLatin1(SELECT_COLUMNA) + QStringLiteral("WHERE id = ? AND borrado = 0"), {id});
        if (q.next()) return leerColumna(q);
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

std::optional<Columna> Stock::columnaPorClave(const QString& clave) const
{
    try {
        QSqlQuery q = consulta(db_, QString::fromLatin1(SELECT_COLUMNA) + QStringLiteral("WHERE clave = ? AND borrado = 0"), {clave});
        if (q.next()) return leerColumna(q);
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

bool Stock::columnaActiva(const char* clave) const
{
    try {
        return valor(db_, "SELECT COUNT(*) FROM columnas WHERE clave = ? AND borrado = 0", {QLatin1String(clave)}).toInt() > 0;
    } catch (const std::exception&) {
        return true;
    }
}

namespace {
// ¿Ya hay una columna puesta con ese nombre (sin importar mayúsculas ni tildes)?
bool nombreDeColumnaOcupado(const QList<Columna>& columnas, const QString& nombre, qint64 excepto)
{
    const QString buscado = normalizarBusqueda(nombre.simplified());
    for (const Columna& c : columnas)
        if (c.id != excepto && normalizarBusqueda(c.nombre.simplified()) == buscado) return true;
    return false;
}
}

Resultado Stock::agregarColumna(const QString& nombre, TipoColumna tipo)
{
    const QString n = nombre.simplified();
    if (n.isEmpty()) return {false, QStringLiteral("La columna necesita un nombre."), 0};
    if (tipo != TipoColumna::Texto && tipo != TipoColumna::Numero && tipo != TipoColumna::Fecha)
        return {false, QStringLiteral("Las columnas propias pueden ser de texto, número o fecha."), 0};
    const QList<Columna> existentes = columnas();
    if (nombreDeColumnaOcupado(existentes, n, 0)) return {false, QStringLiteral("Ya hay una columna con ese nombre."), 0};
    try {
        Transaccion t(db_);
        const int orden = valor(db_, "SELECT COALESCE(MAX(orden), -1) + 1 FROM columnas WHERE borrado = 0").toInt();
        QSqlQuery q = consulta(db_, "INSERT INTO columnas (uuid, clave, nombre, tipo, orden, visible) VALUES (?, '', ?, ?, ?, 1)",
                               {uuidNuevo(), n, textoTipoColumna(tipo), orden});
        const qint64 id = q.lastInsertId().toLongLong();
        marcar("columnas", id);
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), 0};
    }
}

Resultado Stock::renombrarColumna(qint64 id, const QString& nombre)
{
    const QString n = nombre.simplified();
    if (n.isEmpty()) return {false, QStringLiteral("La columna necesita un nombre."), id};
    const QList<Columna> existentes = columnas();
    const auto actual = columna(id);
    if (!actual) return {false, QStringLiteral("La columna ya no existe."), id};
    if (nombreDeColumnaOcupado(existentes, n, id)) return {false, QStringLiteral("Ya hay una columna con ese nombre."), id};
    if (n == actual->nombre) return {true, QString(), id};
    try {
        Transaccion t(db_);
        consulta(db_, "UPDATE columnas SET nombre = ? WHERE id = ?", {n, id});
        marcar("columnas", id);
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), id};
    }
}

Resultado Stock::cambiarTipoColumna(qint64 id, TipoColumna tipo)
{
    const auto actual = columna(id);
    if (!actual) return {false, QStringLiteral("La columna ya no existe."), id};
    if (!actual->propia()) return {false, QStringLiteral("«%1» es una columna del programa: no se le puede cambiar el tipo.").arg(actual->nombre), id};
    if (tipo != TipoColumna::Texto && tipo != TipoColumna::Numero && tipo != TipoColumna::Fecha)
        return {false, QStringLiteral("Las columnas propias pueden ser de texto, número o fecha."), id};
    if (tipo == actual->tipo) return {true, QString(), id};
    try {
        Transaccion t(db_);
        consulta(db_, "UPDATE columnas SET tipo = ? WHERE id = ?", {textoTipoColumna(tipo), id});
        marcar("columnas", id);
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), id};
    }
}

Resultado Stock::mostrarColumna(qint64 id, bool visible)
{
    const auto actual = columna(id);
    if (!actual) return {false, QStringLiteral("La columna ya no existe."), id};
    if (!visible && actual->clave == QLatin1String(CLAVE_NOMBRE))
        return {false, QStringLiteral("El nombre siempre se ve: sin él no se sabría de qué producto es cada renglón."), id};
    if (actual->visible == visible) return {true, QString(), id};
    try {
        Transaccion t(db_);
        consulta(db_, "UPDATE columnas SET visible = ? WHERE id = ?", {visible ? 1 : 0, id});
        marcar("columnas", id);
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), id};
    }
}

namespace {
// Se renumera todo: 0, 1, 2... (solo se marcan como cambiadas las que cambian)
void guardarOrdenDeColumnas(const QSqlDatabase& db, const QList<Columna>& lista, const std::function<void(qint64)>& marcar)
{
    for (int i = 0; i < lista.size(); ++i) {
        if (lista[i].orden == i) continue;
        consulta(db, "UPDATE columnas SET orden = ? WHERE id = ?", {i, lista[i].id});
        marcar(lista[i].id);
    }
}
}

Resultado Stock::moverColumna(qint64 id, int posicion)
{
    QList<Columna> lista = columnas();
    int actual = -1;
    for (int i = 0; i < lista.size(); ++i) if (lista[i].id == id) actual = i;
    if (actual < 0) return {false, QStringLiteral("La columna ya no existe."), id};
    posicion = std::clamp(posicion, 0, int(lista.size()) - 1);
    if (posicion == actual) return {true, QString(), id};
    lista.move(actual, posicion);
    try {
        Transaccion t(db_);
        guardarOrdenDeColumnas(db_, lista, [this](qint64 c) { marcar("columnas", c); });
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), id};
    }
}

Resultado Stock::ordenarColumnas(const QList<qint64>& ids)
{
    const QList<Columna> actuales = columnas();
    QList<Columna> lista;
    for (qint64 id : ids)
        for (const Columna& c : actuales)
            if (c.id == id) { lista << c; break; }
    for (const Columna& c : actuales) {                       // las que no se nombraron, al final y en su orden de antes
        bool esta = false;
        for (const Columna& l : lista) if (l.id == c.id) esta = true;
        if (!esta) lista << c;
    }
    try {
        Transaccion t(db_);
        guardarOrdenDeColumnas(db_, lista, [this](qint64 c) { marcar("columnas", c); });
        t.confirmar();
        return {true, QString(), 0};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), 0};
    }
}

Resultado Stock::eliminarColumna(qint64 id)
{
    const auto c = columna(id);
    if (!c) return {false, QStringLiteral("La columna ya no existe."), id};
    if (c->esencial())
        return {false, QStringLiteral("La columna «%1» no se puede eliminar: el programa la necesita para vender, comprar y buscar. "
                                      "Si no querés verla, podés ocultarla.").arg(c->nombre), id};
    try {
        Transaccion t(db_);
        if (c->propia()) consulta(db_, "DELETE FROM valores_extra WHERE columna_id = ?", {id});   // lo cargado se borra con ella
        consulta(db_, "UPDATE columnas SET borrado = 1 WHERE id = ?", {id});
        marcar("columnas", id);
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), id};
    }
}

Resultado Stock::restaurarColumna(qint64 id)
{
    try {
        QSqlQuery q = consulta(db_, QString::fromLatin1(SELECT_COLUMNA) + QStringLiteral("WHERE id = ? AND borrado = 1 AND clave != ''"), {id});
        if (!q.next()) return {false, QStringLiteral("Esa columna no se puede volver a poner."), id};
        Transaccion t(db_);
        const int orden = valor(db_, "SELECT COALESCE(MAX(orden), -1) + 1 FROM columnas WHERE borrado = 0").toInt();
        consulta(db_, "UPDATE columnas SET borrado = 0, visible = 1, orden = ? WHERE id = ?", {orden, id});
        marcar("columnas", id);
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), id};
    }
}

// ====================================================================== lectura de productos

QList<Producto> Stock::listar(const QString& filtro) const
{
    QList<Producto> productos;
    try {
        QSqlQuery q = consulta(db_, QString::fromLatin1(SELECT_PRODUCTO) + QStringLiteral("WHERE borrado = 0 ORDER BY nombre COLLATE NOCASE"));
        QHash<qint64, int> posicion;
        while (q.next()) {
            productos.append(leerProducto(q));
            posicion.insert(productos.last().id, productos.size() - 1);
        }
        QSqlQuery r = consulta(db_, "SELECT producto_id, presentacion, precio FROM precios ORDER BY producto_id, orden, id");
        while (r.next()) {
            const auto it = posicion.constFind(r.value(0).toLongLong());
            if (it != posicion.constEnd()) productos[*it].precios.append({r.value(1).toString(), r.value(2).toDouble()});
        }
        QSqlQuery e = consulta(db_, "SELECT v.producto_id, c.uuid, v.valor FROM valores_extra v JOIN columnas c ON c.id = v.columna_id "
                                    "WHERE c.borrado = 0");
        while (e.next()) {
            const auto it = posicion.constFind(e.value(0).toLongLong());
            if (it != posicion.constEnd()) productos[*it].extra.insert(e.value(1).toString(), e.value(2).toString());
        }
    } catch (const std::exception&) {
        return {};
    }
    return filtrarProductos(productos, filtro);
}

namespace {
// Completa los precios y lo cargado en las columnas propias de un producto ya leído
void completarProducto(const QSqlDatabase& db, Producto* p)
{
    QSqlQuery r = consulta(db, "SELECT presentacion, precio FROM precios WHERE producto_id = ? ORDER BY orden, id", {p->id});
    while (r.next()) p->precios.append({r.value(0).toString(), r.value(1).toDouble()});
    QSqlQuery e = consulta(db, "SELECT c.uuid, v.valor FROM valores_extra v JOIN columnas c ON c.id = v.columna_id "
                               "WHERE v.producto_id = ? AND c.borrado = 0", {p->id});
    while (e.next()) p->extra.insert(e.value(0).toString(), e.value(1).toString());
}
}

std::optional<Producto> Stock::obtener(qint64 id) const
{
    try {
        QSqlQuery q = consulta(db_, QString::fromLatin1(SELECT_PRODUCTO) + QStringLiteral("WHERE id = ? AND borrado = 0"), {id});
        if (!q.next()) return std::nullopt;
        Producto p = leerProducto(q);
        completarProducto(db_, &p);
        return p;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<Producto> Stock::obtenerPorUuid(const QString& uuid) const
{
    try {
        QSqlQuery q = consulta(db_, QString::fromLatin1(SELECT_PRODUCTO) + QStringLiteral("WHERE uuid = ? AND borrado = 0"), {uuid});
        if (!q.next()) return std::nullopt;
        Producto p = leerProducto(q);
        completarProducto(db_, &p);
        return p;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

QStringList Stock::categorias() const { return distintos("categoria"); }
QStringList Stock::proveedores() const { return distintos("proveedor"); }

QStringList Stock::distintos(const QString& columna) const
{
    QStringList lista;
    try {
        QSqlQuery q = consulta(db_, QStringLiteral("SELECT DISTINCT %1 FROM productos WHERE %1 != '' AND borrado = 0 ORDER BY %1").arg(columna));
        while (q.next()) lista << q.value(0).toString();
    } catch (const std::exception&) {
    }
    return lista;
}

// ====================================================================== escritura de productos

qint64 Stock::insertarProducto(const Producto& p)
{
    QSqlQuery q = consulta(db_,
        "INSERT INTO productos (uuid, codigo, nombre, categoria, cantidad, stock_minimo, proveedor, vencimiento, medida_bolsa, notas) "
        "VALUES (?, ?, ?, ?, 0, ?, ?, ?, ?, ?)",
        {uuidNuevo(), textoONulo(p.codigo), texto(p.nombre), texto(p.categoria), p.stockMinimo, texto(p.proveedor),
         textoONulo(p.vencimiento), texto(p.medidaBolsa), texto(p.notas)});
    const qint64 id = q.lastInsertId().toLongLong();
    marcar("productos", id);
    guardarPrecios(db_, id, p.precios);
    guardarExtras(db_, id, p.extra);
    return id;
}

Resultado Stock::crear(const Producto& p)
{
    if (p.cantidad < 0) return {false, QStringLiteral("La cantidad no puede ser negativa."), 0};
    try {
        Transaccion t(db_);
        const qint64 id = insertarProducto(p);
        const QString uuid = valor(db_, "SELECT uuid FROM productos WHERE id = ?", {id}).toString();
        if (p.cantidad > 0) {
            anotarAjuste(uuid, p.cantidad, QStringLiteral("Stock inicial"));
            recalcularCantidad(uuid);
        }
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, mensajeDeError(e), 0};
    }
}

Resultado Stock::actualizar(const Producto& p)
{
    try {
        Transaccion t(db_);
        if (!valor(db_, "SELECT id FROM productos WHERE id = ? AND borrado = 0", {p.id}).isValid())
            return {false, QStringLiteral("El producto ya no existe."), 0};
        consulta(db_,
            "UPDATE productos SET codigo=?, nombre=?, categoria=?, stock_minimo=?, proveedor=?, vencimiento=?, medida_bolsa=?, notas=? "
            "WHERE id=?",
            {textoONulo(p.codigo), texto(p.nombre), texto(p.categoria), p.stockMinimo, texto(p.proveedor),
             textoONulo(p.vencimiento), texto(p.medidaBolsa), texto(p.notas), p.id});
        marcar("productos", p.id);
        guardarPrecios(db_, p.id, p.precios);
        guardarExtras(db_, p.id, p.extra);
        t.confirmar();
        return {true, QString(), p.id};
    } catch (const std::exception& e) {
        return {false, mensajeDeError(e), 0};
    }
}

Resultado Stock::eliminar(qint64 id)
{
    try {
        Transaccion t(db_);
        // No se borra de verdad: queda marcado, así el otro dispositivo se entera. El código se libera para poder usarlo de nuevo.
        consulta(db_, "UPDATE productos SET borrado = 1, codigo = NULL WHERE id = ? AND borrado = 0", {id});
        marcar("productos", id);
        consulta(db_, "DELETE FROM precios WHERE producto_id = ?", {id});
        consulta(db_, "DELETE FROM valores_extra WHERE producto_id = ?", {id});
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), 0};
    }
}

void Stock::anotarAjuste(const QString& productoUuid, qint64 delta, const QString& motivo)
{
    QSqlQuery q = consulta(db_, "INSERT INTO ajustes_stock (uuid, producto_uuid, delta, fecha, motivo) VALUES (?, ?, ?, ?, ?)",
                           {uuidNuevo(), productoUuid, delta, QDateTime::currentDateTime().toString(Qt::ISODate), motivo});
    marcar("ajustes_stock", q.lastInsertId().toLongLong());
}

qint64 Stock::sumaMovimientos(const QString& productoUuid) const
{
    return valor(db_,
        "SELECT COALESCE((SELECT SUM(delta) FROM ajustes_stock WHERE producto_uuid = ?), 0) "
        "     + COALESCE((SELECT SUM(CASE o.tipo WHEN 'compra' THEN i.cantidad ELSE -i.cantidad END) "
        "                 FROM operacion_items i JOIN operaciones o ON o.id = i.operacion_id WHERE i.producto_uuid = ?), 0)",
        {productoUuid, productoUuid}).toLongLong();
}

// Si entre dos dispositivos se vendió más de lo que había, la suma da negativa: se muestra 0
void Stock::recalcularCantidad(const QString& productoUuid)
{
    const qint64 suma = sumaMovimientos(productoUuid);
    consulta(db_, "UPDATE productos SET cantidad = ? WHERE uuid = ?", {int(std::clamp<qint64>(suma, 0, 2000000000)), productoUuid});
}

Resultado Stock::fijarCantidad(qint64 id, int cantidad)
{
    if (cantidad < 0) return {false, QStringLiteral("La cantidad no puede ser negativa."), id};
    try {
        Transaccion t(db_);
        const QVariant uuid = valor(db_, "SELECT uuid FROM productos WHERE id = ? AND borrado = 0", {id});
        if (!uuid.isValid()) return {false, QStringLiteral("El producto ya no existe."), id};
        const qint64 delta = cantidad - sumaMovimientos(uuid.toString());
        if (delta != 0) anotarAjuste(uuid.toString(), delta, QStringLiteral("Corrección manual"));
        recalcularCantidad(uuid.toString());
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), id};
    }
}

Resultado Stock::ajustar(qint64 id, int delta)
{
    try {
        Transaccion t(db_);
        const QVariant uuid = valor(db_, "SELECT uuid FROM productos WHERE id = ? AND borrado = 0", {id});
        if (!uuid.isValid()) return {false, QStringLiteral("El producto ya no existe."), id};
        if (delta < 0 && sumaMovimientos(uuid.toString()) + delta < 0)
            return {false, QStringLiteral("No hay stock suficiente para restar esa cantidad."), id};
        if (delta != 0) anotarAjuste(uuid.toString(), delta, QStringLiteral("Ajuste"));
        recalcularCantidad(uuid.toString());
        t.confirmar();
        return {true, QString(), id};
    } catch (const std::exception& e) {
        return {false, QString::fromUtf8(e.what()), id};
    }
}

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
