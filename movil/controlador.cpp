#include "controlador.h"
#include "sincronizacion.h"
#include "util.h"

#include <QDate>
#include <QJSEngine>
#include <QLocale>
#include <QPointer>
#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif
#include <QQmlEngine>
#include <algorithm>

namespace {

Controlador* instancia = nullptr;

TipoColumna tipoDeIndice(int i)
{
    return i == 1 ? TipoColumna::Numero : (i == 2 ? TipoColumna::Fecha : TipoColumna::Texto);
}

QString numeroPlano(double v) { return formatearNumero(v); }

} // namespace

Controlador::Controlador(Stock& stock, QObject* parent)
    : QObject(parent), stock_(stock)
{
    cliente_ = new ClienteSinc(stock_, this);
    connect(cliente_, &ClienteSinc::estado, this, [this](const QString& t) {
        estadoSinc_ = t;
        emit sincCambio();
    });
    connect(cliente_, &ClienteSinc::terminado, this, [this](bool ok, const QString& mensaje, int recibidas, int enviadas) {
        sincronizando_ = false;
        estadoSinc_ = mensaje;
        // Siempre con una sola PC: si se emparejó con otra, la anterior se olvida
        const QList<Par> pares = stock_.pares();
        if (ok && pares.size() > 1) {
            Par masNuevo = pares.first();
            for (const Par& p : pares) if (p.ultimaSinc > masNuevo.ultimaSinc) masNuevo = p;
            for (const Par& p : pares) if (p.dispositivo != masNuevo.dispositivo) stock_.eliminarPar(p.dispositivo);
        }
        if (ok && recibidas > 0) refrescar();
        // En silencio solo se avisa si trajo algo; si falla en silencio (fuera de casa) no se molesta
        if (!sincEnSilencio_ || (ok && recibidas > 0)) emit aviso(mensaje);
        Q_UNUSED(enviadas);
        sincEnSilencio_ = false;
        emit sincTerminada(ok, mensaje);
        emit sincCambio();
    });
    connect(cliente_, &ClienteSinc::encontrado, this,
            [this](const QString& nombre, const QString& host, quint16 puerto, const QString&) { emit pcEncontrada(nombre, host, puerto); });
    connect(cliente_, &ClienteSinc::busquedaTerminada, this, &Controlador::busquedaTerminada);

    temporizadorSinc_.setSingleShot(true);
    connect(&temporizadorSinc_, &QTimer::timeout, this, [this] {
        if (sincronizando_) { temporizadorSinc_.start(4000); return; }
        if (!pcEmparejada()) return;
        sincEnSilencio_ = true;
        sincronizar();
    });
    refrescar();
}

Controlador::~Controlador()
{
    if (instancia == this) instancia = nullptr;
}

Controlador* Controlador::create(QQmlEngine*, QJSEngine*)
{
    QJSEngine::setObjectOwnership(instancia, QJSEngine::CppOwnership);   // la crea y la borra main()
    return instancia;
}

void Controlador::setInstancia(Controlador* c)
{
    instancia = c;
}

QString Controlador::precioTexto(double precio) const { return formatearPrecio(precio); }

// ---------------------------------------------------------------- lista de productos

void Controlador::setBusqueda(const QString& b)
{
    if (b == busqueda_) return;
    busqueda_ = b;
    refrescar();
}

