// Cómo va una tarea: el estado (con ícono si terminó bien o mal), la barra del archivo actual y,
// si son varios, cuántos van
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Seccion {
    id: tarjeta
    property string texto
    property int tipo: 0               // 0 info, 1 éxito, 2 error
    property bool trabajando: false
    property int progreso: -1          // 0..100; -1 sin barra (o indeterminada si está trabajando)
    property string detalle: ""        // debajo de la barra (p. ej. el archivo que se copia)
    property int hechas: 0
    property int total: 0              // más de 1: se muestra «2 de 5»

    visible: texto !== "" || trabajando

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Icono {
            visible: !tarjeta.trabajando && tarjeta.tipo !== 0
            Layout.alignment: Qt.AlignTop
            nombre: tarjeta.tipo === 1 ? "listo" : "error"
            lado: 22
            color: Tema.colorEstado(tarjeta.tipo)
        }
        Label {
            Layout.fillWidth: true
            text: tarjeta.texto
            wrapMode: Text.Wrap
            color: tarjeta.tipo === 0 ? Tema.tinta : Tema.colorEstado(tarjeta.tipo)
            font.pixelSize: 15
        }
    }
    ProgressBar {
        visible: tarjeta.trabajando || tarjeta.progreso >= 0
        Layout.fillWidth: true
        from: 0; to: 100
        value: Math.max(0, tarjeta.progreso)
        indeterminate: tarjeta.trabajando && tarjeta.progreso < 0
    }
    Label {
        visible: tarjeta.detalle !== ""
        Layout.fillWidth: true
        text: tarjeta.detalle
        elide: Text.ElideMiddle
        font.pixelSize: 12
        color: Tema.tintaSecundaria
    }
    RowLayout {
        visible: tarjeta.trabajando && tarjeta.total > 1
        Layout.fillWidth: true
        spacing: 10
        ProgressBar {
            Layout.fillWidth: true
            from: 0; to: Math.max(1, tarjeta.total)
            value: tarjeta.hechas
            Material.accent: Tema.verde
        }
        Label {
            text: tarjeta.hechas + " de " + tarjeta.total
            font.pixelSize: 12
            color: Tema.tintaSecundaria
        }
    }
}
