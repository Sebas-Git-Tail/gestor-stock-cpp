#include "portapapeles.h"
#include "util.h"

#include <QDate>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {

enum Campo { Codigo, Nombre, Precios, Categoria, Proveedor, Cantidad, Medida, Vencimiento, StockMin, Notas, Ninguno };

const Campo ORDEN_TABLA[] = {Codigo, Nombre, Precios, Categoria, Proveedor, Cantidad, Medida, Vencimiento, StockMin};

// Reconoce el título de una columna (sin importar mayúsculas, tildes ni puntos)
Campo campoDeTitulo(const QString& titulo)
{
    const QString t = normalizarBusqueda(titulo).remove(QLatin1Char('.')).simplified();
    if (t.isEmpty()) return Ninguno;
    if (t.startsWith(QLatin1String("cod"))) return Codigo;
    if (t.startsWith(QLatin1String("nombre")) || t.startsWith(QLatin1String("producto"))) return Nombre;
    if (t.startsWith(QLatin1String("precio"))) return Precios;
    if (t.startsWith(QLatin1String("categ"))) return Categoria;
    if (t.startsWith(QLatin1String("proveedor"))) return Proveedor;
    if (t.startsWith(QLatin1String("stock"))) return StockMin;            // "Stock mín." / "Stock mínimo"
    if (t.startsWith(QLatin1String("cantidad"))) return Cantidad;
    if (t.startsWith(QLatin1String("medida"))) return Medida;
    if (t.startsWith(QLatin1String("venc")) || t.startsWith(QLatin1String("vto"))) return Vencimiento;
    if (t.startsWith(QLatin1String("nota"))) return Notas;
    return Ninguno;
}

// Una celda no puede tener tabulaciones ni saltos de línea (romperían las columnas al pegar)
QString celda(const QString& s)
{
    QString c = s;
    c.replace(QLatin1Char('\t'), QLatin1Char(' '));
    c.replace(QLatin1Char('\r'), QLatin1Char(' '));
    c.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return c;
}

QString vencimientoDesdeTexto(const QString& texto)
{
    const QString t = texto.trimmed();
    for (const QString& formato : {QStringLiteral("dd/MM/yyyy"), QStringLiteral("d/M/yyyy"), QStringLiteral("yyyy-MM-dd"),
                                   QStringLiteral("dd/MM/yy"), QStringLiteral("d/M/yy")}) {
        QDate f = QDate::fromString(t, formato);
        if (f.isValid()) {
            if (formato.endsWith(QLatin1String("/yy"))) f = f.addYears(100);   // Qt toma "26" como 1926
            return f.toString(Qt::ISODate);
        }
    }
    return QString();
}

int numeroDesdeTexto(const QString& texto)
{
    bool ok = false;
    const int n = texto.trimmed().toInt(&ok);
    return ok && n > 0 ? n : 0;
}

} // namespace

QStringList columnasPortapapeles(const QList<Columna>& propias)
{
    QStringList l = {QStringLiteral("Código"), QStringLiteral("Nombre"), QStringLiteral("Precios"), QStringLiteral("Categoría"),
                     QStringLiteral("Proveedor"), QStringLiteral("Cantidad"), QStringLiteral("Medida bolsa"),
                     QStringLiteral("Vencimiento"), QStringLiteral("Stock mín.")};
    for (const Columna& c : propias) l << c.nombre;
    return l;
}

// ====================================================================== copiar

