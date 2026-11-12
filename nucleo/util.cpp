#include "util.h"
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <limits>

namespace {

// '1.500,50' | '1500,5' | '1.500' | '1500.5' (con o sin "$" y espacios). El signo solo se acepta si se pide.
bool parsearDecimal(const QString& texto, double* valor, bool permitirNegativo)
{
    QString t = texto;
    t.remove(QLatin1Char('$'));
    t.remove(QLatin1Char(' '));
    t.remove(QChar(0x00A0));
    t = t.trimmed();
    if (t.isEmpty()) return false;
    QString signo;
    if (t.startsWith(QLatin1Char('-')) || t.startsWith(QLatin1Char('+'))) {
        signo = t.left(1);
        t.remove(0, 1);
    }

    static const QRegularExpression miles(QStringLiteral(R"(^\d{1,3}(\.\d{3})+$)"));
    if (t.contains(QLatin1Char(',')) && t.contains(QLatin1Char('.'))) {
        t.remove(QLatin1Char('.'));                      // 1.500,50
        t.replace(QLatin1Char(','), QLatin1Char('.'));
    } else if (t.contains(QLatin1Char(','))) {
        t.replace(QLatin1Char(','), QLatin1Char('.'));   // 1500,5
    } else if (miles.match(t).hasMatch()) {
        t.remove(QLatin1Char('.'));                      // 1.500 -> 1500
    }
    static const QRegularExpression numero(QStringLiteral(R"(^(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$)"));
    if (!numero.match(t).hasMatch()) return false;
    bool ok = false;
    const double v = (signo + t).toDouble(&ok);
    if (!ok || !std::isfinite(v) || (v < 0 && !permitirNegativo)) return false;
    if (valor) *valor = v;
    return true;
}

// Separa los miles con punto y los decimales con coma: 1234567.5 -> "1.234.567,5"
QString agruparMiles(const QString& numeroPlano)
{
    QString parteEntera = numeroPlano, decimales;
    const int punto = numeroPlano.indexOf(QLatin1Char('.'));
    if (punto >= 0) {
        parteEntera = numeroPlano.left(punto);
        decimales = numeroPlano.mid(punto + 1);
    }
    const bool negativo = parteEntera.startsWith(QLatin1Char('-'));
    if (negativo) parteEntera.remove(0, 1);
    QString agrupada;
    for (int i = 0; i < parteEntera.size(); ++i) {
        if (i > 0 && (parteEntera.size() - i) % 3 == 0) agrupada += QLatin1Char('.');
        agrupada += parteEntera[i];
    }
    return (negativo ? QStringLiteral("-") : QString()) + agrupada + (decimales.isEmpty() ? QString() : QStringLiteral(",") + decimales);
}

// Fechas como las escribe una persona (o las guarda la base): devuelve una fecha inválida si no se entiende
QDate fechaDesdeTexto(const QString& texto)
{
    const QString t = texto.trimmed();
    for (const QString& formato : {QStringLiteral("yyyy-MM-dd"), QStringLiteral("dd/MM/yyyy"), QStringLiteral("d/M/yyyy"),
                                   QStringLiteral("dd-MM-yyyy"), QStringLiteral("d-M-yyyy"), QStringLiteral("dd/MM/yy"),
                                   QStringLiteral("d/M/yy")}) {
        QDate f = QDate::fromString(t, formato);
        if (!f.isValid()) continue;
        if (formato.endsWith(QLatin1String("/yy"))) f = f.addYears(100);   // Qt toma "26" como 1926
        if (f.year() < 1000) continue;
        return f;
    }
    return QDate();
}

} // namespace

bool parsearPrecio(const QString& texto, double* valor)
{
    return parsearDecimal(texto, valor, false);
}

bool parsearNumero(const QString& texto, double* valor)
{
    return parsearDecimal(texto, valor, true);
}

QString formatearPrecio(double valor)
{
    const bool entero = std::floor(valor) == valor;
    return QStringLiteral("$ ") + agruparMiles(QString::number(valor, 'f', entero ? 0 : 2));
}

QString formatearNumero(double valor)
{
    QString s = QString::number(valor, 'f', 6);
    if (s.contains(QLatin1Char('.'))) {                  // sin ceros de más: 12.500000 -> 12.5 | 12.000000 -> 12
        while (s.endsWith(QLatin1Char('0'))) s.chop(1);
        if (s.endsWith(QLatin1Char('.'))) s.chop(1);
    }
    if (s == QLatin1String("-0")) s = QStringLiteral("0");
    return agruparMiles(s);
}

