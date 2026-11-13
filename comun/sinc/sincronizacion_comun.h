#pragma once

#include "base.h"
#include "cripto.h"

#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <memory>

class QTcpServer;
class QTcpSocket;
class QUdpSocket;
class QTimer;

/**
 * Sincronización directa entre la PC (servidor) y el celular (cliente), sin nube ni cuentas.
 * La usan el gestor de stock y el de ingresos y gastos (cada uno con su PerfilSinc).
 *
 *   - Funciona en el mismo Wi-Fi, o desde cualquier lado si los dos están en la misma red de Tailscale.
 *   - Emparejar (una sola vez): la PC muestra un código de 6 dígitos que vale 5 minutos; el celular
 *     lo escribe. Con el código se hace un intercambio de claves (CPace, ver cripto.h): escuchando la
 *     red no se puede averiguar ni el código ni la clave, y quien se haga pasar por uno de los dos
 *     tiene un solo intento por conexión (y la PC acepta sinc::INTENTOS_CODIGO conexiones por código).
 *     De ahí sale un secreto de 256 bits que guardan los dos y nunca viaja por la red.
 *   - Cada vez: intercambio de claves nuevo mezclado con ese secreto; se prueban mutuamente que lo
 *     tienen y después TODO va cifrado (XChaCha20-Poly1305). Se intercambia solo lo que cambió desde la
 *     última vez; si el mismo dato se cambió en los dos, gana el cambio más nuevo (BaseSinc::aplicarCambios).
 *
 * Protocolo (versión 2): TCP, un mensaje por línea.
 *   C→S {"msg":"hola","v":2,"modo":"emparejar"|"auth","disp":..,"nombre":..,"nonce":..,"pub":..}   (JSON)
 *   S→C {"msg":"hola","v":2,"disp":..,"nombre":..,"nonce":..,"pub":..}                              (JSON)
 *   desde acá, cada línea es base64(MAC + JSON cifrado):
 *   C→S {"msg":"prueba","prueba":..}                 S→C {"msg":"ok","prueba":..}
 *   C→S {"msg":"cambios","desde":N,"hasta":M,"filas":[..]}   S→C {"msg":"cambios","hasta":K,"filas":[..]}
 *   (se repite si quedó algo por mandar)             C→S {"msg":"listo"}
 *   Un error se manda como {"msg":"error","texto":..} (cifrado si ya hay canal; si el otro no tiene la
 *   clave, en JSON común).
 * Búsqueda en la red: el celular manda por UDP «XX-BUSCAR 2» a todos; la PC contesta su nombre y puerto.
 */
namespace sinc {
constexpr int VERSION = 2;
constexpr int MINUTOS_CODIGO = 5;
constexpr int INTENTOS_CODIGO = 5;           // conexiones para emparejar que acepta cada código
QStringList direccionesLocales();            // IPv4 de este equipo (incluida la de Tailscale, 100.x.x.x)
bool secretoVigente(const QString &secreto); // emparejado con la versión 2 (si no, hay que emparejar de nuevo)
}

/** Lado PC: escucha conexiones del celular mientras el programa está abierto. */
class ServidorSincBase : public QObject
{
    Q_OBJECT
public:
    ServidorSincBase(BaseSinc &base, const PerfilSinc &perfil, QObject *parent = nullptr);
    ~ServidorSincBase() override;

    bool iniciar(quint16 puerto, bool responderBusquedas = true);   // puerto 0 = cualquiera (pruebas)
    bool iniciar() { return iniciar(perfil_.puerto); }
    void detener();
    bool activo() const;
    quint16 puerto() const;
    QString error() const { return error_; }

    QString nuevoCodigo();                    // 6 dígitos, vale sinc::MINUTOS_CODIGO minutos
    void cancelarCodigo();
    QString codigo() const;                   // vacío si no hay uno vigente
    QDateTime codigoVence() const { return codigoVence_; }

signals:
    void emparejado(const QString &nombre);
    void sincronizado(const QString &nombre, int recibidas, int enviadas);
    void fallo(const QString &mensaje);
    void codigoCambiado();

private:
    struct Sesion;
    void nuevaConexion();
    void procesar(const std::shared_ptr<Sesion> &s, const QJsonObject &m);
    void responderBusqueda();

    BaseSinc &base_;
    PerfilSinc perfil_;
    QTcpServer *servidor_ = nullptr;
    QUdpSocket *busqueda_ = nullptr;
    QString error_;
    QString codigo_;
    QDateTime codigoVence_;
    int intentosCodigo_ = 0;
};

/** Lado celular: se conecta a la PC para emparejar o sincronizar. */
class ClienteSincBase : public QObject
{
    Q_OBJECT
public:
    ClienteSincBase(BaseSinc &base, const PerfilSinc &perfil, QObject *parent = nullptr);
    ~ClienteSincBase() override;

    void emparejar(const QString &host, quint16 puerto, const QString &codigo);
    void sincronizar(const QString &dispositivo);     // con una PC ya emparejada
    void buscar(int milisegundos = 2500);             // buscar PCs en la red local
    bool ocupado() const { return socket_ != nullptr; }

signals:
    void estado(const QString &texto);
    void terminado(bool ok, const QString &mensaje, int recibidas, int enviadas);
    void encontrado(const QString &nombre, const QString &host, quint16 puerto, const QString &dispositivo);
    void busquedaTerminada();

private:
    enum class Modo { Emparejar, Sincronizar };
    void conectar(const QString &host, quint16 puerto);
    void alConectar();
    void alLeer();
    void alError();
    void procesar(const QJsonObject &m);
    void enviar(const QJsonObject &m);
    void enviarCambios(const QJsonArray &filas);
    void terminar(bool ok, const QString &mensaje);
    bool reintentarBuscando();
    QString textoNoContesta() const;

    BaseSinc &base_;
    PerfilSinc perfil_;
    QTcpSocket *socket_ = nullptr;
    QTimer *reloj_ = nullptr;
    QUdpSocket *udp_ = nullptr;
    QSet<QString> vistos_;

    Modo modo_ = Modo::Sincronizar;
    QString host_, codigo_, dispositivoPar_;
    quint16 puerto_ = 0;
    QString nonceCliente_;
    QByteArray secretaEfimera_, publicaPropia_, confirmacion_, clavePar_;
    QString dispositivoServidor_, nombreServidor_, secreto_;
    cripto::Canal canal_;
    enum class Paso { Hola, Auth, Cambios, Fin } paso_ = Paso::Hola;
    int recibidas_ = 0, enviadas_ = 0, rondas_ = 0;
    qint64 hastaPrimerEnvio_ = 0, marcaSecuencia_ = 0;   // para no perder lo que se carga mientras sincroniza
    bool huboCambiosLocales_ = false;
    bool reintentado_ = false;
    bool buscandoParaReintentar_ = false;
};
