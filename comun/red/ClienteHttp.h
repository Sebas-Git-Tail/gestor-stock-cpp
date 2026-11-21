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
// El cuerpo es para texto (JSON, páginas): en Android pasa por un String de Java y un archivo
// binario llegaría roto. Para bajar archivos usar descargar().
void pedir(QObject *contexto, const QUrl &direccion, Respuesta alTerminar);

// Baja un archivo directo al disco (APK, instaladores). alTerminar(código HTTP) en el hilo principal;
// 0 = sin conexión o no se pudo escribir. Si falla no queda el archivo a medias.
using Terminado = std::function<void(int codigo)>;
void descargar(QObject *contexto, const QUrl &direccion, const QString &ruta, Terminado alTerminar);

// Corta la descarga() en curso a esa ruta: su alTerminar recibe 0 y no queda el archivo a medias.
void cancelarDescarga(const QString &ruta);

// POST con un cuerpo JSON (APIs como la de descargas_web del servidor). A diferencia de pedir(),
// el cuerpo llega también con los errores (4xx/5xx), para poder mostrar el mensaje del servidor.
// «segundos»: cuánto se espera la respuesta (algunas operaciones del servidor tardan).
void enviar(QObject *contexto, const QUrl &direccion, const QByteArray &json, Respuesta alTerminar, int segundos = 30);

}