bool normalizarValorColumna(TipoColumna tipo, const QString& texto, QString* guardado)
{
    const QString t = texto.trimmed();
    QString salida;
    if (!t.isEmpty()) {
        switch (tipo) {
        case TipoColumna::Numero: {
            double v = 0;
            if (!parsearNumero(t, &v)) return false;
            salida = QString::number(v, 'g', 15);
            break;
        }
        case TipoColumna::Entero: {
            double v = 0;
            if (!parsearNumero(t, &v) || std::floor(v) != v) return false;
            salida = QString::number(qlonglong(v));
            break;
        }
        case TipoColumna::Fecha: {
            const QDate f = fechaDesdeTexto(t);
            if (!f.isValid()) return false;
            salida = f.toString(Qt::ISODate);
            break;
        }
        case TipoColumna::Texto:
        case TipoColumna::Precios:
            salida = t;
            break;
        }
    }
    if (guardado) *guardado = salida;
    return true;
}

QString textoValorColumna(TipoColumna tipo, const QString& guardado)
{
    if (guardado.isEmpty()) return QString();
    switch (tipo) {
    case TipoColumna::Numero:
    case TipoColumna::Entero: {
        bool ok = false;
        const double v = guardado.toDouble(&ok);      // lo guardado siempre usa el punto como decimal
        return ok ? formatearNumero(v) : guardado;
    }
    case TipoColumna::Fecha: {
        const QDate f = QDate::fromString(guardado, Qt::ISODate);
        return f.isValid() ? f.toString(QStringLiteral("dd/MM/yyyy")) : guardado;
    }
    case TipoColumna::Texto:
    case TipoColumna::Precios:
        break;
    }
    return guardado;
}

QVariant valorOrdenColumna(TipoColumna tipo, const QString& guardado)
{
    switch (tipo) {
    case TipoColumna::Numero:
    case TipoColumna::Entero: {
        bool ok = false;
        const double v = guardado.toDouble(&ok);
        return ok ? v : std::numeric_limits<double>::max();           // sin valor: al final
    }
    case TipoColumna::Fecha:
        return QDate::fromString(guardado, Qt::ISODate).isValid() ? guardado : QStringLiteral("9999-12-31");
    case TipoColumna::Texto:
    case TipoColumna::Precios:
        break;
    }
    return guardado.toLower();
}

QString textoPrecios(const QList<Precio>& precios)
{
    QStringList partes;
    for (const Precio& p : precios) {
        partes << (p.presentacion.isEmpty() ? formatearPrecio(p.precio)
                                            : p.presentacion + QStringLiteral(": ") + formatearPrecio(p.precio));
    }
    return partes.join(QStringLiteral(" · "));
}

std::optional<int> diasParaVencer(const QString& vencimiento, const QDate& hoy)
{
    if (vencimiento.isEmpty()) return std::nullopt;
    const QDate fecha = QDate::fromString(vencimiento, Qt::ISODate);
    if (!fecha.isValid()) return std::nullopt;
    return int(hoy.daysTo(fecha));
}

EstadoVencimiento estadoVencimiento(const QString& vencimiento, const QDate& hoy)
{
    const auto dias = diasParaVencer(vencimiento, hoy);
    if (!dias) return EstadoVencimiento::Ninguno;
    if (*dias < 0) return EstadoVencimiento::Vencido;
    if (*dias <= DIAS_AVISO_VENCIMIENTO) return EstadoVencimiento::PorVencer;
    return EstadoVencimiento::Ok;
}

// ------------------------------------------------------------------ búsqueda

QString normalizarBusqueda(const QString& texto)
{
    const QString descompuesto = texto.normalized(QString::NormalizationForm_D);
    QString salida;
    salida.reserve(descompuesto.size());
    for (const QChar c : descompuesto) {
        if (c.category() != QChar::Mark_NonSpacing) salida += c;   // saca las tildes sueltas
    }
    return salida.toCaseFolded();
}

bool FiltroPrecio::coincide(double precio) const
{
    const bool okMin = minimoInclusivo ? precio >= minimo : precio > minimo;
    const bool okMax = maximoInclusivo ? precio <= maximo : precio < maximo;
    return okMin && okMax;
}

