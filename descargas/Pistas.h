// Lo que ofrece un video (lo informa la consulta «yt-dlp -J») y lo que se elige descargar.
// Sin interfaz: lo usan el diálogo de Widgets (DialogoPistas) y las apps en QML.

#pragma once

#include <QString>
#include <QStringList>

// Lo que informan los eventos AUDIO y SUBTITULO de Descargar.sh --pistas
struct PistaAudio
{
    QString idioma;             // código, p. ej. «es» o «en-US»
    bool original = false;      // es la pista original del video
};

struct Subtitulo
{
    QString idioma;             // código, p. ej. «es» o «en-orig»
    bool automatico = false;    // generado o traducido automáticamente
    QString nombre;             // cómo lo llama el sitio (puede venir vacío)
};

// Una calidad y códec de video que ofrece el video (evento VIDEO de la consulta)
struct FormatoVideo
{
    int alto = 0;               // en píxeles, p. ej. 1080
    QString familia;            // av1, vp9, h264, h265 o vp8
};

// Lo que se eligió en el diálogo (vacío o 0 = sin preferencia: lo de siempre)
struct Eleccion
{
    QStringList audios;         // idiomas de las pistas de audio, en orden
    QStringList subtitulos;     // idiomas de los subtítulos
    int calidad = 0;            // altura máxima; 0 = la mejor
    QString vcodec;             // familia de video preferida
    QString acodec;             // familia de audio preferida: opus, aac, vorbis, mp3, ac3, eac3 o flac
};
