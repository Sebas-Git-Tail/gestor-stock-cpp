// Pruebas de la sincronización cifrada con una base de datos de juguete en memoria
#include "sincronizacion_comun.h"

#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

namespace {

// Base de juguete: filas {"u": uuid, "v": valor, "t": fecha del cambio}; gana la más nueva
class BaseJuguete : public BaseSinc
{
public:
    explicit BaseJuguete(QString id) : id_(std::move(id)) {}

    QString dispositivo() const override { return id_; }
    QString nombreDispositivo() const override { return QStringLiteral("Equipo ") + id_; }
    qint64 secuencia() const override { return seq_; }
    QJsonArray cambiosDesde(qint64 desde) const override
    {
        QJsonArray a;
        for (auto it = filas_.cbegin(); it != filas_.cend(); ++it)
            if (seqFila_.value(it.key()) > desde) a << it.value();
        return a;
    }
    QJsonArray filasPorUuid(const QStringList &uuids) const override
    {
        QJsonArray a;
        for (const QString &u : uuids)
            if (filas_.contains(u)) a << filas_.value(u);
        return a;
    }
    ResultadoAplicar aplicarCambios(const QJsonArray &filas) override
    {
        ResultadoAplicar r;
        for (const QJsonValue &v : filas) {
            const QJsonObject o = v.toObject();
            const QString u = o.value(QLatin1String("u")).toString();
            if (u.isEmpty()) { ++r.ignoradas; continue; }
            if (filas_.contains(u) && filas_.value(u).value(QLatin1String("t")).toInteger() >= o.value(QLatin1String("t")).toInteger()) {
                if (filas_.value(u) != o) r.ganadorasLocales << u;
                continue;
            }
            filas_.insert(u, o);
            seqFila_.insert(u, ++seq_);
            r.aceptadas << u;
            ++r.aplicadas;
        }
        return r;
    }
    std::optional<Par> par(const QString &dispositivo) const override
    {
        if (!pares_.contains(dispositivo)) return std::nullopt;
        return pares_.value(dispositivo);
    }
    void guardarPar(const Par &p) override { pares_.insert(p.dispositivo, p); }

    void escribir(const QString &u, const QString &valor, qint64 t)
    {
        filas_.insert(u, QJsonObject{{"u", u}, {"v", valor}, {"t", t}});
        seqFila_.insert(u, ++seq_);
    }
    QString valor(const QString &u) const { return filas_.value(u).value(QLatin1String("v")).toString(); }
    QHash<QString, Par> pares_;

private:
    QString id_;
    QHash<QString, QJsonObject> filas_;
    QHash<QString, qint64> seqFila_;
    qint64 seq_ = 0;
};

const PerfilSinc Perfil{QStringLiteral("Programa de prueba"), "PP", QStringLiteral("pp"), 0, 0};

// Espera a que el cliente termine y devuelve (ok, mensaje)
QPair<bool, QString> esperar(ClienteSincBase &cliente)
{
    QSignalSpy fin(&cliente, &ClienteSincBase::terminado);
    if (!fin.wait(10000)) return {false, QStringLiteral("(no terminó)")};
    return {fin.at(0).at(0).toBool(), fin.at(0).at(1).toString()};
}

// Intermediario que copia todo lo que pasa entre el celular y la PC (para ver qué viaja por la red)
class Espia : public QObject
{
public:
    QByteArray capturado;
    quint16 iniciar(quint16 destino)
    {
        servidor_.listen(QHostAddress::LocalHost, 0);
        connect(&servidor_, &QTcpServer::newConnection, this, [this, destino] {
            QTcpSocket *entrada = servidor_.nextPendingConnection();
            auto *salida = new QTcpSocket(entrada);
            salida->connectToHost(QHostAddress::LocalHost, destino);
            connect(entrada, &QTcpSocket::readyRead, this, [this, entrada, salida] {
                const QByteArray d = entrada->readAll(); capturado += d; salida->write(d); });
            connect(salida, &QTcpSocket::readyRead, this, [this, entrada, salida] {
                const QByteArray d = salida->readAll(); capturado += d; entrada->write(d); });
            connect(salida, &QTcpSocket::disconnected, entrada, &QTcpSocket::disconnectFromHost);
            connect(entrada, &QTcpSocket::disconnected, salida, &QTcpSocket::disconnectFromHost);
        });
        return servidor_.serverPort();
    }

private:
    QTcpServer servidor_;
};

}

