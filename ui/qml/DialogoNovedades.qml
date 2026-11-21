// «Novedades» en QML: lo nuevo de esta versión (se abre solo la primera vez que se abre una versión nueva,
// si hay algo para este equipo) o todas las versiones (desde el menú), con «Enviar informe de fallas».
// Usa «Novedades» (soporte::Novedades de comun, puesta en main.cpp como propiedad de contexto).
//   DialogoNovedades { id: novedades }
//   ItemMenu { text: "Novedades"; icono: "regalo"; onClicked: { menu.close(); novedades.abrirTodas() }
//              onPressAndHold: novedades.alternarDesarrollador() }     // escondido: modo Sebastián (Android)
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Dialogo {
    id: dialogo
    property bool abrirAlInicio: true
    property bool soloNuevas: false
    property string resultado: ""          // lo que pasó con el informe

    function abrirNuevas() { soloNuevas = true; resultado = ""; open() }
    function abrirTodas() { soloNuevas = false; resultado = ""; open() }
    function alternarDesarrollador() {
        Novedades.alternarDesarrollador()
        aviso.text = Novedades.desarrollador ? "Modo Sebastián: se ven también las novedades «solo vos»"
                                             : "Modo Sebastián apagado"
        aviso.open()
    }

    title: soloNuevas ? "Qué hay de nuevo" : "Novedades · versión " + Novedades.version
    height: Math.min(implicitHeight, (parent ? parent.height : 640) - 96)
    padding: 20
    topPadding: 8

    Component.onCompleted: if (abrirAlInicio && Novedades.hayNuevas) temporizador.start()
    Timer { id: temporizador; interval: 900; onTriggered: if (Novedades.hayNuevas) dialogo.abrirNuevas() }
    onClosed: if (soloNuevas) Novedades.marcarVistas()

    Connections {
        target: Novedades
        function onInformeEnviado(ok) {
            dialogo.resultado = ok ? "Informe enviado. ¡Gracias!"
                                   : "No se pudo enviar ahora: se manda solo la próxima vez que haya conexión."
        }
    }

    contentItem: ColumnLayout {
        spacing: 12
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: Math.min(texto.implicitHeight, 420)
            contentHeight: texto.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}
            Label {
                id: texto
                width: parent.width
                wrapMode: Text.Wrap
                textFormat: Text.MarkdownText
                color: Tema.tinta
                lineHeight: 1.15
                text: {
                    const t = dialogo.soloNuevas ? Novedades.textoNuevas : Novedades.textoTodas
                    return t !== "" ? t : "Todavía no hay novedades anotadas para esta versión."
                }
            }
        }
        Label {
            visible: Novedades.desarrollador
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: Tema.tintaApagada
            text: "Este equipo es tuyo: también ves lo marcado «solo vos»."
        }
        Label {
            visible: dialogo.resultado !== ""
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: dialogo.resultado.startsWith("Informe") ? Tema.verde : Tema.naranja
            text: dialogo.resultado
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                flat: true
                text: Novedades.enviandoInforme ? "Enviando…" : "Enviar informe de fallas"
                enabled: !Novedades.enviandoInforme
                icon.source: Tema.icono("falla")
                onClicked: comentario.open()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: dialogo.soloNuevas ? "Entendido" : "Cerrar"
                highlighted: true
                onClicked: dialogo.close()
            }
        }
    }

    Dialogo {
        id: comentario
        title: "Enviar informe de fallas"
        standardButtons: Dialog.Cancel | Dialog.Ok
        onOpened: campo.text = ""
        onAccepted: { dialogo.resultado = ""; Novedades.enviarInforme(campo.text) }
        contentItem: ColumnLayout {
            spacing: 10
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Tema.tintaSecundaria
                text: "Contá qué pasó (opcional). Se manda junto con el registro de la app, sin tus datos."
            }
            TextArea {
                id: campo
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                wrapMode: TextEdit.Wrap
                placeholderText: "Por ejemplo: se cerró al apretar Sincronizar"
            }
        }
    }

    ToolTip {
        id: aviso
        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: parent.height - height - 48
        timeout: 2500
    }
}
