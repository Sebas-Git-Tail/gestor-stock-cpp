// Avisa si hay una versión nueva del programa: pregunta a GitHub por el último release
// (api.github.com/repos/<usuario>/<repo>/releases/latest) y lo compara con
// QCoreApplication::applicationVersion(). Como mucho una vez por día.

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class Actualizaciones : public QObject
{
    Q_OBJECT

public:
    // repo: «Sebas-Git-Tail/Android-Manager»
    explicit Actualizaciones(const QString &repo, QObject *parent = nullptr);

    void comprobar(bool forzar = false);

    // Compara «1.10.0» con «1.9.2» número por número (una «v» adelante se ignora). >0 si a es más nueva.
    static int compararVersiones(QString a, QString b);

signals:
    // Hay una versión más nueva que la instalada; url: la página del release
    void hayVersionNueva(const QString &version, const QUrl &url);

private:
    QString repo_;
};
