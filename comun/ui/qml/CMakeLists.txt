# Módulo QML «Comun.Ui» (COMUN_UI_QML): tema e interfaz comunes de las apps en QML.
# En la app:  target_link_libraries(App PRIVATE comun_ui_qmlplugin)
#             main.cpp:  #include <QtQml/qqmlextensionplugin.h>  y  Q_IMPORT_QML_PLUGIN(Comun_UiPlugin)
#             QML:       import Comun.Ui
find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 QmlCore)
set(CMAKE_AUTOMOC ON)
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set_source_files_properties(Tema.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(comun_ui_qml
    URI Comun.Ui
    VERSION 1.0
    STATIC
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Comun/Ui
    QML_FILES
        Tema.qml Icono.qml Seccion.qml Chip.qml Dialogo.qml Hoja.qml Segmentos.qml
        TarjetaHerramienta.qml TarjetaEstado.qml Avisos.qml EstadoVacio.qml AvisoFlotante.qml
        PaginaApps.qml BotonMenu.qml ItemMenu.qml DialogoNovedades.qml
)
target_link_libraries(comun_ui_qml PUBLIC Qt6::Quick Qt6::QuickControls2 Qt6::QmlCore)   # QmlCore: Settings del modo claro/oscuro

# Los mismos íconos que usan las apps con Widgets (qrc:/comun/iconos/<nombre>.svg)
file(GLOB COMUN_ICONOS_SVG RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ../iconos/*.svg)
qt_add_resources(comun_ui_qml comun_ui_iconos
    PREFIX /comun/iconos
    BASE ../iconos
    FILES ${COMUN_ICONOS_SVG})
