#include "sincronizacion_comun.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <functional>

namespace {

constexpr qint64 MAX_LINEA = 128LL * 1024 * 1024;     // un mensaje nunca debería pasar de esto
constexpr int ESPERA_CONEXION_MS = 8000;
constexpr int ESPERA_MENSAJE_MS = 90000;               // la primera sincronización puede traer miles de filas
const QString PREFIJO_SECRETO = QStringLiteral("v2:");

QByteArray pedidoBusqueda(const PerfilSinc &perfil)
{
    return perfil.prefijoBusqueda + "-BUSCAR " + QByteArray::number(sinc::VERSION);
}

QString hex(const QByteArray &b) { return QString::fromLatin1(b.toHex()); }

QByteArray deHex(const QString &s, int bytes)
{
    const QByteArray b = QByteArray::fromHex(s.toLatin1());
    return b.size() == bytes ? b : QByteArray();
}

QByteArray json(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }

// Lo que se firma y de donde salen las claves de la conexión: todo lo que mandaron los dos en el saludo
QByteArray transcripcion(const QString &modo, const QString &dispCliente, const QString &dispServidor,
                         const QString &nonceCliente, const QString &nonceServidor,
                         const QByteArray &pubCliente, const QByteArray &pubServidor)
{
    return QStringLiteral("comun-qt/sinc/%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(QString::number(sinc::VERSION), modo, dispCliente, dispServidor, nonceCliente, nonceServidor,
             hex(pubCliente), hex(pubServidor)).toUtf8();
}

struct Claves {
    QByteArray clienteAServidor, servidorACliente, confirmacion, secretoNuevo;
};

Claves derivar(const QByteArray &base, const QByteArray &t)
{
    return {cripto::hash(base, "c2s|" + t), cripto::hash(base, "s2c|" + t), cripto::hash(base, "conf|" + t),
            cripto::hash(base, "secreto|" + t)};
}

// Contexto del punto de partida del emparejamiento: lo que el celular manda en su saludo
QByteArray contextoEmparejar(const QString &dispCliente, const QString &nonceCliente)
{
    return ("emparejar|" + dispCliente + u'|' + nonceCliente).toUtf8();
}

// Parte la entrada en líneas. Devuelve false si llegó algo enorme.
bool leerLineas(QTcpSocket *s, const std::function<bool(const QByteArray &)> &procesar)
{
    while (s && s->canReadLine()) {
        const QByteArray l = s->readLine().trimmed();
        if (l.isEmpty()) continue;
        if (!procesar(l)) return false;
    }
    return !s || s->bytesAvailable() < MAX_LINEA;
}

// Una línea: cifrada si ya hay canal (o un error en JSON común, si el otro no tiene la clave)
bool decodificar(cripto::Canal &canal, const QByteArray &linea, QJsonObject &m)
{
    QByteArray plano = linea;
    if (canal.activo() && !canal.descifrar(linea, plano)) {
        // ¿Un error del otro lado, que no pudo armar el canal (código o clave equivocados)?
        const QJsonObject o = QJsonDocument::fromJson(linea).object();
        if (o.value(QLatin1String("msg")).toString() != QLatin1String("error")) return false;
        m = o;
        return true;
    }
    QJsonParseError e;
    const QJsonDocument d = QJsonDocument::fromJson(plano, &e);
    if (e.error != QJsonParseError::NoError || !d.isObject()) return false;
    m = d.object();
    return true;
}

QByteArray codificar(cripto::Canal &canal, const QJsonObject &o)
{
    return (canal.activo() ? canal.cifrar(json(o)) : json(o)) + '\n';
}

QString soloDigitos(const QString &s)
{
    QString d;
    for (const QChar c : s) if (c.isDigit()) d += c;
    return d;
}

} // namespace

QStringList sinc::direccionesLocales()
{
    QStringList lista;
    for (const QHostAddress &a : QNetworkInterface::allAddresses()) {
        if (a.protocol() != QAbstractSocket::IPv4Protocol || a.isLoopback()) continue;
        if (a.isInSubnet(QHostAddress(QStringLiteral("169.254.0.0")), 16)) continue;   // sin red de verdad
        lista << a.toString();
    }
    return lista;
}

