// Pruebas de sinc/cripto: claves, CPace y canal cifrado
#include "cripto.h"

#include <QTest>

class TstCripto : public QObject
{
    Q_OBJECT
private slots:
    void x25519Acuerdan()
    {
        const QByteArray a = cripto::aleatorio(32), b = cripto::aleatorio(32);
        const QByteArray ka = cripto::compartido(a, cripto::publica(b));
        const QByteArray kb = cripto::compartido(b, cripto::publica(a));
        QCOMPARE(ka.size(), 32);
        QCOMPARE(ka, kb);
    }

    void cpaceMismoCodigoMismaClave()
    {
        const QByteArray ctx = "emparejar|cel|0123";
        const QByteArray a = cripto::aleatorio(32), b = cripto::aleatorio(32);
        const QByteArray ga = cripto::generadorEmparejar(QStringLiteral("123456"), ctx);
        const QByteArray gb = cripto::generadorEmparejar(QStringLiteral("123456"), ctx);
        QCOMPARE(ga, gb);
        QCOMPARE(cripto::compartido(a, cripto::publica(b, gb)), cripto::compartido(b, cripto::publica(a, ga)));
    }

    void cpaceOtroCodigoOtraClave()
    {
        const QByteArray ctx = "emparejar|cel|0123";
        const QByteArray a = cripto::aleatorio(32), b = cripto::aleatorio(32);
        const QByteArray ga = cripto::generadorEmparejar(QStringLiteral("123456"), ctx);
        const QByteArray gb = cripto::generadorEmparejar(QStringLiteral("123457"), ctx);
        QVERIFY(ga != gb);
        QVERIFY(cripto::compartido(a, cripto::publica(b, gb)) != cripto::compartido(b, cripto::publica(a, ga)));
        // y otro contexto (otro celular u otra conexión) también da otro punto
        QVERIFY(cripto::generadorEmparejar(QStringLiteral("123456"), "emparejar|otro|0123") != ga);
    }

    void rechazaPuntosDeOrdenChico()
    {
        const QByteArray secreta = cripto::aleatorio(32);
        QVERIFY(cripto::compartido(secreta, QByteArray(32, '\0')).isEmpty());   // el punto 0
        QByteArray uno(32, '\0');
        uno[0] = 1;
        QVERIFY(cripto::compartido(secreta, uno).isEmpty());                     // el punto 1 (orden 4)
        QVERIFY(cripto::compartido(secreta, QByteArray(31, 'x')).isEmpty());     // tamaño equivocado
    }

    void canalIdaYVuelta()
    {
        const QByteArray k1 = cripto::aleatorio(32), k2 = cripto::aleatorio(32);
        cripto::Canal pc, cel;
        pc.iniciar(k1, k2);
        cel.iniciar(k2, k1);
        for (const QByteArray &texto : {QByteArray("hola"), QByteArray(), QByteArray(100000, 'z')}) {
            const QByteArray linea = pc.cifrar(texto);
            QVERIFY(!linea.contains(texto.left(10)) || texto.isEmpty());
            QByteArray plano;
            QVERIFY(cel.descifrar(linea, plano));
            QCOMPARE(plano, texto);
        }
        QByteArray plano;
        QVERIFY(pc.descifrar(cel.cifrar("respuesta"), plano));
        QCOMPARE(plano, QByteArray("respuesta"));
    }

    void canalRechazaCambiosRepetidosYDesorden()
    {
        const QByteArray k = cripto::aleatorio(32);
        cripto::Canal a, b;
        a.iniciar(k, k);
        b.iniciar(k, k);
        const QByteArray m1 = a.cifrar("uno"), m2 = a.cifrar("dos");
        QByteArray plano;
        QVERIFY(!b.descifrar(m2, plano));             // fuera de orden
        QVERIFY(b.descifrar(m1, plano));
        QVERIFY(!b.descifrar(m1, plano));             // repetido
        QByteArray tocado = QByteArray::fromBase64(m2);
        tocado[17] = char(tocado[17] ^ 1);    // un byte del texto cifrado (los primeros 16 son la MAC)
        QVERIFY(!b.descifrar(tocado.toBase64(), plano));   // cambiado
        QVERIFY(b.descifrar(m2, plano));
        QCOMPARE(plano, QByteArray("dos"));
        QVERIFY(!b.descifrar("esto no es base64!!", plano));
    }

    void canalConOtraClaveNoDescifra()
    {
        cripto::Canal a, b;
        a.iniciar(cripto::aleatorio(32), cripto::aleatorio(32));
        b.iniciar(cripto::aleatorio(32), cripto::aleatorio(32));
        QByteArray plano;
        QVERIFY(!b.descifrar(a.cifrar("secreto"), plano));
    }

    void igualesYBorrar()
    {
        QVERIFY(cripto::iguales("abc", "abc"));
        QVERIFY(!cripto::iguales("abc", "abd"));
        QVERIFY(!cripto::iguales("abc", "ab"));
        QByteArray b("clave");
        cripto::borrar(b);
        QVERIFY(b.isEmpty());
    }
};

QTEST_MAIN(TstCripto)
#include "tst_cripto.moc"
