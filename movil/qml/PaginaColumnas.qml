// Columnas: agregar, renombrar, cambiar el tipo, mostrar u ocultar, ordenar y eliminar.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GestorStock

Page {
    id: pagina
    property var elegida: ({})
    property bool agregando: false

    function mostrar(error) { if (error !== "") ventana.avisar(error) }

    header: ToolBar {
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "←"; font.pixelSize: 22; onClicked: pila.pop(); Accessible.name: "Volver" }
            Label { text: "Columnas de la tabla"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Label {
            Layout.fillWidth: true
            Layout.margins: 12
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: ventana.tintaApagada
            text: "El tilde es «se ve en la lista». Tocá una columna para renombrarla, cambiarle el tipo o eliminarla. Nombre, Precios y Cantidad no se pueden eliminar."
        }
        ListView {
            id: lista
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.columnas
            delegate: ItemDelegate {
                required property var modelData
                required property int index
                width: lista.width
                onClicked: { pagina.elegida = modelData; opciones.popup() }
                contentItem: RowLayout {
                    spacing: 6
                    CheckBox {
                        checked: modelData.visible
                        onClicked: pagina.mostrar(App.mostrarColumna(modelData.id, checked))
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Label { text: modelData.nombre; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { text: modelData.tipoNombre; font.pixelSize: 12; color: ventana.tintaApagada }
                    }
                    Button { text: "Subir"; flat: true; enabled: index > 0; onClicked: pagina.mostrar(App.moverColumna(modelData.id, -1)) }
                    Button { text: "Bajar"; flat: true; enabled: index < lista.count - 1; onClicked: pagina.mostrar(App.moverColumna(modelData.id, 1)) }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            Button { text: "Volver a poner…"; visible: App.columnasQuitadas.length > 0; Layout.fillWidth: true; onClicked: restaurar.open() }
            Button { text: "+ Agregar columna"; highlighted: true; Layout.fillWidth: true; onClicked: { pagina.agregando = true; campoNombre.text = ""; tipoNuevo.currentIndex = 0; dialogoNombre.open() } }
        }
    }

    Menu {
        id: opciones
        MenuItem { text: "Renombrar"; onTriggered: { pagina.agregando = false; campoNombre.text = pagina.elegida.nombre; dialogoNombre.open() } }
        MenuItem { text: "Cambiar tipo"; enabled: pagina.elegida.propia === true; onTriggered: dialogoTipo.open() }
        MenuItem { text: pagina.elegida.propia ? "Eliminar" : "Quitar"; enabled: pagina.elegida.esencial !== true; onTriggered: dialogoEliminar.open() }
    }

    Dialog {
        id: dialogoNombre
        title: pagina.agregando ? "Columna nueva" : "Renombrar «" + pagina.elegida.nombre + "»"
        modal: true
        anchors.centerIn: parent
        width: Math.min(ventana.width - 32, 380)
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            width: parent.width
            TextField { id: campoNombre; Layout.fillWidth: true; placeholderText: "Nombre" }
            ComboBox { id: tipoNuevo; visible: pagina.agregando; Layout.fillWidth: true; model: ["Texto", "Número", "Fecha"] }
        }
        onOpened: campoNombre.forceActiveFocus()
        onAccepted: pagina.mostrar(pagina.agregando ? App.agregarColumna(campoNombre.text, tipoNuevo.currentIndex)
                                                    : App.renombrarColumna(pagina.elegida.id, campoNombre.text))
    }

    Dialog {
        id: dialogoTipo
        title: "Tipo de «" + pagina.elegida.nombre + "»"
        modal: true
        anchors.centerIn: parent
        width: Math.min(ventana.width - 32, 380)
        standardButtons: Dialog.Ok | Dialog.Cancel
        ComboBox { id: tipoElegido; width: parent.width; model: ["Texto", "Número", "Fecha"]; currentIndex: pagina.elegida.tipo || 0 }
        onAccepted: pagina.mostrar(App.cambiarTipoColumna(pagina.elegida.id, tipoElegido.currentIndex))
    }

    Dialog {
        id: dialogoEliminar
        title: pagina.elegida.propia ? "¿Eliminar «" + pagina.elegida.nombre + "»?" : "¿Quitar «" + pagina.elegida.nombre + "»?"
        modal: true
        anchors.centerIn: parent
        width: Math.min(ventana.width - 32, 380)
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: pagina.elegida.propia ? "Se borra también lo que cargaste en ella en todos los productos. No se puede deshacer."
                                        : "No se pierde nada: lo cargado queda guardado y se recupera con «Volver a poner»."
        }
        onAccepted: pagina.mostrar(App.eliminarColumna(pagina.elegida.id))
    }

    Dialog {
        id: restaurar
        title: "Volver a poner"
        modal: true
        anchors.centerIn: parent
        width: Math.min(ventana.width - 32, 380)
        standardButtons: Dialog.Cancel
        ColumnLayout {
            width: parent.width
            Repeater {
                model: App.columnasQuitadas
                Button {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.nombre
                    onClicked: { pagina.mostrar(App.restaurarColumna(modelData.id)); restaurar.close() }
                }
            }
        }
    }
}
