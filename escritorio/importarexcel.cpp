#include "importarexcel.h"
#include "util.h"

#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <optional>
#include <vector>

namespace {

constexpr int LARGO_MAX_NOMBRE = 90;   // una fila sin datos con más texto que esto es un párrafo explicativo
constexpr int MAX_COLS = 10;

struct Marcador { const char* texto; int nivel; };
// prefijos que la planilla usa para "hijo de..."
const Marcador MARCADORES[] = {{"¬¬", 1}, {"**", 2}, {"#", 1}};

const char* const ETIQUETAS_POR_DEFECTO[3] = {"", "Precio 2", "Precio 3"};

const QHash<QString, QString>& unidades()
{
    static const QHash<QString, QString> u = {
        {"g", "g"}, {"gr", "g"}, {"grs", "g"}, {"gramos", "g"}, {"kg", "kg"},
        {"cc", "cc"}, {"ml", "ml"}, {"l", "l"}, {"lt", "l"}};
    return u;
}

QString limpiar(const QString& s) { return s.simplified(); }

// str(valor) de Python, para los números que vienen de la planilla
QString textoNumero(double v)
{
    if (v == double(qlonglong(v)) && std::abs(v) < 1e15) return QString::number(qlonglong(v));
    return QString::number(v, 'g', QLocale::FloatingPointShortest);
}

QString textoDeCelda(const Celda& c)
{
    switch (c.tipo) {
    case Celda::Vacia: return QString();
    case Celda::Texto: return c.texto;
    case Celda::Numero: return textoNumero(c.numero);
    case Celda::Fecha: return c.fecha.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    case Celda::Logico: return c.logico ? QStringLiteral("True") : QStringLiteral("False");
    }
    return QString();
}

// el texto en blanco cuenta como vacío
Celda limpiarCelda(Celda c)
{
    if (c.tipo == Celda::Texto) {
        c.texto = c.texto.trimmed();
        if (c.texto.isEmpty()) c.tipo = Celda::Vacia;
    }
    return c;
}

std::pair<int, QString> separarMarcador(const QString& texto)
{
    const QString t = texto.trimmed();
    for (const Marcador& m : MARCADORES) {
        const QString prefijo = QString::fromUtf8(m.texto);
        if (t.startsWith(prefijo)) return {m.nivel, limpiar(t.mid(prefijo.size()))};
    }
    return {0, limpiar(t)};
}

struct Columnas {
    int nombre = 0;
    int precios[3] = {1, 2, 3};
    int proveedor = 4, vto = 5, medida = 6;
};

Celda celdaEn(const FilaXlsx& f, int i)
{
    return (i >= 0 && i < f.size()) ? limpiarCelda(f[i]) : Celda();
}

struct FilaPlanilla {
    int n = 0;
    QString crudo, nombre, proveedor;
    int nivel = 0;
    Celda precios[3];
    Celda vto, medida;

    FilaPlanilla(int numero, const FilaXlsx& celdas, const Columnas& cols)
        : n(numero)
    {
        crudo = textoDeCelda(celdaEn(celdas, cols.nombre));
        const auto [niv, nom] = separarMarcador(crudo);
        nivel = niv;
        nombre = nom;
        for (int j = 0; j < 3; ++j) precios[j] = celdaEn(celdas, cols.precios[j]);
        proveedor = limpiar(textoDeCelda(celdaEn(celdas, cols.proveedor)));
        vto = celdaEn(celdas, cols.vto);
        medida = celdaEn(celdas, cols.medida);
    }

