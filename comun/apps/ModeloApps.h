// Lista «Instalar otras apps» como modelo de Qt: la usan tanto las apps con Widgets (PanelApps) como las
// de QML (ListView con estos roles). También maneja la actualización de la propia app.
//
//   QML:   ListView { model: modeloApps; delegate: ... model.nombre, model.estado, model.accion ...
//                     onClicked: modeloApps.instalar(index) }
//          modeloApps.versionNueva != "" → mostrar «Actualizar a ...» y llamar a modeloApps.actualizarEstaApp()
//          (y marcar el menú: BotonMenu { punto: Apps.versionNueva !== "" }, ItemMenu { destacado: ... })
//          onAvisoVersionNueva → preguntar «¿La instalo?» (como mucho una vez por día)
//
// Nunca lista las apps ocultas (Carina, Bodrio Supremo) ni la propia app.
#pragma once

#include "Actualizador.h"

#include <QAbstractListModel>
#include <QList>

namespace apps {

class ModeloApps : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString versionNueva READ versionNueva NOTIFY versionNuevaCambio)     // de esta app ("" = al día)
    Q_PROPERTY(QString mensaje READ mensaje NOTIFY mensajeCambio)                     // lo último que pasó
    Q_PROPERTY(bool actualizando READ actualizando NOTIFY actualizandoCambio)         // esta app se está actualizando
    Q_PROPERTY(double progresoPropio READ progresoPropio NOTIFY actualizandoCambio)   // 0..1, -1 si no se sabe
    Q_PROPERTY(QString versionPropia READ versionPropia CONSTANT)
    Q_PROPERTY(int cantidad READ rowCount NOTIFY cantidadCambio)

public:
    enum Rol {
        Nombre = Qt::UserRole + 1,
        Descripcion,
        Instalada,      // «2.4.0» o ""
        Disponible,     // «2.4.0» o ""
        Estado,         // texto para mostrar debajo del nombre
        Accion,         // «Instalar», «Actualizar» o "" (nada que hacer)
        Ocupado,        // buscando o instalando
        Progreso,       // 0..1, -1 si no se sabe
        Id,
    };

    // estaApp: id del catálogo de la app que muestra la lista (Carina y Bodrio también pueden)
    explicit ModeloApps(const QString &estaApp, QObject *parent = nullptr);

    int rowCount(const QModelIndex &padre = {}) const override;
    QVariant data(const QModelIndex &indice, int rol) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString versionNueva() const { return versionNueva_; }
    QString mensaje() const { return mensaje_; }
    bool actualizando() const { return propia_ && propia_->ocupado(); }
    double progresoPropio() const { return progresoPropio_; }
    QString versionPropia() const;

    Q_INVOKABLE void refrescar();                  // vuelve a buscar las versiones de la lista
    Q_INVOKABLE void instalar(int fila);
    // Busca una versión nueva de esta app. Sin forzar (al abrir la app) busca igual, pero avisoVersionNueva()
    // sale como mucho una vez por día; versionNueva queda puesta para marcar el menú cada vez que se abre.
    Q_INVOKABLE void buscarActualizacion(bool forzar = false);
    Q_INVOKABLE void actualizarEstaApp();

signals:
    void versionNuevaCambio();
    void avisoVersionNueva(const QString &version);   // hay versión nueva y toca preguntar (una vez por día)
    void mensajeCambio();
    void actualizandoCambio();
    void cantidadCambio();
    void fallo(const QString &texto);

private:
    struct Fila {
        Actualizador *act = nullptr;
        bool buscando = false;
        bool buscado = false;
        bool sinConexion = false;
        double progreso = -1;
    };
    void conectar(int fila);
    void cambio(int fila);
    void ponerMensaje(const QString &texto);

    QString estaApp_;
    QList<Fila> filas_;
    Actualizador *propia_ = nullptr;
    QString versionNueva_, mensaje_;
    double progresoPropio_ = -1;
};

} // namespace apps
