// Pedidos HTTP(S) que funcionan igual en la PC y en Android.
// En Android, Qt no trae OpenSSL y no puede hacer HTTPS: ahí el pedido lo hace Java
// (android/com/sebastian/comun/ClienteHttp.java, que cada app tiene que incluir en su carpeta
// android/src). En la PC se usa QNetworkAccessManager.

#pragma once

#include <QByteArray>
#include <QObject>
#include <QUrl>

#include <functional>

namespace http {

// alTerminar(código HTTP, cuerpo) se llama en el hilo principal. código 0 = no hubo conexión.
// Si «contexto» se destruye antes de la respuesta, alTerminar no se llama.
using Respuesta = std::function<void(int codigo, const QByteArray &cuerpo)>;
void pedir(QObject *contexto, const QUrl &direccion, Respuesta alTerminar);

}
