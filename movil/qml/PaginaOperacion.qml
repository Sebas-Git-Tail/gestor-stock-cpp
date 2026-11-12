// Venta (baja el stock y suma el importe) o compra (suma stock) de varios productos.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GestorStock

Page {
    id: pagina
    property bool venta: true
    property var lineas: []                          // [{id, nombre, stock, cantidad, presentacion, precio, presets}]
    property var resultados: []

    function numero(t) {                             // «1.234,5» o «1234.5» -> número
        const s = String(t).replace(/\s|\$/g, "")
        return parseFloat(s.indexOf(",") >= 0 ? s.replace(/\./g, "").replace(",", ".") : (/^\d{1,3}(\.\d{3})+$/.test(s) ? s.replace(/\./g, "") : s)) || 0
    }
    readonly property real total: { let t = 0; for (const l of lineas) t += l.cantidad * numero(l.precio); return t }

    function agregar(p) {
        const ls = lineas.slice()
        for (const l of ls) if (l.id === p.id) { l.cantidad += 1; lineas = ls; return }
        const pre = p.precios.length > 0 ? p.precios[0] : { presentacion: "", texto: "" , precio: 0 }
        ls.push({ id: p.id, nombre: p.nombre, stock: p.cantidad, cantidad: 1, presentacion: pre.presentacion,
                  precio: pre.precio ? String(pre.precio).replace(".", ",") : "", presets: p.precios })
        lineas = ls
        campoBuscar.text = ""
        resultados = []
    }
    function confirmar() {
        const r = App.registrarOperacion(venta, lineas.map(l => ({ id: l.id, cantidad: l.cantidad, presentacion: l.presentacion, precio: l.precio })))
        if (r.ok) { ventana.avisar(venta ? "Venta registrada: " + r.total : "Compra registrada: el stock ya está actualizado"); pila.pop() }
        else error.text = r.error
    }

    header: ToolBar {
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "←"; font.pixelSize: 22; onClicked: pila.pop(); Accessible.name: "Volver" }
            Label { text: pagina.venta ? "Venta" : "Compra"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        TextField {
            id: campoBuscar
            Layout.fillWidth: true
            placeholderText: "Buscar un producto para agregar…"
            inputMethodHints: Qt.ImhNoPredictiveText
            onDisplayTextChanged: pagina.resultados = displayText.trim() === "" ? [] : App.buscarProductos(displayText)
        }
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 180)
            visible: pagina.resultados.length > 0
            clip: true
            model: pagina.resultados
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: modelData.nombre
                      + (modelData.precios.length > 0
                         ? " — " + modelData.precios.map(p => (p.presentacion ? p.presentacion + ": " : "") + p.texto).join(" · ")
                         : "")
                      + "   (hay " + modelData.cantidad + ")"
                onClicked: pagina.agregar(modelData)
            }
        }
        ListView {
            id: listaLineas
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: pagina.lineas
            delegate: Pane {
                required property var modelData
                required property int index
                width: listaLineas.width
                background: Rectangle { radius: 8; color: ventana.superficie; border.color: ventana.borde }
                ColumnLayout {
                    anchors.fill: parent
                    RowLayout {
                        Label { text: modelData.nombre; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { text: "hay " + modelData.stock; color: ventana.tintaApagada; font.pixelSize: 12 }
                        Button { text: "X"; flat: true; onClicked: { const l = pagina.lineas.slice(); l.splice(index, 1); pagina.lineas = l } }
                    }
                    RowLayout {
                        Button { text: "-"; enabled: modelData.cantidad > 1; onClicked: { const l = pagina.lineas.slice(); l[index].cantidad -= 1; pagina.lineas = l } }
                        Label { text: modelData.cantidad; font.pixelSize: 20; font.bold: true; horizontalAlignment: Text.AlignHCenter; Layout.minimumWidth: 40 }
                        Button { text: "+"; onClicked: { const l = pagina.lineas.slice(); l[index].cantidad += 1; pagina.lineas = l } }
                        Item { Layout.fillWidth: true }
                        TextField {
                            visible: pagina.venta
                            implicitWidth: 120
                            placeholderText: "Precio"
                            text: modelData.precio
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onTextEdited: { const l = pagina.lineas.slice(); l[index].precio = text; pagina.lineas = l }
                        }
                    }
                    Flow {
                        visible: pagina.venta && modelData.presets.length > 1
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: modelData.presets
                            Button {
                                required property var modelData
                                text: modelData.presentacion + " " + modelData.texto
                                flat: true
                                onClicked: { const l = pagina.lineas.slice(); l[index].presentacion = modelData.presentacion; l[index].precio = String(modelData.precio).replace(".", ","); pagina.lineas = l }
                            }
                        }
                    }
                }
            }
            Label { anchors.centerIn: parent; visible: listaLineas.count === 0; color: ventana.tintaApagada; text: "Buscá los productos arriba para agregarlos." }
        }
        Label { id: error; Layout.fillWidth: true; color: "#e34948"; wrapMode: Text.WordWrap; visible: text !== "" }
        Label { visible: pagina.venta; text: "Total: " + App.precioTexto(pagina.total); font.pixelSize: 22; font.bold: true }
        Button {
            Layout.fillWidth: true
            text: pagina.venta ? "Confirmar venta" : "Confirmar compra"
            highlighted: true
            enabled: pagina.lineas.length > 0
            onClicked: pagina.confirmar()
        }
    }
}
