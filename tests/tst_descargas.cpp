// Pruebas de descargas/: resumen de la simulación de rsync y opciones de yt-dlp
#include "ConsultaVideo.h"
#include "OpcionesPistas.h"
#include "ResumenSimulacion.h"

#include <QTest>

class TstDescargas : public QObject
{
    Q_OBJECT
private slots:
    void resumenDeRsync()
    {
        ResumenSimulacion r;
        QVERIFY(r.procesar(QStringLiteral("<f+++++++++ Fotos/a.jpg")));
        QVERIFY(r.procesar(QStringLiteral(">f.st...... Musica/b.mp3")));
        QVERIFY(r.procesar(QStringLiteral(".d..t...... Fotos/")));
        QVERIFY(r.procesar(QStringLiteral("*deleting   Viejo/c.txt")));
        QVERIFY(r.procesar(QStringLiteral("*deleting   Viejo/")));
        QVERIFY(r.procesar(QStringLiteral("Total transferred file size: 1.23G bytes")));
        QVERIFY(!r.procesar(QStringLiteral("otra cosa")));
        QCOMPARE(r.archivosACopiar(), 2);
        QCOMPARE(r.archivosABorrar(), 1);
        QCOMPARE(r.tamanoACopiar(), QStringLiteral("1.23G"));
        QCOMPARE(r.primerosABorrar(), QStringList{QStringLiteral("Viejo/c.txt")});
        QVERIFY(!r.borradosOmitidos());
        r.procesar(QStringLiteral("IO error encountered -- skipping file deletion"));
        QVERIFY(r.borradosOmitidos());
    }

    void siempreMkv()
    {
        Eleccion e;
        e.calidad = 720;
        e.vcodec = QStringLiteral("vp9");
        e.acodec = QStringLiteral("opus");
        e.subtitulos = {QStringLiteral("es")};
        const QStringList a = argumentosEleccion(e, QStringLiteral("/tmp/subs"));
        QVERIFY(a.join(u' ').contains(QStringLiteral("mkv")));
        QVERIFY(a.join(u' ').contains(QStringLiteral("720")));
        QCOMPARE(familiaVideo(QStringLiteral("avc1.64001F")), QStringLiteral("h264"));
        QCOMPARE(familiaAudio(QStringLiteral("opus")), QStringLiteral("opus"));
    }

    // Lo que muestra el diálogo de pistas (Widgets y QML): orden, nombres y avisos
    void opcionesDePistas()
    {
        using namespace OpcionesPistas;
        // Audio: la original primero, aunque por nombre fuera después
        const QList<FilaIdioma> audios = filasAudio({{QStringLiteral("de"), false}, {QStringLiteral("ja"), true}});
        QCOMPARE(audios.size(), 2);
        QCOMPARE(audios.at(0).idioma, QStringLiteral("ja"));
        QVERIFY(audios.at(0).texto.endsWith(QStringLiteral(" — original")));
        QCOMPARE(audios.at(1).texto, QStringLiteral("German [de]"));

        // Subtítulos: los del autor, después «-orig» y las traducciones automáticas (secundarias)
        const QList<FilaIdioma> subs = filasSubtitulos({{QStringLiteral("xx-auto"), true, {}},
                                                        {QStringLiteral("en-orig"), true, {}},
                                                        {QStringLiteral("fr"), false, QStringLiteral("Français")}});
        QCOMPARE(subs.size(), 3);
        QCOMPARE(subs.at(0).texto, QStringLiteral("Français [fr]"));
        QCOMPARE(subs.at(0).ayuda, QStringLiteral("Français"));
        QCOMPARE(subs.at(1).idioma, QStringLiteral("en-orig"));
        QVERIFY(!subs.at(1).secundario);
        QVERIFY(subs.at(2).secundario);

        // Calidades de mayor a menor, sin repetir; códecs de más eficiente a más compatible
        const QList<FormatoVideo> formatos{{720, QStringLiteral("h264")}, {2160, QStringLiteral("vp9")},
                                           {720, QStringLiteral("av1")}, {1080, QStringLiteral("h264")}};
        QCOMPARE(calidades(formatos), (QList<int>{2160, 1080, 720}));
        QCOMPARE(textoCalidad(0), QStringLiteral("La mejor"));
        QCOMPARE(textoCalidad(2160), QStringLiteral("2160p (4K)"));
        QCOMPARE(familiasVideo(formatos), (QStringList{QStringLiteral("av1"), QStringLiteral("vp9"), QStringLiteral("h264")}));
        QCOMPARE(familiasAudio({QStringLiteral("mp3"), QStringLiteral("opus")}), (QStringList{QStringLiteral("opus"), QStringLiteral("mp3")}));
        QCOMPARE(nombreCodec(QStringLiteral("h265")), QStringLiteral("H.265 (HEVC)"));

        // Un códec que no está en la calidad elegida se avisa y se explica qué se baja
        QCOMPARE(textoCodecVideo(QStringLiteral("vp9"), 1080, formatos), QStringLiteral("VP9 — no está en 1080p"));
        QCOMPARE(textoCodecVideo(QStringLiteral("vp9"), 0, formatos), QStringLiteral("VP9"));
        QVERIFY(explicacion(1080, QStringLiteral("vp9"), formatos).startsWith(QStringLiteral("Se baja VP9")));
        QVERIFY(explicacion(1080, QStringLiteral("h264"), formatos).startsWith(QStringLiteral("El archivo queda en MKV")));
    }
};

QTEST_MAIN(TstDescargas)
#include "tst_descargas.moc"
