// Colores y medidas comunes de las apps en QML (Android-Manager, Gestor de Stock, Ingresos y Gastos).
// Sigue el tema del sistema (claro u oscuro), igual que Material.System. Cada app elige su acento
// en main.qml:  Component.onCompleted: Tema.acento = "#2e7d32"   (o con un binding)
pragma Singleton
import QtQuick

QtObject {
    property bool oscuro: Application.styleHints.colorScheme === Qt.ColorScheme.Dark
    property color acento: "#2a78d6"

    readonly property color fondo: oscuro ? "#111110" : "#f3f3f1"
    readonly property color superficie: oscuro ? "#1c1c1b" : "#ffffff"
    readonly property color presionado: oscuro ? "#262625" : "#efefec"
    readonly property color borde: oscuro ? "#2c2c2a" : "#e1e0d9"
    readonly property color tinta: oscuro ? "#f0efec" : "#1b1b1a"
    readonly property color tintaSecundaria: oscuro ? "#c3c2b7" : "#52514e"
    readonly property color tintaApagada: "#898781"

    // Acento: como texto/ícono (más claro en oscuro, para que se lea), relleno de botones y fondo suave
    readonly property color acentoTexto: oscuro ? Qt.lighter(acento, 1.45) : acento
    readonly property color acentoRelleno: oscuro ? Qt.lighter(acento, 1.1) : acento
    readonly property color acentoSuave: Qt.rgba(acento.r, acento.g, acento.b, oscuro ? 0.2 : 0.12)

    readonly property color verde: oscuro ? "#4cc38a" : "#1f8a54"
    readonly property color verdeRelleno: oscuro ? "#24935c" : "#1f8a54"
    readonly property color rojo: oscuro ? "#f07070" : "#d23c3b"
    readonly property color rojoRelleno: oscuro ? "#c43c3c" : "#d23c3b"
    readonly property color naranja: oscuro ? "#f0954f" : "#c4621c"
    readonly property color naranjaSuave: oscuro ? "#3a2616" : "#fdeedd"

    readonly property int margen: 16
    readonly property int radio: 14

    // 0 info, 1 éxito, 2 error, 3 advertencia
    function colorEstado(tipo) {
        return tipo === 1 ? verde : tipo === 2 ? rojo : tipo === 3 ? naranja : tintaSecundaria
    }
    // Ícono de comun-qt/ui/iconos (o una URL completa, si ya trae «:»)
    function icono(nombre) {
        return nombre.indexOf(":") >= 0 ? nombre : "qrc:/comun/iconos/" + nombre + ".svg"
    }
}
