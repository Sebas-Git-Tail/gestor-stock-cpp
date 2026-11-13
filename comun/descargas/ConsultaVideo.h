// Lo que en el Manager de Linux hace Logica/Descargar.sh con yt-dlp, para los Managers que
// llaman a yt-dlp directo (Windows y Android):
//  - OfertaVideo lee la respuesta de «yt-dlp -J» (qué pistas de audio, subtítulos, calidades y
//    códecs ofrece cada video), como hace --pistas.
//  - argumentosEleccion() arma las opciones de yt-dlp para descargar lo elegido en el diálogo,
//    siempre en MKV, con el mismo selector de formatos que Descargar.sh.
// Es el mismo archivo en Windows-Manager y Android-Manager.

#pragma once

#include "DialogoPistas.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct OfertaVideo
{
    QHash<QString, PistaAudio> audios;
    QHash<QString, Subtitulo> subtitulos;
    QList<FormatoVideo> formatos;
    QStringList codecsAudio;

    // Suma lo que informa la respuesta de «yt-dlp -J» de una URL. Falso si no se entiende.
    bool agregar(const QByteArray &json);
    // Nada para elegir: una sola pista de audio, sin subtítulos ni calidades informadas
    bool vacia() const { return audios.size() < 2 && subtitulos.isEmpty() && formatos.isEmpty(); }
    void limpiar() { *this = {}; }
};

// Argumentos de yt-dlp para la consulta de una URL (antes de la URL): -J y un solo video
QStringList argumentosConsulta();

// Opciones de yt-dlp para un video: MKV, subtítulos incrustados y selector de formatos.
// carpetaSubtitulos: donde quedan los .vtt sueltos (temporal, para que no queden junto al video)
QStringList argumentosEleccion(const Eleccion &eleccion, const QString &carpetaSubtitulos);

// Familias de códecs como las informa la consulta (vacío si no se conoce)
QString familiaVideo(const QString &codec);
QString familiaAudio(const QString &codec);
