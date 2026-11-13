#include "Actualizaciones.h"

#include "ClienteHttp.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

Actualizaciones::Actualizaciones(const QString &repo, QObject *parent)
    : QObject(parent), repo_(repo)
{
}

int Actualizaciones::compararVersiones(QString a, QString b)
{
    if (a.startsWith(u'v') || a.startsWith(u'V')) a.remove(0, 1);
    if (b.startsWith(u'v') || b.startsWith(u'V')) b.remove(0, 1);
    const QStringList x = a.split(u'.'), y = b.split(u'.');
    for (qsizetype i = 0; i < qMax(x.size(), y.size()); ++i) {
        const int nx = x.value(i).toInt(), ny = y.value(i).toInt();
        if (nx != ny)
            return nx > ny ? 1 : -1;
    }
    return 0;
}

void Actualizaciones::comprobar(bool forzar)
{
    QSettings ajustes;
    const QDateTime ultima = ajustes.value(QStringLiteral("actualizaciones/ultima")).toDateTime();
    if (!forzar && ultima.isValid() && ultima.secsTo(QDateTime::currentDateTime()) < 24 * 3600)
        return;
    const QUrl direccion(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repo_));
    http::pedir(this, direccion, [this](int codigo, const QByteArray &cuerpo) {
        if (codigo != 200)
            return;
        QSettings().setValue(QStringLiteral("actualizaciones/ultima"), QDateTime::currentDateTime());
        const QJsonObject release = QJsonDocument::fromJson(cuerpo).object();
        const QString version = release.value(QLatin1String("tag_name")).toString();
        if (!version.isEmpty() && compararVersiones(version, QCoreApplication::applicationVersion()) > 0)
            emit hayVersionNueva(version.startsWith(u'v') ? version.mid(1) : version,
                                 QUrl(release.value(QLatin1String("html_url")).toString()));
    });
}
