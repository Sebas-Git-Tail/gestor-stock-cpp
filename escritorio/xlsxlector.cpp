#include "xlsxlector.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <cmath>
#include <zlib.h>

namespace {

// ====================================================================== zip

quint16 le16(const char* p) { return quint16(quint8(p[0])) | quint16(quint8(p[1])) << 8; }
quint32 le32(const char* p)
{
    return quint32(quint8(p[0])) | quint32(quint8(p[1])) << 8 | quint32(quint8(p[2])) << 16 | quint32(quint8(p[3])) << 24;
}

class LectorZip
{
public:
    bool abrir(const QByteArray& datos, QString* error)
    {
        datos_ = datos;
        const int n = datos_.size();
        int eocd = -1;
        for (int i = n - 22; i >= 0 && i >= n - 22 - 65535; --i) {
            if (le32(datos_.constData() + i) == 0x06054b50) { eocd = i; break; }
        }
        if (eocd < 0) { *error = QStringLiteral("no es un archivo .xlsx válido (no es un zip)"); return false; }
        const char* e = datos_.constData() + eocd;
        const quint32 total = le16(e + 10), tamCd = le32(e + 12), offCd = le32(e + 16);
        if (total == 0xFFFF || offCd == 0xFFFFFFFFu) { *error = QStringLiteral("zip64 no soportado"); return false; }
        if (quint64(offCd) + tamCd > quint64(n)) { *error = QStringLiteral("zip dañado"); return false; }

        int pos = int(offCd);
        for (quint32 i = 0; i < total; ++i) {
            if (pos + 46 > n || le32(datos_.constData() + pos) != 0x02014b50) { *error = QStringLiteral("zip dañado"); return false; }
            const char* c = datos_.constData() + pos;
            const quint16 largoNombre = le16(c + 28), largoExtra = le16(c + 30), largoComentario = le16(c + 32);
            if (pos + 46 + largoNombre > n) { *error = QStringLiteral("zip dañado"); return false; }
            Entrada ent;
            ent.metodo = le16(c + 10);
            ent.comprimido = le32(c + 20);
            ent.descomprimido = le32(c + 24);
            ent.offsetLocal = le32(c + 42);
            entradas_.insert(QString::fromUtf8(c + 46, largoNombre), ent);
            pos += 46 + largoNombre + largoExtra + largoComentario;
        }
        return true;
    }

    bool contiene(const QString& nombre) const { return entradas_.contains(nombre); }

    QByteArray extraer(const QString& nombre, QString* error) const
    {
        const auto it = entradas_.constFind(nombre);
        if (it == entradas_.constEnd()) { *error = QStringLiteral("falta «%1» dentro del archivo").arg(nombre); return {}; }
        const Entrada& ent = *it;
        const int n = datos_.size();
        if (quint64(ent.offsetLocal) + 30 > quint64(n) || le32(datos_.constData() + ent.offsetLocal) != 0x04034b50) {
            *error = QStringLiteral("zip dañado"); return {};
        }
        const char* l = datos_.constData() + ent.offsetLocal;
        const quint64 inicio = quint64(ent.offsetLocal) + 30 + le16(l + 26) + le16(l + 28);
        if (inicio + ent.comprimido > quint64(n)) { *error = QStringLiteral("zip dañado"); return {}; }
        const char* origen = datos_.constData() + inicio;

        if (ent.metodo == 0) return QByteArray(origen, int(ent.comprimido));
        if (ent.metodo != 8) { *error = QStringLiteral("compresión no soportada"); return {}; }
        if (ent.descomprimido > 512u * 1024u * 1024u) { *error = QStringLiteral("una parte del archivo es demasiado grande"); return {}; }

        QByteArray salida(int(ent.descomprimido), Qt::Uninitialized);
        z_stream s{};
        if (inflateInit2(&s, -MAX_WBITS) != Z_OK) { *error = QStringLiteral("no se pudo iniciar zlib"); return {}; }
        s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(origen));
        s.avail_in = ent.comprimido;
        s.next_out = reinterpret_cast<Bytef*>(salida.data());
        s.avail_out = ent.descomprimido;
        const int r = inflate(&s, Z_FINISH);
        inflateEnd(&s);
        if (r != Z_STREAM_END) { *error = QStringLiteral("no se pudo descomprimir «%1»").arg(nombre); return {}; }
        return salida;
    }

private:
    struct Entrada { quint16 metodo = 0; quint32 comprimido = 0, descomprimido = 0, offsetLocal = 0; };
    QByteArray datos_;
    QHash<QString, Entrada> entradas_;
};

// ====================================================================== XML

const QString NS_REL = QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships");

