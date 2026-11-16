// Catálogo de los programas de Sebastián: de dónde se bajan, qué archivo corresponde a cada sistema y
// cómo saber qué versión hay instalada. Lo usan el actualizador de cada app (Actualizador) y la lista
// «Instalar otras apps» (ModeloApps / DialogoApps).
//
// Carina (la app de la mamá) y Bodrio Supremo están marcadas como «ocultas»: se actualizan solas,
// pero NUNCA aparecen en la lista de apps para instalar, en ninguna app.
#pragma once

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>
#include <QVersionNumber>

namespace apps {

enum class Sistema { Linux, Android, Windows };
Sistema sistemaActual();

struct Fuente {
    enum Tipo { GitHub, Servidor };
    Tipo tipo;
    QString lugar;      // GitHub: «Sebas-Git-Tail/Android-Manager»; Servidor: carpeta, «/Bodrio-Supremo/»
};

struct InfoApp {
    QString id;                     // el que se pasa a buscar() y a ModeloApps
    QString nombre;
    QString descripcion;
    bool oculta = false;            // no se lista para instalar (Carina, Bodrio)
    QList<Fuente> fuentes;          // se prueban en orden
    // Nombre del archivo en cada sistema (expresión regular; el grupo 1 es la versión). Vacío: no hay.
    QString archivoLinux, archivoAndroid, archivoWindows;
    QString paqueteLinux;           // pacman -Q <paquete>
    QString paqueteAndroid;         // ar.gestorstock.app
    QString appIdWindows;           // {GUID} de Inno Setup (sin la llave doble)

    QString archivo(Sistema s) const;
    bool disponibleEn(Sistema s) const { return !archivo(s).isEmpty(); }
};

const QList<InfoApp> &catalogo();
const InfoApp *buscar(const QString &id);

// Las que se pueden instalar desde «estaApp» en el sistema dado: visibles, con archivo para ese
// sistema y sin la propia app.
QList<InfoApp> paraInstalar(const QString &estaApp, Sistema s = sistemaActual());

// ---- Funciones sin red ni sistema (las usan las pruebas)

using ListaArchivos = QList<QPair<QString, QUrl>>;   // nombre, dirección de descarga

struct Candidato {
    QString nombre;
    QUrl url;
    QVersionNumber version;
    bool valido() const { return !version.isNull(); }
};

// El archivo con la versión más alta que coincide con el patrón
Candidato elegir(const ListaArchivos &archivos, const QString &patron);

// Versión a partir del texto capturado: «2.4.0» → 2.4.0; «2.4.0-3» (paquete de Arch) → 2.4.0.3
QVersionNumber versionDeTexto(const QString &texto);

// Los archivos de un release de GitHub (api.github.com/repos/<repo>/releases/latest)
ListaArchivos archivosDeGitHub(const QByteArray &json);

// Los archivos del listado del servidor (<carpeta>?formato=json de servidor-web.py)
ListaArchivos archivosDeServidor(const QByteArray &json, const QUrl &carpeta);

// «gestor-stock 2.4.0-1» (salida de pacman -Q) → 2.4.0.1. Nulo si no se entiende.
QVersionNumber versionDePacman(const QString &salida);

} // namespace apps
