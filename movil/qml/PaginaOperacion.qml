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
    property string codigoPendiente: ""              // código escaneado sin producto: se está creando
    // Solo en la venta, opcionales: descuento (se resta) y recargo (se suma), en pesos o en % del subtotal
    property string descuento: ""
    property bool descuentoPorcentaje: false
    property string recargo: ""
    property bool recargoPorcentaje: false

    function numero(t) {                             // «1.234,5» o «1234.5» -> número
        const s = String(t).replace(/\s|\$|%/g, "")
        return parseFloat(s.indexOf(",") >= 0 ? s.replace(/\./g, "").replace(",", ".") : (/^\d{1,3}(\.\d{3})+$/.test(s) ? s.replace(/\./g, "") : s)) || 0
    }
    function enPesos(t, porcentaje) {                // como montoEnPesos del núcleo: el % va sobre el subtotal
        return Math.round((porcentaje ? subtotal * numero(t) / 100 : numero(t)) * 100) / 100
    }
    readonly property real subtotal: { let t = 0; for (const l of lineas) t += l.cantidad * numero(l.precio); return Math.round(t * 100) / 100 }
    readonly property real montoDescuento: venta ? enPesos(descuento, descuentoPorcentaje) : 0
    readonly property real montoRecargo: venta ? enPesos(recargo, recargoPorcentaje) : 0
    readonly property real total: Math.round((subtotal - montoDescuento + montoRecargo) * 100) / 100
    // Lo que el núcleo rechazaría: se avisa antes y no se deja confirmar
    readonly property string problemaAjustes: !venta ? ""
        : descuentoPorcentaje && numero(descuento) > 100 ? "El descuento no puede ser de más del 100 %"
        : total < 0 ? "El descuento es mayor que el total" : ""

    function agregar(p) {
        const ls = lineas.slice()
        for (const l of ls) if (l.id === p.id) { l.cantidad += 1; lineas = ls; return }
        const pre = p.precios.length > 0 ? p.precios[0] : { presentacion: "", texto: "" , precio: 0 }
        // Compra: el precio es el costo de cada uno (opcional), arranca vacío
        ls.push({ id: p.id, nombre: p.nombre, stock: p.cantidad, cantidad: 1, presentacion: pre.presentacion,
                  precio: venta && pre.precio ? String(pre.precio).replace(".", ",") : "", presets: p.precios })
        lineas = ls
        campoBuscar.text = ""
        resultados = []
    }
    function confirmar() {
        const r = App.registrarOperacion(venta, lineas.map(l => ({ id: l.id, cantidad: l.cantidad, presentacion: l.presentacion, precio: l.precio })),
                                         { medioPago: campoMedio.editText, cliente: campoCliente.text,
                                           descuento: pagina.descuento, descuentoPorcentaje: pagina.descuentoPorcentaje,
                                           recargo: pagina.recargo, recargoPorcentaje: pagina.recargoPorcentaje })
        if (r.ok) { ventana.avisar(venta ? "Venta registrada: " + r.total : "Compra registrada: el stock ya está actualizado"); pila.pop() }
        else error.text = r.error
    }
    // Al volver de crear el producto del código escaneado, entra directo en la operación. Va en la página:
    // StackView.onActivated solo llega al elemento que se apiló, no a los de adentro
    StackView.onActivated: {
        if (pagina.codigoPendiente === "") return
        const r = App.buscarProductos(pagina.codigoPendiente)
        pagina.codigoPendiente = ""
        if (r.length === 1) pagina.agregar(r[0])
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
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: campoBuscar
                Layout.fillWidth: true
                placeholderText: "Buscar un producto para agregar…"
                inputMethodHints: Qt.ImhNoPredictiveText
                onDisplayTextChanged: pagina.resultados = displayText.trim() === "" ? [] : App.buscarProductos(displayText)
            }
            Button { text: "Escanear"; visible: App.puedeEscanear; onClicked: App.escanearCodigo() }
        }
        // Código de barras leído: si es de un solo producto se agrega directo; si no, queda buscado
        Connections {
            target: App
            function onCodigoEscaneado(codigo) {
                if (codigo === "") return
                const r = App.buscarProductos(codigo)
                if (r.length === 1) pagina.agregar(r[0])
                else if (r.length === 0) { crearConCodigo.codigo = codigo; crearConCodigo.open() }
                else campoBuscar.text = codigo
            }
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
                            implicitWidth: 120
                            placeholderText: pagina.venta ? "Precio" : "Costo c/u"
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
        // Venta: medio de pago, cliente, descuento y recargo (todo opcional)
        GridLayout {
            visible: pagina.venta
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            ComboBox {
                id: campoMedio
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                editable: true
                model: [""].concat(App.mediosDePago)
                Accessible.name: "Medio de pago"
                // Editable, el combo muestra su campo de texto y no displayText: la guía va encima, como un placeholder
                Label {
                    x: campoMedio.contentItem.x + campoMedio.contentItem.leftPadding
                    width: campoMedio.availableWidth - campoMedio.contentItem.leftPadding
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Medio de pago"
                    font: campoMedio.contentItem.font
                    color: ventana.tintaApagada
                    elide: Text.ElideRight
                    visible: campoMedio.editText === "" && !campoMedio.contentItem.activeFocus
                }
            }
            TextField {
                id: campoCliente
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                placeholderText: "Cliente (opcional)"
            }
            // Descuento y recargo: el número y si va en pesos o en %
            Repeater {
                model: [{ nombre: "Descuento", clave: "descuento" }, { nombre: "Recargo", clave: "recargo" }]
                RowLayout {
                    id: ajuste
                    required property var modelData
                    readonly property bool porcentaje: pagina[modelData.clave + "Porcentaje"]
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    spacing: 0
                    TextField {
                        Layout.fillWidth: true
                        placeholderText: ajuste.modelData.nombre      // si va en $ o en % lo dicen los botones de al lado
                        text: pagina[ajuste.modelData.clave]
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        onTextEdited: {
                            pagina[ajuste.modelData.clave] = text
                            if (text.indexOf("%") >= 0) pagina[ajuste.modelData.clave + "Porcentaje"] = true   // «10%» es un 10 %
                        }
                        Accessible.name: ajuste.modelData.nombre
                    }
                    Repeater {
                        model: ["$", "%"]
                        ToolButton {
                            required property string modelData
                            required property int index
                            // Sin checkable: tocar el ya elegido lo destildaría y rompería el binding
                            readonly property bool elegido: ajuste.porcentaje === (index === 1)
                            implicitWidth: 32
                            text: modelData
                            font.bold: elegido
                            onClicked: pagina[ajuste.modelData.clave + "Porcentaje"] = index === 1
                            Material.foreground: elegido ? Material.accent : ventana.tintaApagada
                            Accessible.name: ajuste.modelData.nombre + (index === 1 ? " en porcentaje" : " en pesos")
                        }
                    }
                }
            }
        }
        Label { id: error; Layout.fillWidth: true; color: "#e34948"; wrapMode: Text.WordWrap; visible: text !== "" }
        // La cuenta (si hay descuento o recargo) en chico, y el total grande en un solo renglón
        Label {
            visible: pagina.venta && (pagina.montoDescuento > 0 || pagina.montoRecargo > 0)
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: ventana.tintaApagada
            text: "Subtotal " + App.precioTexto(pagina.subtotal)
                  + (pagina.montoDescuento > 0 ? " − descuento " + App.precioTexto(pagina.montoDescuento) : "")
                  + (pagina.montoRecargo > 0 ? " + recargo " + App.precioTexto(pagina.montoRecargo) : "")
        }
        Label {
            visible: pagina.venta || pagina.total > 0
            text: !pagina.venta ? "Total de la compra: " + App.precioTexto(pagina.total)
                  : pagina.problemaAjustes !== "" ? pagina.problemaAjustes
                  : "Total: " + App.precioTexto(pagina.total)
            color: pagina.problemaAjustes !== "" ? "#e34948" : Material.foreground
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 22; font.bold: true
        }
        Label {
            visible: !pagina.venta
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: ventana.tintaApagada
            font.pixelSize: 12
            text: "El costo es opcional: con él, Reportes calcula la ganancia."
        }
        Button {
            Layout.fillWidth: true
            text: pagina.venta ? "Confirmar venta" : "Confirmar compra"
            highlighted: true
            enabled: pagina.lineas.length > 0 && pagina.problemaAjustes === ""
            onClicked: pagina.confirmar()
        }
    }

    Dialog {
        id: crearConCodigo
        property string codigo: ""
        title: "Código nuevo"
        modal: true
        anchors.centerIn: parent
        width: Math.min(ventana.width - 32, 380)
        standardButtons: Dialog.Cancel
        ColumnLayout {
            width: parent.width
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Ningún producto tiene el código " + crearConCodigo.codigo + "." }
            Button {
                Layout.fillWidth: true
                text: "Crear producto con este código"
                highlighted: true
                onClicked: {
                    crearConCodigo.close()
                    pagina.codigoPendiente = crearConCodigo.codigo
                    pila.push(editor, { idProducto: 0, codigoInicial: crearConCodigo.codigo })
                }
            }
        }
    }
}
