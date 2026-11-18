#include "DialogoPistas.h"
#include "ui_DialogoPistas.h"

#include "OpcionesPistas.h"

#include <QComboBox>
#include <QKeyEvent>
#include <QTimer>
#include <QLocale>
#include <QPushButton>

#include <QHash>

#include <algorithm>
#include <functional>

// Dato de cada fila: el código de idioma (lo que después recibe Descargar.sh)
constexpr int RolIdioma = Qt::UserRole;
// Subtítulos automáticos que no se ven de entrada (traducciones a otros idiomas)
constexpr int RolSecundario = Qt::UserRole + 1;

namespace {

// Llena la lista con casillas, en el orden que dio OpcionesPistas
void llenar(QListWidget *lista, const QList<OpcionesPistas::FilaIdioma> &filas)
{
    for (const OpcionesPistas::FilaIdioma &fila : filas) {
        auto *item = new QListWidgetItem(fila.texto, lista);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(RolIdioma, fila.idioma);
        item->setData(RolSecundario, fila.secundario);
        item->setToolTip(fila.ayuda);
    }
    // Un clic en cualquier parte de la fila marca o desmarca, no solo en la casilla. Si el
    // clic fue en la casilla, Qt ya la cambió: por eso se compara con el estado que tenía
    // al apretar el botón (guardado en la propia lista).
    QObject::connect(lista, &QListWidget::itemPressed, lista, [lista](QListWidgetItem *item) {
        lista->setProperty("estadoAlApretar", static_cast<int>(item->checkState()));
    });
    QObject::connect(lista, &QListWidget::itemClicked, lista, [lista](QListWidgetItem *item) {
        if (static_cast<int>(item->checkState()) == lista->property("estadoAlApretar").toInt())
            item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    });
}

QStringList marcados(const QListWidget *lista)
{
    QStringList idiomas;
    for (int i = 0; i < lista->count(); ++i) {
        if (lista->item(i)->checkState() == Qt::Checked)
            idiomas << lista->item(i)->data(RolIdioma).toString();
    }
    return idiomas;
}

} // namespace

