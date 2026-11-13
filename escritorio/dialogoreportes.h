#pragma once
// ☰ → Reportes y avisos: ventas, compras y ganancia de un período (con lo más vendido, por mes y
// por día) y los avisos de poco stock y vencimientos. La pantalla está en dialogo_reportes.ui.
#include <QDialog>

class Stock;
namespace Ui { class DialogoReportes; }

class DialogoReportes : public QDialog
{
    Q_OBJECT
public:
    enum class Pestana { Reportes, Avisos };
    DialogoReportes(Stock& stock, Pestana pestana, QWidget* parent = nullptr);
    ~DialogoReportes() override;

    void elegirPeriodo(int indice);          // para las pruebas
    QString textoVentas() const;
    QString textoGanancia() const;
    int cantidadAvisos() const;

private:
    void periodoElegido(int indice);
    void actualizar();
    void cargarAvisos();

    Ui::DialogoReportes* ui;
    Stock& stock_;
    bool armando_ = false;
};
