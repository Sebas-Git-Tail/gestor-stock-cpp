// Mensajito abajo de la pantalla que se va solo («Guardado», «Sincronizado»). Con un botón opcional
// («Deshacer»): se llama a alBoton si lo tocan, o a alVencer si se cierra sin tocarlo (por tiempo,
// tocando afuera o porque llegó otro aviso).
//   AvisoFlotante { id: aviso }
//   aviso.mostrar("Producto borrado", { textoBoton: "Deshacer", alBoton: deshacer, alVencer: confirmar })
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Popup {
    id: aviso
    property string texto
    property int tipo: 0               // 0 info, 1 éxito, 2 error
    property string textoBoton: ""
    property var alBoton: null
    property var alVencer: null
    property int margenAbajo: 48

    function mostrar(nuevoTexto, opciones) {
        if (opened)
            resolver(false)
        const o = opciones || {}
        texto = nuevoTexto
        tipo = o.tipo || 0
        textoBoton = o.textoBoton || ""
        alBoton = o.alBoton || null
        alVencer = o.alVencer || null
        reloj.interval = o.milisegundos || (textoBoton !== "" ? 6000 : 3000)
        open()
    }
    function resolver(porBoton) {
        const funcion = porBoton ? alBoton : alVencer
        alBoton = null
        alVencer = null
        reloj.stop()
        if (opened)
            close()
        if (funcion)
            funcion()
    }

    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? parent.height - height - margenAbajo - (parent.SafeArea ? parent.SafeArea.margins.bottom : 0) : 0
    width: parent ? Math.min(parent.width - 32, fila.implicitWidth + 32) : 300
    padding: 12
    leftPadding: 16
    modal: false
    closePolicy: Popup.CloseOnPressOutside
    onOpened: reloj.restart()
    onClosed: if (alBoton || alVencer) resolver(false)

    background: Rectangle { radius: 10; color: Tema.oscuro ? "#f0efec" : "#2c2c2a" }
    contentItem: RowLayout {
        id: fila
        spacing: 12
        Icono {
            visible: aviso.tipo !== 0
            nombre: aviso.tipo === 1 ? "listo" : "error"
            color: aviso.tipo === 1 ? (Tema.oscuro ? "#1f8a54" : "#4cc38a") : (Tema.oscuro ? "#d23c3b" : "#f07070")
        }
        Label {
            Layout.fillWidth: true
            text: aviso.texto
            wrapMode: Text.WordWrap
            color: Tema.oscuro ? "#0b0b0b" : "#f0efec"
        }
        Button {
            visible: aviso.textoBoton !== ""
            text: aviso.textoBoton
            flat: true
            Material.foreground: Tema.oscuro ? Qt.darker(Tema.acento, 1.2) : Qt.lighter(Tema.acento, 1.5)
            font.weight: Font.DemiBold
            onClicked: aviso.resolver(true)
        }
    }
    Timer { id: reloj; interval: 3000; onTriggered: aviso.resolver(false) }
}