void Controlador::refrescar()
{
    const QList<Columna> cols = stock_.columnas();
    const bool conVence = stock_.columnaActiva(CLAVE_VENCIMIENTO), conMinimo = stock_.columnaActiva(CLAVE_STOCK_MIN);
    const QList<Producto> lista = stock_.listar(busqueda_.simplified());
    QVariantList salida;
    int bajos = 0, vencidos = 0;
    for (const Producto& p : lista) {
        // estado: 0 normal, 1 stock bajo, 2 por vencer, 3 vencido
        int estado = 0;
        QString vence;
        if (conVence) {
            const EstadoVencimiento ev = estadoVencimiento(p.vencimiento);
            if (ev == EstadoVencimiento::Vencido) { estado = 3; ++vencidos; }
            else if (ev == EstadoVencimiento::PorVencer) estado = 2;
        }
        if (conMinimo && p.stockMinimo > 0 && p.cantidad <= p.stockMinimo) { ++bajos; if (estado == 0) estado = 1; }
        // El resto de las columnas que se ven, en una línea: «Categoría: Almacén · Marca: Sol»
        QStringList detalle;
        for (const Columna& c : cols) {
            if (!c.visible || c.esencial() || c.clave == QLatin1String(CLAVE_NOTAS)) continue;
            QString v;
            if (c.clave == QLatin1String(CLAVE_CODIGO)) v = p.codigo;
            else if (c.clave == QLatin1String(CLAVE_CATEGORIA)) v = p.categoria;
            else if (c.clave == QLatin1String(CLAVE_PROVEEDOR)) v = p.proveedor;
            else if (c.clave == QLatin1String(CLAVE_MEDIDA)) v = p.medidaBolsa;
            else if (c.clave == QLatin1String(CLAVE_STOCK_MIN)) v = p.stockMinimo > 0 ? QString::number(p.stockMinimo) : QString();
            else if (c.clave == QLatin1String(CLAVE_VENCIMIENTO)) { v = textoValorColumna(TipoColumna::Fecha, p.vencimiento); vence = v; }
            else if (c.propia()) v = textoValorColumna(c.tipo, p.extra.value(c.uuid));
            if (!v.isEmpty() && c.clave != QLatin1String(CLAVE_VENCIMIENTO)) detalle << QStringLiteral("%1: %2").arg(c.nombre, v);
        }
        if (!vence.isEmpty()) detalle << QStringLiteral("Vence: %1").arg(vence);
        salida << QVariantMap{{"id", p.id}, {"nombre", p.nombre}, {"precios", textoPrecios(p.precios)}, {"cantidad", p.cantidad},
                              {"detalle", detalle.join(QStringLiteral(" · "))}, {"estado", estado}, {"notas", p.notas}};
    }
    productos_ = salida;
    QStringList r{QStringLiteral("%1 productos").arg(lista.size())};
    if (conMinimo) r << QStringLiteral("%1 con stock bajo").arg(bajos);
    if (conVence) r << QStringLiteral("%1 vencidos").arg(vencidos);
    resumen_ = r.join(QStringLiteral(" · "));
    emit cambio();
}

QString Controlador::resultado(const Resultado& r)
{
    if (!r.ok) return r.error.isEmpty() ? QStringLiteral("No se pudo guardar.") : r.error;
    refrescar();
    programarSincronizacion();
    return QString();
}

// ---------------------------------------------------------------- productos

QVariantMap Controlador::producto(qint64 id) const
{
    Producto p;
    if (id) {
        const auto o = stock_.obtener(id);
        if (!o) return {};
        p = *o;
    }
    QVariantList precios;
    for (const Precio& pr : p.precios) precios << QVariantMap{{"presentacion", pr.presentacion}, {"precio", numeroPlano(pr.precio)}};
    QVariantMap extra;
    for (const Columna& c : stock_.columnas())
        if (c.propia()) extra.insert(c.uuid, textoValorColumna(c.tipo, p.extra.value(c.uuid)));
    return QVariantMap{{"id", p.id}, {"codigo", p.codigo}, {"nombre", p.nombre}, {"categoria", p.categoria}, {"proveedor", p.proveedor},
                       {"cantidad", id ? QString::number(p.cantidad) : QString()},
                       {"stockMinimo", p.stockMinimo ? QString::number(p.stockMinimo) : QString()},
                       {"vencimiento", textoValorColumna(TipoColumna::Fecha, p.vencimiento)}, {"medidaBolsa", p.medidaBolsa},
                       {"notas", p.notas}, {"precios", precios}, {"extra", extra}};
}

