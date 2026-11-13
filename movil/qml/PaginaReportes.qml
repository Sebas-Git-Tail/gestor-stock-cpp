// Reportes y avisos: ventas, compras y ganancia del período elegido, lo más vendido, ventas por mes y
// los avisos de poco stock y vencimientos. Lo mismo que ☰ → Reportes y avisos en la PC.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GestorStock

Page {
    id: pagina
    property bool verAvisos: false
    property int periodo: 2                        // este mes
    property var datos: App.reporte(periodo)

    header: ToolBar {
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "←"; font.pixelSize: 22; onClicked: pila.pop(); Accessible.name: "Volver" }
            Label { text: "Reportes y avisos"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
        }
    }

    TabBar {
        id: pestanas
        width: parent.width
        currentIndex: pagina.verAvisos ? 1 : 0
        TabButton { text: "Ventas" }
        TabButton { text: App.alertas.length > 0 ? "Avisos (" + App.alertas.length + ")" : "Avisos" }
    }

    StackLayout {
        anchors.top: pestanas.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentIndex: pestanas.currentIndex

        Flickable {
            contentHeight: columna.implicitHeight + 24
            clip: true
            ColumnLayout {
                id: columna
                x: 12; y: 12
                width: parent.width - 24
                spacing: 10
                ComboBox {
                    Layout.fillWidth: true
                    model: ["Hoy", "Últimos 7 días", "Este mes", "Mes anterior", "Este año"]
                    currentIndex: pagina.periodo
                    onActivated: (i) => pagina.periodo = i
                }
                Label { text: pagina.datos.periodo; color: ventana.tintaApagada; font.pixelSize: 12 }
                GridLayout {
                    columns: 3
                    Layout.fillWidth: true
                    Label { text: "Ventas"; color: ventana.tintaSecundaria }
                    Label { text: "Compras"; color: ventana.tintaSecundaria }
                    Label { text: "Ganancia"; color: ventana.tintaSecundaria }
                    Label { text: pagina.datos.ventas; font.pixelSize: 20; font.bold: true; Layout.fillWidth: true }
                    Label { text: pagina.datos.compras; font.pixelSize: 20; font.bold: true; Layout.fillWidth: true }
                    Label { text: pagina.datos.ganancia; font.pixelSize: 20; font.bold: true; Layout.fillWidth: true }
                }
                Label { text: pagina.datos.detalleVentas; color: ventana.tintaApagada; font.pixelSize: 12 }
                Label { text: pagina.datos.nota; visible: text !== ""; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: ventana.tintaApagada; font.pixelSize: 12 }

                Label { text: "Más vendidos"; font.bold: true; font.pixelSize: 16; Layout.topMargin: 8 }
                Label { visible: pagina.datos.masVendidos.length === 0; text: "No hubo ventas en este período."; color: ventana.tintaApagada }
                Repeater {
                    model: pagina.datos.masVendidos
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Label { text: modelData.nombre; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { text: modelData.cantidad + " u."; color: ventana.tintaSecundaria }
                        Label { text: modelData.importe; font.bold: true; Layout.minimumWidth: 90; horizontalAlignment: Text.AlignRight }
                    }
                }

                Label { text: "Por mes"; font.bold: true; font.pixelSize: 16; Layout.topMargin: 8; visible: pagina.datos.meses.length > 1 }
                Repeater {
                    model: pagina.datos.meses.length > 1 ? pagina.datos.meses : []
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Label { text: modelData.mes; Layout.fillWidth: true }
                        Label { text: modelData.importe; font.bold: true }
                    }
                }
            }
        }

        ListView {
            clip: true
            model: App.alertas
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                onClicked: ventana.editar(modelData.id)
                contentItem: ColumnLayout {
                    spacing: 2
                    Label { text: modelData.nombre; font.bold: true; font.pixelSize: 16 }
                    Label { text: modelData.detalle; color: modelData.grave ? "#e34948" : "#b07800" }
                }
            }
            Label {
                anchors.centerIn: parent
                width: parent.width - 48
                visible: App.alertas.length === 0
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: ventana.tintaApagada
                text: "No hay avisos: ningún producto está por debajo de su stock mínimo ni vence en los próximos 30 días."
            }
        }
    }
}
