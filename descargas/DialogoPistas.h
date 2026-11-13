// Diálogo «Calidad, códecs, audio y subtítulos» de Descargar Vídeos: muestra lo que ofrecen
// los videos (lo detecta la consulta previa, no hay listas fijas) y deja elegir la calidad,
// los códecs y qué pistas de audio y subtítulos van dentro del archivo (siempre MKV).
// Solo es la interfaz. Es el mismo archivo en Arch-Linux-Manager, Windows-Manager y Android-Manager.

#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

namespace Ui { class DialogoPistas; }

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

class DialogoPistas : public QDialog
{
    Q_OBJECT

public:
    DialogoPistas(const QList<PistaAudio> &audios, const QList<Subtitulo> &subtitulos,
                  const QList<FormatoVideo> &formatos, const QStringList &codecsAudio, QWidget *parent = nullptr);
    ~DialogoPistas() override;

    // Lo abre y espera la respuesta (como exec()). En Android, a pantalla completa.
    int abrir();

    // Los idiomas marcados, en el orden de cada lista, y la calidad y códecs elegidos
    Eleccion eleccion() const;

    // Nombre para mostrar de una familia de códec («h264» -> «H.264 (AVC)»)
    static QString nombreCodec(const QString &familia);

protected:
    void keyPressEvent(QKeyEvent *evento) override;

private:
    void filtrarSubtitulos();
    void marcarCodecsDisponibles();

    QList<FormatoVideo> formatos;
    Ui::DialogoPistas *ui;
};
