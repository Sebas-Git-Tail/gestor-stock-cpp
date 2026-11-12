// Emparejar el celular con la PC (una vez) y sincronizar.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GestorStock

Page {
    id: pagina
    property var encontradas: []
    property bool buscando: false
    property bool emparejandoOtra: false

    header: ToolBar {
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "←"; font.pixelSize: 22; onClicked: pila.pop(); Accessible.name: "Volver" }
            Label { text: "Sincronizar con la PC"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
        }
    }

    Connections {
        target: App
        function onPcEncontrada(nombre, host, puerto) {
            pagina.encontradas = pagina.encontradas.concat([{ nombre: nombre, host: host, puerto: puerto }])
            if (campoHost.text === "") { campoHost.text = host; campoPuerto.text = puerto }
        }
        function onBusquedaTerminada() { pagina.buscando = false }
        function onSincTerminada(ok, mensaje) { if (ok) pagina.emparejandoOtra = false }
    }
    function buscar() { encontradas = []; buscando = true; App.buscarPcs() }
    Component.onCompleted: if (!App.pcEmparejada) buscar()

    Flickable {
        anchors.fill: parent
        contentHeight: columna.implicitHeight + 32
        clip: true

        ColumnLayout {
            id: columna
            width: parent.width - 32
            x: 16
            y: 16
            spacing: 12

            // ---- Ya emparejado
            Pane {
                Layout.fillWidth: true
                visible: App.pcEmparejada && !pagina.emparejandoOtra
                background: Rectangle { radius: 8; color: ventana.superficie; border.color: ventana.borde }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6
                    Label { text: App.nombrePc; font.pixelSize: 18; font.bold: true }
                    Label { text: App.direccionPc; color: ventana.tintaSecundaria }
                    Label { text: App.ultimaSincTexto; color: ventana.tintaSecundaria }
                    Button {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        text: App.sincronizando ? "Sincronizando…" : "Sincronizar ahora"
                        enabled: !App.sincronizando
                        highlighted: true
                        onClicked: App.sincronizar()
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Se sincroniza solo al abrir la app y un rato después de cada cambio, si la PC está a mano."
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: ventana.tintaApagada
                    }
                    RowLayout {
                        Button { text: "Emparejar otra vez"; flat: true; onClicked: { pagina.emparejandoOtra = true; pagina.buscar() } }
                        Button { text: "Olvidar esta PC"; flat: true; onClicked: confirmarOlvidar.open() }
                    }
                }
            }

            // ---- Emparejar
            ColumnLayout {
                Layout.fillWidth: true
                visible: !App.pcEmparejada || pagina.emparejandoOtra
                spacing: 10
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "1. En la PC, abrí Gestor de Stock → menú Opciones → «Sincronizar con el celular…» → «Mostrar código».\n"
                          + "2. Elegí la PC acá abajo (o escribí su dirección).\n3. Escribí el código de 6 números."
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "PCs en este Wi-Fi"; font.bold: true; Layout.fillWidth: true }
                    BusyIndicator { running: pagina.buscando; visible: running; implicitWidth: 32; implicitHeight: 32 }
                    Button { text: "Buscar"; flat: true; enabled: !pagina.buscando; onClicked: pagina.buscar() }
                }
                Repeater {
                    model: pagina.encontradas
                    ItemDelegate {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.nombre + "  (" + modelData.host + ")"
                        highlighted: campoHost.text === modelData.host
                        onClicked: { campoHost.text = modelData.host; campoPuerto.text = modelData.puerto }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    visible: !pagina.buscando && pagina.encontradas.length === 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: ventana.tintaApagada
                    text: "No apareció ninguna. Revisá que en la PC esté activada la sincronización, o escribí la dirección "
                          + "que muestra la PC (con Tailscale, la que empieza con 100.)."
                }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: campoHost; Layout.fillWidth: true; placeholderText: "Dirección de la PC (ej. 192.168.0.10)"; inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly }
                    TextField { id: campoPuerto; implicitWidth: 80; text: App.puertoPorDefecto; inputMethodHints: Qt.ImhDigitsOnly; horizontalAlignment: TextInput.AlignHCenter }
                }
                TextField {
                    id: campoCodigo
                    Layout.fillWidth: true
                    placeholderText: "Código de 6 números"
                    inputMethodHints: Qt.ImhDigitsOnly
                    font.pixelSize: 22
                    horizontalAlignment: TextInput.AlignHCenter
                    maximumLength: 7
                }
                Button {
                    Layout.fillWidth: true
                    text: App.sincronizando ? "Emparejando…" : "Emparejar"
                    enabled: !App.sincronizando && campoHost.text !== "" && campoCodigo.text.replace(/\D/g, "").length === 6
                    highlighted: true
                    onClicked: App.emparejar(campoHost.text, parseInt(campoPuerto.text), campoCodigo.text)
                }
            }

            Label {
                Layout.fillWidth: true
                text: App.estadoSinc
                visible: text !== ""
                wrapMode: Text.WordWrap
            }

            Label { text: "Nombre de este celular"; color: ventana.tintaSecundaria; Layout.topMargin: 12 }
            TextField {
                Layout.fillWidth: true
                text: App.nombreDispositivo
                onEditingFinished: App.nombreDispositivo = text
            }
        }
    }

    Dialog {
        id: confirmarOlvidar
        title: "¿Olvidar «" + App.nombrePc + "»?"
        modal: true
        anchors.centerIn: parent
        width: Math.min(ventana.width - 32, 380)
        standardButtons: Dialog.Yes | Dialog.No
        Label { width: parent.width; wrapMode: Text.WordWrap; text: "Los datos del celular no se borran. Para volver a sincronizar hay que emparejar de nuevo." }
        onAccepted: App.olvidarPc()
    }
}
