// Botón grande de una portada: ícono de color, título, una línea que explica qué hace y, si la
// herramienta está trabajando, su estado y una barrita de progreso
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

ItemDelegate {
    id: tarjeta
    property string icono               // nombre de comun-qt/ui/iconos o URL completa
    property string titulo
    property string detalle
    property color tono: Tema.acentoTexto
    property bool ocupada: false
    property int progreso: -1          // 0..100; -1 = sin saber cuánto falta
    property string insignia: ""       // p. ej. «Nueva versión»

    Layout.fillWidth: true
    padding: 14
    leftPadding: 14
    rightPadding: 10
    Accessible.name: titulo
    Accessible.description: detalle

    background: Rectangle {
        radius: 16
        color: tarjeta.down ? Tema.presionado : Tema.superficie
        border.color: tarjeta.visualFocus ? Tema.acentoTexto : Tema.borde
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: RowLayout {
        spacing: 14
        Rectangle {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignTop
            radius: 14
            color: Qt.rgba(tarjeta.tono.r, tarjeta.tono.g, tarjeta.tono.b, Tema.oscuro ? 0.18 : 0.12)
            Icono {
                anchors.centerIn: parent
                nombre: tarjeta.icono
                lado: 24
                color: tarjeta.tono
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3
            RowLayout {
                spacing: 8
                Label {
                    text: tarjeta.titulo
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    color: Tema.tinta
                }
                Label {
                    visible: tarjeta.insignia !== ""
                    text: tarjeta.insignia
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: "white"
                    leftPadding: 7; rightPadding: 7; topPadding: 2; bottomPadding: 2
                    background: Rectangle { radius: height / 2; color: Tema.naranja }
                }
            }
            Label {
                Layout.fillWidth: true
                text: tarjeta.detalle
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                font.pixelSize: 13
                color: tarjeta.ocupada ? tarjeta.tono : Tema.tintaSecundaria
            }
            ProgressBar {
                visible: tarjeta.ocupada
                Layout.fillWidth: true
                Layout.topMargin: 4
                from: 0; to: 100
                value: Math.max(0, tarjeta.progreso)
                indeterminate: tarjeta.progreso < 0
                Material.accent: tarjeta.tono
            }
        }
        Icono {
            Layout.alignment: Qt.AlignVCenter
            nombre: "siguiente"
            color: Tema.tintaApagada
        }
    }
}