std::optional<FiltroPrecio> interpretarFiltroPrecio(const QString& token)
{
    constexpr double infinito = std::numeric_limits<double>::infinity();
    double a = 0, b = 0;
    FiltroPrecio f;

    static const QRegularExpression comparacion(QStringLiteral(R"(^(<=|>=|<|>)(.+)$)"));
    const auto c = comparacion.match(token);
    if (c.hasMatch()) {
        if (!parsearPrecio(c.captured(2), &a)) return std::nullopt;
        f.soloPrecio = true;
        f.minimo = -infinito;
        f.maximo = infinito;
        const QString op = c.captured(1);
        if (op == QLatin1String("<"))  { f.maximo = a; f.maximoInclusivo = false; }
        if (op == QLatin1String("<=")) { f.maximo = a; }
        if (op == QLatin1String(">"))  { f.minimo = a; f.minimoInclusivo = false; }
        if (op == QLatin1String(">=")) { f.minimo = a; }
        return f;
    }

    static const QRegularExpression rango(QStringLiteral(R"(^([^-]+)-([^-]+)$)"));
    const auto r = rango.match(token);
    if (r.hasMatch() && parsearPrecio(r.captured(1), &a) && parsearPrecio(r.captured(2), &b)) {
        f.soloPrecio = true;
        f.minimo = std::min(a, b);
        f.maximo = std::max(a, b);
        return f;
    }

    if (parsearPrecio(token, &a)) {       // número suelto: ese precio (con medio centavo de tolerancia)
        f.minimo = a - 0.005;
        f.maximo = a + 0.005;
        return f;
    }
    return std::nullopt;
}

bool productoCoincide(const Producto& p, const QString& consulta)
{
    QString q = consulta;
    static const QRegularExpression guion(QStringLiteral(R"((\d)\s*-\s*(\d))"));
    static const QRegularExpression comparador(QStringLiteral(R"((<=|>=|<|>)\s+)"));
    static const QRegularExpression espacios(QStringLiteral(R"(\s+)"));
    q.replace(guion, QStringLiteral("\\1-\\2"));            // "2000 - 5000" -> "2000-5000"
    q.replace(comparador, QStringLiteral("\\1"));           // "< 3000" -> "<3000"
    q.replace(QStringLiteral("$ "), QStringLiteral("$"));    // "$ 4000" -> "$4000"
    QStringList palabras = q.split(espacios, Qt::SkipEmptyParts);
    palabras.removeAll(QStringLiteral("$"));                 // un "$" suelto no es una palabra a buscar
    if (palabras.isEmpty()) return true;

    QString propias;                                          // lo cargado en las columnas propias
    for (const QString& v : p.extra) propias += QLatin1Char(' ') + v;
    const QString textoPropias = normalizarBusqueda(propias);
    const QString texto = normalizarBusqueda(p.nombre + QLatin1Char(' ') + p.codigo + QLatin1Char(' ')
                                             + p.categoria + QLatin1Char(' ') + p.proveedor) + textoPropias;
    for (const QString& palabra : palabras) {
        const auto filtro = interpretarFiltroPrecio(palabra);
        bool coincide = false;
        if (!filtro || !filtro->soloPrecio) {
            coincide = texto.contains(normalizarBusqueda(palabra));
            double numero = 0;
            // Un número escrito como se acostumbra («1.234,5») se busca también como se guarda en las columnas propias («1234.5»);
            // solo ahí: en el resto del texto (códigos, nombres) un «4.000» no tiene por qué encontrar «M-4000»
            if (!coincide && parsearPrecio(palabra, &numero)) coincide = textoPropias.contains(QString::number(numero, 'g', 15));
        }
        if (!coincide && filtro) {
            for (const Precio& precio : p.precios) {
                if (filtro->coincide(precio.precio)) { coincide = true; break; }
            }
        }
        if (!coincide) return false;
    }
    return true;
}

QList<Producto> filtrarProductos(const QList<Producto>& todos, const QString& consulta)
{
    if (consulta.trimmed().isEmpty()) return todos;
    QList<Producto> salida;
    for (const Producto& p : todos) {
        if (productoCoincide(p, consulta)) salida.append(p);
    }
    return salida;
}
