#pragma once
#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <memory>

class Stock;
class QTcpServer;
class QTcpSocket;
class QUdpSocket;
class QTimer;

/**
 * Sincronización directa entre la PC (servidor) y el celular (cliente), sin nube ni stock.
 *
 *   - Funciona en el mismo Wi-Fi, o desde cualquier lado si los dos están en la misma red de Tailscale.
 *   - Emparejar (una sola vez): la PC muestra un código de 6 dígitos que vale 5 minutos; el celular
 *     lo escribe. De ahí sale una clave que guardan los dos y nunca viaja por la red.
 *   - Cada vez: se prueban mutuamente que tienen la clave (HMAC-SHA256 sobre números al azar) e
 *     intercambian solo lo que cambió desde la última vez. Si el mismo dato se cambió en los dos,
 *     gana el cambio más nuevo (ver Stock::aplicarCambios).
 *   - Los datos no van cifrados: en la casa no hace falta, y por Internet Tailscale ya los cifra.
 *
 * Protocolo: TCP, un mensaje JSON por línea.
 *   C→S {"msg":"hola","v":1,"disp":..,"nombre":..,"nonce":..}      S→C {"msg":"hola",...igual}
 *   C→S {"msg":"emparejar","prueba":..}  o  {"msg":"auth","prueba":..}
 *   S→C {"msg":"emparejado","prueba":..} o  {"msg":"ok","prueba":..}   (o {"msg":"error","texto":..})
 *   C→S {"msg":"cambios","desde":N,"hasta":M,"filas":[..]}          S→C {"msg":"cambios","hasta":K,"filas":[..]}
 *   (se repite si quedó algo por mandar)                            C→S {"msg":"listo"}
 * Búsqueda en la red: el celular manda por UDP «GS-BUSCAR 1» a todos; la PC contesta su nombre y puerto.
 */
namespace sinc {
constexpr quint16 PUERTO = 45464;
constexpr quint16 PUERTO_BUSQUEDA = 45465;
constexpr int VERSION = 1;
constexpr int MINUTOS_CODIGO = 5;
constexpr int INTENTOS_CODIGO = 5;
QStringList direccionesLocales();   // IPv4 de este equipo (incluida la de Tailscale, 100.x.x.x)
}

/** Lado PC: escucha conexiones del celular mientras el programa está abierto. */
class ServidorSinc : public QObject
{
    Q_OBJECT
public:
    explicit ServidorSinc(Stock& stock, QObject* parent = nullptr);
    ~ServidorSinc() override;

    bool iniciar(quint16 puerto = sinc::PUERTO, bool responderBusquedas = true);   // puerto 0 = cualquiera (pruebas)
    void detener();
    bool activo() const;
    quint16 puerto() const;
    QString error() const { return error_; }

    QString nuevoCodigo();                    // 6 dígitos, vale sinc::MINUTOS_CODIGO minutos
    void cancelarCodigo();
    QString codigo() const;                   // vacío si no hay uno vigente
    QDateTime codigoVence() const { return codigoVence_; }

signals:
    void emparejado(const QString& nombre);
    void sincronizado(const QString& nombre, int recibidas, int enviadas);
    void fallo(const QString& mensaje);
    void codigoCambiado();

private:
    struct Sesion;
    void nuevaConexion();
    void procesar(const std::shared_ptr<Sesion>& s, const QJsonObject& m);
    void responderBusqueda();

    Stock& stock_;
    QTcpServer* servidor_ = nullptr;
    QUdpSocket* busqueda_ = nullptr;
    QString error_;
    QString codigo_;
    QDateTime codigoVence_;
    int intentosFallidos_ = 0;
};

/** Lado celular: se conecta a la PC para emparejar o sincronizar. */
class ClienteSinc : public QObject
{
    Q_OBJECT
public:
    explicit ClienteSinc(Stock& stock, QObject* parent = nullptr);
    ~ClienteSinc() override;

    void emparejar(const QString& host, quint16 puerto, const QString& codigo);
    void sincronizar(const QString& dispositivo);     // con una PC ya emparejada
    void buscar(int milisegundos = 2500);             // buscar PCs en la red local
    bool ocupado() const { return socket_ != nullptr; }

signals:
    void estado(const QString& texto);
    void terminado(bool ok, const QString& mensaje, int recibidas, int enviadas);
    void encontrado(const QString& nombre, const QString& host, quint16 puerto, const QString& dispositivo);
    void busquedaTerminada();

private:
    enum class Modo { Emparejar, Sincronizar };
    void conectar(const QString& host, quint16 puerto);
    void alConectar();
    void alLeer();
    void alError();
    void procesar(const QJsonObject& m);
    void enviar(const QJsonObject& m);
    void enviarCambios(const QJsonArray& filas);
    void terminar(bool ok, const QString& mensaje);
    bool reintentarBuscando();

    Stock& stock_;
    QTcpSocket* socket_ = nullptr;
    QTimer* reloj_ = nullptr;
    QUdpSocket* udp_ = nullptr;
    QSet<QString> vistos_;

    Modo modo_ = Modo::Sincronizar;
    QString host_, codigo_, dispositivoPar_;
    quint16 puerto_ = 0;
    QByteArray nonceCliente_, nonceServidor_;
    QString dispositivoServidor_, nombreServidor_, secreto_;
    enum class Paso { Hola, Auth, Cambios, Fin } paso_ = Paso::Hola;
    int recibidas_ = 0, enviadas_ = 0, rondas_ = 0;
    qint64 hastaPrimerEnvio_ = 0, marcaSecuencia_ = 0;   // para no perder lo que se carga mientras sincroniza
    bool huboCambiosLocales_ = false;
    bool reintentado_ = false;
    bool buscandoParaReintentar_ = false;
};