class TstSinc : public QObject
{
    Q_OBJECT
private slots:
    void emparejarYSincronizar()
    {
        BaseJuguete pc(QStringLiteral("pc")), cel(QStringLiteral("cel"));
        ServidorSincBase servidor(pc, Perfil);
        QVERIFY(servidor.iniciar(0, false));
        ClienteSincBase cliente(cel, Perfil);
        QSignalSpy emparejado(&servidor, &ServidorSincBase::emparejado);

        cliente.emparejar(QStringLiteral("127.0.0.1"), servidor.puerto(), servidor.nuevoCodigo());
        const auto [ok, mensaje] = esperar(cliente);
        QVERIFY2(ok, qPrintable(mensaje));
        QCOMPARE(emparejado.size(), 1);
        QVERIFY(servidor.codigo().isEmpty());                // el código se usa una sola vez
        QVERIFY(sinc::secretoVigente(cel.par(QStringLiteral("pc"))->secreto));
        QCOMPARE(cel.par(QStringLiteral("pc"))->secreto, pc.par(QStringLiteral("cel"))->secreto);

        pc.escribir(QStringLiteral("a"), QStringLiteral("de la PC"), 10);
        cel.escribir(QStringLiteral("b"), QStringLiteral("del celular"), 11);
        cliente.sincronizar(QStringLiteral("pc"));
        const auto [ok2, mensaje2] = esperar(cliente);
        QVERIFY2(ok2, qPrintable(mensaje2));
        QCOMPARE(cel.valor(QStringLiteral("a")), QStringLiteral("de la PC"));
        QCOMPARE(pc.valor(QStringLiteral("b")), QStringLiteral("del celular"));

        // Gana lo más nuevo
        pc.escribir(QStringLiteral("a"), QStringLiteral("viejo"), 20);
        cel.escribir(QStringLiteral("a"), QStringLiteral("nuevo"), 30);
        cliente.sincronizar(QStringLiteral("pc"));
        QVERIFY(esperar(cliente).first);
        QCOMPARE(pc.valor(QStringLiteral("a")), QStringLiteral("nuevo"));
        QCOMPARE(cel.valor(QStringLiteral("a")), QStringLiteral("nuevo"));
    }

    void codigoEquivocado()
    {
        BaseJuguete pc(QStringLiteral("pc")), cel(QStringLiteral("cel"));
        ServidorSincBase servidor(pc, Perfil);
        QVERIFY(servidor.iniciar(0, false));
        const QString codigo = servidor.nuevoCodigo();
        const QString otro = QStringLiteral("%1").arg((codigo.toInt() + 1) % 1000000, 6, 10, QLatin1Char('0'));
        ClienteSincBase cliente(cel, Perfil);
        cliente.emparejar(QStringLiteral("127.0.0.1"), servidor.puerto(), otro);
        const auto [ok, mensaje] = esperar(cliente);
        QVERIFY(!ok);
        QCOMPARE(mensaje, QStringLiteral("El código no es correcto."));
        QVERIFY(!pc.par(QStringLiteral("cel")));
        QVERIFY(!cel.par(QStringLiteral("pc")));
        QCOMPARE(servidor.codigo(), codigo);                 // sigue vigente (quedan intentos)
    }

    void pocosIntentosPorCodigo()
    {
        BaseJuguete pc(QStringLiteral("pc")), cel(QStringLiteral("cel"));
        ServidorSincBase servidor(pc, Perfil);
        QVERIFY(servidor.iniciar(0, false));
        const QString codigo = servidor.nuevoCodigo();
        const QString otro = QStringLiteral("%1").arg((codigo.toInt() + 7) % 1000000, 6, 10, QLatin1Char('0'));
        ClienteSincBase cliente(cel, Perfil);
        for (int i = 0; i < sinc::INTENTOS_CODIGO; ++i) {
            cliente.emparejar(QStringLiteral("127.0.0.1"), servidor.puerto(), otro);
            QVERIFY(!esperar(cliente).first);
        }
        QVERIFY(servidor.codigo().isEmpty());                // se anuló
        cliente.emparejar(QStringLiteral("127.0.0.1"), servidor.puerto(), codigo);
        QVERIFY(!esperar(cliente).first);                    // ni con el correcto
    }

