// Lo que se ve cuando una lista está vacía: ícono grande, título, una explicación y (si hay
// textoBoton) un botón para empezar
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

ColumnLayout {
    id: vacio
    property string icono: "paquete"
    property string titulo
    property string texto
    property string textoBoton: ""
    signal accion()

    spacing: 8
    Icono {
        Layout.alignment: Qt.AlignHCenter
        nombre: vacio.icono
        lado: 56
        color: Tema.tintaApagada
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 4
        text: vacio.titulo
        font.pixelSize: 18
        font.weight: Font.DemiBold
        color: Tema.tinta
    }
    Label {
        visible: vacio.texto !== ""
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: 320
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: vacio.texto
        wrapMode: Text.WordWrap
        color: Tema.tintaSecundaria
    }
    Button {
        visible: vacio.textoBoton !== ""
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: 8
        text: vacio.textoBoton
        highlighted: true
        onClicked: vacio.accion()
    }
}
