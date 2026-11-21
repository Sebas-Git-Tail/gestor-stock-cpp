// Ventana principal del celular: buscador y lista de productos. Las 3 rayitas abren lo demás (con un
// puntito si hay una versión nueva; «Apps y actualizaciones» se ve destacada en el menú).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Comun.Ui
import GestorStock

ApplicationWindow {
    id: ventana
    width: 412
    height: 880
    visible: true
    title: "Gestor de Stock"

    Material.theme: Tema.oscuro ? Material.Dark : Material.Light   // claro u oscuro: lo elige el menú (Tema.alternar)
    Component.onCompleted: Tema.acento = "#2e7d32"   // tema común (Comun.Ui) con el color de la app
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

    // Código de barras leído desde la pantalla principal: se busca (en la de venta/compra lo maneja esa página)
    Connections {
        target: App
        function onCodigoEscaneado(codigo) { if (codigo !== "" && pila.depth === 1) App.busqueda = codigo }
    }

    // Mensajito abajo («Guardado», «Sincronizado…»): AvisoFlotante de Comun.Ui
    function avisar(texto) { aviso.mostrar(texto) }
    Connections { target: App; function onAviso(texto) { ventana.avisar(texto) } }
    AvisoFlotante { id: aviso; margenAbajo: 90 }

    header: ToolBar {
        Material.foreground: "white"
        visible: pila.depth === 1
        RowLayout {
            anchors.fill: parent
            spacing: 4
            BotonMenu { punto: Apps.versionNueva !== ""; onClicked: menu.open() }
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
                text: "Escanear"
                visible: App.puedeEscanear
                onClicked: App.escanearCodigo()
                Accessible.name: "Buscar con el código de barras"
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
                    { texto: "Reportes y avisos", accion: "reportes" },
                    { texto: "Sincronizar con la PC", accion: "sinc" },
                    { texto: "Apps y actualizaciones", accion: "apps" },
                    { texto: "Acerca de", accion: "acerca" }
                ]
                ItemMenu {
                    text: modelData.texto
                    destacado: modelData.accion === "apps" && Apps.versionNueva !== ""
                    onClicked: { menu.close(); ventana.abrir(modelData.accion) }
                }
            }
            ItemMenu {   // qué cambió e informe de fallas; dejarlo apretado prende el modo Sebastián (escondido)
                text: "Novedades"
                onClicked: { menu.close(); novedades.abrirTodas() }
                onPressAndHold: novedades.alternarDesarrollador()
            }
            Rectangle { Layout.fillWidth: true; Layout.topMargin: 8; Layout.bottomMargin: 8; Layout.preferredHeight: 1; color: ventana.borde }
            ItemMenu {   // el menú queda abierto: se ve el cambio enseguida
                text: Tema.textoAlternar
                icono: Tema.iconoAlternar
                onClicked: Tema.alternar()
            }
            Item { Layout.fillHeight: true }
        }
    }

    function abrir(accion) {
        if (accion === "columnas") pila.push(paginaColumnas)
        else if (accion === "venta") pila.push(paginaOperacion, { venta: true })
        else if (accion === "compra") pila.push(paginaOperacion, { venta: false })
        else if (accion === "sinc") pila.push(paginaSinc)
        else if (accion === "reportes") pila.push(paginaReportes)
        else if (accion === "avisos") pila.push(paginaReportes, { verAvisos: true })
        else if (accion === "apps") pila.push(paginaApps)
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
                // Avisos: poco stock, vencidos o por vencer (tocar abre la lista)
                ItemDelegate {
                    Layout.fillWidth: true
                    visible: App.resumenAlertas !== ""
                    text: App.resumenAlertas + " · Ver"
                    icon.source: Tema.icono("aviso")
                    onClicked: ventana.abrir("avisos")
                    background: Rectangle { color: ventana.oscuro ? "#5a4d10" : "#fff1aa" }
                }
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
                    EstadoVacio {
                        anchors.centerIn: parent
                        width: parent.width - 48
                        visible: lista.count === 0
                        icono: App.busqueda !== "" ? "buscar" : "paquete"
                        titulo: App.busqueda !== "" ? "No hay productos que coincidan" : "Todavía no hay productos"
                        texto: App.busqueda !== "" ? "Probá con otra palabra, un código o un precio."
                                                   : "Agregá el primero o sincronizá con la PC para traer la lista."
                        textoBoton: App.busqueda !== "" ? "Limpiar la búsqueda" : "Agregar un producto"
                        onAccion: App.busqueda !== "" ? App.busqueda = "" : ventana.editar(0)
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
    Component { id: paginaReportes; PaginaReportes {} }
    // Lo nuevo de esta versión: se abre solo la primera vez (comun/ui/qml/DialogoNovedades.qml)
    DialogoNovedades { id: novedades }

    Component { id: paginaApps; PaginaApps { conBarra: true; nombreApp: "Gestor de Stock" } }

    // Versión nueva de la app (se busca cada vez que se abre: marca el menú; pregunta como mucho una vez por día)
    Timer { interval: 4000; running: true; onTriggered: Apps.buscarActualizacion(false) }
    Connections {
        target: Apps
        function onAvisoVersionNueva() { if (pila.depth === 1) dialogoVersion.open() }
    }
    Dialog {
        id: dialogoVersion
        title: "Versión nueva"
        anchors.centerIn: parent
        width: Math.min(ventana.width - 48, 360)
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Hay una versión nueva del Gestor de Stock: la " + Apps.versionNueva + " (tenés la " + Apps.versionPropia + ").\n\n¿La instalo ahora?"
        }
        onAccepted: { Apps.actualizarEstaApp(); pila.push(paginaApps) }
    }

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