QString productosATexto(const QList<Producto>& productos, const QList<Columna>& propias)
{
    QStringList filas;
    filas << columnasPortapapeles(propias).join(QLatin1Char('\t'));
    for (const Producto& p : productos) {
        const QString venc = p.vencimiento.isEmpty()
                                 ? QString()
                                 : QDate::fromString(p.vencimiento, Qt::ISODate).toString(QStringLiteral("dd/MM/yyyy"));
        QStringList celdas = {celda(p.codigo), celda(p.nombre), celda(textoPrecios(p.precios)), celda(p.categoria),
                              celda(p.proveedor), QString::number(p.cantidad), celda(p.medidaBolsa), venc,
                              QString::number(p.stockMinimo)};
        for (const Columna& c : propias) celdas << celda(textoValorColumna(c.tipo, p.extra.value(c.uuid)));
        filas << celdas.join(QLatin1Char('\t'));
    }
    return filas.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QByteArray productosAJson(const QList<Producto>& productos, const QList<Columna>& propias)
{
    QJsonArray lista;
    for (const Producto& p : productos) {
        QJsonArray precios;
        for (const Precio& pr : p.precios) precios.append(QJsonObject{{"presentacion", pr.presentacion}, {"precio", pr.precio}});
        QJsonObject extra;                                   // por nombre de columna (no por uuid)
        for (const Columna& c : propias)
            if (p.extra.contains(c.uuid)) extra.insert(c.nombre, p.extra.value(c.uuid));
        lista.append(QJsonObject{
            {"codigo", p.codigo}, {"nombre", p.nombre}, {"categoria", p.categoria}, {"cantidad", p.cantidad},
            {"stockMinimo", p.stockMinimo}, {"proveedor", p.proveedor}, {"vencimiento", p.vencimiento},
            {"medidaBolsa", p.medidaBolsa}, {"notas", p.notas}, {"precios", precios}, {"extra", extra}});
    }
    return QJsonDocument(lista).toJson(QJsonDocument::Compact);
}

QList<Producto> productosDesdeJson(const QByteArray& datos, const QList<Columna>& propias)
{
    QList<Producto> productos;
    const QJsonDocument doc = QJsonDocument::fromJson(datos);
    for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        Producto p;
        p.codigo = o.value("codigo").toString();
        p.nombre = o.value("nombre").toString().trimmed();
        p.categoria = o.value("categoria").toString();
        p.cantidad = o.value("cantidad").toInt();
        p.stockMinimo = o.value("stockMinimo").toInt();
        p.proveedor = o.value("proveedor").toString();
        p.vencimiento = o.value("vencimiento").toString();
        p.medidaBolsa = o.value("medidaBolsa").toString();
        p.notas = o.value("notas").toString();
        for (const QJsonValue& pr : o.value("precios").toArray())
            p.precios.append({pr.toObject().value("presentacion").toString(), pr.toObject().value("precio").toDouble()});
        const QJsonObject extra = o.value("extra").toObject();
        for (const Columna& c : propias) {                    // las que esta base también tiene (se buscan por nombre)
            QString guardado;
            for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
                if (normalizarBusqueda(it.key()) != normalizarBusqueda(c.nombre)) continue;
                if (normalizarValorColumna(c.tipo, it.value().toString(), &guardado) && !guardado.isEmpty()) p.extra.insert(c.uuid, guardado);
            }
        }
        if (!p.nombre.isEmpty()) productos.append(p);
    }
    return productos;
}

// ====================================================================== pegar

QList<QStringList> partirTabulado(const QString& texto)
{
    QList<QStringList> filas;
    QStringList fila;
    QString actual;
    bool entreComillas = false, empezoCelda = true;
    const int n = texto.size();
    for (int i = 0; i < n; ++i) {
        const QChar c = texto[i];
        if (entreComillas) {
            if (c == QLatin1Char('"')) {
                if (i + 1 < n && texto[i + 1] == QLatin1Char('"')) { actual += QLatin1Char('"'); ++i; }   // "" = una comilla
                else entreComillas = false;
            } else {
                actual += c;
            }
            continue;
        }
        if (c == QLatin1Char('"') && empezoCelda) {          // Excel pone entre comillas las celdas con saltos de línea
            entreComillas = true;
            empezoCelda = false;
        } else if (c == QLatin1Char('\t')) {
            fila << actual;
            actual.clear();
            empezoCelda = true;
        } else if (c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            if (c == QLatin1Char('\r') && i + 1 < n && texto[i + 1] == QLatin1Char('\n')) ++i;
            fila << actual;
            filas << fila;
            fila.clear();
            actual.clear();
            empezoCelda = true;
        } else {
            actual += c;
            empezoCelda = false;
        }
    }
    if (!actual.isEmpty() || !fila.isEmpty()) {
        fila << actual;
        filas << fila;
    }
    return filas;
}

