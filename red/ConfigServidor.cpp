#include "ConfigServidor.h"

#include "ClienteHttp.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

// Huellas del servidor de respaldos (/etc/ssh/ssh_host_*_key.pub de la laptop). Si se reinstala el
// servidor, actualizarlas acá y en Arch-Linux-Manager (Scripts/actualizar-huellas-ssh.sh).
const QStringList Huellas{
    QStringLiteral("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAII/sui5PwkogBYEpNwHnwBQ4tGEG4rIPi+iJlnNs1XJC"),
    QStringLiteral("ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBPb+hIC7I3q7+IY2elPWZuOELWrtlBIzxC0xo/T7DAnj6vPdrcEDS9J3Z1VYKM1RsDvVGvFGybUi2h8jG1dRqOc="),
    QStringLiteral("ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQDKdoUWxkghfT0vPu72sgl7nmFQd/kIrdwu+m+82Yh5dcdbScExHkCLUP80L1JRXXF4dvJ4lzb6MlbVQknHripLzmcv5erlQSI9CLMzDPAOJCPCXgR1t1SvGc7yDTZ+mC992eBF6FcoA3CX0sIKlrA0ROiPSCJuaIjYTcZfjmhJ24yHj7FhqY38eXhmJUsJsR/jcOARArwJtx5o5II7zA/fzGDJV43Ak3QNkSIZo30nXlSFA6xR105UC4+qqLM1IXi69FH5tgLz+IvBW0ejhR2BmYGkyQ+8P5G75Tuwk10D9rpYd3Cwlirzdv/QhNPgY35Q6nlZVV4rWrAH2uZJftgPgXLPjTyTfM/X96/RenzJxzUOsH4LeuqSpOZ5g+HsFVGrZZfFj8r+namlHGrvvJNxTXrijmZ6cdm9TzOhqDaErg3u+gtEsY02ogN/WIJr3HEgUCfAX/dOub7YOXsI9xJJrJ7P8wi8IGaLNpLr9QaaXmhUTaCvOouKL2ci46LFVIU="),
};

bool coincide(const QString &valor, const QRegularExpression &formato)
{
    return formato.match(valor).hasMatch();
}

}

ConfigServidor &ConfigServidor::instancia()
{
    static ConfigServidor config;
    return config;
}

ConfigServidor::ConfigServidor()
{
    restablecer();
    QFile copia(rutaCopia());
    if (copia.open(QIODevice::ReadOnly))
        aplicar(copia.readAll());
}

void ConfigServidor::restablecer()
{
    usuario_ = QStringLiteral("sebastian");
    respaldos_ = QStringLiteral("/home/sebastian/Respaldos/");
    ipLocal_ = QStringLiteral("192.168.100.14");
    ipTailscale_ = QStringLiteral("100.101.213.44");
    webPublica_ = QStringLiteral("https://bodrio.seb-dev.fyi");
    bodrio_ = {QStringLiteral("https://bodrio.seb-dev.fyi/Web/Videos/Bodrio/"),
               QStringLiteral("https://archlinux.tail0dedb3.ts.net/Web/Videos/Bodrio/")};
}

QString ConfigServidor::urlConfig()
{
    const QString prueba = qEnvironmentVariable("SERVIDOR_CONFIG_URL");
    return prueba.isEmpty() ? QStringLiteral("https://bodrio.seb-dev.fyi/recursos/servidor.json") : prueba;
}

QString ConfigServidor::rutaCopia()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("servidor.json"));
}

QStringList ConfigServidor::huellasSsh(const QStringList &nombres)
{
    QStringList lineas;
    for (const QString &nombre : nombres)
        for (const QString &huella : Huellas)
            lineas << nombre + u' ' + huella;
    return lineas;
}

bool ConfigServidor::aplicar(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument documento = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !documento.isObject())
        return false;
    const QJsonObject datos = documento.object();
    if (datos.value(QLatin1String("version")).toInt() != 1)
        return false;

    static const QRegularExpression ip(QStringLiteral("^\\d{1,3}(\\.\\d{1,3}){3}$"));
    static const QRegularExpression url(QStringLiteral("^https?://[A-Za-z0-9.\\-]+(:\\d+)?(/[A-Za-z0-9._~%/\\-]*)?$"));
    static const QRegularExpression ruta(QStringLiteral("^/[A-Za-z0-9._/\\-]+$"));
    static const QRegularExpression nombreUsuario(QStringLiteral("^[a-z_][a-z0-9_\\-]*$"));
    const auto tomar = [&](const char *clave, QString &destino, const QRegularExpression &formato) {
        const QString valor = datos.value(QLatin1String(clave)).toString();
        if (coincide(valor, formato))
            destino = valor;
    };
    tomar("usuario", usuario_, nombreUsuario);
    tomar("ip_local", ipLocal_, ip);
    tomar("ip_tailscale", ipTailscale_, ip);
    tomar("web_publica", webPublica_, url);
    QString respaldos;
    tomar("respaldos", respaldos, ruta);
    if (!respaldos.isEmpty() && !respaldos.contains(QLatin1String("..")))
        respaldos_ = respaldos.endsWith(u'/') ? respaldos : respaldos + u'/';
    QStringList bodrio;
    for (const QJsonValue &valor : datos.value(QLatin1String("bodrio")).toArray()) {
        const QString direccion = valor.toString();
        if (coincide(direccion, url))
            bodrio << (direccion.endsWith(u'/') ? direccion : direccion + u'/');
    }
    if (!bodrio.isEmpty())
        bodrio_ = bodrio;
    return true;
}

void ConfigServidor::actualizar(bool forzar)
{
    if (pidiendo_)
        return;
    const QFileInfo copia(rutaCopia());
    if (!forzar && copia.exists() && copia.lastModified().secsTo(QDateTime::currentDateTime()) < 24 * 3600)
        return;
    pidiendo_ = true;
    http::pedir(this, QUrl(urlConfig()), [this](int codigo, const QByteArray &cuerpo) {
        pidiendo_ = false;
        if (codigo != 200 || !aplicar(cuerpo))
            return;    // sin internet o roto: queda lo que había
        QDir().mkpath(QFileInfo(rutaCopia()).absolutePath());
        QSaveFile archivo(rutaCopia());
        if (archivo.open(QIODevice::WriteOnly)) {
            archivo.write(cuerpo);
            archivo.commit();
        }
        emit actualizada();
    });
}
