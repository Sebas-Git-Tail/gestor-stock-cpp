// Opción del menú lateral (Drawer): ícono opcional y texto. «destacado» la pinta de naranja con la
// insignia «Nueva versión» (para «Apps y actualizaciones» cuando hay una versión nueva).
//   ItemMenu { text: "Apps y actualizaciones"; icono: "paquete"; destacado: Apps.versionNueva !== "" }
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

ItemDelegate {
    id: item
    property string icono: ""              // nombre de comun-qt/ui/iconos (o URL completa); "" = sin ícono
    property bool destacado: false
    property string insignia: "Nueva versión"
    property color colorTexto: Tema.tinta

    Layout.fillWidth: true
    leftPadding: 20
    rightPadding: 16
    Accessible.name: destacado ? text + " (" + insignia + ")" : text

    contentItem: RowLayout {
        spacing: 16
        opacity: item.enabled ? 1 : 0.45
        Icono {
            visible: item.icono !== ""
            nombre: item.icono
            color: item.destacado ? Tema.naranja : Tema.tintaSecundaria
        }
        Label {
            Layout.fillWidth: true
            text: item.text
            elide: Text.ElideRight
            font.pixelSize: item.font.pixelSize
            font.weight: item.destacado ? Font.DemiBold : Font.Normal
            color: item.destacado ? Tema.naranja : item.colorTexto
        }
        Label {
            visible: item.destacado && item.insignia !== ""
            text: item.insignia
            font.pixelSize: 11
            font.weight: Font.DemiBold
            color: "white"
            leftPadding: 7; rightPadding: 7; topPadding: 2; bottomPadding: 2
            background: Rectangle { radius: height / 2; color: Tema.naranja }
        }
    }

    background: Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        radius: 10
        color: item.down ? Tema.presionado
             : item.destacado ? Tema.naranjaSuave : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