QString Controlador::guardarProducto(const QVariantMap& d)
{
    Producto p;
    p.id = d.value(QStringLiteral("id")).toLongLong();
    p.codigo = d.value(QStringLiteral("codigo")).toString().trimmed();
    p.nombre = d.value(QStringLiteral("nombre")).toString().trimmed();
    p.categoria = d.value(QStringLiteral("categoria")).toString().trimmed();
    p.proveedor = d.value(QStringLiteral("proveedor")).toString().trimmed();
    p.medidaBolsa = d.value(QStringLiteral("medidaBolsa")).toString().trimmed();
    p.notas = d.value(QStringLiteral("notas")).toString().trimmed();
    if (p.nombre.isEmpty()) return QStringLiteral("El producto necesita un nombre.");

    const auto entero = [&](const char* clave, const QString& titulo, int* destino) -> QString {
        const QString t = d.value(QLatin1String(clave)).toString().trimmed();
        if (t.isEmpty()) { *destino = 0; return QString(); }
        bool ok = false;
        const int n = t.toInt(&ok);
        if (!ok || n < 0) return QStringLiteral("«%1» tiene que ser un número entero (escribiste «%2»).").arg(titulo, t);
        *destino = n;
        return QString();
    };
    if (const QString e = entero("cantidad", QStringLiteral("Cantidad"), &p.cantidad); !e.isEmpty()) return e;
    if (const QString e = entero("stockMinimo", QStringLiteral("Stock mínimo"), &p.stockMinimo); !e.isEmpty()) return e;
    const QString venc = d.value(QStringLiteral("vencimiento")).toString();
    if (!normalizarValorColumna(TipoColumna::Fecha, venc, &p.vencimiento))
        return QStringLiteral("El vencimiento tiene que ser una fecha, por ejemplo 01/11/2026 (escribiste «%1»).").arg(venc.trimmed());
    for (const QVariant& v : d.value(QStringLiteral("precios")).toList()) {
        const QVariantMap m = v.toMap();
        const QString pres = m.value(QStringLiteral("presentacion")).toString().trimmed(), texto = m.value(QStringLiteral("precio")).toString().trimmed();
        if (pres.isEmpty() && texto.isEmpty()) continue;
        double valor = 0;
        if (!parsearPrecio(texto, &valor)) return QStringLiteral("El precio «%1» no es un número válido.").arg(texto);
        p.precios.append({pres, valor});
    }
    const QVariantMap extra = d.value(QStringLiteral("extra")).toMap();
    for (const Columna& c : stock_.columnas()) {
        if (!c.propia() || !extra.contains(c.uuid)) continue;
        QString guardado;
        if (!normalizarValorColumna(c.tipo, extra.value(c.uuid).toString(), &guardado))
            return QStringLiteral("«%1» no es válido (%2): «%3»").arg(c.nombre, nombreTipoColumna(c.tipo).toLower(), extra.value(c.uuid).toString().trimmed());
        if (!guardado.isEmpty()) p.extra.insert(c.uuid, guardado);
    }

    if (!p.id) return resultado(stock_.crear(p));                       // la cantidad es el stock inicial
    const auto antes = stock_.obtener(p.id);
    Resultado r = stock_.actualizar(p);
    if (r.ok && antes && antes->cantidad != p.cantidad) r = stock_.fijarCantidad(p.id, p.cantidad);   // solo si la cambió la persona
    return resultado(r);
}

QString Controlador::eliminarProducto(qint64 id)
{
    return resultado(stock_.eliminar(id));
}

QVariantList Controlador::buscarProductos(const QString& texto) const
{
    QVariantList salida;
    for (const Producto& p : stock_.listar(texto.simplified())) {
        QVariantList precios;
        for (const Precio& pr : p.precios)
            precios << QVariantMap{{"presentacion", pr.presentacion}, {"precio", pr.precio}, {"texto", formatearPrecio(pr.precio)}};
        salida << QVariantMap{{"id", p.id}, {"nombre", p.nombre}, {"cantidad", p.cantidad}, {"precios", precios}};
        if (salida.size() >= 40) break;
    }
    return salida;
}

// ---------------------------------------------------------------- columnas

