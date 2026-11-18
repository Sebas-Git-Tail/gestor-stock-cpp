#include "OpcionesPistas.h"

#include <QCoreApplication>
#include <QHash>
#include <QLocale>

#include <algorithm>
#include <functional>

namespace OpcionesPistas {

namespace {

QString tr(const char *texto)
{
    return QCoreApplication::translate("DialogoPistas", texto);
}

// Idioma de un código como «es-419» o «en-orig» (lo que va antes del primer guion)
QLocale::Language idiomaDe(const QString &codigo)
{
    return QLocale::codeToLanguage(codigo.section(u'-', 0, 0));
}

// El idioma en que está el sistema: sus pistas y subtítulos van arriba en las listas
bool delSistema(const QString &codigo)
{
    const QLocale::Language idiomaSistema = QLocale::system().language();
    return idiomaSistema != QLocale::C && idiomaDe(codigo) == idiomaSistema;
}

struct FilaOrdenable
{
    int grupo;          // para ordenar: primero los grupos más chicos
    FilaIdioma fila;
};

QList<FilaIdioma> ordenar(QList<FilaOrdenable> filas)
{
    std::stable_sort(filas.begin(), filas.end(), [](const FilaOrdenable &a, const FilaOrdenable &b) {
        return a.grupo != b.grupo ? a.grupo < b.grupo : QString::localeAwareCompare(a.fila.texto, b.fila.texto) < 0;
    });
    QList<FilaIdioma> resultado;
    for (const FilaOrdenable &f : std::as_const(filas))
        resultado << f.fila;
    return resultado;
}

const QStringList OrdenVideo{QStringLiteral("av1"), QStringLiteral("vp9"), QStringLiteral("h265"),
                             QStringLiteral("h264"), QStringLiteral("vp8")};
const QStringList OrdenAudio{QStringLiteral("opus"), QStringLiteral("aac"), QStringLiteral("eac3"),
                             QStringLiteral("ac3"), QStringLiteral("flac"), QStringLiteral("vorbis"), QStringLiteral("mp3")};

QStringList enEseAlto(int alto, const QList<FormatoVideo> &formatos)
{
    QStringList familias;
    for (const FormatoVideo &formato : formatos) {
        if (alto == 0 || formato.alto == alto)
            familias << formato.familia;
    }
    return familias;
}

} // namespace

QString nombreIdioma(const QString &codigo)
{
    const QLocale::Language idioma = idiomaDe(codigo);
    if (idioma == QLocale::AnyLanguage)
        return {};
    QString nombre = QLocale::languageToString(idioma);
    // Lo que sigue al idioma puede ser una escritura (zh-Hans) o un país o región (pt-BR)
    const QStringList resto = codigo.split(u'-').mid(1);
    for (const QString &parte : resto) {
        if (const QLocale::Script escritura = QLocale::codeToScript(parte); escritura != QLocale::AnyScript)
            nombre += QStringLiteral(" (%1)").arg(QLocale::scriptToString(escritura));
        else if (const QLocale::Territory territorio = QLocale::codeToTerritory(parte); territorio != QLocale::AnyTerritory)
            nombre += QStringLiteral(" (%1)").arg(QLocale::territoryToString(territorio));
    }
    return nombre;
}

QString nombreCodec(const QString &familia)
{
    static const QHash<QString, QString> nombres{
        {QStringLiteral("av1"), QStringLiteral("AV1")},
        {QStringLiteral("vp9"), QStringLiteral("VP9")},
        {QStringLiteral("h264"), QStringLiteral("H.264 (AVC)")},
        {QStringLiteral("h265"), QStringLiteral("H.265 (HEVC)")},
        {QStringLiteral("vp8"), QStringLiteral("VP8")},
        {QStringLiteral("opus"), QStringLiteral("Opus")},
        {QStringLiteral("aac"), QStringLiteral("AAC")},
        {QStringLiteral("vorbis"), QStringLiteral("Vorbis")},
        {QStringLiteral("mp3"), QStringLiteral("MP3")},
        {QStringLiteral("ac3"), QStringLiteral("AC-3 (Dolby Digital)")},
        {QStringLiteral("eac3"), QStringLiteral("E-AC-3 (Dolby Digital Plus)")},
        {QStringLiteral("flac"), QStringLiteral("FLAC")},
    };
    return nombres.value(familia, familia);
}

QList<FilaIdioma> filasAudio(const QList<PistaAudio> &audios)
{
    QList<FilaOrdenable> filas;
    for (const PistaAudio &audio : audios) {
        const QString nombre = nombreIdioma(audio.idioma);
        QString texto = nombre.isEmpty() ? audio.idioma : QStringLiteral("%1 [%2]").arg(nombre, audio.idioma);
        if (audio.original)
            texto += tr(" — original");
        else if (audio.idioma.endsWith(QLatin1String("-desc")))
            texto += tr(" — audiodescripción");
        filas << FilaOrdenable{audio.original ? 0 : delSistema(audio.idioma) ? 1 : 2, {texto, audio.idioma, {}, false}};
    }
    return ordenar(filas);
}

QList<FilaIdioma> filasSubtitulos(const QList<Subtitulo> &subtitulos)
{
    QList<FilaOrdenable> filas;
    for (const Subtitulo &subtitulo : subtitulos) {
        const QString nombre = subtitulo.nombre.isEmpty() ? nombreIdioma(subtitulo.idioma) : subtitulo.nombre;
        QString texto = nombre.isEmpty() ? subtitulo.idioma : QStringLiteral("%1 [%2]").arg(nombre, subtitulo.idioma);
        int grupo = 0;
        if (subtitulo.automatico) {
            texto += tr(" — automático");
            if (subtitulo.idioma.endsWith(QLatin1String("-orig")))
                grupo = 1;
            else if (delSistema(subtitulo.idioma))
                grupo = 2;
            else
                grupo = 3;
        }
        filas << FilaOrdenable{grupo, {texto, subtitulo.idioma, subtitulo.nombre, grupo == 3}};
    }
    return ordenar(filas);
}

QList<int> calidades(const QList<FormatoVideo> &formatos)
{
    QList<int> altos;
    for (const FormatoVideo &formato : formatos) {
        if (!altos.contains(formato.alto))
            altos << formato.alto;
    }
    std::sort(altos.begin(), altos.end(), std::greater<int>());
    return altos;
}

QString textoCalidad(int alto)
{
    if (alto == 0)
        return tr("La mejor");
    return alto >= 2160 ? QStringLiteral("%1p (4K)").arg(alto) : QStringLiteral("%1p").arg(alto);
}

QStringList familiasVideo(const QList<FormatoVideo> &formatos)
{
    QStringList familias;
    for (const QString &familia : OrdenVideo) {
        if (std::any_of(formatos.cbegin(), formatos.cend(), [&](const FormatoVideo &f) { return f.familia == familia; }))
            familias << familia;
    }
    return familias;
}

QStringList familiasAudio(const QStringList &codecsAudio)
{
    QStringList familias;
    for (const QString &familia : OrdenAudio) {
        if (codecsAudio.contains(familia))
            familias << familia;
    }
    return familias;
}

// Con una calidad elegida, los códecs de video que no existen en ella se marcan: si se eligen
// igual, se baja ese códec en la calidad más cercana por debajo (o la calidad con otro códec),
// nunca falla
QString textoCodecVideo(const QString &familia, int alto, const QList<FormatoVideo> &formatos)
{
    if (alto == 0 || enEseAlto(alto, formatos).contains(familia))
        return nombreCodec(familia);
    return tr("%1 — no está en %2p").arg(nombreCodec(familia)).arg(alto);
}

QString explicacion(int alto, const QString &vcodec, const QList<FormatoVideo> &formatos)
{
    if (!vcodec.isEmpty() && alto != 0 && !enEseAlto(alto, formatos).contains(vcodec))
        return tr("Se baja %1 en la mejor calidad que tenga por debajo de %2p.").arg(nombreCodec(vcodec)).arg(alto);
    return tr("El archivo queda en MKV. Si un video no tiene lo elegido, se baja lo más parecido.");
}

} // namespace OpcionesPistas
