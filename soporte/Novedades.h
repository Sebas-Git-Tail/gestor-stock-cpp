// «Novedades»: qué cambió en cada versión, para mostrarlo la primera vez que se abre una versión nueva
// (las apps se actualizan solas y si no, nadie se entera de qué hay de nuevo) y desde el menú.
//
// Cada app trae su NOVEDADES.md compilado en «:/novedades/NOVEDADES.md» (ver LEEME):
//
//     ## 1.9.0 (21/11/2026)
//     - Si la app se cierra por una falla, manda un informe para poder arreglarla.
//     - [Sebastián] Sección Servidor: ahora también corre los generadores.
//
// Los cambios que empiezan con «[Sebastián]» solo le afectan a él: se muestran únicamente en sus
// equipos (esDesarrollador()). Una versión que no tenga nada para mostrar no aparece.
//
// En sus equipos:
//   PC (Linux/Windows)  existe el archivo «comun-qt/desarrollador» en la carpeta de configuración
//                       (~/.config/comun-qt/desarrollador; Arch-Linux-Manager lo crea en pc e instituto)
//   Android             cada app lo guarda en sus ajustes: se prende y se apaga dejando apretado
//                       «Novedades» en el menú (Novedades.alternarDesarrollador())
//   Pruebas             COMUN_DESARROLLADOR=1 o =0 manda sobre todo lo anterior
#pragma once

#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVersionNumber>

namespace soporte {

bool esDesarrollador();
void ponerDesarrollador(bool si);     // PC: crea/borra el archivo; Android: ajustes de la app
QString archivoDesarrollador();       // el archivo de la PC

struct VersionNovedades {
    QVersionNumber version;
    QString textoVersion;              // como está escrito («1.9.0»)
    QString fecha;                     // lo que está entre paréntesis («21/11/2026») o ""
    QStringList cambios;
    QList<bool> soloDesarrollador;     // uno por cambio
};

// Lee un NOVEDADES.md. Con desarrollador = false saca los cambios «[Sebastián]» y las versiones que quedan
// vacías; con true los deja (sin la marca). Ordenadas de la más nueva a la más vieja.
QList<VersionNovedades> leerNovedades(const QString &texto, bool desarrollador);

// Las versiones a mostrar al abrir: las que hay entre «vista» (excluida) y «actual» (incluida). Si nunca se
// vio ninguna (vista nula), solo la actual.
QList<VersionNovedades> nuevasDesde(const QList<VersionNovedades> &todas, const QVersionNumber &vista,
                                    const QVersionNumber &actual);

// Markdown para mostrar (QLabel/QTextBrowser con Markdown en Widgets, Label en QML)
QString aMarkdown(const QList<VersionNovedades> &versiones, bool marcarDesarrollador);

// Para QML (y la usa también la ventana de Widgets). La app lo pone como propiedad de contexto «Novedades».
//   DialogoNovedades { id: novedades }        (Comun.Ui: se abre solo si hay novedades sin ver)
//   ItemMenu { text: "Novedades"; icono: "regalo"; onClicked: novedades.abrirTodas()
//              onPressAndHold: Novedades.alternarDesarrollador() }
class Novedades : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hayNuevas READ hayNuevas NOTIFY cambio)
    Q_PROPERTY(QString textoNuevas READ textoNuevas NOTIFY cambio)
    Q_PROPERTY(QString textoTodas READ textoTodas NOTIFY cambio)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool desarrollador READ desarrollador NOTIFY cambio)
    Q_PROPERTY(bool enviandoInforme READ enviandoInforme NOTIFY enviandoInformeCambio)

public:
    // archivo: el NOVEDADES.md (por defecto el del recurso); ajustes: dónde se guarda la última versión vista
    // (por defecto QSettings de la app)
    explicit Novedades(QObject *parent = nullptr, const QString &archivo = QStringLiteral(":/novedades/NOVEDADES.md"),
                       QSettings *ajustes = nullptr);

    bool hayNuevas() const { return !nuevas_.isEmpty(); }
    QString textoNuevas() const;
    QString textoTodas() const;
    QString version() const;
    bool desarrollador() const { return desarrollador_; }
    const QList<VersionNovedades> &nuevas() const { return nuevas_; }
    const QList<VersionNovedades> &todas() const { return todas_; }

    Q_INVOKABLE void marcarVistas();            // ya se mostraron: no vuelven a salir al abrir
    Q_INVOKABLE void recargar();                // otra vez, por si cambió esDesarrollador()
    Q_INVOKABLE void alternarDesarrollador();   // escondido (Android): prende/apaga las novedades «[Sebastián]»

    // «Enviar informe de fallas» (Fallas::enviarManual), para el diálogo de QML
    bool enviandoInforme() const { return enviandoInforme_; }
    Q_INVOKABLE void enviarInforme(const QString &comentario);

signals:
    void cambio();
    void enviandoInformeCambio();
    void informeEnviado(bool ok);

private:
    void cargar();

    QString archivo_;
    QSettings *ajustes_;
    QString texto_;
    bool desarrollador_ = false;
    bool enviandoInforme_ = false;
    QList<VersionNovedades> todas_, nuevas_;
};

} // namespace soporte
