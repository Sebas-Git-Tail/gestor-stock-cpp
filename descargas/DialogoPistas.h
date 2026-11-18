// Diálogo «Calidad, códecs, audio y subtítulos» de Descargar Vídeos: muestra lo que ofrecen
// los videos (lo detecta la consulta previa, no hay listas fijas) y deja elegir la calidad,
// los códecs y qué pistas de audio y subtítulos van dentro del archivo (siempre MKV).
// Solo es la interfaz: qué se lista y en qué orden está en OpcionesPistas (lo comparte con las
// apps en QML). Lo usan Arch-Linux-Manager y Windows-Manager.

#pragma once

#include "Pistas.h"

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

namespace Ui { class DialogoPistas; }

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

    // Nombre para mostrar de una familia de códec («h264» -> «H.264 (AVC)»): OpcionesPistas::nombreCodec
    static QString nombreCodec(const QString &familia);

protected:
    void keyPressEvent(QKeyEvent *evento) override;

private:
    void filtrarSubtitulos();
    void marcarCodecsDisponibles();

    QList<FormatoVideo> formatos;
    Ui::DialogoPistas *ui;
};
