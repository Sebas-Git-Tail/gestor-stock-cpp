// Opción para tocar (una de varias, como las calidades o los filtros): borde fino y, si está elegida,
// fondo y texto del color de acento. Con «icono» muestra un ícono a la izquierda.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

AbstractButton {
    id: chip
    property bool elegido: false
    property string icono: ""
    implicitWidth: fila.implicitWidth + 28
    implicitHeight: 40
    Accessible.role: Accessible.CheckBox
    Accessible.checked: elegido
    background: Rectangle {
        radius: 10
        color: chip.elegido ? Tema.acentoSuave : (chip.down ? Tema.presionado : "transparent")
        border.color: chip.elegido ? Tema.acentoTexto : Tema.borde
        border.width: chip.elegido ? 1.5 : 1
    }
    contentItem: Item {
        implicitWidth: fila.implicitWidth
        RowLayout {
            id: fila
            anchors.centerIn: parent
            spacing: 6
            Icono {
                visible: chip.icono !== ""
                nombre: chip.icono
                lado: 16
                color: chip.elegido ? Tema.acentoTexto : Tema.tintaSecundaria
            }
            Label {
                text: chip.text
                font.weight: chip.elegido ? Font.DemiBold : Font.Normal
                color: chip.elegido ? Tema.acentoTexto : Tema.tinta
            }
        }
    }
}
