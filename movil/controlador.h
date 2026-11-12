#pragma once
#include "db.h"
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class ClienteSinc;
class QQmlEngine;
class QJSEngine;

/**
 * Todo lo que usan las pantallas QML del celular. En QML se llama «App» (import GestorStock).
 * Hace lo mismo que la ventana de la PC: buscar, agregar, editar y eliminar productos (filas), administrar las
 * columnas, registrar ventas y compras, y además sincroniza con la PC.
 */
class Controlador : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(QString busqueda READ busqueda WRITE setBusqueda NOTIFY cambio)
    Q_PROPERTY(QVariantList productos READ productos NOTIFY cambio)
    Q_PROPERTY(QString resumen READ resumen NOTIFY cambio)
    Q_PROPERTY(QVariantList columnas READ columnas NOTIFY cambio)                 // las puestas, en orden
    Q_PROPERTY(QVariantList columnasQuitadas READ columnasQuitadas NOTIFY cambio)
    // Sincronización
    Q_PROPERTY(bool pcEmparejada READ pcEmparejada NOTIFY sincCambio)
    Q_PROPERTY(QString nombrePc READ nombrePc NOTIFY sincCambio)
    Q_PROPERTY(QString direccionPc READ direccionPc NOTIFY sincCambio)
    Q_PROPERTY(QString ultimaSincTexto READ ultimaSincTexto NOTIFY sincCambio)
    Q_PROPERTY(bool sincronizando READ sincronizando NOTIFY sincCambio)
    Q_PROPERTY(QString estadoSinc READ estadoSinc NOTIFY sincCambio)
    Q_PROPERTY(QString nombreDispositivo READ nombreDispositivo WRITE setNombreDispositivo NOTIFY sincCambio)
    Q_PROPERTY(int puertoPorDefecto READ puertoPorDefecto CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit Controlador(Stock& stock, QObject* parent = nullptr);
    ~Controlador() override;
    static Controlador* create(QQmlEngine*, QJSEngine*);   // para QML_SINGLETON
    static void setInstancia(Controlador* c);

    QString busqueda() const { return busqueda_; }
    void setBusqueda(const QString& b);
    QVariantList productos() const { return productos_; }
    QString resumen() const { return resumen_; }
    QVariantList columnas() const;
    QVariantList columnasQuitadas() const;

    bool pcEmparejada() const;
    QString nombrePc() const;
    QString direccionPc() const;
    QString ultimaSincTexto() const;
    bool sincronizando() const { return sincronizando_; }
    QString estadoSinc() const { return estadoSinc_; }
    QString nombreDispositivo() const { return stock_.nombreDispositivo(); }
    void setNombreDispositivo(const QString& n);
    int puertoPorDefecto() const;
    QString version() const;

    // ---- productos (filas). Los que devuelven QString dan "" si salió bien; si no, el error
    Q_INVOKABLE QVariantMap producto(qint64 id) const;        // para el editor; id 0 = uno vacío
    Q_INVOKABLE QString guardarProducto(const QVariantMap& datos);
    Q_INVOKABLE QString eliminarProducto(qint64 id);
    Q_INVOKABLE QVariantList buscarProductos(const QString& texto) const;   // para armar una venta o compra

    // ---- columnas
    Q_INVOKABLE QString agregarColumna(const QString& nombre, int tipo);    // tipo: 0 texto, 1 número, 2 fecha
    Q_INVOKABLE QString renombrarColumna(qint64 id, const QString& nombre);
    Q_INVOKABLE QString cambiarTipoColumna(qint64 id, int tipo);
    Q_INVOKABLE QString mostrarColumna(qint64 id, bool visible);
    Q_INVOKABLE QString moverColumna(qint64 id, int delta);                 // -1 sube, +1 baja
    Q_INVOKABLE QString eliminarColumna(qint64 id);
    Q_INVOKABLE QString restaurarColumna(qint64 id);

    // ---- ventas y compras: lineas = [{id, cantidad, presentacion, precio}] -> {ok, error, total}
    Q_INVOKABLE QVariantMap registrarOperacion(bool venta, const QVariantList& lineas);

    // ---- sincronización
    Q_INVOKABLE void emparejar(const QString& host, int puerto, const QString& codigo);
    Q_INVOKABLE void sincronizar();
    Q_INVOKABLE void sincronizarSiCorresponde();     // al volver a la app: en silencio, si hay PC emparejada
    Q_INVOKABLE void buscarPcs();
    Q_INVOKABLE void olvidarPc();

    Q_INVOKABLE QString precioTexto(double precio) const;

public slots:
    void refrescar();

signals:
    void cambio();
    void sincCambio();
    void aviso(const QString& texto);                // mensajito abajo de la pantalla
    void pcEncontrada(const QString& nombre, const QString& host, int puerto);
    void busquedaTerminada();
    void sincTerminada(bool ok, const QString& mensaje);

private:
    void programarSincronizacion();                  // un rato después de cada cambio, si hay PC emparejada
    QString resultado(const Resultado& r);           // avisa al sincronizador y devuelve el error ("" si salió bien)
    static QVariantMap mapaColumna(const Columna& c);

    Stock& stock_;
    ClienteSinc* cliente_ = nullptr;
    QTimer temporizadorSinc_;
    bool sincEnSilencio_ = false;
    bool sincronizando_ = false;
    QString estadoSinc_;
    QDateTime ultimoIntentoAuto_;
    QString busqueda_;
    QVariantList productos_;
    QString resumen_;
};
