// Qué se muestra en el diálogo «Calidad, códecs, audio y subtítulos» y en qué orden, sin la
// interfaz: lo usan DialogoPistas (Widgets) y la página equivalente de las apps en QML, para
// que las dos listen, ordenen y expliquen lo mismo.

#pragma once

#include "Pistas.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace OpcionesPistas {

// Una fila con casilla de las listas de audio o subtítulos, ya en el orden en que se muestra
struct FilaIdioma
{
    QString texto;              // «Spanish (Latin America) [es-419] — automático»
    QString idioma;             // el código, que es lo que recibe yt-dlp
    QString ayuda;              // cómo lo llama el sitio (para un tooltip)
    bool secundario = false;    // traducción automática a otro idioma: oculta de entrada
};

// Nombre para mostrar de un código de idioma («Spanish (Latin America)» para es-419). Qt solo
// trae los nombres en inglés (los mismos que usa yt-dlp). Vacío si no conoce el código.
QString nombreIdioma(const QString &codigo);

// Nombre para mostrar de una familia de códec («h264» -> «H.264 (AVC)»)
QString nombreCodec(const QString &familia);

// Pistas de audio: primero la original, después las del idioma del sistema y después el resto
// por nombre. En ese orden quedan en el archivo (la primera marcada es la que suena por defecto).
QList<FilaIdioma> filasAudio(const QList<PistaAudio> &audios);

// Subtítulos: primero los que subió el autor; después los automáticos, empezando por los del
// idioma original y los traducidos al idioma del sistema. Las demás traducciones automáticas
// (YouTube ofrece más de cien) quedan marcadas como secundarias.
QList<FilaIdioma> filasSubtitulos(const QList<Subtitulo> &subtitulos);

// Calidades que ofrecen los videos, de mayor a menor (sin repetir)
QList<int> calidades(const QList<FormatoVideo> &formatos);
QString textoCalidad(int alto);     // «1080p», «2160p (4K)»; 0 = «La mejor»

// Códecs que ofrecen, de más eficiente a más compatible
QStringList familiasVideo(const QList<FormatoVideo> &formatos);
QStringList familiasAudio(const QStringList &codecsAudio);

// Nombre de un códec de video en la lista, avisando si no está en la calidad elegida (alto 0 = la mejor)
QString textoCodecVideo(const QString &familia, int alto, const QList<FormatoVideo> &formatos);

// Lo que se va a bajar con esa calidad y ese códec (texto de debajo de las opciones)
QString explicacion(int alto, const QString &vcodec, const QList<FormatoVideo> &formatos);

} // namespace OpcionesPistas
