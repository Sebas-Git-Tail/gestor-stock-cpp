// Diálogo de la app: centrado, del ancho del celular (con margen) y con el velo oscuro detrás
// (el de Material salía gris claro en el tema oscuro)
//
// El ancho sale de la ventana (ApplicationWindow.window) y no de «parent» (Overlay.overlay): al crearse,
// el diálogo todavía no tiene padre y el ancho cambiaba de 360 al de la ventana. Con un Label que corta
// renglones adentro, ese cambio rehacía su alto en medio del cálculo del alto del diálogo y Qt avisaba
// «Binding loop detected for property "implicitHeight"» (Material/Dialog.qml) por cada diálogo.
import QtQuick
import QtQuick.Controls.Material

Dialog {
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min((ApplicationWindow.window ? ApplicationWindow.window.width
                                              : parent ? parent.width : 408) - 48, 440)
    modal: true
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, Tema.oscuro ? 0.6 : 0.4) }
}
