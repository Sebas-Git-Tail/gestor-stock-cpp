#include "ConsultaVideo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <functional>

namespace {

// Los idiomas terminan dentro de opciones de yt-dlp: solo valen códigos simples
bool idiomaValido(const QString &idioma)
{
    static const QRegularExpression valido(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_-]*$"));
    return valido.match(idioma).hasMatch();
}

// Recorre la respuesta: un video o, si la URL era una playlist, sus entradas
void recorrer(const QJsonObject &informacion, const std::function<void(const QJsonObject &)> &hacer)
{
    if (informacion.value(QStringLiteral("_type")).toString() == u"playlist") {
        const QJsonArray entradas = informacion.value(QStringLiteral("entries")).toArray();
        for (const QJsonValue &entrada : entradas) {
            if (entrada.isObject())
                recorrer(entrada.toObject(), hacer);
        }
    } else {
        hacer(informacion);
    }
}

QString filtroVcodec(const QString &familia)
{
    if (familia == u"av1") return QStringLiteral("[vcodec^=av01]");
    if (familia == u"vp9") return QStringLiteral("[vcodec~='^vp0?9']");
    if (familia == u"h264") return QStringLiteral("[vcodec~='^(avc[13]|h264)']");
    if (familia == u"h265") return QStringLiteral("[vcodec~='^(hev1|hvc1|hevc|h265)']");
    if (familia == u"vp8") return QStringLiteral("[vcodec^=vp8]");
    return {};
}

QString filtroAcodec(const QString &familia)
{
    if (familia == u"opus") return QStringLiteral("[acodec=opus]");
    if (familia == u"aac") return QStringLiteral("[acodec~='^(mp4a|aac)']");
    if (familia == u"vorbis") return QStringLiteral("[acodec=vorbis]");
    if (familia == u"mp3") return QStringLiteral("[acodec=mp3]");
    if (familia == u"ac3") return QStringLiteral("[acodec~='^ac-?3']");
    if (familia == u"eac3") return QStringLiteral("[acodec~='^(ec-?3|eac3)']");
    if (familia == u"flac") return QStringLiteral("[acodec=flac]");
    return {};
}

// Alternativas con las pistas de audio elegidas (todas dentro del mismo archivo). Un selector
// con «+» falla entero si al video le falta un idioma: por eso van de más a menos idiomas.
QString conIdiomas(const QString &video, const QString &filtroAudio, const QStringList &idiomas)
{
    const int n = int(idiomas.size());
    QString selector;
    if (n <= 6) {
        // Cada número «mascara» es una combinación (el bit i dice si entra el idioma i)
        for (int cantidad = n; cantidad >= 1; --cantidad) {
            for (int mascara = (1 << n) - 1; mascara >= 1; --mascara) {
                if (__builtin_popcount(unsigned(mascara)) != cantidad)
                    continue;
                QString alternativa = video;
                for (int i = 0; i < n; ++i) {
                    if ((mascara >> i) & 1)
                        alternativa += QStringLiteral("+ba[language=%1]%2").arg(idiomas.at(i), filtroAudio);
                }
                selector += alternativa + u'/';
            }
        }
    } else {
        // Con muchos idiomas las combinaciones son demasiadas: todos juntos o cada uno solo
        QString todos = video;
        for (const QString &idioma : idiomas)
            todos += QStringLiteral("+ba[language=%1]%2").arg(idioma, filtroAudio);
        selector += todos + u'/';
        for (const QString &idioma : idiomas)
            selector += QStringLiteral("%1+ba[language=%2]%3/").arg(video, idioma, filtroAudio);
    }
    return selector;
}

} // namespace

QString familiaVideo(const QString &codec)
{
    const QString c = codec.toLower();
    if (c.startsWith(u"av01")) return QStringLiteral("av1");
    if (c.startsWith(u"vp09") || c.startsWith(u"vp9")) return QStringLiteral("vp9");
    if (c.startsWith(u"avc1") || c.startsWith(u"avc3") || c.startsWith(u"h264")) return QStringLiteral("h264");
    if (c.startsWith(u"hev1") || c.startsWith(u"hvc1") || c.startsWith(u"hevc") || c.startsWith(u"h265")) return QStringLiteral("h265");
    if (c.startsWith(u"vp8")) return QStringLiteral("vp8");
    return {};
}

