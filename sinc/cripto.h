// Criptografía de la sincronización PC ↔ celular, sobre Monocypher (terceros/monocypher: dos
// archivos C, dominio público, sin depender de OpenSSL, que Qt para Android no trae).
//
//  - Emparejar: CPace sobre X25519. El código de 6 números no viaja ni se puede adivinar
//    escuchando la red: de él sale el punto de partida (Elligator) del intercambio de claves, y
//    quien no lo sabe obtiene una clave distinta. Quien se hace pasar por la PC o por el celular
//    puede probar UN código por conexión; la PC acepta pocas conexiones por código.
//  - Sincronizar (ya emparejados): X25519 con claves nuevas cada vez, mezclado con el secreto
//    que se acordó al emparejar (256 bits). Cada conexión tiene claves propias.
//  - Después del saludo todo va cifrado y autenticado (XChaCha20-Poly1305) con un contador por
//    dirección: no se puede leer, cambiar, repetir ni reordenar un mensaje.

#pragma once

#include <QByteArray>
#include <QString>

namespace cripto {

constexpr int TAM_CLAVE = 32;

QByteArray aleatorio(int bytes);

// Punto de partida del emparejamiento (CPace): Elligator2 de BLAKE2b(código, contexto)
QByteArray generadorEmparejar(const QString &codigo, const QByteArray &contexto);

// Clave pública X25519 de «secreta» desde el punto «base» (vacío = el punto estándar de X25519)
QByteArray publica(const QByteArray &secreta, const QByteArray &base = {});

// Secreto compartido X25519. Vacío si la clave del otro es inválida (punto de orden chico).
QByteArray compartido(const QByteArray &secreta, const QByteArray &publicaOtro);

// BLAKE2b con clave (32 bytes de salida por defecto)
QByteArray hash(const QByteArray &clave, const QByteArray &mensaje, int bytes = TAM_CLAVE);

// Comparación en tiempo constante
bool iguales(const QByteArray &a, const QByteArray &b);

// Borra el contenido de la memoria (claves que ya no hacen falta)
void borrar(QByteArray &datos);

// Canal cifrado: cada lado tiene una clave para mandar y otra para recibir, y un contador por
// dirección (el nonce). Un mensaje fuera de orden, repetido o cambiado no se descifra.
class Canal
{
public:
    void iniciar(const QByteArray &claveEnvio, const QByteArray &claveRecepcion);
    bool activo() const { return activo_; }
    QByteArray cifrar(const QByteArray &plano);                 // devuelve base64 (una línea)
    bool descifrar(const QByteArray &base64, QByteArray &plano);
    ~Canal();

private:
    QByteArray envio_, recepcion_;
    quint64 enviados_ = 0, recibidos_ = 0;
    bool activo_ = false;
};

}
