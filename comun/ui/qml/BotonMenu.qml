// Botón de las 3 rayitas que abre el menú lateral. Con «punto» lleva un puntito naranja arriba a la
// derecha: hay algo nuevo adentro (una versión nueva en «Apps y actualizaciones»).
//   BotonMenu { punto: Apps.versionNueva !== ""; onClicked: menu.open() }
import QtQuick
import QtQuick.Controls.Material

ToolButton {
    id: boton
    property bool punto: false

    icon.source: Tema.icono("menu")
    Accessible.name: punto ? "Menú (hay una versión nueva)" : "Menú"

    Rectangle {
        visible: boton.punto
        // Sobre la esquina de arriba a la derecha del ícono (24×24, centrado)
        x: boton.width / 2 + 4
        y: boton.height / 2 - 14
        width: 11
        height: 11
        radius: width / 2
        color: Tema.naranja
        border.width: 2
        border.color: boton.Material.primary   // un aro del color de la barra, para que se despegue del ícono
    }
}