QStringList leerCadenasCompartidas(const QByteArray& xml)
{
    QStringList salida;
    QXmlStreamReader r(xml);
    QString actual;
    bool dentroSi = false;
    int fonetico = 0;                 // el texto <rPh> es la pronunciación (furigana): no es parte de la celda
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QLatin1String("si")) { dentroSi = true; actual.clear(); }
            else if (r.name() == QLatin1String("rPh")) ++fonetico;
            else if (r.name() == QLatin1String("t") && dentroSi && fonetico == 0)
                actual += r.readElementText(QXmlStreamReader::SkipChildElements);
        } else if (r.isEndElement()) {
            if (r.name() == QLatin1String("rPh")) --fonetico;
            else if (r.name() == QLatin1String("si")) { salida << actual; dentroSi = false; }
        }
    }
    return salida;
}

bool esFormatoFecha(int id, const QHash<int, QString>& personalizados)
{
    if ((id >= 14 && id <= 22) || (id >= 45 && id <= 47)) return true;
    const auto it = personalizados.constFind(id);
    if (it == personalizados.constEnd()) return false;
    QString codigo = *it;
    static const QRegularExpression entreComillas(QStringLiteral(R"("[^"]*")"));
    static const QRegularExpression entreCorchetes(QStringLiteral(R"(\[[^\]]*\])"));
    static const QRegularExpression escapados(QStringLiteral(R"(\\.)"));
    static const QRegularExpression letrasDeFecha(QStringLiteral("[dmyhsDMYHS]"));
    codigo.remove(entreComillas);
    codigo.remove(entreCorchetes);
    codigo.remove(escapados);
    return codigo.contains(letrasDeFecha);
}

// índice de estilo -> ¿es un formato de fecha?
QVector<bool> leerEstilosFecha(const QByteArray& xml)
{
    QHash<int, QString> personalizados;
    QVector<int> formatosDeXf;
    bool dentroCellXfs = false;
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QLatin1String("numFmt"))
                personalizados.insert(r.attributes().value("numFmtId").toInt(), r.attributes().value("formatCode").toString());
            else if (r.name() == QLatin1String("cellXfs")) dentroCellXfs = true;
            else if (r.name() == QLatin1String("xf") && dentroCellXfs)
                formatosDeXf.append(r.attributes().value("numFmtId").toInt());
        } else if (r.isEndElement() && r.name() == QLatin1String("cellXfs")) {
            dentroCellXfs = false;
        }
    }
    QVector<bool> salida;
    for (int id : formatosDeXf) salida.append(esFormatoFecha(id, personalizados));
    return salida;
}

QDateTime desdeSerial(double v, bool epoca1904)
{
    const double dias = std::floor(v);
    const double fraccion = v - dias;
    const QDate base = epoca1904 ? QDate(1904, 1, 1) : (dias < 61 ? QDate(1899, 12, 31) : QDate(1899, 12, 30));
    const qint64 ms = qint64(std::llround(fraccion * 86400000.0));
    return QDateTime(base.addDays(int(dias)), QTime(0, 0).addMSecs(int(ms % 86400000)), Qt::UTC);
}

// "B12" -> columna 1, fila 11 (índices desde 0)
bool leerReferencia(const QString& ref, int* columna, int* fila)
{
    int i = 0, col = 0;
    while (i < ref.size() && ref[i].isLetter()) { col = col * 26 + (ref[i].toUpper().unicode() - 'A' + 1); ++i; }
    if (i == 0 || i == ref.size()) return false;
    bool ok = false;
    const int f = ref.mid(i).toInt(&ok);
    if (!ok || f < 1) return false;
    *columna = col - 1;
    *fila = f - 1;
    return true;
}

// Texto de un <is> (texto en línea): une los <t>, ignorando la pronunciación <rPh>
QString leerTextoEnLinea(QXmlStreamReader& r)
{
    QString salida;
    int fonetico = 0;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            if (r.name() == QLatin1String("rPh")) ++fonetico;
            else if (r.name() == QLatin1String("t") && fonetico == 0)
                salida += r.readElementText(QXmlStreamReader::SkipChildElements);
        } else if (r.isEndElement()) {
            if (r.name() == QLatin1String("rPh")) --fonetico;
            else if (r.name() == QLatin1String("is")) break;
        }
    }
    return salida;
}

