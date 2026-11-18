// Página cuyo contenido se desplaza: una columna con márgenes que no queda debajo de la barra
// de navegación del celular ni del teclado. Lo que se pone adentro va a la columna.
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: hoja
    default property alias contenido: columna.data
    property alias desplazable: flick
    background: null

    // Con adjustResize (manifiesto) la página se achica al abrir el teclado; falta correrla
    // hasta el campo que se está escribiendo, si quedó debajo
    function mostrarFoco() {
        const item = hoja.Window.activeFocusItem
        if (!item || !Qt.inputMethod.visible || !hoja.visible)
            return
        const punto = item.mapToItem(columna, 0, 0)
        if (punto.x < 0 || punto.y < 0 || punto.y > columna.height)
            return     // no es de esta página
        const arriba = item.mapToItem(flick.contentItem, 0, 0).y
        if (arriba + item.height + 16 > flick.contentY + flick.height)
            flick.contentY = Math.min(arriba + item.height + 16 - flick.height, flick.contentHeight - flick.height)
        else if (arriba - 16 < flick.contentY)
            flick.contentY = Math.max(0, arriba - 16)
    }
    onHeightChanged: Qt.callLater(mostrarFoco)
    Connections {
        target: hoja.Window.window
        function onActiveFocusItemChanged() { Qt.callLater(hoja.mostrarFoco) }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: columna.implicitHeight + 2 * Tema.margen + (Qt.inputMethod.visible ? 0 : hoja.SafeArea.margins.bottom)
        clip: true
        ScrollIndicator.vertical: ScrollIndicator {}

        ColumnLayout {
            id: columna
            x: Tema.margen
            y: Tema.margen
            width: flick.width - 2 * Tema.margen
            spacing: 12
        }
    }
}