    bool vacia() const
    {
        if (!crudo.isEmpty() || !proveedor.isEmpty() || vto.tipo != Celda::Vacia || medida.tipo != Celda::Vacia) return false;
        for (const Celda& p : precios) if (p.tipo != Celda::Vacia) return false;
        return true;
    }
    bool tienePrecio() const
    {
        for (const Celda& p : precios) if (p.tipo == Celda::Numero) return true;
        return false;
    }
    // hay algo en precios, vencimiento o medida (el proveedor solo no cuenta)
    bool tieneDatos() const
    {
        for (const Celda& p : precios) if (p.tipo != Celda::Vacia) return true;
        return vto.tipo != Celda::Vacia || medida.tipo != Celda::Vacia;
    }
    bool puedeSerCategoria() const
    {
        return !tienePrecio() && vto.tipo == Celda::Vacia && medida.tipo == Celda::Vacia;
    }
};

// Busca la fila de encabezados (Productos / Precios / Proveedor / Vto / Medida)
std::pair<Columnas, int> detectarColumnas(const QVector<FilaXlsx>& filas)
{
    Columnas cols;
    for (int i = 0; i < qMin(10, int(filas.size())); ++i) {
        QStringList textos;
        for (const Celda& c : filas[i]) textos << (c.tipo == Celda::Texto ? c.texto.trimmed().toLower() : QString());
        bool hay = false;
        for (const QString& t : textos) if (t.startsWith(QLatin1String("producto"))) hay = true;
        if (!hay) continue;

        QHash<QString, int> idx;
        for (int j = 0; j < textos.size(); ++j) {
            const QString& t = textos[j];
            if (t.startsWith(QLatin1String("producto"))) idx["nombre"] = j;
            else if (t.startsWith(QLatin1String("precio"))) idx["precios"] = j;
            else if (t.startsWith(QLatin1String("proveedor"))) idx["proveedor"] = j;
            else if (t.startsWith(QLatin1String("vto")) || t.startsWith(QLatin1String("venc"))) idx["vto"] = j;
            else if (t.startsWith(QLatin1String("medida"))) idx["medida"] = j;
        }
        cols.nombre = idx.value("nombre", 0);
        cols.proveedor = idx.value("proveedor", cols.proveedor);
        cols.vto = idx.value("vto", cols.vto);
        cols.medida = idx.value("medida", cols.medida);
        if (idx.contains("precios")) {
            const int p = idx["precios"];
            int hasta = p + 3;
            for (int otra : {cols.proveedor, cols.vto, cols.medida}) if (otra > p) hasta = qMin(hasta, otra);
            for (int j = 0; j < 3; ++j) cols.precios[j] = (p + j < hasta) ? p + j : -1;   // tres columnas de precio como máximo
        }
        return {cols, i + 1};
    }
    return {cols, 0};
}

} // namespace

// ====================================================================== utilidades de texto

QString normalizarEtiqueta(const QString& texto)
{
    const QString t = limpiar(texto);
    const QString bajo = t.toLower();
    static const QRegularExpression porUnidad(QRegularExpression::anchoredPattern(QStringLiteral(R"((x|por)\s*unidad)")));
    if (porUnidad.match(bajo).hasMatch()) return QStringLiteral("unidad");

    static const QRegularExpression etiqueta(QRegularExpression::anchoredPattern(
        QStringLiteral(R"(x?\s*(\d+(?:[.,]\d+)?(?:/\d+(?:[.,]\d+)?)*)\s*([a-z]+)?\.?)")));
    const auto m = etiqueta.match(bajo);
    if (m.hasMatch() && (m.captured(2).isEmpty() || unidades().contains(m.captured(2)))) {
        const QString unidad = unidades().value(m.captured(2));
        return (m.captured(1) + QLatin1Char(' ') + unidad).trimmed();
    }
    return t;
}

QString normalizarMedida(const QString& texto)
{
    static const QRegularExpression equis(QStringLiteral(R"((?<=\d)X(?=\d))"));
    QString t = limpiar(texto);
    t.replace(equis, QStringLiteral("x"));
    return t;
}