    void secretoDeLaVersionAnteriorPideEmparejar()
    {
        BaseJuguete pc(QStringLiteral("pc")), cel(QStringLiteral("cel"));
        Par viejo;
        viejo.dispositivo = QStringLiteral("pc");
        viejo.secreto = QString(64, u'a');                   // como los de la versión 1
        viejo.host = QStringLiteral("127.0.0.1");
        cel.guardarPar(viejo);
        ClienteSincBase cliente(cel, Perfil);
        QSignalSpy fin(&cliente, &ClienteSincBase::terminado);
        cliente.sincronizar(QStringLiteral("pc"));
        QCOMPARE(fin.size(), 1);
        QVERIFY(!fin.at(0).at(0).toBool());
        QVERIFY(fin.at(0).at(1).toString().contains(QStringLiteral("emparejar")));
    }

    void otraPcConOtroSecretoNoPasa()
    {
        BaseJuguete pc(QStringLiteral("pc")), cel(QStringLiteral("cel"));
        ServidorSincBase servidor(pc, Perfil);
        QVERIFY(servidor.iniciar(0, false));
        ClienteSincBase cliente(cel, Perfil);
        cliente.emparejar(QStringLiteral("127.0.0.1"), servidor.puerto(), servidor.nuevoCodigo());
        QVERIFY(esperar(cliente).first);
        // La PC «pierde» el secreto (otro equipo con el mismo identificador)
        Par p = *pc.par(QStringLiteral("cel"));
        p.secreto = QStringLiteral("v2:") + QString(64, u'b');
        pc.guardarPar(p);
        pc.escribir(QStringLiteral("x"), QStringLiteral("no debería llegar"), 5);
        cliente.sincronizar(QStringLiteral("pc"));
        const auto [ok, mensaje] = esperar(cliente);
        QVERIFY(!ok);
        QVERIFY(cel.valor(QStringLiteral("x")).isEmpty());
    }

    void porLaRedNoViajaNadaLegible()
    {
        BaseJuguete pc(QStringLiteral("pc")), cel(QStringLiteral("cel"));
        ServidorSincBase servidor(pc, Perfil);
        QVERIFY(servidor.iniciar(0, false));
        Espia espia;
        const quint16 puerto = espia.iniciar(servidor.puerto());
        ClienteSincBase cliente(cel, Perfil);
        const QString codigo = servidor.nuevoCodigo();
        cliente.emparejar(QStringLiteral("127.0.0.1"), puerto, codigo);
        QVERIFY(esperar(cliente).first);
        pc.escribir(QStringLiteral("p1"), QStringLiteral("DATO-PRIVADO-DE-LA-PC"), 1);
        cel.escribir(QStringLiteral("c1"), QStringLiteral("DATO-PRIVADO-DEL-CEL"), 2);
        Par p = *cel.par(QStringLiteral("pc"));
        p.puerto = puerto;                                   // que la sincronización también pase por el espía
        cel.guardarPar(p);
        cliente.sincronizar(QStringLiteral("pc"));
        const auto [ok, mensaje] = esperar(cliente);
        QVERIFY2(ok, qPrintable(mensaje));
        QCOMPARE(pc.valor(QStringLiteral("c1")), QStringLiteral("DATO-PRIVADO-DEL-CEL"));
        QTRY_VERIFY(espia.capturado.size() > 200);
        QVERIFY(!espia.capturado.contains("DATO-PRIVADO"));
        QVERIFY(!espia.capturado.contains(codigo.toLatin1()));
        const QString secreto = cel.par(QStringLiteral("pc"))->secreto.mid(3);
        QVERIFY(!espia.capturado.contains(secreto.toLatin1()));
        QVERIFY(!espia.capturado.contains("cambios"));       // ni siquiera el tipo de mensaje
    }
};

QTEST_MAIN(TstSinc)
#include "tst_sinc.moc"
