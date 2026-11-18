// «Apps y actualizaciones»: la versión nueva de esta app y las otras apps de Sebastián para instalar.
// Usa «Apps» (apps::ModeloApps de comun, puesto en main.cpp). Carina y Bodrio nunca aparecen en la lista.
//   conBarra: true  → la página trae su propia barra con «Volver» (apps donde cada página tiene la suya)
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: pagina
    property string nombreApp: Qt.application.name
    property bool conBarra: false
    title: "Apps y actualizaciones"
    background: null

    header: conBarra ? barra : null
    ToolBar {
        id: barra
        visible: pagina.conBarra
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            ToolButton {
                icon.source: Tema.icono("volver")
                onClicked: pagina.StackView.view.pop()
                Accessible.name: "Volver"
            }
            Label { text: pagina.title; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
        }
    }

    Component.onCompleted: { Apps.refrescar(); Apps.buscarActualizacion(true) }
    // Al volver del instalador de Android, la lista se pone al día
    Connections {
        target: Qt.application
        function onStateChanged() { if (Qt.application.state === Qt.ApplicationActive) Apps.refrescar() }
    }

    ListView {
        id: lista
        anchors.fill: parent
        clip: true
        model: Apps
        spacing: 10
        topMargin: Tema.margen
        bottomMargin: Tema.margen + pagina.SafeArea.margins.bottom
        ScrollIndicator.vertical: ScrollIndicator {}

        header: ColumnLayout {
            width: lista.width
            spacing: 10
            Seccion {
                Layout.leftMargin: Tema.margen
                Layout.rightMargin: Tema.margen
                Layout.bottomMargin: 6
                titulo: "Esta app"
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                        color: Tema.tinta
                        text: pagina.nombreApp + " " + Apps.versionPropia
                    }
                    Label {
                        text: Apps.actualizando ? "Actualizando" : Apps.versionNueva !== "" ? "Versión nueva" : "Al día"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: Apps.versionNueva !== "" || Apps.actualizando ? Tema.naranja : Tema.verde
                        leftPadding: 10; rightPadding: 10; topPadding: 3; bottomPadding: 3
                        background: Rectangle {
                            radius: height / 2
                            color: Apps.versionNueva !== "" || Apps.actualizando ? Tema.naranjaSuave
                                 : Qt.rgba(Tema.verde.r, Tema.verde.g, Tema.verde.b, 0.15)
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Apps.versionNueva !== "" ? Tema.naranja : Tema.tintaSecundaria
                    text: Apps.actualizando ? (Apps.progresoPropio >= 0 ? "Descargando… " + Math.round(Apps.progresoPropio * 100) + " %" : "Descargando…")
                        : Apps.versionNueva !== "" ? "Hay una versión nueva: la " + Apps.versionNueva
                        : "Estás en la última versión."
                }
                ProgressBar {
                    visible: Apps.actualizando
                    Layout.fillWidth: true
                    from: 0; to: 1
                    value: Math.max(0, Apps.progresoPropio)
                    indeterminate: Apps.progresoPropio < 0
                }
                Button {
                    visible: Apps.versionNueva !== "" && !Apps.actualizando
                    text: "Actualizar a la " + Apps.versionNueva
                    highlighted: true
                    onClicked: Apps.actualizarEstaApp()
                }
            }
            Label {
                visible: lista.count > 0
                Layout.leftMargin: Tema.margen + 4
                text: "Otras apps"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.8
                color: Tema.tintaApagada
            }
        }

        delegate: Item {        // ListView ignora la x del delegado: la tarjeta va adentro, con márgenes
            id: fila
            required property int index
            required property string nombre
            required property string descripcion
            required property string estado
            required property string accion
            required property bool ocupado
            required property real progreso
            width: lista.width
            implicitHeight: tarjetaApp.implicitHeight

            Pane {
                id: tarjetaApp
                x: Tema.margen
                width: fila.width - 2 * Tema.margen
                padding: 16
                background: Rectangle { radius: Tema.radio; color: Tema.superficie; border.color: Tema.borde }
                contentItem: RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        Layout.alignment: Qt.AlignTop
                        radius: 12
                        color: Tema.acentoSuave
                        Icono { anchors.centerIn: parent; nombre: "paquete"; color: Tema.acentoTexto }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Label { text: fila.nombre; font.weight: Font.DemiBold; font.pixelSize: 16; color: Tema.tinta; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Label { text: fila.descripcion; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Tema.tintaSecundaria; font.pixelSize: 13 }
                        Label {
                            text: fila.estado
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 12
                            color: Tema.tintaApagada
                        }
                        ProgressBar {
                            visible: fila.ocupado
                            Layout.fillWidth: true
                            from: 0; to: 1
                            value: Math.max(0, fila.progreso)
                            indeterminate: fila.progreso < 0
                        }
                    }
                    Button {
                        visible: fila.accion !== "" && !fila.ocupado
                        text: fila.accion
                        highlighted: true
                        onClicked: Apps.instalar(fila.index)
                    }
                }
            }
        }

        footer: Label {
            width: lista.width
            leftPadding: Tema.margen + 4
            rightPadding: Tema.margen
            topPadding: 12
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Tema.tintaApagada
            text: Apps.mensaje
        }
    }
}