LecturaXlsx leerHoja(const QByteArray& xml, const QStringList& compartidas, const QVector<bool>& estilosFecha,
                     bool epoca1904, int maxColumnas)
{
    LecturaXlsx res;
    QXmlStreamReader r(xml);
    int filaActual = -1;
    auto asegurarFila = [&](int fila) {
        while (res.filas.size() <= fila) res.filas.append(FilaXlsx(maxColumnas));
    };

    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement()) continue;
        if (r.name() == QLatin1String("row")) {
            const QString nr = r.attributes().value("r").toString();
            filaActual = nr.isEmpty() ? filaActual + 1 : nr.toInt() - 1;
        } else if (r.name() == QLatin1String("c")) {
            const auto attrs = r.attributes();
            const QString ref = attrs.value("r").toString();
            const QString tipo = attrs.value("t").toString();
            const int estilo = attrs.value("s").toInt();
            QString valor, enLinea;
            bool hayValor = false;
            while (!r.atEnd()) {                       // hijos de <c> hasta </c>
                r.readNext();
                if (r.isStartElement()) {
                    if (r.name() == QLatin1String("v")) { valor = r.readElementText(); hayValor = true; }
                    else if (r.name() == QLatin1String("is")) { enLinea = leerTextoEnLinea(r); hayValor = true; }
                    else if (r.name() == QLatin1String("f")) r.skipCurrentElement();
                } else if (r.isEndElement() && r.name() == QLatin1String("c")) {
                    break;
                }
            }

            int col = 0, fila = filaActual;
            if (!ref.isEmpty()) { if (!leerReferencia(ref, &col, &fila)) continue; }
            else col = 0;
            if (fila < 0 || col < 0 || col >= maxColumnas || !hayValor) continue;

            Celda celda;
            if (tipo == QLatin1String("s")) {
                const int i = valor.toInt();
                celda = Celda::deTexto(i >= 0 && i < compartidas.size() ? compartidas[i] : QString());
            } else if (tipo == QLatin1String("inlineStr")) {
                celda = Celda::deTexto(enLinea);
            } else if (tipo == QLatin1String("str") || tipo == QLatin1String("e")) {
                celda = Celda::deTexto(valor);
            } else if (tipo == QLatin1String("b")) {
                celda.tipo = Celda::Logico;
                celda.logico = valor.trimmed() == QLatin1String("1");
            } else if (tipo == QLatin1String("d")) {
                const QDateTime f = QDateTime::fromString(valor, Qt::ISODate);
                celda = f.isValid() ? Celda::deFecha(f) : Celda::deTexto(valor);
            } else {                                   // número (o fecha, según el formato de la celda)
                bool ok = false;
                const double n = valor.toDouble(&ok);
                if (!ok) continue;
                celda = (estilo >= 0 && estilo < estilosFecha.size() && estilosFecha[estilo])
                            ? Celda::deFecha(desdeSerial(n, epoca1904)) : Celda::deNumero(n);
            }
            asegurarFila(fila);
            res.filas[fila][col] = celda;
        }
    }
    res.ok = true;
    return res;
}

} // namespace

LecturaXlsx leerPrimeraHoja(const QString& ruta, int maxColumnas)
{
    LecturaXlsx res;
    QFile archivo(ruta);
    if (!archivo.open(QIODevice::ReadOnly)) {
        res.error = QStringLiteral("no se pudo abrir el archivo: %1").arg(archivo.errorString());
        return res;
    }
    const QByteArray datos = archivo.readAll();
    archivo.close();

    LectorZip zip;
    QString error;
    if (!zip.abrir(datos, &error)) { res.error = error; return res; }

    const QByteArray libro = zip.extraer(QStringLiteral("xl/workbook.xml"), &error);
    if (libro.isEmpty()) { res.error = error.isEmpty() ? QStringLiteral("no es una planilla de Excel") : error; return res; }

    // ¿cuál es la primera hoja? (workbook.xml la nombra por rId; workbook.xml.rels dice dónde está)
    bool epoca1904 = false;
    QString idHoja;
    {
        QXmlStreamReader r(libro);
        while (!r.atEnd()) {
            r.readNext();
            if (!r.isStartElement()) continue;
            if (r.name() == QLatin1String("workbookPr")) {
                const QString v = r.attributes().value("date1904").toString();
                epoca1904 = (v == QLatin1String("1") || v == QLatin1String("true"));
            } else if (r.name() == QLatin1String("sheet") && idHoja.isEmpty()) {
                idHoja = r.attributes().value(NS_REL, QStringLiteral("id")).toString();
            }
        }
    }
    QString rutaHoja = QStringLiteral("xl/worksheets/sheet1.xml");
    if (!idHoja.isEmpty() && zip.contiene(QStringLiteral("xl/_rels/workbook.xml.rels"))) {
        const QByteArray rels = zip.extraer(QStringLiteral("xl/_rels/workbook.xml.rels"), &error);
        QXmlStreamReader r(rels);
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement() && r.name() == QLatin1String("Relationship")
                && r.attributes().value("Id") == idHoja) {
                QString destino = r.attributes().value("Target").toString();
                rutaHoja = destino.startsWith(QLatin1Char('/')) ? destino.mid(1) : QStringLiteral("xl/") + destino;
                break;
            }
        }
    }

    QStringList compartidas;
    if (zip.contiene(QStringLiteral("xl/sharedStrings.xml")))
        compartidas = leerCadenasCompartidas(zip.extraer(QStringLiteral("xl/sharedStrings.xml"), &error));
    QVector<bool> estilosFecha;
    if (zip.contiene(QStringLiteral("xl/styles.xml")))
        estilosFecha = leerEstilosFecha(zip.extraer(QStringLiteral("xl/styles.xml"), &error));

    const QByteArray hoja = zip.extraer(rutaHoja, &error);
    if (hoja.isEmpty()) { res.error = error.isEmpty() ? QStringLiteral("la hoja está vacía") : error; return res; }
    return leerHoja(hoja, compartidas, estilosFecha, epoca1904, maxColumnas);
}