QVariantMap Controlador::mapaColumna(const Columna& c)
{
    return QVariantMap{{"id", c.id}, {"uuid", c.uuid}, {"clave", c.clave}, {"nombre", c.nombre}, {"tipo", int(c.tipo)},
                       {"tipoNombre", c.propia() ? nombreTipoColumna(c.tipo) : QStringLiteral("del programa")},
                       {"visible", c.visible}, {"propia", c.propia()}, {"esencial", c.esencial()}};
}

QVariantList Controlador::columnas() const
{
    QVariantList l;
    for (const Columna& c : stock_.columnas()) l << mapaColumna(c);
    return l;
}

QVariantList Controlador::columnasQuitadas() const
{
    QVariantList l;
    for (const Columna& c : stock_.columnasQuitadas()) l << mapaColumna(c);
    return l;
}

QString Controlador::agregarColumna(const QString& nombre, int tipo) { return resultado(stock_.agregarColumna(nombre, tipoDeIndice(tipo))); }
QString Controlador::renombrarColumna(qint64 id, const QString& nombre) { return resultado(stock_.renombrarColumna(id, nombre)); }
QString Controlador::cambiarTipoColumna(qint64 id, int tipo) { return resultado(stock_.cambiarTipoColumna(id, tipoDeIndice(tipo))); }
QString Controlador::mostrarColumna(qint64 id, bool visible) { return resultado(stock_.mostrarColumna(id, visible)); }
QString Controlador::eliminarColumna(qint64 id) { return resultado(stock_.eliminarColumna(id)); }
QString Controlador::restaurarColumna(qint64 id) { return resultado(stock_.restaurarColumna(id)); }

QString Controlador::moverColumna(qint64 id, int delta)
{
    const QList<Columna> cols = stock_.columnas();
    for (int i = 0; i < cols.size(); ++i)
        if (cols[i].id == id) return resultado(stock_.moverColumna(id, i + delta));
    return QStringLiteral("La columna ya no existe.");
}

// ---------------------------------------------------------------- ventas y compras

QVariantMap Controlador::registrarOperacion(bool venta, const QVariantList& lineas)
{
    QList<LineaOperacion> ls;
    for (const QVariant& v : lineas) {
        const QVariantMap m = v.toMap();
        LineaOperacion l;
        l.productoId = m.value(QStringLiteral("id")).toLongLong();
        l.cantidad = m.value(QStringLiteral("cantidad")).toInt();
        l.presentacion = m.value(QStringLiteral("presentacion")).toString();
        // Venta: el precio de cada uno. Compra: el costo de cada uno (opcional, para la ganancia en Reportes)
        double precio = 0;
        const QString t = m.value(QStringLiteral("precio")).toString().trimmed();
        if (!t.isEmpty() && !parsearPrecio(t, &precio))
            return QVariantMap{{"ok", false}, {"error", (venta ? QStringLiteral("El precio «%1» no es un número válido.")
                                                               : QStringLiteral("El costo «%1» no es un número válido.")).arg(t)}};
        l.precioUnitario = precio;
        ls << l;
    }
    const ResultadoOperacion r = stock_.registrarOperacion(venta ? TipoOperacion::Venta : TipoOperacion::Compra, ls);
    if (!r.ok) return QVariantMap{{"ok", false}, {"error", r.error + QStringLiteral(" No se modificó el stock de ningún producto.")}};
    refrescar();
    programarSincronizacion();
    return QVariantMap{{"ok", true}, {"error", QString()}, {"total", formatearPrecio(r.total)}};
}

// ---------------------------------------------------------------- sincronización

bool Controlador::pcEmparejada() const { return !stock_.pares().isEmpty(); }

QString Controlador::nombrePc() const
{
    const QList<Par> p = stock_.pares();
    return p.isEmpty() ? QString() : p.first().nombre;
}

QString Controlador::direccionPc() const
{
    const QList<Par> p = stock_.pares();
    return p.isEmpty() ? QString() : QStringLiteral("%1:%2").arg(p.first().host).arg(p.first().puerto);
}

