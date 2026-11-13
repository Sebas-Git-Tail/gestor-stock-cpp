#pragma once
#include "db.h"
#include <QAbstractItemView>
#include <QMainWindow>
#include <QVector>
#include <memory>

class QSettings;
class QStandardItemModel;
class ServidorSinc;
namespace Ui { class MainWindow; }

/**
 * Ventana principal (main.ui): tabla de productos, buscador, ventas y compras.
 * Las columnas de la tabla salen de la base (Stock::columnas): se pueden agregar, renombrar, ocultar, ordenar
 * (arrastrando los títulos) y eliminar, y se sincronizan con el celular.
 */
class VentanaPrincipal : public QMainWindow
{
    Q_OBJECT
public:
    VentanaPrincipal(Stock* stock, QSettings* ajustes, QWidget* parent = nullptr);
    ~VentanaPrincipal() override;

    void guardarPosicion();       // dónde estaba parado (se llama al cerrar)
    void restaurarPosicion();     // se llama sola al abrir

    // Para las pruebas
    class QTableView* tabla() const;
    class QLineEdit* buscador() const;
    QList<qint64> idsSeleccionados() const;
    qint64 idActual() const;
    qint64 idPrimeraFilaVisible() const;
    int filaDe(qint64 id) const;
    int cantidadDeFilas() const;
    void seleccionar(const QList<qint64>& ids, qint64 actual);
    const QVector<Columna>& columnasVisibles() const { return cols_; }   // las de la tabla, en el orden en que se ven
    int columnaDe(const char* clave) const;                               // lugar en la tabla de una columna del programa (o -1)
    ServidorSinc& servidor() { return *servidor_; }

public slots:
    void refrescar();             // vuelve a leer la base SIN perder la posición ni la selección
    void agregar();
    void editar();
    void eliminar();
    void venta();
    void compra();
    void importarExcel();
    void exportarCsv();
    void hacerBackup();
    void acercaDe();
    void columnas();              // ventana para agregar, renombrar, ordenar y eliminar columnas
    void abrirSincronizacion();   // ventana para emparejar el celular
    void abrirReportes(bool avisos = false);   // ventas, ganancia y avisos de stock y vencimientos
    void actualizarAvisos();      // el aviso arriba de la tabla («⚠ 3 avisos: …»)
    void copiar();                // Ctrl+C: los renglones seleccionados al portapapeles
    void pegar();                 // Ctrl+V: los renglones del portapapeles como productos nuevos

protected:
    void closeEvent(QCloseEvent* evento) override;
    bool eventFilter(QObject* objeto, QEvent* evento) override;   // Ctrl+C / Ctrl+V sobre la tabla

private:
    void armarTabla();
    void armarMenu();
    void conectar();
    bool actualizarColumnas();                       // si cambió la estructura de la tabla, la rearma
    void reconstruirColumnas(const QVector<Columna>& nuevas);
    void menuEncabezado(const QPoint& pos);          // clic derecho sobre un título
    void columnaMovida();                            // se arrastró un título: queda guardado el nuevo orden
    int columnaDeUuid(const QString& uuid) const;
    QList<Columna> columnasPropias() const;          // para copiar, pegar y exportar
    void valorDeCelda(const Producto& p, const Columna& c, QString* texto, QVariant* orden) const;
    void operacion(TipoOperacion tipo);
    void menuContextual(const QPoint& pos);
    void irAFila(int fila, QAbstractItemView::ScrollHint donde);
    QList<qint64> idsSeleccionadosEnOrden() const;   // en el orden en que se ven en la tabla

    std::unique_ptr<Ui::MainWindow> ui;
    Stock* stock_;
    QSettings* ajustes_;
    QStandardItemModel* modelo_ = nullptr;
    ServidorSinc* servidor_ = nullptr;               // para que el celular sincronice
    QVector<Columna> cols_;                          // las columnas que se ven, en el orden de la tabla
    bool armando_ = false;                           // mientras se rearman las columnas, no se guarda nada
    QString uuidOrden_;                              // por qué columna está ordenada la tabla (y en qué sentido)
    Qt::SortOrder sentidoOrden_ = Qt::AscendingOrder;

    // Para no perder el lugar cuando se busca
    bool buscando_ = false;
    QString filtroAnterior_;
    qint64 idPrimeraAntesDeBuscar_ = 0;
    QList<qint64> idsAntesDeBuscar_;
    qint64 idActualAntesDeBuscar_ = 0;
};
