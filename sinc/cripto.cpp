#include "cripto.h"

#include <QRandomGenerator>

extern "C" {
#include "monocypher.h"
}

namespace cripto {

namespace {
const uint8_t *u(const QByteArray &b) { return reinterpret_cast<const uint8_t *>(b.constData()); }
uint8_t *u(QByteArray &b) { return reinterpret_cast<uint8_t *>(b.data()); }

QByteArray nonce(quint64 contador)
{
    QByteArray n(24, '\0');
    for (int i = 0; i < 8; ++i)
        n[i] = char((contador >> (8 * i)) & 0xff);
    return n;
}
}

QByteArray aleatorio(int bytes)
{
    QByteArray b(bytes, Qt::Uninitialized);
    // QRandomGenerator::system() usa el generador del sistema operativo (getrandom, /dev/urandom...)
    QRandomGenerator::system()->generate(reinterpret_cast<quint32 *>(b.data()),
                                         reinterpret_cast<quint32 *>(b.data()) + bytes / 4);
    for (int i = bytes - bytes % 4; i < bytes; ++i)
        b[i] = char(QRandomGenerator::system()->bounded(256));
    return b;
}

QByteArray generadorEmparejar(const QString &codigo, const QByteArray &contexto)
{
    QByteArray oculto(32, '\0'), punto(32, '\0');
    const QByteArray clave = QByteArrayLiteral("comun-qt/sinc/cpace/1");
    const QByteArray mensaje = codigo.toUtf8() + '|' + contexto;
    crypto_blake2b_keyed(u(oculto), 32, u(clave), size_t(clave.size()), u(mensaje), size_t(mensaje.size()));
    crypto_elligator_map(u(punto), u(oculto));
    borrar(oculto);
    return punto;
}

QByteArray publica(const QByteArray &secreta, const QByteArray &base)
{
    QByteArray p(32, '\0');
    if (base.isEmpty())
        crypto_x25519_public_key(u(p), u(secreta));
    else
        crypto_x25519(u(p), u(secreta), u(base));
    return p;
}

QByteArray compartido(const QByteArray &secreta, const QByteArray &publicaOtro)
{
    if (secreta.size() != 32 || publicaOtro.size() != 32)
        return {};
    QByteArray s(32, '\0');
    crypto_x25519(u(s), u(secreta), u(publicaOtro));
    const QByteArray cero(32, '\0');
    if (crypto_verify32(u(s), u(cero)) == 0)    // punto de orden chico: el otro hace trampa
        return {};
    return s;
}

QByteArray hash(const QByteArray &clave, const QByteArray &mensaje, int bytes)
{
    QByteArray h(bytes, '\0');
    crypto_blake2b_keyed(u(h), size_t(bytes), u(clave), size_t(qMin<qsizetype>(clave.size(), 64)),
                         u(mensaje), size_t(mensaje.size()));
    return h;
}

bool iguales(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;
    unsigned char dif = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        dif |= static_cast<unsigned char>(a[i] ^ b[i]);
    return dif == 0;
}

void borrar(QByteArray &datos)
{
    if (!datos.isEmpty())
        crypto_wipe(datos.data(), size_t(datos.size()));
    datos.clear();
}

void Canal::iniciar(const QByteArray &claveEnvio, const QByteArray &claveRecepcion)
{
    envio_ = claveEnvio;
    recepcion_ = claveRecepcion;
    enviados_ = recibidos_ = 0;
    activo_ = true;
}

Canal::~Canal()
{
    borrar(envio_);
    borrar(recepcion_);
}

QByteArray Canal::cifrar(const QByteArray &plano)
{
    QByteArray salida(16 + plano.size(), Qt::Uninitialized);
    const QByteArray n = nonce(enviados_++);
    crypto_aead_lock(u(salida) + 16, u(salida), u(envio_), u(n), nullptr, 0, u(plano), size_t(plano.size()));
    return salida.toBase64();
}

bool Canal::descifrar(const QByteArray &base64, QByteArray &plano)
{
    const auto r = QByteArray::fromBase64Encoding(base64, QByteArray::AbortOnBase64DecodingErrors);
    if (!r || r.decoded.size() < 16)
        return false;
    const QByteArray &datos = r.decoded;
    plano = QByteArray(datos.size() - 16, Qt::Uninitialized);
    const QByteArray n = nonce(recibidos_);
    if (crypto_aead_unlock(u(plano), u(datos), u(recepcion_), u(n), nullptr, 0,
                           u(datos) + 16, size_t(plano.size())) != 0) {
        plano.clear();
        return false;
    }
    ++recibidos_;
    return true;
}

}