QString Controlador::ultimaSincTexto() const
{
    const QList<Par> p = stock_.pares();
    if (p.isEmpty() || !p.first().ultimaSinc.isValid()) return QStringLiteral("Todavía no se sincronizó");
    return QStringLiteral("Última vez: %1").arg(p.first().ultimaSinc.toString(QStringLiteral("dd/MM/yyyy HH:mm")));
}

void Controlador::setNombreDispositivo(const QString& n)
{
    if (n.trimmed().isEmpty()) return;
    stock_.setNombreDispositivo(n);
    emit sincCambio();
}

int Controlador::puertoPorDefecto() const { return sinc::PUERTO; }
QString Controlador::version() const { return QStringLiteral(GS_VERSION); }

void Controlador::emparejar(const QString& host, int puerto, const QString& codigo)
{
    if (sincronizando_) return;
    sincronizando_ = true;
    sincEnSilencio_ = false;
    estadoSinc_ = QStringLiteral("Emparejando…");
    emit sincCambio();
    cliente_->emparejar(host, quint16(puerto > 0 ? puerto : sinc::PUERTO), codigo);
}

void Controlador::sincronizar()
{
    if (sincronizando_) return;
    const QList<Par> p = stock_.pares();
    if (p.isEmpty()) {
        emit aviso(QStringLiteral("Primero emparejá el celular con la PC (menú → Sincronizar con la PC)"));
        return;
    }
    temporizadorSinc_.stop();
    sincronizando_ = true;
    estadoSinc_ = QStringLiteral("Conectando…");
    emit sincCambio();
    cliente_->sincronizar(p.first().dispositivo);
}

void Controlador::sincronizarSiCorresponde()
{
    if (sincronizando_ || !pcEmparejada()) return;
    const QDateTime ahora = QDateTime::currentDateTime();
    if (ultimoIntentoAuto_.isValid() && ultimoIntentoAuto_.secsTo(ahora) < 60) return;   // no más de una vez por minuto
    ultimoIntentoAuto_ = ahora;
    sincEnSilencio_ = true;
    sincronizar();
}

void Controlador::programarSincronizacion()
{
    if (pcEmparejada()) temporizadorSinc_.start(4000);
}

void Controlador::buscarPcs() { cliente_->buscar(2500); }

void Controlador::olvidarPc()
{
    for (const Par& p : stock_.pares()) stock_.eliminarPar(p.dispositivo);
    estadoSinc_.clear();
    emit sincCambio();
}

// ---------------------------------------------------------------- reportes y avisos

QVariantList Controlador::alertas() const
{
    QVariantList lista;
    for (const Alerta& a : stock_.alertas())
        lista << QVariantMap{{"id", a.productoId}, {"nombre", a.nombre}, {"detalle", a.detalle},
                             {"grave", a.tipo != Alerta::Tipo::PorVencer}};
    return lista;
}

QString Controlador::resumenAlertas() const
{
    int poco = 0, vencidos = 0, porVencer = 0;
    for (const Alerta& a : stock_.alertas()) {
        if (a.tipo == Alerta::Tipo::StockBajo) ++poco;
        else if (a.tipo == Alerta::Tipo::Vencido) ++vencidos;
        else ++porVencer;
    }
    QStringList partes;
    if (poco) partes << QStringLiteral("%1 con poco stock").arg(poco);
    if (vencidos) partes << (vencidos == 1 ? QStringLiteral("1 vencido") : QStringLiteral("%1 vencidos").arg(vencidos));
    if (porVencer) partes << QStringLiteral("%1 por vencer").arg(porVencer);
    return partes.join(QStringLiteral(" · "));
}

