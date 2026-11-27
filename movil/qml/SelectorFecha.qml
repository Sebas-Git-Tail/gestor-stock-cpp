// Calendario para elegir una fecha (meses en castellano; la semana empieza el domingo, como en la PC).
// Igual que el de ingresos-gastos, con los colores del gestor.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import GestorStock

Dialog {
    id: selector
    signal elegida(string iso)
    property int anio: 2026
    property int mes: 0                 // 0 = enero (como en JavaScript)
    property string elegidaIso: ""
    readonly property var nombresMeses: ["Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto",
                                         "Septiembre", "Octubre", "Noviembre", "Diciembre"]
    readonly property var nombresDias: ["Do", "Lu", "Ma", "Mi", "Ju", "Vi", "Sá"]   // Qt.Sunday = 0
    readonly property color acento: "#2e7d32"

    function aIso(d) {
        const m = d.getMonth() + 1, dia = d.getDate()
        return d.getFullYear() + "-" + (m < 10 ? "0" : "") + m + "-" + (dia < 10 ? "0" : "") + dia
    }
    function abrir(iso) {
        const p = (iso || App.hoyIso()).split("-")
        anio = parseInt(p[0]); mes = parseInt(p[1]) - 1
        elegidaIso = iso || ""
        open()
    }
    function mover(pasos) {
        let m = mes + pasos, a = anio
        while (m < 0) { m += 12; a -= 1 }
        while (m > 11) { m -= 12; a += 1 }
        mes = m; anio = a
    }

    modal: true
    anchors.centerIn: parent
    width: Math.min(ventana.width - 24, 380)
    padding: 12
    standardButtons: Dialog.Cancel

    ColumnLayout {
        anchors.fill: parent
        RowLayout {
            Layout.fillWidth: true
            ToolButton { text: "‹"; font.pixelSize: 26; onClicked: selector.mover(-1); Accessible.name: "Mes anterior" }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: selector.nombresMeses[selector.mes] + " " + selector.anio
                font.pixelSize: 17
                font.bold: true
            }
            ToolButton { text: "›"; font.pixelSize: 26; onClicked: selector.mover(1); Accessible.name: "Mes siguiente" }
        }
        // Con el locale de EE.UU. la semana arranca el domingo; los nombres van en castellano a mano
        DayOfWeekRow {
            Layout.fillWidth: true
            locale: Qt.locale("en_US")
            delegate: Label {
                required property int day
                text: selector.nombresDias[day]
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
                color: ventana.tintaApagada
            }
        }
        MonthGrid {
            id: grilla
            Layout.fillWidth: true
            month: selector.mes
            year: selector.anio
            locale: Qt.locale("en_US")
            delegate: Label {
                required property var model
                readonly property string iso: selector.aIso(model.date)
                text: model.day
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                opacity: model.month === grilla.month ? 1 : 0.35
                font.bold: model.today
                padding: 8
                color: iso === selector.elegidaIso ? "white" : (ventana.oscuro ? "#f0efec" : "#0b0b0b")
                background: Rectangle {
                    radius: height / 2
                    color: iso === selector.elegidaIso ? selector.acento : "transparent"
                    border.color: model.today ? selector.acento : "transparent"
                }
            }
            onClicked: (fecha) => { selector.elegida(selector.aIso(fecha)); selector.close() }
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            text: "Hoy"
            flat: true
            onClicked: { selector.elegida(App.hoyIso()); selector.close() }
        }
    }
}
