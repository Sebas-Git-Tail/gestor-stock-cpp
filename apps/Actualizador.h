// Busca la última versión de una app del catálogo (CatalogoApps) y la instala:
//   Windows  <app>-<v>-instalador.exe   la propia app: en silencio y se vuelve a abrir;  otra: el instalador normal
//   Android  <app>-<v>.apk              se abre el instalador del sistema (Instalador.java)
//   Linux    <paquete>-<v>-<n>-x86_64.pkg.tar.zst   pkexec pacman -U (la propia app se vuelve a abrir)
// Sirve tanto para que una app se actualice a sí misma como para instalar otra.
#pragma once

#include "CatalogoApps.h"

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVersionNumber>

#ifndef Q_OS_ANDROID
#include <QNetworkAccessManager>
#endif

namespace apps {

// La versión instalada de una app del catálogo en este equipo (nula si no está instalada).
// Linux: pacman -Q; Android: PackageManager; Windows: el registro que deja Inno Setup.
// Si es la app que está corriendo, en Android y Windows se usa la versión del programa
// (QCoreApplication::applicationVersion); en Linux, la del paquete (incluye el pkgrel).
QVersionNumber versionInstalada(const InfoApp &app, bool esEstaApp = false);

// Raíces del servidor casero (https://bodrio.seb-dev.fyi y Tailscale), de servidor.json
QStringList raicesServidor();

class Actualizador : public QObject
{
    Q_OBJECT

public:
    // esEstaApp: la app se actualiza a sí misma (se cierra y se vuelve a abrir al instalar)
    Actualizador(const InfoApp &app, bool esEstaApp, QObject *parent = nullptr);

    const InfoApp &app() const { return app_; }

    // Busca la última versión: emite encontrada() (y además hayVersionNueva() si es más nueva que la
    // instalada) o noEncontrada(). Con la variable COMUN_APPS_SIN_RED=1 no sale a la red (pruebas).
    void buscar();
    // Como buscar(), pero como mucho una vez por día (para el aviso al abrir la app)
    void buscarUnaVezPorDia();
    void descargarEInstalar();     // después de encontrada()

    QString version() const { return disponible_.version.toString(); }
    QVersionNumber disponible() const { return disponible_.version; }
    QVersionNumber instalada() const { return instalada_; }
    bool ocupado() const { return ocupado_; }

    // Texto lindo de una versión: 4.3.1.2 (paquete de Arch con pkgrel) → «4.3.1-2» si hay 4 números
    static QString texto(const QVersionNumber &v);

signals:
    void encontrada(const QString &version);
    void hayVersionNueva(const QString &version);
    void noEncontrada();
    void progreso(qint64 recibido, qint64 total);   // total < 0: no se sabe
    void mensaje(const QString &texto);
    void fallo(const QString &texto);
    void instalado(const QString &version);         // terminó la instalación (Linux) o se abrió el instalador

private:
    void probar(int fuente, int raiz);
    void terminarBusqueda();
    void instalar(const QString &archivo);

    InfoApp app_;
    bool esEstaApp_;
    bool ocupado_ = false;
    Candidato disponible_;
    QVersionNumber instalada_;
#ifndef Q_OS_ANDROID
    QNetworkAccessManager red_;
#endif
};

} // namespace apps
