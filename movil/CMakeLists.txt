find_package(Qt6 6.4 REQUIRED COMPONENTS Quick QuickControls2)

# Todo menos main(): así las pruebas usan el mismo código que la app
qt_add_library(gsmovil STATIC)
qt_add_qml_module(gsmovil
    URI GestorStock
    VERSION 1.0
    RESOURCE_PREFIX /
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/GestorStock
    QML_FILES
        qml/Main.qml
        qml/EditorProducto.qml
        qml/PaginaColumnas.qml
        qml/PaginaOperacion.qml
        qml/PaginaSincronizacion.qml
    SOURCES
        controlador.h controlador.cpp
)
target_include_directories(gsmovil PUBLIC .)
target_link_libraries(gsmovil PUBLIC gsnucleo Qt6::Quick Qt6::QuickControls2)

qt_add_executable(gestor-stock-movil main.cpp)
target_link_libraries(gestor-stock-movil PRIVATE gsmovil gsmovilplugin)

# versionCode sale de la versión (2.2.1 -> 20201): Android solo instala encima si es mayor que el anterior
math(EXPR GS_VERSION_CODE "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")

# Android: nombre, ícono y paquete de la app. En la PC se ignoran.
set_target_properties(gestor-stock-movil PROPERTIES
    QT_ANDROID_PACKAGE_NAME "ar.gestorstock.app"
    QT_ANDROID_APP_NAME "Gestor de Stock"
    QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE ${GS_VERSION_CODE}
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
)