VencimientoLeido parsearVencimiento(const Celda& valor)
{
    if (valor.tipo == Celda::Vacia) return {};
    if (valor.tipo == Celda::Fecha) return {valor.fecha.date().toString(Qt::ISODate), QString()};
    if (valor.tipo == Celda::Numero) return {QString(), QStringLiteral("Dato en Vto: %1").arg(QString::number(valor.numero, 'g'))};
    if (valor.tipo == Celda::Logico) return {QString(), QStringLiteral("Dato en Vto: %1").arg(valor.logico ? 1 : 0)};

    const QString s = valor.texto.trimmed();
    static const QRegularExpression mesAnio(QRegularExpression::anchoredPattern(QStringLiteral(R"((\d{1,2})/(\d{2}|\d{4}))")));
    static const QRegularExpression fechaCompleta(QRegularExpression::anchoredPattern(QStringLiteral(R"((\d{1,2})/(\d{1,2})/(\d{2}|\d{4}))")));

    // MM/YY y MM/YYYY: el ÚLTIMO día de ese mes (así vencen los alimentos)
    auto m = mesAnio.match(s);
    if (m.hasMatch() && m.captured(1).toInt() >= 1 && m.captured(1).toInt() <= 12) {
        const int anio = m.captured(2).toInt() + (m.captured(2).size() == 2 ? 2000 : 0);
        const int mes = m.captured(1).toInt();
        const QDate primero(anio, mes, 1);
        if (primero.isValid()) return {QDate(anio, mes, primero.daysInMonth()).toString(Qt::ISODate), QString()};
    }
    m = fechaCompleta.match(s);
    if (m.hasMatch()) {
        const int anio = m.captured(3).toInt() + (m.captured(3).size() == 2 ? 2000 : 0);
        const QDate f(anio, m.captured(2).toInt(), m.captured(1).toInt());
        if (f.isValid()) return {f.toString(Qt::ISODate), QString()};
    }
    return {QString(), QStringLiteral("Vto: %1").arg(s)};
}

// ====================================================================== el recorrido de la planilla

