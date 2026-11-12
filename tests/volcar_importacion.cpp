#include "importarexcel.h"
#include <QCoreApplication>
#include <QTextStream>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTextStream out(stdout); out.setEncoding(QStringConverter::Utf8);
    const auto r = leerPlanilla(argv[1]);
    if (!r.ok) { out << "ERROR: " << r.error << "\n"; return 1; }
    for (const auto& p : r.productos) {
        QStringList pr; for (const auto& x : p.precios) pr << x.presentacion + "=" + QString::number(x.precio, 'f', 2);
        out << "P\t" << p.fila << "\t" << p.nombre << "\t" << p.categoria << "\t" << p.proveedor << "\t" << p.vencimiento
            << "\t" << p.medidaBolsa << "\t" << p.notas << "\t" << pr.join(";") << "\n";
    }
    for (const auto& a : r.avisos) out << "A\t" << a << "\n";
    return 0;
}
