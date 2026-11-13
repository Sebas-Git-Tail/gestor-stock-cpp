#include "ResumenSimulacion.h"

#include <QRegularExpression>

namespace {

// Puede haber decenas de miles: se cuentan todos, pero se listan solo estos
constexpr qsizetype MaximoEnLista = 200;

} // namespace

bool ResumenSimulacion::procesar(const QString &linea)
{
    static const QRegularExpression borrado(QStringLiteral("^\\*deleting\\s+(.+)$"));
    // <f+++++++++ ruta: tipo de cambio, tipo de archivo y 9 atributos
    static const QRegularExpression cambio(QStringLiteral("^([<>ch.])([fdLDS])\\S{9} (.+)$"));
    static const QRegularExpression total(QStringLiteral("^Total transferred file size: (\\S+) bytes$"));

    const QRegularExpressionMatch aBorrar = borrado.match(linea);
    if (aBorrar.hasMatch()) {
        const QString ruta = aBorrar.captured(1);
        if (!ruta.endsWith(u'/')) {    // las carpetas no cuentan: sus archivos ya aparecen uno por uno
            ++borrar;
            if (listaBorrar.size() < MaximoEnLista)
                listaBorrar << ruta;
        }
        return true;
    }

    const QRegularExpressionMatch cambiado = cambio.match(linea);
    if (cambiado.hasMatch()) {
        // <f o >f: el contenido se copiaría. El resto son carpetas o cambios de fecha y permisos.
        if (cambiado.captured(2) == u"f" && (cambiado.captured(1) == u"<" || cambiado.captured(1) == u">"))
            ++copiar;
        return true;
    }

    const QRegularExpressionMatch tamanoTotal = total.match(linea);
    if (tamanoTotal.hasMatch()) {
        tamano = tamanoTotal.captured(1);
        return true;
    }

    if (linea.contains(QStringLiteral("skipping file deletion")))
        omitidos = true;    // esta línea también se muestra como aviso
    return false;
}