QString familiaAudio(const QString &codec)
{
    const QString c = codec.toLower();
    if (c.startsWith(u"opus")) return QStringLiteral("opus");
    if (c.startsWith(u"mp4a") || c.startsWith(u"aac")) return QStringLiteral("aac");
    if (c.startsWith(u"vorbis")) return QStringLiteral("vorbis");
    if (c.startsWith(u"mp3")) return QStringLiteral("mp3");
    if (c.startsWith(u"ec-3") || c.startsWith(u"eac3")) return QStringLiteral("eac3");
    if (c.startsWith(u"ac-3") || c.startsWith(u"ac3")) return QStringLiteral("ac3");
    if (c.startsWith(u"flac")) return QStringLiteral("flac");
    return {};
}

bool OfertaVideo::agregar(const QByteArray &json)
{
    const QJsonDocument documento = QJsonDocument::fromJson(json);
    if (!documento.isObject())
        return false;
    recorrer(documento.object(), [this](const QJsonObject &video) {
        // Calidades: las de solo video (se pueden juntar con cualquier audio); si el sitio solo
        // tiene formatos con video y audio juntos, esas
        QList<FormatoVideo> soloVideo, conAudioJunto;
        const auto anotarFormato = [](QList<FormatoVideo> &lista, int alto, const QString &familia) {
            const bool repetido = std::any_of(lista.cbegin(), lista.cend(), [&](const FormatoVideo &f) {
                return f.alto == alto && f.familia == familia;
            });
            if (!familia.isEmpty() && alto > 0 && !repetido)
                lista << FormatoVideo{alto, familia};
        };
        const QJsonArray formatosJson = video.value(QStringLiteral("formats")).toArray();
        for (const QJsonValue &valor : formatosJson) {
            const QJsonObject formato = valor.toObject();
            const QString vcodec = formato.value(QStringLiteral("vcodec")).toString();
            const QString acodec = formato.value(QStringLiteral("acodec")).toString();
            const bool conVideo = !vcodec.isEmpty() && vcodec != u"none";
            const bool conAudio = !acodec.isEmpty() && acodec != u"none";

            // Pistas de audio: los idiomas de los formatos de solo audio. La original es la que
            // yt-dlp marca con la preferencia de idioma más alta (10)
            const QString idioma = formato.value(QStringLiteral("language")).toString();
            if (!conVideo && conAudio && !idioma.isEmpty() && idiomaValido(idioma)) {
                PistaAudio &pista = audios[idioma];
                pista.idioma = idioma;
                pista.original = pista.original || formato.value(QStringLiteral("language_preference")).toInt() >= 10;
            }
            // Calidades y códecs
            if (conVideo) {
                anotarFormato(conAudio ? conAudioJunto : soloVideo, formato.value(QStringLiteral("height")).toInt(),
                              familiaVideo(vcodec));
            } else if (conAudio) {
                const QString familia = familiaAudio(acodec);
                if (!familia.isEmpty() && !codecsAudio.contains(familia))
                    codecsAudio << familia;
            }
        }

        for (const FormatoVideo &f : std::as_const(soloVideo.isEmpty() ? conAudioJunto : soloVideo)) {
            const bool repetido = std::any_of(formatos.cbegin(), formatos.cend(), [&](const FormatoVideo &g) {
                return g.alto == f.alto && g.familia == f.familia;
            });
            if (!repetido)
                formatos << f;
        }

        // Subtítulos: los que subió el autor y los automáticos (si de un idioma hay de los dos,
        // yt-dlp usa el manual). live_chat es el chat de un directo, no un subtítulo.
        const QJsonObject manuales = video.value(QStringLiteral("subtitles")).toObject();
        const QJsonObject automaticos = video.value(QStringLiteral("automatic_captions")).toObject();
        for (const bool automatico : {false, true}) {
            const QJsonObject &lista = automatico ? automaticos : manuales;
            for (auto it = lista.begin(); it != lista.end(); ++it) {
                const QString idiomaSub = it.key();
                if (idiomaSub == u"live_chat" || !idiomaValido(idiomaSub) || (automatico && manuales.contains(idiomaSub)))
                    continue;
                QString nombre;
                const QJsonArray formatosSub = it.value().toArray();
                for (const QJsonValue &f : formatosSub) {
                    nombre = f.toObject().value(QStringLiteral("name")).toString().simplified();
                    if (!nombre.isEmpty())
                        break;
                }
                const auto existente = subtitulos.constFind(idiomaSub);
                if (existente == subtitulos.cend() || (existente->automatico && !automatico))
                    subtitulos.insert(idiomaSub, Subtitulo{idiomaSub, automatico, nombre});
            }
        }
    });
    return true;
}

