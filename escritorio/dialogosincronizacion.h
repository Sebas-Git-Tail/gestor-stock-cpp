#pragma once
#include <QDialog>
#include <memory>

namespace Ui { class DialogoSincronizacion; }
class Stock;
class QSettings;
class ServidorSinc;
class QTimer;

/** Activar la sincronización, emparejar un celular (código de 6 dígitos) y ver/quitar los emparejados. */
class DialogoSincronizacion : public QDialog
{
    Q_OBJECT
public:
    DialogoSincronizacion(Stock& stock, ServidorSinc& servidor, QSettings* ajustes, QWidget* parent = nullptr);
    ~DialogoSincronizacion() override;

    // Prende o apaga el servidor y lo recuerda para la próxima vez que se abra el programa
    static bool activar(ServidorSinc& servidor, bool activa, QSettings* ajustes, QString* error = nullptr);

private:
    void actualizar();
    void cargarPares();

    std::unique_ptr<Ui::DialogoSincronizacion> ui;
    Stock& stock_;
    ServidorSinc& servidor_;
    QSettings* ajustes_;
    QTimer* reloj_ = nullptr;
};
