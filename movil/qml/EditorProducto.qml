// Alta y edición de un producto: un campo por cada columna que esté puesta (más los precios).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GestorStock

Page {
    id: editor
    property int idProducto: 0
    property var datos: App.producto(idProducto)
    property var precios: datos.precios ? datos.precios.slice() : []
    property var propios: ({})                       // lo escrito en las columnas propias: uuid -> texto
    property var simples: ({})                       // lo escrito en los campos del programa: clave -> texto
    readonly property bool nuevo: idProducto === 0
    title: nuevo ? "Producto nuevo" : "Editar producto"

    function valorInicial(c) {
        switch (c.clave) {
        case "codigo": return datos.codigo
        case "nombre": return datos.nombre
        case "categoria": return datos.categoria
        case "proveedor": return datos.proveedor
        case "cantidad": return datos.cantidad
        case "medida_bolsa": return datos.medidaBolsa
        case "vencimiento": return datos.vencimiento
        case "stock_minimo": return datos.stockMinimo
        case "notas": return datos.notas
        }
        return datos.extra[c.uuid] || ""
    }
    function guardar() {
        const d = { id: idProducto, codigo: simples.codigo || (datos.codigo || ""), nombre: simples.nombre !== undefined ? simples.nombre : datos.nombre,
                    categoria: datos.categoria, proveedor: datos.proveedor, cantidad: datos.cantidad, stockMinimo: datos.stockMinimo,
                    vencimiento: datos.vencimiento, medidaBolsa: datos.medidaBolsa, notas: datos.notas, precios: precios, extra: {} }
        const claves = { codigo: "codigo", nombre: "nombre", categoria: "categoria", proveedor: "proveedor", cantidad: "cantidad",
                         medida_bolsa: "medidaBolsa", vencimiento: "vencimiento", stock_minimo: "stockMinimo", notas: "notas" }
        for (const k in simples) d[claves[k]] = simples[k]
        d.extra = Object.assign({}, datos.extra, propios)
        const e = App.guardarProducto(d)
        if (e === "") { ventana.avisar("Producto guardado"); pila.pop() }
        else error.text = e
    }

    header: ToolBar {
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "←"; font.pixelSize: 22; onClicked: pila.pop(); Accessible.name: "Volver" }
            Label { text: editor.title; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
            ToolButton { text: "Eliminar"; visible: !editor.nuevo; onClicked: confirmar.open() }
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: formulario.implicitHeight + 32
        clip: true
        ColumnLayout {
            id: formulario
            width: parent.width - 32
            x: 16
            y: 16
            spacing: 10

            Repeater {
                model: App.columnas
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 2
                    visible: modelData.clave !== "precios"
                    Label { text: modelData.nombre + (modelData.tipo === 1 && modelData.propia ? " (número)" : modelData.tipo === 2 && modelData.propia ? " (fecha)" : modelData.clave === "vencimiento" ? " (dd/mm/aaaa)" : ""); color: ventana.tintaSecundaria }
                    TextField {
                        Layout.fillWidth: true
                        text: editor.valorInicial(modelData)
                        inputMethodHints: (modelData.tipo === 1 || modelData.clave === "cantidad" || modelData.clave === "stock_minimo") ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone
                        onTextEdited: {
                            if (modelData.propia) { const p = editor.propios; p[modelData.uuid] = text; editor.propios = p }
                            else { const s = editor.simples; s[modelData.clave] = text; editor.simples = s }
                        }
                    }
                }
            }

            // Precios: una fila por presentación
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Label { text: "Precios"; color: ventana.tintaSecundaria }
                Repeater {
                    model: editor.precios
                    RowLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "Presentación (500 g)"
                            text: modelData.presentacion
                            onTextEdited: { const p = editor.precios; p[index].presentacion = text; editor.precios = p }
                        }
                        TextField {
                            implicitWidth: 110
                            placeholderText: "Precio"
                            text: modelData.precio
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onTextEdited: { const p = editor.precios; p[index].precio = text; editor.precios = p }
                        }
                        Button {
                            text: "X"
                            flat: true
                            onClicked: { const p = editor.precios; p.splice(index, 1); editor.precios = p }
                            Accessible.name: "Quitar precio"
                        }
                    }
                }
                Button { text: "+ Agregar precio"; flat: true; onClicked: editor.precios = editor.precios.concat([{ presentacion: "", precio: "" }]) }
            }

            Label { id: error; Layout.fillWidth: true; color: "#e34948"; wrapMode: Text.WordWrap; visible: text !== "" }
            Button { Layout.fillWidth: true; text: "Guardar"; highlighted: true; onClicked: editor.guardar() }
        }
    }

    Dialog {
        id: confirmar
        title: "¿Eliminar este producto?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: {
            const e = App.eliminarProducto(editor.idProducto)
            if (e === "") { ventana.avisar("Producto eliminado"); pila.pop() }
            else error.text = e
        }
    }
}
