#pragma once
// Sincronización PC ↔ celular del gestor de stock. El protocolo (cifrado) está en comun-qt
// (comun/sinc/sincronizacion_comun.h); acá solo van los datos propios de este programa.
#include "db.h"
#include "sincronizacion_comun.h"

namespace sinc {
constexpr quint16 PUERTO = 45464;
constexpr quint16 PUERTO_BUSQUEDA = 45465;
inline PerfilSinc perfil()
{
    return {QStringLiteral("Gestor de Stock"), "GS", QStringLiteral("gs"), PUERTO, PUERTO_BUSQUEDA};
}
}

/** Lado PC: escucha conexiones del celular mientras el programa está abierto. */
class ServidorSinc : public ServidorSincBase
{
    Q_OBJECT
public:
    explicit ServidorSinc(Stock& stock, QObject* parent = nullptr) : ServidorSincBase(stock, sinc::perfil(), parent) {}
};

/** Lado celular: se conecta a la PC para emparejar o sincronizar. */
class ClienteSinc : public ClienteSincBase
{
    Q_OBJECT
public:
    explicit ClienteSinc(Stock& stock, QObject* parent = nullptr) : ClienteSincBase(stock, sinc::perfil(), parent) {}
};