DialogoPistas::DialogoPistas(const QList<PistaAudio> &audios, const QList<Subtitulo> &subtitulos,
                             const QList<FormatoVideo> &formatos_, const QStringList &codecsAudio, QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogoPistas), formatos(formatos_)
{
    ui->setupUi(this);
    ui->botones->button(QDialogButtonBox::Ok)->setText(tr("Descargar"));
    ui->botones->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(ui->botones, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->botones, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ── Calidad y códecs: solo lo que ofrece el video, de mayor a menor calidad. Lo primero
    // de cada lista es «sin preferencia» (lo de siempre: lo mejor que haya) ──
    ui->comboCalidad->addItem(OpcionesPistas::textoCalidad(0), 0);
    for (int alto : OpcionesPistas::calidades(formatos))
        ui->comboCalidad->addItem(OpcionesPistas::textoCalidad(alto), alto);
    ui->comboVcodec->addItem(tr("Cualquiera"), QString());
    for (const QString &familia : OpcionesPistas::familiasVideo(formatos))
        ui->comboVcodec->addItem(nombreCodec(familia), familia);
    ui->comboAcodec->addItem(tr("Cualquiera"), QString());
    for (const QString &familia : OpcionesPistas::familiasAudio(codecsAudio))
        ui->comboAcodec->addItem(nombreCodec(familia), familia);

#ifdef Q_OS_ANDROID
    // En Android la lista de un desplegable es una ventana aparte que no se abre bien sobre el
    // diálogo: cada desplegable se cambia por opciones para tocar, una al lado de la otra
    for (QComboBox *combo : {ui->comboCalidad, ui->comboVcodec, ui->comboAcodec}) {
        auto *opciones = new QListWidget(combo->parentWidget());
        opciones->setObjectName(combo->objectName() + QStringLiteral("Opciones"));
        opciones->setFlow(QListView::LeftToRight);
        opciones->setWrapping(true);
        opciones->setResizeMode(QListView::Adjust);
        opciones->setSpacing(4);
        opciones->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        opciones->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        opciones->setSelectionMode(QAbstractItemView::SingleSelection);
        opciones->setStyleSheet(QStringLiteral("QListWidget::item { padding: 10px 14px; border: 1px solid palette(mid); }"
                                               "QListWidget::item:selected { background: palette(highlight); }"));
        for (int i = 0; i < combo->count(); ++i)
            new QListWidgetItem(combo->itemText(i), opciones);
        opciones->setCurrentRow(combo->currentIndex());
        connect(opciones, &QListWidget::currentRowChanged, combo, [combo](int fila) {
            if (fila >= 0)
                combo->setCurrentIndex(fila);
        });
        // Alto justo para que entren todas las opciones (sin barra para desplazar)
        opciones->setFixedHeight(qMax(56, ((combo->count() + 2) / 3) * 58));
        ui->layoutCalidad->replaceWidget(combo, opciones);
        combo->hide();
    }
#endif
    if (formatos.isEmpty()) {
        ui->grupoCalidad->hide();      // el sitio no informó calidades: se baja la mejor
    } else {
        connect(ui->comboCalidad, &QComboBox::currentIndexChanged, this, &DialogoPistas::marcarCodecsDisponibles);
        connect(ui->comboVcodec, &QComboBox::currentIndexChanged, this, &DialogoPistas::marcarCodecsDisponibles);
        marcarCodecsDisponibles();
    }

    // ── Pistas de audio y subtítulos, en el orden de OpcionesPistas ──
    const QList<OpcionesPistas::FilaIdioma> filasAudio = OpcionesPistas::filasAudio(audios);
    if (filasAudio.size() > 1) {
        llenar(ui->listaAudios, filasAudio);
        // Marca todas o, si ya lo están, las desmarca
        connect(ui->botonTodosAudios, &QPushButton::clicked, this, [this] {
            const bool marcar = marcados(ui->listaAudios).size() < ui->listaAudios->count();
            for (int i = 0; i < ui->listaAudios->count(); ++i)
                ui->listaAudios->item(i)->setCheckState(marcar ? Qt::Checked : Qt::Unchecked);
        });
    } else {
        // Con una sola pista no hay nada que elegir
        ui->etiquetaAudios->setText(tr("El video tiene una sola pista de audio: no hay otras para elegir."));
        ui->listaAudios->hide();
        ui->botonTodosAudios->hide();
    }

    const QList<OpcionesPistas::FilaIdioma> filasSubtitulos = OpcionesPistas::filasSubtitulos(subtitulos);
    const auto secundarios = std::count_if(filasSubtitulos.cbegin(), filasSubtitulos.cend(),
                                           [](const OpcionesPistas::FilaIdioma &f) { return f.secundario; });
    if (filasSubtitulos.isEmpty()) {
        ui->etiquetaSubtitulos->setText(tr("El video no ofrece subtítulos."));
        ui->editFiltro->hide();
        ui->listaSubtitulos->hide();
        ui->checkAutomaticos->hide();
    } else {
        llenar(ui->listaSubtitulos, filasSubtitulos);
        // El filtro solo hace falta cuando la lista es larga
        ui->editFiltro->setVisible(filasSubtitulos.size() > 8);
        ui->checkAutomaticos->setVisible(secundarios > 0);
        ui->checkAutomaticos->setText(tr("Mostrar todas las traducciones automáticas (%1 más)").arg(secundarios));
        connect(ui->editFiltro, &QLineEdit::textChanged, this, &DialogoPistas::filtrarSubtitulos);
        connect(ui->checkAutomaticos, &QCheckBox::toggled, this, &DialogoPistas::filtrarSubtitulos);
        filtrarSubtitulos();
    }
}

DialogoPistas::~DialogoPistas()
{
    delete ui;
}

// Enter mientras se escribe en el filtro no hace nada: sin esto QDialog lo toma como un
// clic en «Descargar» y la descarga empezaría antes de terminar de elegir
void DialogoPistas::keyPressEvent(QKeyEvent *evento)
{
    const bool enter = evento->key() == Qt::Key_Return || evento->key() == Qt::Key_Enter;
    if (enter && ui->editFiltro->hasFocus())
        return;
    QDialog::keyPressEvent(evento);
}

int DialogoPistas::abrir()
{
#ifdef Q_OS_ANDROID
    setWindowState(Qt::WindowMaximized);   // en el celular, a pantalla completa
    // Cuando ya están acomodadas, cada lista de opciones toma el alto justo para que se vean todas
    QTimer::singleShot(0, this, [this] {
        const QList<QListWidget *> listas = findChildren<QListWidget *>();
        for (QListWidget *opciones : listas) {
            if (!opciones->objectName().endsWith(QLatin1String("Opciones")) || opciones->count() == 0)
                continue;
            const QRect ultima = opciones->visualItemRect(opciones->item(opciones->count() - 1));
            opciones->setFixedHeight(ultima.bottom() + opciones->spacing() + 2 * opciones->frameWidth() + 2);
        }
    });
#endif
    return exec();
}

Eleccion DialogoPistas::eleccion() const
{
    Eleccion e;
    e.audios = marcados(ui->listaAudios);
    e.subtitulos = marcados(ui->listaSubtitulos);
    e.calidad = ui->comboCalidad->currentData().toInt();
    e.vcodec = ui->comboVcodec->currentData().toString();
    e.acodec = ui->comboAcodec->currentData().toString();
    return e;
}

QString DialogoPistas::nombreCodec(const QString &familia)
{
    return OpcionesPistas::nombreCodec(familia);
}

// Con una calidad elegida, los códecs de video que no existen en ella se marcan: si se
// eligen igual, se baja ese códec en la calidad más cercana por debajo (o la calidad con
// otro códec), nunca falla. Debajo se explica qué se va a bajar.
void DialogoPistas::marcarCodecsDisponibles()
{
    const int alto = ui->comboCalidad->currentData().toInt();
    for (int i = 1; i < ui->comboVcodec->count(); ++i)
        ui->comboVcodec->setItemText(i, OpcionesPistas::textoCodecVideo(ui->comboVcodec->itemData(i).toString(), alto, formatos));
#ifdef Q_OS_ANDROID
    if (auto *opciones = findChild<QListWidget *>(QStringLiteral("comboVcodecOpciones"))) {
        for (int i = 0; i < opciones->count() && i < ui->comboVcodec->count(); ++i)
            opciones->item(i)->setText(ui->comboVcodec->itemText(i));
    }
#endif
    ui->etiquetaCalidad->setText(OpcionesPistas::explicacion(alto, ui->comboVcodec->currentData().toString(), formatos));
}

// Muestra las filas que coinciden con el filtro. Las traducciones automáticas a otros
// idiomas solo se ven si se pidieron con la casilla, si se las busca por nombre o si ya
// están marcadas (para no perder de vista lo que se va a descargar).
void DialogoPistas::filtrarSubtitulos()
{
    const QString filtro = ui->editFiltro->text().trimmed();
    for (int i = 0; i < ui->listaSubtitulos->count(); ++i) {
        QListWidgetItem *item = ui->listaSubtitulos->item(i);
        const bool coincide = item->text().contains(filtro, Qt::CaseInsensitive);
        const bool oculto = item->data(RolSecundario).toBool() && filtro.isEmpty()
                            && !ui->checkAutomaticos->isChecked() && item->checkState() != Qt::Checked;
        item->setHidden(!coincide || oculto);
    }
}
