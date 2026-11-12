// Ventana principal del celular: buscador y lista de productos. El botón «Opciones» abre lo demás.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GestorStock

ApplicationWindow {
    id: ventana
    width: 412
    height: 880
    visible: true
    title: "Gestor de Stock"

    Material.theme: Material.System
    Material.accent: "#2e7d32"
    Material.primary: "#2e7d32"

    readonly property bool oscuro: Material.theme === Material.Dark
    readonly property color tintaSecundaria: oscuro ? "#c3c2b7" : "#52514e"
    readonly property color tintaApagada: "#898781"
    readonly property color superficie: oscuro ? "#1a1a19" : "#fcfcfb"
    readonly property color borde: oscuro ? "#2c2c2a" : "#e1e0d9"

    // Botón «atrás» de Android: cierra el menú o vuelve a la pantalla anterior antes de salir
    onClosing: (cierre) => {
        if (menu.opened) { menu.close(); cierre.accepted = false }
        else if (pila.depth > 1) { pila.pop(); cierre.accepted = false }
    }

    // Al volver a la app (o abrirla), sincroniza sola si hay PC emparejada
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive) App.sincronizarSiCorresponde()
        }
    }
    Timer { interval: 1500; running: true; onTriggered: App.sincronizarSiCorresponde() }

    // Mensajito abajo («Producto guardado», «Sincronizado…»)
    function avisar(texto) { aviso.texto = texto; aviso.open() }
    Connections { target: App; function onAviso(texto) { ventana.avisar(texto) } }
    Popup {
        id: aviso
        property string texto
        x: (ventana.width - width) / 2
        y: ventana.height - height - 90
        width: Math.min(ventana.width - 32, etiquetaAviso.implicitWidth + 32)
        padding: 12
        modal: false
        closePolicy: Popup.CloseOnPressOutside
        background: Rectangle { radius: 8; color: ventana.oscuro ? "#f0efec" : "#2c2c2a" }
        contentItem: Label { id: etiquetaAviso; text: aviso.texto; wrapMode: Text.WordWrap; color: ventana.oscuro ? "#0b0b0b" : "#f0efec" }
        onOpened: cerrarAviso.restart()
        Timer { id: cerrarAviso; interval: 3500; onTriggered: aviso.close() }
    }

    header: ToolBar {
        Material.foreground: "white"
        visible: pila.depth === 1
        RowLayout {
            anchors.fill: parent
            spacing: 4
            ToolButton { text: "Opciones"; onClicked: menu.open(); Accessible.name: "Menú" }
            TextField {
                id: campoBuscar
                Layout.fillWidth: true
                placeholderText: "Buscar nombre, código o precio…"
                text: App.busqueda
                inputMethodHints: Qt.ImhNoPredictiveText
                onDisplayTextChanged: App.busqueda = displayText
                color: "white"
                placeholderTextColor: "#c8e6c9"
            }
            ToolButton {
                text: App.sincronizando ? "Sinc…" : "Sinc"
                visible: App.pcEmparejada
                enabled: !App.sincronizando
                onClicked: App.sincronizar()
                Accessible.name: "Sincronizar con la PC"
            }
        }
    }

    Drawer {
        id: menu
        width: Math.min(ventana.width * 0.8, 320)
        height: ventana.height
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Label { text: "Gestor de Stock"; font.pixelSize: 20; font.bold: true; Layout.margins: 20 }
            Repeater {
                model: [
                    { texto: "Columnas de la tabla", accion: "columnas" },
                    { texto: "Registrar una venta", accion: "venta" },
                    { texto: "Registrar una compra", accion: "compra" },
                    { texto: "Sincronizar con la PC", accion: "sinc" },
                    { texto: "Acerca de", accion: "acerca" }
                ]
                ItemDelegate {
                    Layout.fillWidth: true
                    text: modelData.texto
                    onClicked: { menu.close(); ventana.abrir(modelData.accion) }
                }
            }
            Item { Layout.fillHeight: true }
        }
    }

    function abrir(accion) {
        if (accion === "columnas") pila.push(paginaColumnas)
        else if (accion === "venta") pila.push(paginaOperacion, { venta: true })
        else if (accion === "compra") pila.push(paginaOperacion, { venta: false })
        else if (accion === "sinc") pila.push(paginaSinc)
        else if (accion === "acerca") acerca.open()
    }
    function editar(id) { pila.push(editor, { idProducto: id }) }

    StackView { id: pila; anchors.fill: parent; initialItem: principal }

    Component {
        id: principal
        Page {
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                Label { text: App.resumen; color: ventana.tintaSecundaria; Layout.margins: 10; font.pixelSize: 13 }
                ListView {
                    id: lista
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: App.productos
                    delegate: ItemDelegate {
                        required property var modelData
                        width: lista.width
                        onClicked: ventana.editar(modelData.id)
                        background: Rectangle {
                            color: modelData.estado === 3 ? (ventana.oscuro ? "#6b2a2a" : "#f08080")
                                 : modelData.estado === 2 ? (ventana.oscuro ? "#5a4d10" : "#fff1aa")
                                 : modelData.estado === 1 ? (ventana.oscuro ? "#4a2a2a" : "#ffd6d6") : "transparent"
                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: ventana.borde }
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { text: modelData.nombre; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: modelData.precios; visible: text !== ""; font.pixelSize: 13; color: ventana.tintaSecundaria; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: modelData.detalle; visible: text !== ""; font.pixelSize: 12; color: ventana.tintaApagada; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            }
                            Label { text: modelData.cantidad; font.pixelSize: 24; font.bold: true }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: lista.count === 0
                        width: parent.width - 48
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: ventana.tintaApagada
                        text: App.busqueda !== "" ? "No hay productos que coincidan." : "Todavía no hay productos. Tocá «+ Producto» o sincronizá con la PC."
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 8
                    spacing: 8
                    Button { text: "Compra"; Layout.fillWidth: true; onClicked: ventana.abrir("compra") }
                    Button { text: "Venta"; Layout.fillWidth: true; onClicked: ventana.abrir("venta") }
                    Button { text: "+ Producto"; Layout.fillWidth: true; highlighted: true; onClicked: ventana.editar(0) }
                }
            }
        }
    }

    Component { id: editor; EditorProducto {} }
    Component { id: paginaColumnas; PaginaColumnas {} }
    Component { id: paginaOperacion; PaginaOperacion {} }
    Component { id: paginaSinc; PaginaSincronizacion {} }

    Dialog {
        id: acerca
        title: "Gestor de Stock " + App.version
        modal: true
        anchors.centerIn: parent
        width: Math.min(ventana.width - 32, 400)
        standardButtons: Dialog.Close
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Productos, columnas, ventas y compras.\n\nSe sincroniza con Gestor de Stock de la PC (Linux).\n\nHecho con C++ y Qt."
        }
    }
}