QStringList argumentosConsulta()
{
    // -J: toda la información en JSON, sin descargar. De una playlist se mira un solo video
    return {QStringLiteral("-J"), QStringLiteral("--no-warnings"), QStringLiteral("--no-playlist"),
            QStringLiteral("--playlist-items"), QStringLiteral("1")};
}

QStringList argumentosEleccion(const Eleccion &eleccion, const QString &carpetaSubtitulos)
{
    // Los videos siempre en MKV: une video y audio en MKV y pasa a MKV lo que venga en un solo archivo
    QStringList argumentos{QStringLiteral("--merge-output-format"), QStringLiteral("mkv"),
                           QStringLiteral("--remux-video"), QStringLiteral("mkv")};

    // Subtítulos elegidos: incrustados (se activan desde el reproductor)
    QStringList subtitulos;
    for (const QString &idioma : eleccion.subtitulos) {
        if (idiomaValido(idioma))
            subtitulos << idioma;
    }
    if (!subtitulos.isEmpty()) {
        argumentos << QStringLiteral("--write-subs") << QStringLiteral("--write-auto-subs") << QStringLiteral("--embed-subs")
                   << QStringLiteral("--sub-langs") << subtitulos.join(u',')
                   << QStringLiteral("-o") << QStringLiteral("subtitle:%1/%(id)s.%(ext)s").arg(carpetaSubtitulos);
    }

    // Selector de formatos: de lo más parecido a lo pedido a lo menos (como Descargar.sh)
    QStringList idiomas;
    for (const QString &idioma : eleccion.audios) {
        if (idiomaValido(idioma))
            idiomas << idioma;
    }
    const QString alto = eleccion.calidad > 0 ? QStringLiteral("[height<=%1]").arg(eleccion.calidad) : QString();
    const QString vcodec = filtroVcodec(eleccion.vcodec);
    const QString acodec = filtroAcodec(eleccion.acodec);
    // «bv» y no «bv*»: bv* acepta formatos que ya traen audio y entonces no se les puede sumar otro
    if (!(alto + vcodec + acodec).isEmpty() || !idiomas.isEmpty()) {
        const QString pedido = QStringLiteral("bv") + alto + vcodec;
        const QString soloAlto = QStringLiteral("bv") + alto;
        QString selector;
        if (!idiomas.isEmpty()) {
            selector += conIdiomas(pedido, acodec, idiomas);
            if (!(vcodec + acodec).isEmpty())
                selector += conIdiomas(soloAlto, {}, idiomas);
            argumentos << QStringLiteral("--audio-multistreams");
        }
        if (!acodec.isEmpty())
            selector += pedido + QStringLiteral("+ba") + acodec + u'/';
        selector += pedido + QStringLiteral("+ba/");
        if (!(alto + vcodec).isEmpty())
            selector += QStringLiteral("b") + alto + vcodec + u'/';    // video y audio juntos (algunos sitios)
        if (!vcodec.isEmpty())
            selector += soloAlto + QStringLiteral("+ba/");
        if (!alto.isEmpty() && !vcodec.isEmpty())
            selector += QStringLiteral("b") + alto + u'/';
        argumentos << QStringLiteral("-f") << selector + QStringLiteral("bv*+ba/b");
    }
    return argumentos;
}