QList<Precio> preciosDesdeTexto(const QString& texto)
{
    QList<Precio> precios;
    static const QRegularExpression separador(QStringLiteral(R"([·|;])"));
    for (const QString& parte : texto.split(separador, Qt::SkipEmptyParts)) {
        const QString p = parte.trimmed();
        if (p.isEmpty()) continue;
        const int dosPuntos = p.lastIndexOf(QLatin1Char(':'));
        const QString presentacion = dosPuntos >= 0 ? p.left(dosPuntos).trimmed() : QString();
        double valor = 0;
        if (parsearPrecio(dosPuntos >= 0 ? p.mid(dosPuntos + 1) : p, &valor)) precios.append({presentacion, valor});
    }
    return precios;
}

LecturaPortapapeles productosDesdeTexto(const QString& texto, const QList<Columna>& propias)
{
    LecturaPortapapeles res;
    QList<QStringList> filas = partirTabulado(texto);
    while (!filas.isEmpty() && filas.last().join(QString()).trimmed().isEmpty()) filas.removeLast();
    if (filas.isEmpty()) return res;

    // ¿La primera fila son títulos? (al menos dos columnas reconocidas, una de ellas el nombre).
    // Los títulos que coinciden con el nombre de una columna propia mandan sobre los del programa.
    QList<Campo> campos;
    QList<int> propiaDe;                      // de cada columna del texto: cuál columna propia es (o -1)
    int desde = 0;
    {
        QList<Campo> titulos;
        QList<int> propiasDeTitulos;
        int reconocidos = 0;
        bool hayNombre = false;
        for (const QString& t : filas.first()) {
            int k = -1;
            for (int i = 0; i < propias.size(); ++i)
                if (normalizarBusqueda(propias[i].nombre.simplified()) == normalizarBusqueda(t.simplified())) k = i;
            const Campo c = k >= 0 ? Ninguno : campoDeTitulo(t);
            titulos << c;
            propiasDeTitulos << k;
            if (c != Ninguno || k >= 0) ++reconocidos;
            if (c == Nombre) hayNombre = true;
        }
        if (hayNombre && reconocidos >= 2) {
            campos = titulos;
            propiaDe = propiasDeTitulos;
            desde = 1;
        }
    }
    if (campos.isEmpty()) {
        // Sin títulos: si es una sola columna, son nombres; si no, el orden de la tabla del programa
        if (filas.first().size() == 1) campos = {Nombre};
        else for (Campo c : ORDEN_TABLA) campos << c;
        propiaDe = QList<int>(campos.size(), -1);
    }

    for (int i = desde; i < filas.size(); ++i) {
        const QStringList& f = filas[i];
        if (f.join(QString()).trimmed().isEmpty()) continue;           // renglón vacío
        Producto p;
        for (int j = 0; j < f.size() && j < campos.size(); ++j) {
            const QString v = f[j].trimmed();
            if (propiaDe[j] >= 0) {               // una columna propia: se interpreta según su tipo
                const Columna& c = propias[propiaDe[j]];
                QString guardado;
                if (normalizarValorColumna(c.tipo, v, &guardado)) { if (!guardado.isEmpty()) p.extra.insert(c.uuid, guardado); }
                else res.avisos << QStringLiteral("Renglón %1: «%2» no es un valor válido para «%3», se dejó vacío.").arg(i + 1).arg(v, c.nombre);
                continue;
            }
            switch (campos[j]) {
            case Codigo: p.codigo = v; break;
            case Nombre: p.nombre = v; break;
            case Precios: p.precios = preciosDesdeTexto(v); break;
            case Categoria: p.categoria = v; break;
            case Proveedor: p.proveedor = v; break;
            case Cantidad: p.cantidad = numeroDesdeTexto(v); break;
            case Medida: p.medidaBolsa = v; break;
            case Vencimiento: p.vencimiento = vencimientoDesdeTexto(v); break;
            case StockMin: p.stockMinimo = numeroDesdeTexto(v); break;
            case Notas: p.notas = v; break;
            case Ninguno: break;
            }
        }
        if (p.nombre.isEmpty()) {
            res.avisos << QStringLiteral("Renglón %1: no tiene nombre, no se pegó.").arg(i + 1);
            continue;
        }
        res.productos << p;
    }
    return res;
}
