#include "ClienteHttp.h"

#include <QCoreApplication>
#include <QPointer>

#include <QSaveFile>

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

void descargar(QObject *contexto, const QUrl &direccion, const QString &ruta, Terminado alTerminar)
{
    QPointer<QObject> yo(contexto);
#ifdef Q_OS_ANDROID
    const QString texto = direccion.toString(QUrl::FullyEncoded);
    QThreadPool::globalInstance()->start([yo, texto, ruta, alTerminar] {
        const jint codigo = QJniObject::callStaticMethod<jint>(
            "com/sebastian/comun/ClienteHttp", "descargar", "(Ljava/lang/String;Ljava/lang/String;)I",
            QJniObject::fromString(texto).object<jstring>(), QJniObject::fromString(ruta).object<jstring>());
        QJniEnvironment jni;
        const int resultado = jni.checkAndClearExceptions() ? 0 : int(codigo);
        QMetaObject::invokeMethod(qApp, [yo, alTerminar, resultado] {
            if (yo)
                alTerminar(resultado);
        }, Qt::QueuedConnection);
    });
#else
    static QNetworkAccessManager *red = new QNetworkAccessManager(qApp);
    auto *salida = new QSaveFile(ruta);
    if (!salida->open(QIODevice::WriteOnly)) {
        delete salida;
        QMetaObject::invokeMethod(qApp, [yo, alTerminar] { if (yo) alTerminar(0); }, Qt::QueuedConnection);
        return;
    }
    QNetworkRequest pedido(direccion);
    pedido.setHeader(QNetworkRequest::UserAgentHeader, QCoreApplication::applicationName());
    pedido.setTransferTimeout(60000);
    QNetworkReply *respuesta = red->get(pedido);
    QObject::connect(respuesta, &QNetworkReply::readyRead, respuesta, [respuesta, salida] { salida->write(respuesta->readAll()); });
    QObject::connect(respuesta, &QNetworkReply::finished, respuesta, [respuesta, salida, yo, alTerminar] {
        respuesta->deleteLater();
        int codigo = respuesta->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (respuesta->error() == QNetworkReply::NoError) {
            codigo = codigo ? codigo : 200;
            salida->write(respuesta->readAll());
        } else if (codigo == 0 || codigo == 200) {
            codigo = 0;
        }
        if (codigo != 200 || !salida->commit()) {
            salida->cancelWriting();
            if (codigo == 200)
                codigo = 0;
        }
        delete salida;
        if (yo)
            alTerminar(codigo);
    });
#endif
}

}
