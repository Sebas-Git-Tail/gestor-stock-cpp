// Diálogo de la app: centrado, del ancho del celular (con margen) y con el velo oscuro detrás
// (el de Material salía gris claro en el tema oscuro)
import QtQuick
import QtQuick.Controls.Material

Dialog {
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 48 : 360, 440)
    modal: true
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, Tema.oscuro ? 0.6 : 0.4) }
}