QVariantMap Controlador::reporte(int periodo) const
{
    const QDate hoy = QDate::currentDate();
    QDate desde = hoy, hasta = hoy;
    switch (periodo) {
    case 1: desde = hoy.addDays(-6); break;
    case 2: desde = QDate(hoy.year(), hoy.month(), 1); break;
    case 3: desde = QDate(hoy.year(), hoy.month(), 1).addMonths(-1); hasta = desde.addMonths(1).addDays(-1); break;
    case 4: desde = QDate(hoy.year(), 1, 1); break;
    default: break;
    }
    const Reporte r = stock_.reporte(desde, hasta);
    QVariantList masVendidos;
    for (const ProductoVendido& p : r.masVendidos)
        masVendidos << QVariantMap{{"nombre", p.nombre}, {"cantidad", p.cantidad}, {"importe", formatearPrecio(p.importe)}};
    QVariantList meses;
    for (const auto& [mes, importe] : r.ventasPorMes) {
        const QDate d = QDate::fromString(mes + QStringLiteral("-01"), Qt::ISODate);
        QString nombre = d.isValid() ? QLocale().standaloneMonthName(d.month()) + u' ' + QString::number(d.year()) : mes;
        if (!nombre.isEmpty()) nombre[0] = nombre[0].toUpper();
        meses << QVariantMap{{"mes", nombre}, {"importe", formatearPrecio(importe)}};
    }
    QString nota;
    if (r.unidadesSinCosto > 0)
        nota = QStringLiteral("%1 unidad(es) vendida(s) sin costo no entran en la ganancia: cargá la «Ganancia %» del producto o el costo al registrar las compras.")
                   .arg(r.unidadesSinCosto);
    return QVariantMap{
        {"periodo", desde == hasta ? QLocale().toString(desde, QStringLiteral("dd/MM/yyyy"))
                                   : QLocale().toString(desde, QStringLiteral("dd/MM/yyyy")) + QStringLiteral(" al ")
                                         + QLocale().toString(hasta, QStringLiteral("dd/MM/yyyy"))},
        {"ventas", formatearPrecio(r.ventas)},
        {"detalleVentas", QStringLiteral("%1 venta(s), %2 unidades").arg(r.cantidadVentas).arg(r.unidadesVendidas)},
        {"compras", formatearPrecio(r.compras)},
        {"ganancia", r.ventasConCosto > 0 ? formatearPrecio(r.ganancia) : QStringLiteral("—")},
        {"nota", nota},
        {"masVendidos", masVendidos},
        {"meses", meses},
    };
}

// ---------------------------------------------------------------- código de barras

#ifdef Q_OS_ANDROID
namespace {
QPointer<Controlador> escaneando;

// Lo llama Escaner.java (en el hilo de Android): se pasa al hilo de la app
void codigoDesdeJava(JNIEnv*, jclass, jstring codigo, jstring error)
{
    const QString c = QJniObject(codigo).toString(), e = QJniObject(error).toString();
    QMetaObject::invokeMethod(qApp, [c, e] { if (escaneando) escaneando->recibirCodigo(c, e); }, Qt::QueuedConnection);
}

bool registrarMetodosNativos()
{
    static bool hecho = false;
    if (hecho) return true;
    const JNINativeMethod metodos[] = {
        {"resultado", "(Ljava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void*>(codigoDesdeJava)}};
    QJniEnvironment entorno;
    hecho = entorno.registerNativeMethods("ar/gestorstock/app/Escaner", metodos, 1);
    return hecho;
}
}
#endif

bool Controlador::puedeEscanear() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

void Controlador::escanearCodigo()
{
#ifdef Q_OS_ANDROID
    if (!registrarMetodosNativos()) {
        emit aviso(QStringLiteral("No se pudo abrir el lector de códigos."));
        return;
    }
    escaneando = this;
    QJniObject::callStaticMethod<void>("ar/gestorstock/app/Escaner", "escanear", "(Landroid/content/Context;)V",
                                       QNativeInterface::QAndroidApplication::context());
    QJniEnvironment().checkAndClearExceptions();
#else
    emit aviso(QStringLiteral("El lector de códigos de barras está solo en el celular."));
#endif
}

void Controlador::recibirCodigo(const QString& codigo, const QString& error)
{
    if (!error.isEmpty()) {
        emit aviso(QStringLiteral("No se pudo leer el código: %1").arg(error));
        return;
    }
    emit codigoEscaneado(codigo);
}
