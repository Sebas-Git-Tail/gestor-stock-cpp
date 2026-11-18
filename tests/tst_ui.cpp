// Pruebas de ui/: recordarVentana guarda el tamaño y si la ventana estaba maximizada o en pantalla completa;
// el modo claro/oscuro cambia la paleta, se guarda, avisa y los íconos siguen al color del tema.
#include "Piezas.h"

#include <QSignalSpy>
#include <QToolButton>

#include <QApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class PruebaUi : public QObject
{
    Q_OBJECT

    QTemporaryDir dir_;

    QString archivo(const char *nombre) const { return dir_.filePath(QString::fromLatin1(nombre) + QStringLiteral(".ini")); }

private slots:
    void initTestCase()
    {
        QCoreApplication::setApplicationName(QStringLiteral("tst_ui"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir_.path());
        tema::aplicar(*qApp);
    }

    void modoClaroYOscuro()
    {
        QVERIFY(tema::oscuro());                                  // la primera vez, oscuro
        const QColor fondoOscuro = qApp->palette().color(QPalette::Window);
        QSignalSpy aviso(tema::avisos(), &tema::AvisosTema::modoCambiado);
        int extras = 0;
        tema::agregarEstilo([&extras](const tema::Colores &) { ++extras; return QStringLiteral("QLabel#x { }"); });
        QCOMPARE(extras, 1);

        // Ícono del color del texto: en claro tiene que salir oscuro
        const QIcon icono = tema::icono(QStringLiteral("buscar"));
        const auto colorDelIcono = [&icono] {
            const QImage img = icono.pixmap(QSize(24, 24), 1.0).toImage();
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x)
                    if (qAlpha(img.pixel(x, y)) == 255) return QColor(img.pixel(x, y));
            return QColor();
        };
        QVERIFY(colorDelIcono().lightness() > 150);

        tema::alternarModo();
        QVERIFY(!tema::oscuro());
        QCOMPARE(aviso.count(), 1);
        QCOMPARE(aviso.at(0).at(0).toBool(), false);
        QCOMPARE(extras, 2);                                      // el estilo de la app se rehízo
        QVERIFY(qApp->palette().color(QPalette::Window).lightness() > fondoOscuro.lightness());
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("QLabel#x")));
        QVERIFY(colorDelIcono().lightness() < 100);
        QCOMPARE(QSettings().value(QStringLiteral("tema/oscuro")).toBool(), false);

        // El botón alterna y cambia su texto
        QToolButton *boton = tema::botonModo();
        QVERIFY(boton->toolTip().contains(QStringLiteral("oscuro")));
        boton->click();
        QVERIFY(tema::oscuro());
        QVERIFY(boton->toolTip().contains(QStringLiteral("claro")));
        delete boton;
        tema::ponerModo(true);                                    // ya estaba: no avisa de nuevo
        QCOMPARE(aviso.count(), 2);
    }

    void recuerdaElTamano()
    {
        QSettings ajustes(archivo("tamano"), QSettings::IniFormat);
        {
            QWidget v;
            tema::recordarVentana(&v, &ajustes);
            v.resize(731, 517);
            v.show();
            QVERIFY(QTest::qWaitForWindowExposed(&v));
            v.close();
        }
        QVERIFY(ajustes.contains(QStringLiteral("ventana/geometria")));
        QWidget otra;
        otra.resize(300, 200);
        tema::recordarVentana(&otra, &ajustes);
        otra.show();
        QVERIFY(QTest::qWaitForWindowExposed(&otra));
        QCOMPARE(otra.size(), QSize(731, 517));
        QVERIFY(!otra.isMaximized());
    }

    void recuerdaMaximizadaYPantallaCompleta()
    {
        for (const Qt::WindowState estado : {Qt::WindowMaximized, Qt::WindowFullScreen}) {
            QSettings ajustes(archivo(estado == Qt::WindowMaximized ? "max" : "completa"), QSettings::IniFormat);
            {
                QWidget v;
                tema::recordarVentana(&v, &ajustes);
                v.resize(640, 480);
                v.show();
                QVERIFY(QTest::qWaitForWindowExposed(&v));
                v.setWindowState(estado);
                QTRY_VERIFY(v.windowState() & estado);
                v.close();
            }
            QWidget otra;
            tema::recordarVentana(&otra, &ajustes);
            otra.show();
            QVERIFY(QTest::qWaitForWindowExposed(&otra));
            QTRY_VERIFY(otra.windowState() & estado);
            // Al salir de maximizada / pantalla completa vuelve al tamaño de antes
            otra.setWindowState(Qt::WindowNoState);
            QTRY_COMPARE(otra.size(), QSize(640, 480));
        }
    }

    void guardaAlSalirDeLaApp()
    {
        QSettings ajustes(archivo("salir"), QSettings::IniFormat);
        QWidget v;
        tema::recordarVentana(&v, &ajustes);
        v.resize(555, 444);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        QVERIFY(!ajustes.contains(QStringLiteral("ventana/geometria")));
        QVERIFY(QMetaObject::invokeMethod(qApp, "aboutToQuit", Qt::DirectConnection));   // como al salir de la app
        QVERIFY(ajustes.contains(QStringLiteral("ventana/geometria")));
    }
};

QTEST_MAIN(PruebaUi)
#include "tst_ui.moc"