bool sinc::secretoVigente(const QString &secreto)
{
    return secreto.startsWith(PREFIJO_SECRETO) && deHex(secreto.mid(PREFIJO_SECRETO.size()), 32).size() == 32;
}

// ====================================================================== servidor (PC)

struct ServidorSincBase::Sesion {
    QPointer<QTcpSocket> socket;
    QTimer *reloj = nullptr;
    enum class Paso { Hola, Prueba, Cambios } paso = Paso::Hola;
    QString modo, dispositivo, nombre;
    Claves claves;
    cripto::Canal canal;
    int recibidas = 0, enviadas = 0;
};

ServidorSincBase::ServidorSincBase(BaseSinc &base, const PerfilSinc &perfil, QObject *parent)
    : QObject(parent), base_(base), perfil_(perfil)
{
}

ServidorSincBase::~ServidorSincBase() = default;

bool ServidorSincBase::iniciar(quint16 puerto, bool responderBusquedas)
{
    detener();
    servidor_ = new QTcpServer(this);
    if (!servidor_->listen(QHostAddress::Any, puerto)) {
        error_ = tr("No se pudo usar el puerto %1: %2").arg(puerto).arg(servidor_->errorString());
        delete servidor_;
        servidor_ = nullptr;
        return false;
    }
    connect(servidor_, &QTcpServer::newConnection, this, &ServidorSincBase::nuevaConexion);
    if (responderBusquedas) {
        busqueda_ = new QUdpSocket(this);
        if (busqueda_->bind(QHostAddress::AnyIPv4, perfil_.puertoBusqueda, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
            connect(busqueda_, &QUdpSocket::readyRead, this, &ServidorSincBase::responderBusqueda);
        // si no se puede, igual se sincroniza: solo hay que escribir la dirección a mano en el celular
    }
    error_.clear();
    return true;
}

void ServidorSincBase::detener()
{
    delete servidor_;
    servidor_ = nullptr;
    delete busqueda_;
    busqueda_ = nullptr;
}

bool ServidorSincBase::activo() const
{
    return servidor_ && servidor_->isListening();
}

quint16 ServidorSincBase::puerto() const
{
    return servidor_ ? servidor_->serverPort() : 0;
}

QString ServidorSincBase::nuevoCodigo()
{
    codigo_ = QStringLiteral("%1").arg(QRandomGenerator::system()->bounded(1000000), 6, 10, QLatin1Char('0'));
    codigoVence_ = QDateTime::currentDateTime().addSecs(sinc::MINUTOS_CODIGO * 60);
    intentosCodigo_ = 0;
    emit codigoCambiado();
    return codigo_;
}

void ServidorSincBase::cancelarCodigo()
{
    codigo_.clear();
    codigoVence_ = QDateTime();
    emit codigoCambiado();
}

QString ServidorSincBase::codigo() const
{
    if (codigo_.isEmpty() || QDateTime::currentDateTime() > codigoVence_) return QString();
    return codigo_;
}

void ServidorSincBase::responderBusqueda()
{
    while (busqueda_ && busqueda_->hasPendingDatagrams()) {
        QByteArray datos(int(busqueda_->pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress desde;
        quint16 puertoDesde = 0;
        busqueda_->readDatagram(datos.data(), datos.size(), &desde, &puertoDesde);
        if (!datos.startsWith(pedidoBusqueda(perfil_)) || !activo()) continue;
        const QJsonObject r{{perfil_.claveBusqueda, sinc::VERSION}, {"nombre", base_.nombreDispositivo()},
                            {"disp", base_.dispositivo()}, {"puerto", int(puerto())}};
        busqueda_->writeDatagram(json(r), desde, puertoDesde);
    }
}

void ServidorSincBase::nuevaConexion()
{
    while (servidor_ && servidor_->hasPendingConnections()) {
        QTcpSocket *socket = servidor_->nextPendingConnection();
        auto s = std::make_shared<Sesion>();
        s->socket = socket;
        s->reloj = new QTimer(socket);
        s->reloj->setSingleShot(true);
        s->reloj->start(ESPERA_MENSAJE_MS);
        connect(s->reloj, &QTimer::timeout, socket, &QTcpSocket::abort);   // se colgó: se corta
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, s] {
            const bool bien = leerLineas(s->socket, [&](const QByteArray &linea) {
                QJsonObject m;
                if (!decodificar(s->canal, linea, m)) {
                    // En la prueba: el celular no tiene la misma clave (código o secreto equivocado)
                    if (s->paso == Sesion::Paso::Prueba && s->socket) {
                        const QString texto = s->modo == QLatin1String("emparejar")
                            ? tr("El código no es correcto.")
                            : tr("La clave no coincide. Emparejá el celular de nuevo.");
                        if (s->modo == QLatin1String("emparejar") && intentosCodigo_ >= sinc::INTENTOS_CODIGO)
                            cancelarCodigo();   // se gastaron los intentos de este código
                        s->socket->write(json(QJsonObject{{"msg", "error"}, {"texto", texto}}) + '\n');
                        s->socket->disconnectFromHost();
                        emit fallo(texto);
                        return true;
                    }
                    return false;
                }
                if (s->reloj) s->reloj->start(ESPERA_MENSAJE_MS);
                procesar(s, m);
                return true;
            });
            if (!bien && s->socket) s->socket->abort();
        });
    }
}

void ServidorSincBase::procesar(const std::shared_ptr<Sesion> &s, const QJsonObject &m)
{
    if (!s->socket || s->socket->state() != QAbstractSocket::ConnectedState) return;
    const QString msg = m.value(QLatin1String("msg")).toString();
    auto enviar = [&](const QJsonObject &o) { s->socket->write(codificar(s->canal, o)); };
    auto rechazar = [&](const QString &texto) {
        enviar(QJsonObject{{"msg", "error"}, {"texto", texto}});
        s->socket->disconnectFromHost();
        emit fallo(texto);
    };

    switch (s->paso) {
    case Sesion::Paso::Hola: {
        if (msg != QLatin1String("hola")) return rechazar(tr("Mensaje inesperado."));
        if (m.value(QLatin1String("v")).toInt() != sinc::VERSION)
            return rechazar(tr("La versión del programa en el celular y en la PC no es la misma: actualizá las dos."));
        const QString nonceCliente = m.value(QLatin1String("nonce")).toString();
        const QByteArray pubCliente = deHex(m.value(QLatin1String("pub")).toString(), 32);
        s->modo = m.value(QLatin1String("modo")).toString();
        s->dispositivo = m.value(QLatin1String("disp")).toString();
        s->nombre = m.value(QLatin1String("nombre")).toString();
        if (nonceCliente.size() < 32 || s->dispositivo.isEmpty() || pubCliente.isEmpty())
            return rechazar(tr("Saludo incompleto."));

        QByteArray secreta = cripto::aleatorio(32), pubServidor, base;
        if (s->modo == QLatin1String("emparejar")) {
            const QString cod = codigo();
            if (cod.isEmpty())
                return rechazar(tr("En la PC no hay un código de emparejamiento activo (o ya venció). "
                                   "Tocá «Emparejar un celular» en la PC y probá de nuevo."));
            // Cada conexión para emparejar es un intento (aunque no llegue a mandar su prueba): así,
            // quien no sabe el código puede probar como mucho sinc::INTENTOS_CODIGO de un millón
            if (intentosCodigo_ >= sinc::INTENTOS_CODIGO) {
                cancelarCodigo();
                return rechazar(tr("Demasiados intentos con este código. Pedí uno nuevo en la PC."));
            }
            ++intentosCodigo_;
            const QByteArray generador = cripto::generadorEmparejar(cod, contextoEmparejar(s->dispositivo, nonceCliente));
            pubServidor = cripto::publica(secreta, generador);
            base = cripto::compartido(secreta, pubCliente);
        } else if (s->modo == QLatin1String("auth")) {
            const auto p = base_.par(s->dispositivo);
            if (!p || !sinc::secretoVigente(p->secreto))
                return rechazar(tr("Este celular no está emparejado con «%1» (o se quitó, o se emparejó con una "
                                   "versión anterior). Emparejalo de nuevo.").arg(base_.nombreDispositivo()));
            pubServidor = cripto::publica(secreta);
            const QByteArray dh = cripto::compartido(secreta, pubCliente);
            if (!dh.isEmpty())
                base = cripto::hash(deHex(p->secreto.mid(PREFIJO_SECRETO.size()), 32), "sesion|" + dh);
        } else {
            return rechazar(tr("Mensaje inesperado."));
        }
        cripto::borrar(secreta);
        if (base.isEmpty()) return rechazar(tr("Saludo inválido."));

        const QString nonceServidor = hex(cripto::aleatorio(16));
        const QByteArray t = transcripcion(s->modo, s->dispositivo, base_.dispositivo(), nonceCliente, nonceServidor,
                                           pubCliente, pubServidor);
        s->claves = derivar(base, t);
        cripto::borrar(base);
        enviar(QJsonObject{{"msg", "hola"}, {"v", sinc::VERSION}, {"disp", base_.dispositivo()},
                           {"nombre", base_.nombreDispositivo()}, {"nonce", nonceServidor}, {"pub", hex(pubServidor)}});
        s->canal.iniciar(s->claves.servidorACliente, s->claves.clienteAServidor);
        s->paso = Sesion::Paso::Prueba;
        return;
    }

    case Sesion::Paso::Prueba: {
        // Si llegó acá, el celular ya cifró con la clave correcta; la prueba lo confirma
        const QByteArray prueba = deHex(m.value(QLatin1String("prueba")).toString(), 32);
        if (msg != QLatin1String("prueba") || !cripto::iguales(prueba, cripto::hash(s->claves.confirmacion, "cliente")))
            return rechazar(tr("Mensaje inesperado."));
        if (s->modo == QLatin1String("emparejar")) {
            Par p = base_.par(s->dispositivo).value_or(Par{});
            p.dispositivo = s->dispositivo;
            p.nombre = s->nombre;
            p.secreto = PREFIJO_SECRETO + hex(s->claves.secretoNuevo);
            base_.guardarPar(p);
            cancelarCodigo();
            emit emparejado(s->nombre);
        }
        cripto::borrar(s->claves.secretoNuevo);
        enviar(QJsonObject{{"msg", "ok"}, {"prueba", hex(cripto::hash(s->claves.confirmacion, "servidor"))}});
        s->paso = Sesion::Paso::Cambios;
        return;
    }

    case Sesion::Paso::Cambios:
        if (msg == QLatin1String("cambios")) {
            // Lo que el celular todavía no tiene (calculado ANTES de aplicar lo suyo, para no devolvérselo)
            QJsonArray salida = base_.cambiosDesde(m.value(QLatin1String("desde")).toVariant().toLongLong());
            const ResultadoAplicar r = base_.aplicarCambios(m.value(QLatin1String("filas")).toArray());
            if (!r.ok) return rechazar(tr("No se pudieron guardar los cambios en la PC: %1").arg(r.error));
            // Fuera lo que se acaba de tomar del celular; adentro lo que acá es más nuevo o se tuvo que ajustar
            QSet<QString> quitar(r.aceptadas.begin(), r.aceptadas.end());
            for (const QString &u : r.ganadorasLocales) quitar.insert(u);
            QJsonArray final;
            for (const QJsonValue &v : salida)
                if (!quitar.contains(v.toObject().value(QLatin1String("u")).toString())) final << v;
            for (const QJsonValue &v : base_.filasPorUuid(r.ganadorasLocales)) final << v;
            enviar(QJsonObject{{"msg", "cambios"}, {"hasta", base_.secuencia()}, {"filas", final}});
            s->recibidas += r.aplicadas;
            s->enviadas += final.size();
        } else if (msg == QLatin1String("listo")) {
            if (auto p = base_.par(s->dispositivo)) {           // (si mientras tanto lo quitaron, no vuelve)
                p->nombre = s->nombre;
                p->ultimaSinc = QDateTime::currentDateTime();
                base_.guardarPar(*p);
            }
            s->socket->disconnectFromHost();
            emit sincronizado(s->nombre, s->recibidas, s->enviadas);
        } else {
            rechazar(tr("Mensaje inesperado."));
        }
        return;
    }
}

// ====================================================================== cliente (celular)

ClienteSincBase::ClienteSincBase(BaseSinc &base, const PerfilSinc &perfil, QObject *parent)
    : QObject(parent), base_(base), perfil_(perfil), reloj_(new QTimer(this))
{
    reloj_->setSingleShot(true);
    connect(reloj_, &QTimer::timeout, this, [this] {
        if (paso_ == Paso::Hola && socket_ && socket_->state() != QAbstractSocket::ConnectedState) {
            if (!reintentarBuscando())
                terminar(false, tr("La PC no contesta. ¿Está abierto %1 en la PC, con la sincronización activada?").arg(perfil_.programa));
            return;
        }
        terminar(false, tr("La PC dejó de contestar."));
    });
}

ClienteSincBase::~ClienteSincBase()
{
    if (socket_) socket_->abort();
}

QString ClienteSincBase::textoNoContesta() const
{
    return tr("¿Está abierto %1 en la PC, con la sincronización activada? ¿Están en el mismo Wi-Fi (o los dos con Tailscale)?")
        .arg(perfil_.programa);
}

void ClienteSincBase::emparejar(const QString &host, quint16 puerto, const QString &codigo)
{
    if (ocupado()) return;
    modo_ = Modo::Emparejar;
    codigo_ = soloDigitos(codigo);
    dispositivoPar_.clear();
    secreto_.clear();
    reintentado_ = true;            // al emparejar, la dirección la eligió la persona: no se busca otra
    if (codigo_.size() != 6) {
        emit terminado(false, tr("El código tiene 6 números."), 0, 0);
        return;
    }
    if (host.trimmed().isEmpty()) {
        emit terminado(false, tr("Falta la dirección de la PC."), 0, 0);
        return;
    }
    conectar(host.trimmed(), puerto ? puerto : perfil_.puerto);
}

void ClienteSincBase::sincronizar(const QString &dispositivo)
{
    if (ocupado()) return;
    const auto p = base_.par(dispositivo);
    if (!p) {
        emit terminado(false, tr("Primero hay que emparejar el celular con la PC."), 0, 0);
        return;
    }
    if (!sinc::secretoVigente(p->secreto)) {
        emit terminado(false, tr("La sincronización ahora va cifrada y hay que emparejar de nuevo con la PC "
                                 "(una sola vez): Olvidar esta PC y emparejar con un código nuevo."), 0, 0);
        return;
    }
    modo_ = Modo::Sincronizar;
    dispositivoPar_ = p->dispositivo;
    secreto_ = p->secreto;
    reintentado_ = false;
    conectar(p->host, quint16(p->puerto ? p->puerto : perfil_.puerto));
}

void ClienteSincBase::conectar(const QString &host, quint16 puerto)
{
    host_ = host;
    puerto_ = puerto;
    paso_ = Paso::Hola;
    recibidas_ = enviadas_ = rondas_ = 0;
    canal_ = cripto::Canal();
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected, this, &ClienteSincBase::alConectar);
    connect(socket_, &QTcpSocket::readyRead, this, &ClienteSincBase::alLeer);
    connect(socket_, &QTcpSocket::errorOccurred, this, &ClienteSincBase::alError);
    emit estado(tr("Conectando con %1…").arg(host));
    reloj_->start(ESPERA_CONEXION_MS);
    socket_->connectToHost(host, puerto);
}

void ClienteSincBase::enviar(const QJsonObject &m)
{
    if (socket_) socket_->write(codificar(canal_, m));
}

void ClienteSincBase::alConectar()
{
    reloj_->start(ESPERA_MENSAJE_MS);
    nonceCliente_ = hex(cripto::aleatorio(16));
    secretaEfimera_ = cripto::aleatorio(32);
    if (modo_ == Modo::Emparejar)
        publicaPropia_ = cripto::publica(secretaEfimera_, cripto::generadorEmparejar(codigo_, contextoEmparejar(base_.dispositivo(), nonceCliente_)));
    else
        publicaPropia_ = cripto::publica(secretaEfimera_);
    enviar(QJsonObject{{"msg", "hola"}, {"v", sinc::VERSION},
                       {"modo", modo_ == Modo::Emparejar ? QStringLiteral("emparejar") : QStringLiteral("auth")},
                       {"disp", base_.dispositivo()}, {"nombre", base_.nombreDispositivo()},
                       {"nonce", nonceCliente_}, {"pub", hex(publicaPropia_)}});
}

void ClienteSincBase::alLeer()
{
    const bool bien = leerLineas(socket_, [this](const QByteArray &linea) {
        QJsonObject m;
        if (!decodificar(canal_, linea, m)) return false;
        reloj_->start(ESPERA_MENSAJE_MS);
        procesar(m);
        return true;
    });
    if (!bien) terminar(false, tr("La PC mandó algo que no se entiende (o no es la PC emparejada)."));
}

void ClienteSincBase::alError()
{
    if (!socket_ || paso_ == Paso::Fin) return;
    const QString error = socket_->errorString();
    if (paso_ == Paso::Hola && socket_->state() != QAbstractSocket::ConnectedState) {
        if (reintentarBuscando()) return;
        terminar(false, tr("No se pudo conectar con la PC (%1).").arg(error) + u' ' + textoNoContesta());
        return;
    }
    terminar(false, tr("Se cortó la conexión con la PC: %1").arg(error));
}

// Si la PC cambió de dirección (el router le dio otra), se la busca en la red y se reintenta una vez
bool ClienteSincBase::reintentarBuscando()
{
    if (reintentado_ || modo_ != Modo::Sincronizar) return false;
    reintentado_ = true;
    reloj_->stop();
    if (socket_) {
        socket_->disconnect(this);
        socket_->deleteLater();
        socket_ = nullptr;
    }
    emit estado(tr("Buscando la PC en la red…"));
    buscandoParaReintentar_ = true;
    buscar(2000);
    return true;
}

void ClienteSincBase::procesar(const QJsonObject &m)
{
    const QString msg = m.value(QLatin1String("msg")).toString();
    if (msg == QLatin1String("error")) {
        terminar(false, m.value(QLatin1String("texto")).toString());
        return;
    }
    switch (paso_) {
    case Paso::Hola: {
        if (msg != QLatin1String("hola")) return terminar(false, tr("La PC contestó algo inesperado."));
        if (m.value(QLatin1String("v")).toInt() != sinc::VERSION)
            return terminar(false, tr("La versión del programa en el celular y en la PC no es la misma: actualizá las dos."));
        dispositivoServidor_ = m.value(QLatin1String("disp")).toString();
        nombreServidor_ = m.value(QLatin1String("nombre")).toString();
        const QString nonceServidor = m.value(QLatin1String("nonce")).toString();
        const QByteArray pubServidor = deHex(m.value(QLatin1String("pub")).toString(), 32);
        if (modo_ == Modo::Sincronizar && dispositivoServidor_ != dispositivoPar_)
            return terminar(false, tr("En %1 hay otra PC («%2»), no la emparejada.").arg(host_, nombreServidor_));
        if (nonceServidor.size() < 32 || pubServidor.isEmpty())
            return terminar(false, tr("La PC contestó algo inesperado."));

        QByteArray base;
        const QByteArray dh = cripto::compartido(secretaEfimera_, pubServidor);
        cripto::borrar(secretaEfimera_);
        if (!dh.isEmpty())
            base = modo_ == Modo::Emparejar
                ? dh
                : cripto::hash(deHex(secreto_.mid(PREFIJO_SECRETO.size()), 32), "sesion|" + dh);
        if (base.isEmpty()) return terminar(false, tr("La PC contestó algo inesperado."));
        const QString modo = modo_ == Modo::Emparejar ? QStringLiteral("emparejar") : QStringLiteral("auth");
        const QByteArray t = transcripcion(modo, base_.dispositivo(), dispositivoServidor_, nonceCliente_,
                                           nonceServidor, publicaPropia_, pubServidor);
        const Claves claves = derivar(base, t);
        cripto::borrar(base);
        confirmacion_ = claves.confirmacion;
        if (modo_ == Modo::Emparejar)
            secreto_ = PREFIJO_SECRETO + hex(claves.secretoNuevo);
        canal_.iniciar(claves.clienteAServidor, claves.servidorACliente);
        enviar(QJsonObject{{"msg", "prueba"}, {"prueba", hex(cripto::hash(confirmacion_, "cliente"))}});
        paso_ = Paso::Auth;
        return;
    }
    case Paso::Auth: {
        if (msg != QLatin1String("ok")) return terminar(false, tr("La PC contestó algo inesperado."));
        const QByteArray prueba = deHex(m.value(QLatin1String("prueba")).toString(), 32);
        if (!cripto::iguales(prueba, cripto::hash(confirmacion_, "servidor")))
            return terminar(false, tr("La PC no pudo probar que es la emparejada. Emparejá de nuevo."));
        Par p = base_.par(dispositivoServidor_).value_or(Par{});
        if (modo_ == Modo::Emparejar) {                 // par nuevo: empieza de cero
            p = Par{};
            p.dispositivo = dispositivoServidor_;
            p.secreto = secreto_;
            dispositivoPar_ = dispositivoServidor_;
        }
        p.nombre = nombreServidor_;
        p.host = host_;
        p.puerto = puerto_;
        base_.guardarPar(p);
        emit estado(tr("Sincronizando con %1…").arg(nombreServidor_));
        paso_ = Paso::Cambios;
        enviarCambios(base_.cambiosDesde(p.enviado));
        return;
    }
    case Paso::Cambios: {
        if (msg != QLatin1String("cambios")) return terminar(false, tr("La PC contestó algo inesperado."));
        // ¿Se cargó algo acá mientras se esperaba la respuesta? Entonces eso todavía no viajó
        if (base_.secuencia() != marcaSecuencia_) huboCambiosLocales_ = true;
        const ResultadoAplicar r = base_.aplicarCambios(m.value(QLatin1String("filas")).toArray());
        if (!r.ok) return terminar(false, tr("No se pudieron guardar los cambios: %1").arg(r.error));
        marcaSecuencia_ = base_.secuencia();
        recibidas_ += r.aplicadas;
        Par p = base_.par(dispositivoPar_).value_or(Par{});
        p.recibido = m.value(QLatin1String("hasta")).toVariant().toLongLong();
        if (!r.ganadorasLocales.isEmpty() && rondas_ < 4) {
            // Algo que acá quedó distinto (más nuevo, o con nombre ajustado): otra vuelta para mandarlo
            base_.guardarPar(p);
            enviarCambios(base_.filasPorUuid(r.ganadorasLocales));
            return;
        }
        // Lo recién recibido la PC ya lo tiene; lo cargado acá durante la sincronización va la próxima vez
        p.enviado = huboCambiosLocales_ ? hastaPrimerEnvio_ : base_.secuencia();
        p.ultimaSinc = QDateTime::currentDateTime();
        base_.guardarPar(p);
        enviar(QJsonObject{{"msg", "listo"}});
        paso_ = Paso::Fin;
        if (socket_) socket_->disconnectFromHost();
        QString texto = tr("Sincronizado con %1.").arg(nombreServidor_);
        if (recibidas_ || enviadas_) texto += QLatin1Char(' ') + tr("Recibidos: %1 · Enviados: %2").arg(recibidas_).arg(enviadas_);
        else texto += QLatin1Char(' ') + tr("No había nada nuevo.");
        terminar(true, texto);
        return;
    }
    case Paso::Fin:
        return;
    }
}

void ClienteSincBase::enviarCambios(const QJsonArray &filas)
{
    if (rondas_ == 0) {
        hastaPrimerEnvio_ = base_.secuencia();
        huboCambiosLocales_ = false;
    }
    marcaSecuencia_ = base_.secuencia();
    ++rondas_;
    enviadas_ += filas.size();
    const auto p = base_.par(dispositivoPar_);
    enviar(QJsonObject{{"msg", "cambios"}, {"desde", p ? p->recibido : 0}, {"hasta", base_.secuencia()}, {"filas", filas}});
}

void ClienteSincBase::terminar(bool ok, const QString &mensaje)
{
    reloj_->stop();
    cripto::borrar(secretaEfimera_);
    cripto::borrar(confirmacion_);
    if (socket_) {
        socket_->disconnect(this);
        if (ok && socket_->state() != QAbstractSocket::UnconnectedState) {
            // que termine de salir el «listo» antes de soltar la conexión
            connect(socket_, &QAbstractSocket::disconnected, socket_, &QObject::deleteLater);
        } else {
            socket_->abort();
            socket_->deleteLater();
        }
        socket_ = nullptr;
    }
    paso_ = Paso::Fin;
    emit terminado(ok, mensaje, recibidas_, enviadas_);
}

// ---------------------------------------------------------------- búsqueda en la red

void ClienteSincBase::buscar(int milisegundos)
{
    if (udp_) return;
    vistos_.clear();
    udp_ = new QUdpSocket(this);
    if (!udp_->bind(QHostAddress::AnyIPv4, 0)) {
        delete udp_;
        udp_ = nullptr;
        emit busquedaTerminada();
        return;
    }
    connect(udp_, &QUdpSocket::readyRead, this, [this] {
        while (udp_ && udp_->hasPendingDatagrams()) {
            QByteArray datos(int(udp_->pendingDatagramSize()), Qt::Uninitialized);
            QHostAddress desde;
            udp_->readDatagram(datos.data(), datos.size(), &desde);
            const QJsonObject o = QJsonDocument::fromJson(datos).object();
            if (o.value(perfil_.claveBusqueda).toInt() != sinc::VERSION) continue;
            const QString host = QHostAddress(desde.toIPv4Address()).toString();
            const QString disp = o.value(QLatin1String("disp")).toString();
            const quint16 puerto = quint16(o.value(QLatin1String("puerto")).toInt());
            if (vistos_.contains(disp + host)) continue;
            vistos_.insert(disp + host);
            emit encontrado(o.value(QLatin1String("nombre")).toString(), host, puerto, disp);
            if (buscandoParaReintentar_ && disp == dispositivoPar_) {   // ¡apareció la PC en otra dirección!
                buscandoParaReintentar_ = false;
                if (auto p = base_.par(disp)) {
                    p->host = host;
                    p->puerto = puerto;
                    base_.guardarPar(*p);
                }
                udp_->deleteLater();
                udp_ = nullptr;
                emit busquedaTerminada();
                conectar(host, puerto);
                return;
            }
        }
    });
    // A todas las redes del equipo (y a sí mismo, para cuando la PC y el «celular» son la misma máquina)
    QList<QHostAddress> destinos{QHostAddress::Broadcast, QHostAddress::LocalHost};
    for (const QNetworkInterface &i : QNetworkInterface::allInterfaces())
        for (const QNetworkAddressEntry &e : i.addressEntries())
            if (!e.broadcast().isNull()) destinos << e.broadcast();
    for (const QHostAddress &d : destinos) udp_->writeDatagram(pedidoBusqueda(perfil_), d, perfil_.puertoBusqueda);

    QTimer::singleShot(milisegundos, this, [this, u = QPointer<QUdpSocket>(udp_)] {
        if (!u || u != udp_) return;                      // ya terminó (encontró la PC al reintentar)
        udp_->deleteLater();
        udp_ = nullptr;
        emit busquedaTerminada();
        if (buscandoParaReintentar_) {
            buscandoParaReintentar_ = false;
            terminar(false, tr("No se encontró la PC.") + u' ' + textoNoContesta());
        }
    });
}
