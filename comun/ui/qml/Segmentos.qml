// Dos o tres opciones excluyentes en una barra (como «YouTube | Redes sociales»)
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: segmentos
    property var opciones: []
    property int elegida: 0
    signal elegir(int indice)

    Layout.fillWidth: true
    implicitHeight: 46
    radius: 12
    color: Tema.oscuro ? "#262625" : "#ebebe7"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4
        Repeater {
            model: segmentos.opciones
            AbstractButton {
                required property string modelData
                required property int index
                readonly property bool elegido: segmentos.elegida === index
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: modelData
                onClicked: segmentos.elegir(index)
                Accessible.role: Accessible.RadioButton
                Accessible.checked: elegido
                background: Rectangle {
                    radius: 9
                    color: parent.elegido ? (Tema.oscuro ? "#3a3a38" : Tema.superficie) : "transparent"
                    border.color: parent.elegido ? Tema.borde : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                contentItem: Label {
                    text: parent.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.weight: parent.elegido ? Font.DemiBold : Font.Normal
                    color: parent.elegido ? Tema.acentoTexto : Tema.tintaSecundaria
                }
            }
        }
    }
}
