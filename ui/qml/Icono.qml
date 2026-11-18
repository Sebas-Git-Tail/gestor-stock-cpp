// Ícono de trazo de comun-qt/ui/iconos, pintado de un color:  Icono { nombre: "buscar"; color: Tema.tinta }
import QtQuick
import QtQuick.Controls.impl

IconImage {
    property string nombre
    property int lado: 20
    source: nombre === "" ? "" : Tema.icono(nombre)
    sourceSize: Qt.size(lado, lado)
    color: Tema.tinta
}
