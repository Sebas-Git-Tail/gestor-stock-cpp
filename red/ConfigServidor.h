// Datos del servidor casero (IPs, usuario, carpeta de respaldos, direcciones de Bodrio) en un solo
// lugar para Android-Manager, Windows-Manager y Bodrio Supremo. Lo mismo hace Common/Servidor.sh
// en Arch-Linux-Manager.
//
// Arranca con los valores de fábrica (compilados acá) y encima pone los de la última copia de
// https://bodrio.seb-dev.fyi/recursos/servidor.json que se bajó. actualizar() vuelve a bajarla si
// la copia tiene más de un día. Así, si cambia una IP, se cambia en ese archivo del servidor y los
// programas la toman solos, sin publicar versiones nuevas.
//
// Las huellas SSH del servidor NO salen de ese archivo (ver huellasSsh()): si alguien lo cambiara,
// lo peor que podría pasar es que el respaldo no se conecte, nunca que la contraseña vaya a otro equipo.

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

class ConfigServidor : public QObject
{
    Q_OBJECT

public:
    static ConfigServidor &instancia();

    QString usuario() const { return usuario_; }
    QString carpetaRespaldos() const { return respaldos_; }          // termina en /
    QStringList ipsSsh() const { return {ipLocal_, ipTailscale_}; }  // primero la de casa
    QString ipLocal() const { return ipLocal_; }
    QString ipTailscale() const { return ipTailscale_; }
    QString webPublica() const { return webPublica_; }
    QStringList raicesBodrio() const { return bodrio_; }

    // Líneas para un known_hosts con las huellas del servidor de respaldos, con el nombre dado
    // (HostKeyAlias, o una IP por línea para dbclient). Compiladas en el programa a propósito.
    static QStringList huellasSsh(const QStringList &nombres);

    // Vuelve a bajar servidor.json si la copia guardada tiene más de un día (o forzar)
    void actualizar(bool forzar = false);

    // Aplica un servidor.json (lo usan actualizar() y las pruebas). Solo toma los campos conocidos y
    // con el formato esperado. Falso si no se entiende.
    bool aplicar(const QByteArray &json);
    void restablecer();    // vuelve a los valores de fábrica

    static QString urlConfig();
    static QString rutaCopia();

signals:
    void actualizada();

private:
    ConfigServidor();

    QString usuario_, respaldos_, ipLocal_, ipTailscale_, webPublica_;
    QStringList bodrio_;
    bool pidiendo_ = false;
};
