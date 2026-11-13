#include "ClienteHttp.h"

#include <QCoreApplication>
#include <QPointer>

#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#include <QThreadPool>
#else
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

namespace http {

void pedir(QObject *contexto, const QUrl &direccion, Respuesta alTerminar)
{
    QPointer<QObject> yo(contexto);
#ifdef Q_OS_ANDROID
    // El pedido de Java se queda esperando la respuesta: va en otro hilo para no trabar la ventana
    const QString texto = direccion.toString(QUrl::FullyEncoded);
    QThreadPool::globalInstance()->start([yo, texto, alTerminar] {
        int codigo = 0;
        QByteArray datos;
        const QJniObject respuesta = QJniObject::callStaticObjectMethod(
            "com/sebastian/comun/ClienteHttp", "pedir", "(Ljava/lang/String;)[Ljava/lang/String;",
            QJniObject::fromString(texto).object<jstring>());
        QJniEnvironment jni;
        if (jni.checkAndClearExceptions() || !respuesta.isValid()) {
            codigo = 0;
        } else {
            const auto partes = respuesta.object<jobjectArray>();
            const QJniObject codigoJava(jni->GetObjectArrayElement(partes, 0));
            const QJniObject cuerpoJava(jni->GetObjectArrayElement(partes, 1));
            codigo = codigoJava.toString().toInt();
            datos = cuerpoJava.toString().toUtf8();
        }
        QMetaObject::invokeMethod(qApp, [yo, alTerminar, codigo, datos] {
            if (yo)
                alTerminar(codigo, datos);
        }, Qt::QueuedConnection);
    });
#else
    static QNetworkAccessManager *red = new QNetworkAccessManager(qApp);
    QNetworkRequest pedido(direccion);
    pedido.setHeader(QNetworkRequest::UserAgentHeader, QCoreApplication::applicationName());
    pedido.setTransferTimeout(20000);
    QNetworkReply *respuesta = red->get(pedido);
    QObject::connect(respuesta, &QNetworkReply::finished, respuesta, [respuesta, yo, alTerminar] {
        respuesta->deleteLater();
        if (!yo)
            return;
        int codigo = respuesta->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (respuesta->error() == QNetworkReply::NoError)
            codigo = codigo ? codigo : 200;
        alTerminar(codigo, codigo == 200 ? respuesta->readAll() : QByteArray());
    });
#endif
}

}
