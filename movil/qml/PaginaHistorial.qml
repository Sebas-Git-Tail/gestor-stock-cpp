// Historial de ventas y compras: lo hecho, por fecha, con el detalle de cada una (tocándola). «Anular»
// devuelve el stock y la deja marcada como anulada (sirve para una devolución); los reportes ya no la cuentan.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GestorStock

Page {
    id: pagina
    property int tipo: 0                             // 0 todas, 1 ventas, 2 compras
    property string desdeIso: App.fechaIso(30)
    property string hastaIso: App.hoyIso()
    property bool conAnuladas: true
    property var lista: []
    property int abierta: -1                         // id de la que muestra el detalle
    property var aAnular: ({})

    function recargar() { lista = App.historial(tipo, desdeIso, hastaIso, conAnuladas) }
    onTipoChanged: recargar()
    onDesdeIsoChanged: recargar()
    onHastaIsoChanged: recargar()
    onConAnuladasChanged: recargar()
    Component.onCompleted: recargar()

    header: ToolBar {
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "←"; font.pixelSize: 22; onClicked: pila.pop(); Accessible.name: "Volver" }
            Label { text: "Historial"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 4
            ComboBox {
                Layout.fillWidth: true
                model: ["Ventas y compras", "Solo ventas", "Solo compras"]
                currentIndex: pagina.tipo
                onActivated: (i) => pagina.tipo = i
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: "Desde " + App.fechaCorta(pagina.desdeIso); flat: true; Layout.fillWidth: true; onClicked: selectorDesde.abrir(pagina.desdeIso) }
                Button { text: "Hasta " + App.fechaCorta(pagina.hastaIso); flat: true; Layout.fillWidth: true; onClicked: selectorHasta.abrir(pagina.hastaIso) }
            }
            CheckBox { text: "Mostrar las anuladas"; checked: pagina.conAnuladas; onToggled: pagina.conAnuladas = checked }
        }
        ListView {
            id: listaHistorial
            objectName: "listaHistorial"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: pagina.lista
            delegate: ItemDelegate {
                required property var modelData
                width: listaHistorial.width
                onClicked: pagina.abierta = pagina.abierta === modelData.id ? -1 : modelData.id
                background: Rectangle {
                    color: "transparent"
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: ventana.borde }
                }
                contentItem: ColumnLayout {
                    spacing: 2
                    opacity: modelData.anulada ? 0.55 : 1
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: modelData.tipo; font.bold: true; color: modelData.venta ? "#2e7d32" : ventana.tintaSecundaria }
                        Label { text: modelData.fecha; color: ventana.tintaApagada; font.pixelSize: 13; Layout.fillWidth: true }
                        Label { text: modelData.total; font.bold: true; font.pixelSize: 16; font.strikeout: modelData.anulada }
                    }
                    Label { text: modelData.resumen; Layout.fillWidth: true; elide: Text.ElideRight; font.strikeout: modelData.anulada }
                    Label { text: modelData.datos; visible: text !== ""; color: ventana.tintaSecundaria; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: modelData.estado; visible: text !== ""; color: "#e34948"; font.pixelSize: 12 }
                    // El detalle, al tocarla
                    Repeater {
                        model: pagina.abierta === modelData.id ? modelData.items : []
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.leftMargin: 12
                            Label { text: modelData.texto; Layout.fillWidth: true; wrapMode: Text.WordWrap; font.pixelSize: 13 }
                            Label { text: modelData.subtotal; font.pixelSize: 13 }
                        }
                    }
                    Button {
                        visible: pagina.abierta === modelData.id && !modelData.anulada
                        text: modelData.venta ? "Anular la venta (devolución)" : "Anular la compra"
                        flat: true
                        Material.foreground: "#e34948"
                        onClicked: { pagina.aAnular = modelData; confirmar.open() }
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: listaHistorial.count === 0
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: ventana.tintaApagada
                text: "No hay ventas ni compras en estas fechas."
            }
        }
    }

    SelectorFecha { id: selectorDesde; onElegida: (iso) => pagina.desdeIso = iso }
    SelectorFecha { id: selectorHasta; onElegida: (iso) => pagina.hastaIso = iso }

    Dialog {
        id: confirmar
        title: pagina.aAnular.venta ? "¿Anular la venta?" : "¿Anular la compra?"
        modal: true
        anchors.centerIn: parent
        width: Math.min(ventana.width - 32, 380)
        standardButtons: Dialog.Yes | Dialog.No
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: (pagina.aAnular.venta ? "Lo vendido vuelve al stock" : "Lo comprado se descuenta del stock")
                  + " y los reportes ya no la cuentan. Queda en el historial como anulada. No se puede deshacer."
        }
        onAccepted: {
            const e = App.anularOperacion(pagina.aAnular.id)
            if (e === "") { ventana.avisar(pagina.aAnular.venta ? "Venta anulada: el stock ya volvió" : "Compra anulada"); pagina.recargar() }
            else ventana.avisar(e)
        }
    }
}
