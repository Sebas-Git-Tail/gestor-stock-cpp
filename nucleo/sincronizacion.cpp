#include "sincronizacion.h"
#include "db.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
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
const QByteArray PEDIDO_BUSQUEDA = QByteArrayLiteral("GS-BUSCAR 1");

QString nonce()
{
    QByteArray b(16, Qt::Uninitialized);
    for (char& c : b) c = char(QRandomGenerator::system()->bounded(256));
    return QString::fromLatin1(b.toHex());
}

QString hmac(const QString& clave, const QString& mensaje)
{
    return QString::fromLatin1(QMessageAuthenticationCode::hash(mensaje.toUtf8(), clave.toUtf8(), QCryptographicHash::Sha256).toHex());
}

// Comparación que tarda lo mismo aunque difieran al principio (no da pistas a quien prueba claves)
bool igualesSeguro(const QString& a, const QString& b)
{
    const QByteArray x = a.toLatin1(), y = b.toLatin1();
    if (x.size() != y.size()) return false;
    unsigned char dif = 0;
    for (int i = 0; i < x.size(); ++i) dif |= static_cast<unsigned char>(x[i] ^ y[i]);
    return dif == 0;
}

QByteArray linea(const QJsonObject& o)
{
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}

// Lo que mandó el otro, una línea JSON por vez. Devuelve false si llegó basura o algo enorme.
bool leerLineas(QTcpSocket* s, const std::function<void(const QJsonObject&)>& procesar)
{
    while (s && s->canReadLine()) {
        const QByteArray l = s->readLine().trimmed();
        if (l.isEmpty()) continue;
        QJsonParseError e;
        const QJsonDocument d = QJsonDocument::fromJson(l, &e);
        if (e.error != QJsonParseError::NoError || !d.isObject()) return false;
        procesar(d.object());
    }
    return !s || s->bytesAvailable() < MAX_LINEA;
}

QString soloDigitos(const QString& s)
{
    QString d;
    for (const QChar c : s) if (c.isDigit()) d += c;
    return d;
}

} // namespace

QStringList sinc::direccionesLocales()
{
    QStringList lista;
    for (const QHostAddress& a : QNetworkInterface::allAddresses()) {
        if (a.protocol() != QAbstractSocket::IPv4Protocol || a.isLoopback()) continue;
        if (a.isInSubnet(QHostAddress(QStringLiteral("169.254.0.0")), 16)) continue;   // sin red de verdad
        lista << a.toString();
    }
    return lista;
}

// ====================================================================== servidor (PC)

struct ServidorSinc::Sesion {
    QPointer<QTcpSocket> socket;
    QTimer* reloj = nullptr;
    enum class Paso { Hola, Auth, Cambios } paso = Paso::Hola;
    QString nonceServidor, nonceCliente, dispositivo, nombre, secreto;
    int recibidas = 0, enviadas = 0;
};

ServidorSinc::ServidorSinc(Stock& stock, QObject* parent)
    : QObject(parent), stock_(stock)
{
}

ServidorSinc::~ServidorSinc() = default;