LecturaPlanilla procesarFilas(const QVector<FilaXlsx>& filas)
{
    LecturaPlanilla res;
    const auto [cols, desde] = detectarColumnas(filas);

    std::vector<std::optional<FilaPlanilla>> reg;
    for (int i = desde; i < filas.size(); ++i) {
        FilaPlanilla f(i + 1, filas[i], cols);     // i + 1 = número de fila tal como se ve en Excel
        if (f.vacia()) reg.emplace_back(std::nullopt);
        else reg.emplace_back(std::move(f));
    }

    struct Grupo { QString nombre, proveedor; bool activo = false; };
    QList<QStringList> notasDe;                    // aclaraciones y datos que no se entendieron, por producto
    QString categoria, provCat;
    QString etiquetas[3] = {QString::fromUtf8(ETIQUETAS_POR_DEFECTO[0]), QString::fromUtf8(ETIQUETAS_POR_DEFECTO[1]),
                            QString::fromUtf8(ETIQUETAS_POR_DEFECTO[2])};
    bool nuevoBloque = true;
    Grupo g1, g2;
    int ultimo = -1;                               // último producto, para anexarle aclaraciones "(...)"

    for (size_t k = 0; k < reg.size(); ++k) {
        if (!reg[k]) {                             // fila en blanco
            nuevoBloque = true;
            g1 = Grupo();
            g2 = Grupo();
            ultimo = -1;
            continue;
        }
        const FilaPlanilla& f = *reg[k];
        const FilaPlanilla* siguiente = (k + 1 < reg.size() && reg[k + 1]) ? &*reg[k + 1] : nullptr;
        const bool sinDatos = !f.tieneDatos();

        if (f.nombre.isEmpty()) {
            res.avisos << QStringLiteral("Fila %1: tiene datos pero no tiene nombre, se omitió.").arg(f.n);
            continue;
        }
        if (sinDatos && f.crudo.size() > LARGO_MAX_NOMBRE) {
            res.avisos << QStringLiteral("Fila %1: párrafo de texto ignorado («%2…»).").arg(f.n).arg(f.nombre.left(35));
            continue;
        }
        if (sinDatos && f.crudo.startsWith(QLatin1Char('(')) && ultimo >= 0) {
            notasDe[ultimo] << f.nombre;           // aclaración del producto anterior
            continue;
        }

        if (nuevoBloque && f.puedeSerCategoria()) {    // encabezado de categoría
            categoria = f.nombre;
            provCat = f.proveedor;
            for (int j = 0; j < 3; ++j) {
                etiquetas[j] = (f.precios[j].tipo == Celda::Texto && !f.precios[j].texto.isEmpty())
                                   ? normalizarEtiqueta(f.precios[j].texto)
                                   : QString::fromUtf8(ETIQUETAS_POR_DEFECTO[j]);
            }
            nuevoBloque = false;
            ultimo = -1;
            continue;
        }
        nuevoBloque = false;

        if (sinDatos && siguiente) {               // ¿grupo o subgrupo?
            if (f.nivel == 0 && siguiente->nivel > 0) {
                g1 = {f.nombre, f.proveedor, true};
                g2 = Grupo();
                continue;
            }
            if (f.nivel > 0 && (siguiente->nivel > f.nivel || (siguiente->nivel == 0 && siguiente->tienePrecio()))) {
                g2 = {f.nombre, f.proveedor, true};
                continue;
            }
        }

        if (f.nivel == 0 && f.tieneDatos() && !g2.activo) g1 = Grupo();   // un producto suelto cierra el grupo

        QStringList prefijo;
        if (g1.activo && (f.nivel > 0 || g2.activo)) prefijo << g1.nombre;
        if (g2.activo) prefijo << g2.nombre;
        prefijo << f.nombre;
        const QString nombre = prefijo.join(QStringLiteral(" — "));
        QString proveedor = f.proveedor;
        if (proveedor.isEmpty() && g2.activo) proveedor = g2.proveedor;
        if (proveedor.isEmpty() && g1.activo) proveedor = g1.proveedor;
        if (proveedor.isEmpty()) proveedor = provCat;

        QStringList notas;
        QList<Precio> precios;
        for (int j = 0; j < 3; ++j) {
            const Celda& raw = f.precios[j];
            if (raw.tipo == Celda::Vacia) continue;
            const QString etiqueta = etiquetas[j];
            if (raw.tipo == Celda::Numero) {
                precios.append({etiqueta, raw.numero});
                continue;
            }
            const QString textoRaw = textoDeCelda(raw);
            double v = 0;
            if (raw.tipo == Celda::Texto && parsearPrecio(raw.texto, &v)) {
                precios.append({etiqueta, v});
            } else {
                notas << QStringLiteral("%1: %2").arg(etiqueta.isEmpty() ? QStringLiteral("Precio") : etiqueta, textoRaw);
                res.avisos << QStringLiteral("Fila %1 (%2): precio con texto «%3», se guardó en notas.")
                                  .arg(f.n).arg(f.nombre.left(30), textoRaw);
            }
        }

        const VencimientoLeido venc = parsearVencimiento(f.vto);
        if (!venc.nota.isEmpty()) {
            notas << venc.nota;
            res.avisos << QStringLiteral("Fila %1 (%2): vencimiento no interpretable «%3», se guardó en notas.")
                              .arg(f.n).arg(f.nombre.left(30), textoDeCelda(f.vto));
        }

        ProductoImportado p;
        p.nombre = nombre;
        p.categoria = categoria;
        p.proveedor = proveedor;
        p.vencimiento = venc.iso;
        p.medidaBolsa = f.medida.tipo != Celda::Vacia ? normalizarMedida(textoDeCelda(f.medida)) : QString();
        p.precios = precios;
        p.fila = f.n;
        res.productos.append(p);
        notasDe.append(notas);
        ultimo = int(res.productos.size()) - 1;
    }

    for (int i = 0; i < res.productos.size(); ++i) res.productos[i].notas = notasDe[i].join(QStringLiteral(" | "));
    res.ok = true;
    return res;
}

LecturaPlanilla leerPlanilla(const QString& ruta)
{
    const LecturaXlsx x = leerPrimeraHoja(ruta, MAX_COLS);
    if (!x.ok) {
        LecturaPlanilla r;
        r.error = x.error;
        return r;
    }
    return procesarFilas(x.filas);
}
