#include "ModeloApps.h"

#include <QCoreApplication>

namespace apps {

ModeloApps::ModeloApps(const QString &estaApp, QObject *parent)
    : QAbstractListModel(parent), estaApp_(estaApp)
{
    for (const InfoApp &app : paraInstalar(estaApp)) {
        Fila f;
        f.act = new Actualizador(app, false, this);
        filas_ << f;
        conectar(int(filas_.size()) - 1);
    }

    if (const InfoApp *propia = buscar(estaApp)) {
        propia_ = new Actualizador(*propia, true, this);
        connect(propia_, &Actualizador::hayVersionNueva, this, [this](const QString &v) {
            versionNueva_ = v;
            emit versionNuevaCambio();
            if (propia_->tocaAvisar())
                emit avisoVersionNueva(v);
        });
        // Encontró la última y no es más nueva que la instalada (p. ej. ya se actualizó): ya no hay nada que marcar
        connect(propia_, &Actualizador::encontrada, this, [this] {
            if (versionNueva_.isEmpty() || propia_->instalada().isNull() || propia_->disponible() > propia_->instalada())
                return;
            versionNueva_.clear();
            emit versionNuevaCambio();
        });
        connect(propia_, &Actualizador::progreso, this, [this](qint64 recibido, qint64 total) {
            progresoPropio_ = total > 0 ? double(recibido) / double(total) : -1;
            emit actualizandoCambio();
        });
        connect(propia_, &Actualizador::mensaje, this, &ModeloApps::ponerMensaje);
        connect(propia_, &Actualizador::fallo, this, [this](const QString &t) {
            ponerMensaje(t);
            emit actualizandoCambio();
            emit fallo(t);
        });
        connect(propia_, &Actualizador::instalado, this, [this] { emit actualizandoCambio(); });
    }
}

QString ModeloApps::versionPropia() const
{
    return QCoreApplication::applicationVersion();
}

void ModeloApps::conectar(int fila)
{
    Actualizador *act = filas_[fila].act;
    connect(act, &Actualizador::encontrada, this, [this, fila] {
        filas_[fila].buscando = false;
        filas_[fila].buscado = true;
        filas_[fila].sinConexion = false;
        cambio(fila);
    });
    connect(act, &Actualizador::noEncontrada, this, [this, fila] {
        filas_[fila].buscando = false;
        filas_[fila].buscado = true;
        filas_[fila].sinConexion = true;
        cambio(fila);
    });
    connect(act, &Actualizador::progreso, this, [this, fila](qint64 recibido, qint64 total) {
        filas_[fila].progreso = total > 0 ? double(recibido) / double(total) : -1;
        cambio(fila);
    });
    connect(act, &Actualizador::mensaje, this, &ModeloApps::ponerMensaje);
    connect(act, &Actualizador::fallo, this, [this, fila](const QString &t) {
        filas_[fila].progreso = -1;
        ponerMensaje(t);
        cambio(fila);
        emit fallo(t);
    });
    connect(act, &Actualizador::instalado, this, [this, fila] {
        filas_[fila].progreso = -1;
        cambio(fila);
    });
}

void ModeloApps::cambio(int fila)
{
    const QModelIndex i = index(fila);
    emit dataChanged(i, i);
}

void ModeloApps::ponerMensaje(const QString &texto)
{
    mensaje_ = texto;
    emit mensajeCambio();
}

int ModeloApps::rowCount(const QModelIndex &padre) const
{
    return padre.isValid() ? 0 : int(filas_.size());
}

QHash<int, QByteArray> ModeloApps::roleNames() const
{
    return {{Nombre, "nombre"}, {Descripcion, "descripcion"}, {Instalada, "instalada"}, {Disponible, "disponible"},
            {Estado, "estado"}, {Accion, "accion"}, {Ocupado, "ocupado"}, {Progreso, "progreso"}, {Id, "id"}};
}

QVariant ModeloApps::data(const QModelIndex &indice, int rol) const
{
    if (!indice.isValid() || indice.row() >= filas_.size())
        return {};
    const Fila &f = filas_.at(indice.row());
    const Actualizador *a = f.act;
    const QString instalada = a->instalada().isNull() ? QString() : Actualizador::texto(a->instalada());
    const QString disponible = a->disponible().isNull() ? QString() : Actualizador::texto(a->disponible());
    const bool nueva = !a->disponible().isNull() && (a->instalada().isNull() || a->disponible() > a->instalada());

    switch (rol) {
    case Qt::DisplayRole:
    case Nombre: return a->app().nombre;
    case Descripcion: return a->app().descripcion;
    case Instalada: return instalada;
    case Disponible: return disponible;
    case Id: return a->app().id;
    case Ocupado: return f.buscando || a->ocupado();
    case Progreso: return f.progreso;
    case Accion:
        if (f.buscando || a->ocupado() || !nueva)
            return QString();
        return a->instalada().isNull() ? tr("Instalar") : tr("Actualizar");
    case Estado:
        if (a->ocupado())
            return tr("Instalando…");
        if (f.buscando || !f.buscado)
            return instalada.isEmpty() ? tr("Buscando…") : tr("Instalada %1 · buscando…").arg(instalada);
        if (f.sinConexion)
            return instalada.isEmpty() ? tr("No instalada · sin conexión") : tr("Instalada %1 · sin conexión").arg(instalada);
        if (instalada.isEmpty())
            return tr("No instalada · última versión %1").arg(disponible);
        if (nueva)
            return tr("Instalada %1 · hay una versión nueva: %2").arg(instalada, disponible);
        return tr("Instalada %1 · al día").arg(instalada);
    }
    return {};
}

void ModeloApps::refrescar()
{
    ponerMensaje(QString());          // «Abriendo el instalador…» ya no corresponde
    for (int i = 0; i < filas_.size(); ++i) {
        if (filas_[i].act->ocupado())
            continue;
        filas_[i].buscando = true;
        cambio(i);
        filas_[i].act->buscar();
    }
}

void ModeloApps::instalar(int fila)
{
    if (fila < 0 || fila >= filas_.size())
        return;
    filas_[fila].progreso = -1;
    filas_[fila].act->descargarEInstalar();
    cambio(fila);
}

void ModeloApps::buscarActualizacion(bool forzar)
{
    if (!propia_)
        return;
    if (forzar)
        propia_->buscar();
    else
        propia_->buscarUnaVezPorDia();
}

void ModeloApps::actualizarEstaApp()
{
    if (!propia_ || versionNueva_.isEmpty())
        return;
    progresoPropio_ = -1;
    propia_->descargarEInstalar();
    emit actualizandoCambio();
}

} // namespace apps