bool ServidorSinc::iniciar(quint16 puerto, bool responderBusquedas)
{
    detener();
    servidor_ = new QTcpServer(this);
    if (!servidor_->listen(QHostAddress::Any, puerto)) {
        error_ = tr("No se pudo usar el puerto %1: %2").arg(puerto).arg(servidor_->errorString());
        delete servidor_;
        servidor_ = nullptr;
        return false;
    }
    connect(servidor_, &QTcpServer::newConnection, this, &ServidorSinc::nuevaConexion);
    if (responderBusquedas) {
        busqueda_ = new QUdpSocket(this);
        if (busqueda_->bind(QHostAddress::AnyIPv4, sinc::PUERTO_BUSQUEDA, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
            connect(busqueda_, &QUdpSocket::readyRead, this, &ServidorSinc::responderBusqueda);
        // si no se puede, igual se sincroniza: solo hay que escribir la dirección a mano en el celular
    }
    error_.clear();
    return true;
}

void ServidorSinc::detener()
{
    delete servidor_;
    servidor_ = nullptr;
    delete busqueda_;
    busqueda_ = nullptr;
}

bool ServidorSinc::activo() const
{
    return servidor_ && servidor_->isListening();
}

quint16 ServidorSinc::puerto() const
{
    return servidor_ ? servidor_->serverPort() : 0;
}

QString ServidorSinc::nuevoCodigo()
{
    codigo_ = QStringLiteral("%1").arg(QRandomGenerator::system()->bounded(1000000), 6, 10, QLatin1Char('0'));
    codigoVence_ = QDateTime::currentDateTime().addSecs(sinc::MINUTOS_CODIGO * 60);
    intentosFallidos_ = 0;
    emit codigoCambiado();
    return codigo_;
}

void ServidorSinc::cancelarCodigo()
{
    codigo_.clear();
    codigoVence_ = QDateTime();
    emit codigoCambiado();
}

QString ServidorSinc::codigo() const
{
    if (codigo_.isEmpty() || QDateTime::currentDateTime() > codigoVence_) return QString();
    return codigo_;
}

void ServidorSinc::responderBusqueda()
{
    while (busqueda_ && busqueda_->hasPendingDatagrams()) {
        QByteArray datos(int(busqueda_->pendingDatagramSize()), Qt::Uninitialized);
        QHostAddress desde;
        quint16 puertoDesde = 0;
        busqueda_->readDatagram(datos.data(), datos.size(), &desde, &puertoDesde);
        if (!datos.startsWith(PEDIDO_BUSQUEDA) || !activo()) continue;
        const QJsonObject r{{"gs", sinc::VERSION}, {"nombre", stock_.nombreDispositivo()},
                            {"disp", stock_.dispositivo()}, {"puerto", int(puerto())}};
        busqueda_->writeDatagram(QJsonDocument(r).toJson(QJsonDocument::Compact), desde, puertoDesde);
    }
}

void ServidorSinc::nuevaConexion()
{
    while (servidor_ && servidor_->hasPendingConnections()) {
        QTcpSocket* socket = servidor_->nextPendingConnection();
        auto s = std::make_shared<Sesion>();
        s->socket = socket;
        s->reloj = new QTimer(socket);
        s->reloj->setSingleShot(true);
        s->reloj->start(ESPERA_MENSAJE_MS);
        connect(s->reloj, &QTimer::timeout, socket, &QTcpSocket::abort);   // se colgó: se corta
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, this, [this, s] {
            const bool bien = leerLineas(s->socket, [&](const QJsonObject& m) {
                if (s->reloj) s->reloj->start(ESPERA_MENSAJE_MS);
                procesar(s, m);
            });
            if (!bien && s->socket) s->socket->abort();
        });
    }
}

void ServidorSinc::procesar(const std::shared_ptr<Sesion>& s, const QJsonObject& m)
{
    if (!s->socket || s->socket->state() != QAbstractSocket::ConnectedState) return;
    const QString msg = m.value(QLatin1String("msg")).toString();
    auto enviar = [&](const QJsonObject& o) { s->socket->write(linea(o)); };
    auto rechazar = [&](const QString& texto) {
        enviar(QJsonObject{{"msg", "error"}, {"texto", texto}});
        s->socket->disconnectFromHost();
        emit fallo(texto);
    };

    switch (s->paso) {
    case Sesion::Paso::Hola:
        if (msg != QLatin1String("hola")) return rechazar(tr("Mensaje inesperado."));
        if (m.value(QLatin1String("v")).toInt() != sinc::VERSION)
            return rechazar(tr("La versión del programa en el celular y en la PC no es la misma: actualizá las dos."));
        s->nonceCliente = m.value(QLatin1String("nonce")).toString();
        s->dispositivo = m.value(QLatin1String("disp")).toString();
        s->nombre = m.value(QLatin1String("nombre")).toString();
        if (s->nonceCliente.size() < 16 || s->dispositivo.isEmpty()) return rechazar(tr("Saludo incompleto."));
        s->nonceServidor = nonce();
        enviar(QJsonObject{{"msg", "hola"}, {"v", sinc::VERSION}, {"disp", stock_.dispositivo()},
                           {"nombre", stock_.nombreDispositivo()}, {"nonce", s->nonceServidor}});
        s->paso = Sesion::Paso::Auth;
        return;

    case Sesion::Paso::Auth: {
        const QString prueba = m.value(QLatin1String("prueba")).toString();
        if (msg == QLatin1String("emparejar")) {
            const QString cod = codigo();
            if (cod.isEmpty())
                return rechazar(tr("En la PC no hay un código de emparejamiento activo (o ya venció). "
                                   "Tocá «Emparejar un celular» en la PC y probá de nuevo."));
            const QString esperada = hmac(cod, QStringLiteral("emparejar|%1|%2|%3").arg(s->nonceServidor, s->nonceCliente, s->dispositivo));
            if (!igualesSeguro(prueba, esperada)) {
                if (++intentosFallidos_ >= sinc::INTENTOS_CODIGO) cancelarCodigo();   // demasiados intentos: se anula
                return rechazar(tr("El código no es correcto."));
            }
            s->secreto = hmac(cod, QStringLiteral("secreto|%1|%2").arg(s->nonceServidor, s->nonceCliente));
            Par p = stock_.par(s->dispositivo).value_or(Par{});
            p.dispositivo = s->dispositivo;
            p.nombre = s->nombre;
            p.secreto = s->secreto;
            stock_.guardarPar(p);
            cancelarCodigo();
            emit emparejado(s->nombre);
            enviar(QJsonObject{{"msg", "emparejado"},
                               {"prueba", hmac(s->secreto, QStringLiteral("servidor|%1|%2").arg(s->nonceCliente, s->nonceServidor))}});
        } else if (msg == QLatin1String("auth")) {
            const auto p = stock_.par(s->dispositivo);
            if (!p) return rechazar(tr("Este celular no está emparejado con «%1» (o se quitó). Emparejalo de nuevo.").arg(stock_.nombreDispositivo()));
            const QString esperada = hmac(p->secreto, QStringLiteral("cliente|%1|%2").arg(s->nonceServidor, s->nonceCliente));
            if (!igualesSeguro(prueba, esperada)) return rechazar(tr("La clave no coincide. Emparejá el celular de nuevo."));
            s->secreto = p->secreto;
            enviar(QJsonObject{{"msg", "ok"},
                               {"prueba", hmac(s->secreto, QStringLiteral("servidor|%1|%2").arg(s->nonceCliente, s->nonceServidor))}});
        } else {
            return rechazar(tr("Mensaje inesperado."));
        }
        s->paso = Sesion::Paso::Cambios;
        return;
    }

    case Sesion::Paso::Cambios:
        if (msg == QLatin1String("cambios")) {
            // Lo que el celular todavía no tiene (calculado ANTES de aplicar lo suyo, para no devolvérselo)
            QJsonArray salida = stock_.cambiosDesde(m.value(QLatin1String("desde")).toVariant().toLongLong());
            const ResultadoAplicar r = stock_.aplicarCambios(m.value(QLatin1String("filas")).toArray());
            if (!r.ok) return rechazar(tr("No se pudieron guardar los cambios en la PC: %1").arg(r.error));
            // Fuera lo que se acaba de tomar del celular; adentro lo que acá es más nuevo o se tuvo que ajustar
            QSet<QString> quitar(r.aceptadas.begin(), r.aceptadas.end());
            for (const QString& u : r.ganadorasLocales) quitar.insert(u);
            QJsonArray final;
            for (const QJsonValue& v : salida)
                if (!quitar.contains(v.toObject().value(QLatin1String("u")).toString())) final << v;
            for (const QJsonValue& v : stock_.filasPorUuid(r.ganadorasLocales)) final << v;
            enviar(QJsonObject{{"msg", "cambios"}, {"hasta", stock_.secuencia()}, {"filas", final}});
            s->recibidas += r.aplicadas;
            s->enviadas += final.size();
        } else if (msg == QLatin1String("listo")) {
            if (auto p = stock_.par(s->dispositivo)) {           // (si mientras tanto lo quitaron, no vuelve)
                p->nombre = s->nombre;
                p->ultimaSinc = QDateTime::currentDateTime();
                stock_.guardarPar(*p);
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

ClienteSinc::ClienteSinc(Stock& stock, QObject* parent)
    : QObject(parent), stock_(stock), reloj_(new QTimer(this))
{
    reloj_->setSingleShot(true);
    connect(reloj_, &QTimer::timeout, this, [this] {
        if (paso_ == Paso::Hola && socket_ && socket_->state() != QAbstractSocket::ConnectedState) {
            if (!reintentarBuscando()) terminar(false, tr("La PC no contesta. ¿Está abierto Gestor de Stock en la PC, con la sincronización activada?"));
            return;
        }
        terminar(false, tr("La PC dejó de contestar."));
    });
}

ClienteSinc::~ClienteSinc()
{
    if (socket_) socket_->abort();
}

void ClienteSinc::emparejar(const QString& host, quint16 puerto, const QString& codigo)
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
    conectar(host.trimmed(), puerto ? puerto : sinc::PUERTO);
}

void ClienteSinc::sincronizar(const QString& dispositivo)
{
    if (ocupado()) return;
    const auto p = stock_.par(dispositivo);
    if (!p) {
        emit terminado(false, tr("Primero hay que emparejar el celular con la PC."), 0, 0);
        return;
    }
    modo_ = Modo::Sincronizar;
    dispositivoPar_ = p->dispositivo;
    secreto_ = p->secreto;
    reintentado_ = false;
    conectar(p->host, quint16(p->puerto ? p->puerto : sinc::PUERTO));
}

void ClienteSinc::conectar(const QString& host, quint16 puerto)
{
    host_ = host;
    puerto_ = puerto;
    paso_ = Paso::Hola;
    recibidas_ = enviadas_ = rondas_ = 0;
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected, this, &ClienteSinc::alConectar);
    connect(socket_, &QTcpSocket::readyRead, this, &ClienteSinc::alLeer);
    connect(socket_, &QTcpSocket::errorOccurred, this, &ClienteSinc::alError);
    emit estado(tr("Conectando con %1…").arg(host));
    reloj_->start(ESPERA_CONEXION_MS);
    socket_->connectToHost(host, puerto);
}

void ClienteSinc::enviar(const QJsonObject& m)
{
    if (socket_) socket_->write(linea(m));
}

void ClienteSinc::alConectar()
{
    reloj_->start(ESPERA_MENSAJE_MS);
    nonceCliente_ = nonce().toLatin1();
    enviar(QJsonObject{{"msg", "hola"}, {"v", sinc::VERSION}, {"disp", stock_.dispositivo()},
                       {"nombre", stock_.nombreDispositivo()}, {"nonce", QString::fromLatin1(nonceCliente_)}});
}

void ClienteSinc::alLeer()
{
    const bool bien = leerLineas(socket_, [this](const QJsonObject& m) {
        reloj_->start(ESPERA_MENSAJE_MS);
        procesar(m);
    });
    if (!bien) terminar(false, tr("La PC mandó algo que no se entiende."));
}

void ClienteSinc::alError()
{
    if (!socket_ || paso_ == Paso::Fin) return;
    const QString error = socket_->errorString();
    if (paso_ == Paso::Hola && socket_->state() != QAbstractSocket::ConnectedState) {
        if (reintentarBuscando()) return;
        terminar(false, tr("No se pudo conectar con la PC (%1). ¿Está abierto Gestor de Stock en la PC, con la "
                           "sincronización activada? ¿Están en el mismo Wi-Fi (o los dos con Tailscale)?").arg(error));
        return;
    }
    terminar(false, tr("Se cortó la conexión con la PC: %1").arg(error));
}

// Si la PC cambió de dirección (el router le dio otra), se la busca en la red y se reintenta una vez
bool ClienteSinc::reintentarBuscando()
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

void ClienteSinc::procesar(const QJsonObject& m)
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
        nonceServidor_ = m.value(QLatin1String("nonce")).toString().toLatin1();
        const QString ns = QString::fromLatin1(nonceServidor_), nc = QString::fromLatin1(nonceCliente_);
        if (modo_ == Modo::Sincronizar && dispositivoServidor_ != dispositivoPar_)
            return terminar(false, tr("En %1 hay otra PC («%2»), no la emparejada.").arg(host_, nombreServidor_));
        if (modo_ == Modo::Emparejar) {
            enviar(QJsonObject{{"msg", "emparejar"},
                               {"prueba", hmac(codigo_, QStringLiteral("emparejar|%1|%2|%3").arg(ns, nc, stock_.dispositivo()))}});
            secreto_ = hmac(codigo_, QStringLiteral("secreto|%1|%2").arg(ns, nc));
        } else {
            enviar(QJsonObject{{"msg", "auth"}, {"prueba", hmac(secreto_, QStringLiteral("cliente|%1|%2").arg(ns, nc))}});
        }
        paso_ = Paso::Auth;
        return;
    }
    case Paso::Auth: {
        if (msg != QLatin1String("ok") && msg != QLatin1String("emparejado")) return terminar(false, tr("La PC contestó algo inesperado."));
        const QString esperada = hmac(secreto_, QStringLiteral("servidor|%1|%2").arg(QString::fromLatin1(nonceCliente_), QString::fromLatin1(nonceServidor_)));
        if (!igualesSeguro(m.value(QLatin1String("prueba")).toString(), esperada))
            return terminar(false, tr("La PC no pudo probar que es la emparejada. Emparejá de nuevo."));
        Par p = stock_.par(dispositivoServidor_).value_or(Par{});
        if (modo_ == Modo::Emparejar) {                 // par nuevo: empieza de cero
            p = Par{};
            p.dispositivo = dispositivoServidor_;
            p.secreto = secreto_;
            dispositivoPar_ = dispositivoServidor_;
        }
        p.nombre = nombreServidor_;
        p.host = host_;
        p.puerto = puerto_;
        stock_.guardarPar(p);
        emit estado(tr("Sincronizando con %1…").arg(nombreServidor_));
        paso_ = Paso::Cambios;
        enviarCambios(stock_.cambiosDesde(p.enviado));
        return;
    }
    case Paso::Cambios: {
        if (msg != QLatin1String("cambios")) return terminar(false, tr("La PC contestó algo inesperado."));
        // ¿Se cargó algo acá mientras se esperaba la respuesta? Entonces eso todavía no viajó
        if (stock_.secuencia() != marcaSecuencia_) huboCambiosLocales_ = true;
        const ResultadoAplicar r = stock_.aplicarCambios(m.value(QLatin1String("filas")).toArray());
        if (!r.ok) return terminar(false, tr("No se pudieron guardar los cambios: %1").arg(r.error));
        marcaSecuencia_ = stock_.secuencia();
        recibidas_ += r.aplicadas;
        Par p = stock_.par(dispositivoPar_).value_or(Par{});
        p.recibido = m.value(QLatin1String("hasta")).toVariant().toLongLong();
        if (!r.ganadorasLocales.isEmpty() && rondas_ < 4) {
            // Algo que acá quedó distinto (más nuevo, o con nombre ajustado): otra vuelta para mandarlo
            stock_.guardarPar(p);
            enviarCambios(stock_.filasPorUuid(r.ganadorasLocales));
            return;
        }
        // Lo recién recibido la PC ya lo tiene; lo cargado acá durante la sincronización va la próxima vez
        p.enviado = huboCambiosLocales_ ? hastaPrimerEnvio_ : stock_.secuencia();
        p.ultimaSinc = QDateTime::currentDateTime();
        stock_.guardarPar(p);
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

void ClienteSinc::enviarCambios(const QJsonArray& filas)
{
    if (rondas_ == 0) {
        hastaPrimerEnvio_ = stock_.secuencia();
        huboCambiosLocales_ = false;
    }
    marcaSecuencia_ = stock_.secuencia();
    ++rondas_;
    enviadas_ += filas.size();
    const auto p = stock_.par(dispositivoPar_);
    enviar(QJsonObject{{"msg", "cambios"}, {"desde", p ? p->recibido : 0}, {"hasta", stock_.secuencia()}, {"filas", filas}});
}

void ClienteSinc::terminar(bool ok, const QString& mensaje)
{
    reloj_->stop();
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

void ClienteSinc::buscar(int milisegundos)
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
            if (o.value(QLatin1String("gs")).toInt() != sinc::VERSION) continue;
            const QString host = QHostAddress(desde.toIPv4Address()).toString();
            const QString disp = o.value(QLatin1String("disp")).toString();
            const quint16 puerto = quint16(o.value(QLatin1String("puerto")).toInt());
            if (vistos_.contains(disp + host)) continue;
            vistos_.insert(disp + host);
            emit encontrado(o.value(QLatin1String("nombre")).toString(), host, puerto, disp);
            if (buscandoParaReintentar_ && disp == dispositivoPar_) {   // ¡apareció la PC en otra dirección!
                buscandoParaReintentar_ = false;
                if (auto p = stock_.par(disp)) {
                    p->host = host;
                    p->puerto = puerto;
                    stock_.guardarPar(*p);
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
    for (const QNetworkInterface& i : QNetworkInterface::allInterfaces())
        for (const QNetworkAddressEntry& e : i.addressEntries())
            if (!e.broadcast().isNull()) destinos << e.broadcast();
    for (const QHostAddress& d : destinos) udp_->writeDatagram(PEDIDO_BUSQUEDA, d, sinc::PUERTO_BUSQUEDA);

    QTimer::singleShot(milisegundos, this, [this, u = QPointer<QUdpSocket>(udp_)] {
        if (!u || u != udp_) return;                      // ya terminó (encontró la PC al reintentar)
        udp_->deleteLater();
        udp_ = nullptr;
        emit busquedaTerminada();
        if (buscandoParaReintentar_) {
            buscandoParaReintentar_ = false;
            terminar(false, tr("No se encontró la PC. ¿Está abierto Gestor de Stock en la PC, con la sincronización "
                               "activada? ¿Están en el mismo Wi-Fi (o los dos con Tailscale)?"));
        }
    });
}
