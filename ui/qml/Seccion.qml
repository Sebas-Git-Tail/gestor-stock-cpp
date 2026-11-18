// Tarjeta con un título chico arriba; lo que se pone adentro va debajo, en columna
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Pane {
    id: seccion
    property string titulo
    default property alias contenido: columna.data

    Layout.fillWidth: true
    padding: 16
    background: Rectangle { radius: Tema.radio; color: Tema.superficie; border.color: Tema.borde }

    contentItem: ColumnLayout {
        id: columna
        spacing: 10
        Label {
            visible: seccion.titulo !== ""
            text: seccion.titulo
            font.pixelSize: 12
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.8
            color: Tema.tintaApagada
        }
    }
}
