// Pruebas de descargas/: resumen de la simulación de rsync y opciones de yt-dlp
#include "ConsultaVideo.h"
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
};

QTEST_MAIN(TstDescargas)
#include "tst_descargas.moc"
