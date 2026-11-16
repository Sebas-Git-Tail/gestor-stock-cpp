// «Apps y actualizaciones»: la versión nueva de esta app y las otras apps de Sebastián para instalar.
// Usa «Apps» (apps::ModeloApps de comun, puesto en main.cpp). Carina y Bodrio nunca aparecen en la lista.
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Page {
    id: pagina

    header: ToolBar {
        Material.foreground: "white"
        RowLayout {
            anchors.fill: parent
            ToolButton { text: "←"; font.pixelSize: 22; onClicked: pila.pop(); Accessible.name: "Volver" }
            Label { text: "Apps y actualizaciones"; font.pixelSize: 18; font.bold: true; Layout.fillWidth: true }
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
        spacing: 8
        topMargin: 12
        bottomMargin: 12

        header: ColumnLayout {
            width: lista.width
            spacing: 6
            Pane {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Material.elevation: 1
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.bold: true
                        text: Qt.application.displayName + " " + Apps.versionPropia
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: Apps.actualizando ? (Apps.progresoPropio >= 0 ? "Descargando… " + Math.round(Apps.progresoPropio * 100) + " %" : "Descargando…")
                            : Apps.versionNueva !== "" ? "Hay una versión nueva: " + Apps.versionNueva
                            : "Estás en la última versión."
                    }
                    Button {
                        visible: Apps.versionNueva !== "" && !Apps.actualizando
                        text: "Actualizar a la " + Apps.versionNueva
                        highlighted: true
                        onClicked: Apps.actualizarEstaApp()
                    }
                }
            }
            Label {
                text: "Otras apps"
                font.bold: true
                font.pixelSize: 16
                Layout.leftMargin: 16
                Layout.topMargin: 8
                visible: lista.count > 0
            }
        }

        delegate: Pane {
            width: lista.width - 24
            x: 12
            Material.elevation: 1
            RowLayout {
                anchors.fill: parent
                spacing: 8
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label { text: model.nombre; font.bold: true; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Label { text: model.descripcion; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Label {
                        text: model.estado + (model.ocupado && model.progreso >= 0 ? " " + Math.round(model.progreso * 100) + " %" : "")
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.7
                    }
                }
                Button {
                    visible: model.accion !== ""
                    text: model.accion
                    onClicked: Apps.instalar(index)
                }
                BusyIndicator { visible: model.ocupado; running: visible; implicitWidth: 32; implicitHeight: 32 }
            }
        }

        footer: Label {
            width: lista.width
            padding: 16
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: Apps.mensaje
        }
    }
}
