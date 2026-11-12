#include "dialogosincronizacion.h"
#include "ui_dialogo_sincronizacion.h"
#include "db.h"
#include "sincronizacion.h"
#include <QHostAddress>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTimer>

DialogoSincronizacion::DialogoSincronizacion(Stock& stock, ServidorSinc& servidor, QSettings* ajustes, QWidget* parent)
    : QDialog(parent), ui(std::make_unique<Ui::DialogoSincronizacion>()), stock_(stock), servidor_(servidor), ajustes_(ajustes)
{
    ui->setupUi(this);
    ui->botones->button(QDialogButtonBox::Close)->setText(tr("Cerrar"));
    ui->chkActivar->setChecked(servidor_.activo());
    ui->inputNombre->setText(stock_.nombreDispositivo());

    connect(ui->chkActivar, &QCheckBox::toggled, this, [this](bool si) {
        QString error;
        if (!activar(servidor_, si, ajustes_, &error)) {
            QMessageBox::warning(this, windowTitle(), error);
            const QSignalBlocker b(ui->chkActivar);
            ui->chkActivar->setChecked(false);
        }
        actualizar();
    });
    connect(ui->inputNombre, &QLineEdit::editingFinished, this, [this] {
        if (!ui->inputNombre->text().trimmed().isEmpty()) stock_.setNombreDispositivo(ui->inputNombre->text());
    });
    connect(ui->btnEmparejar, &QPushButton::clicked, this, [this] {
        if (!servidor_.activo()) {
            ui->chkActivar->setChecked(true);           // para emparejar tiene que estar escuchando
            if (!servidor_.activo()) return;
        }
        if (servidor_.codigo().isEmpty()) servidor_.nuevoCodigo();
        else servidor_.cancelarCodigo();
        actualizar();
    });
    connect(ui->btnQuitar, &QPushButton::clicked, this, [this] {
        const QListWidgetItem* it = ui->listaPares->currentItem();
        if (!it) return;
        if (QMessageBox::question(this, tr("Quitar"), tr("¿Quitar «%1»? Va a tener que emparejarse de nuevo para sincronizar.")
                                                          .arg(it->data(Qt::UserRole + 1).toString())) != QMessageBox::Yes)
            return;
        stock_.eliminarPar(it->data(Qt::UserRole).toString());
        cargarPares();
    });
    connect(ui->listaPares, &QListWidget::currentRowChanged, this, [this] { ui->btnQuitar->setEnabled(ui->listaPares->currentItem()); });
    connect(&servidor_, &ServidorSinc::codigoCambiado, this, &DialogoSincronizacion::actualizar);
    connect(&servidor_, &ServidorSinc::emparejado, this, [this] { cargarPares(); actualizar(); });
    connect(&servidor_, &ServidorSinc::sincronizado, this, &DialogoSincronizacion::cargarPares);

    reloj_ = new QTimer(this);                          // cuenta regresiva del código
    connect(reloj_, &QTimer::timeout, this, &DialogoSincronizacion::actualizar);
    reloj_->start(1000);
    cargarPares();
    actualizar();
}

DialogoSincronizacion::~DialogoSincronizacion() = default;

bool DialogoSincronizacion::activar(ServidorSinc& servidor, bool activa, QSettings* ajustes, QString* error)
{
    if (!activa) {
        servidor.detener();
        ajustes->setValue(QStringLiteral("sinc/activa"), false);
        return true;
    }
    const quint16 puerto = quint16(ajustes->value(QStringLiteral("sinc/puerto"), sinc::PUERTO).toUInt());
    if (!servidor.activo() && !servidor.iniciar(puerto)) {
        if (error) *error = servidor.error();
        return false;
    }
    ajustes->setValue(QStringLiteral("sinc/activa"), true);
    return true;
}

void DialogoSincronizacion::actualizar()
{
    const bool activo = servidor_.activo();
    if (activo) {
        ui->labelEstado->setText(tr("Activada: escuchando en el puerto %1. El celular puede sincronizar mientras este programa esté abierto.")
                                     .arg(servidor_.puerto()));
        QStringList dirs;
        for (const QString& d : sinc::direccionesLocales()) {
            const bool tailscale = QHostAddress(d).isInSubnet(QHostAddress(QStringLiteral("100.64.0.0")), 10);
            dirs << (tailscale ? QStringLiteral("<b>%1</b> (Tailscale: sirve fuera de casa)").arg(d) : QStringLiteral("<b>%1</b>").arg(d));
        }
        ui->labelDirecciones->setText(dirs.isEmpty() ? tr("No se encontró ninguna red.")
                                                     : tr("Direcciones de esta PC: %1").arg(dirs.join(QStringLiteral(", "))));
    } else {
        ui->labelEstado->setText(tr("Desactivada. El celular no puede sincronizar."));
        ui->labelDirecciones->clear();
    }

    const QString codigo = servidor_.codigo();
    if (codigo.isEmpty()) {
        ui->labelCodigo->clear();
        ui->labelVence->clear();
        ui->btnEmparejar->setText(tr("&Mostrar código"));
    } else {
        ui->labelCodigo->setText(codigo.left(3) + QLatin1Char(' ') + codigo.mid(3));
        const qint64 seg = std::max<qint64>(0, QDateTime::currentDateTime().secsTo(servidor_.codigoVence()));
        ui->labelVence->setText(tr("Vale por %1:%2").arg(seg / 60).arg(seg % 60, 2, 10, QLatin1Char('0')));
        ui->btnEmparejar->setText(tr("&Cancelar código"));
    }
}

void DialogoSincronizacion::cargarPares()
{
    ui->listaPares->clear();
    for (const Par& p : stock_.pares()) {
        const QString cuando = p.ultimaSinc.isValid() ? tr("última vez: %1").arg(p.ultimaSinc.toString(QStringLiteral("dd/MM/yyyy HH:mm")))
                                                      : tr("todavía no sincronizó");
        auto* it = new QListWidgetItem(QStringLiteral("%1 — %2").arg(p.nombre, cuando), ui->listaPares);
        it->setData(Qt::UserRole, p.dispositivo);
        it->setData(Qt::UserRole + 1, p.nombre);
    }
    ui->btnQuitar->setEnabled(ui->listaPares->currentItem());
    if (ui->listaPares->count() == 0) {
        auto* it = new QListWidgetItem(tr("Ninguno todavía"), ui->listaPares);
        it->setFlags(Qt::NoItemFlags);
    }
}
