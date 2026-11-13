cmake_minimum_required(VERSION 3.20)
project(gestor-stock VERSION 2.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)   # convierte los .ui de Qt Designer en código al compilar
set(CMAKE_AUTORCC ON)

# Qué se construye:
#   PC (Arch):  la versión de escritorio. Con -DGS_MOVIL=ON también la del celular, para probarla en la PC.
#   Android:    solo la del celular (Qt Creator con el kit «Android Qt 6.x»)
if(ANDROID)
    set(GS_ESCRITORIO OFF)
    set(GS_MOVIL ON)
else()
    option(GS_ESCRITORIO "Versión de escritorio (Qt Widgets, pantallas de Qt Designer)" ON)
    option(GS_MOVIL "Versión para celular (Qt Quick); en la PC sirve para probarla" OFF)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Sql Network)

# Código compartido (repo comun-qt, traído con git subtree a comun/): la sincronización cifrada
# PC ↔ celular, la misma que usa ingresos-gastos
set(COMUN_SINC ON)
add_subdirectory(comun)

# El núcleo lo comparten las dos versiones: base de datos, búsqueda, columnas y sincronización
add_library(gsnucleo STATIC
    nucleo/modelos.h
    nucleo/util.h nucleo/util.cpp
    nucleo/db.h nucleo/db_interno.h nucleo/db.cpp nucleo/db_operaciones.cpp nucleo/db_importacion.cpp nucleo/db_sinc.cpp
    nucleo/sincronizacion.h
)
target_include_directories(gsnucleo PUBLIC nucleo)
target_link_libraries(gsnucleo PUBLIC Qt6::Core Qt6::Sql Qt6::Network comun_sinc)
target_compile_definitions(gsnucleo PUBLIC GS_VERSION="${PROJECT_VERSION}")
if(NOT MSVC)
    target_compile_options(gsnucleo PRIVATE -Wall -Wextra)
endif()

if(GS_ESCRITORIO)
    add_subdirectory(escritorio)
    install(TARGETS gestor-stock RUNTIME DESTINATION bin)
    install(FILES data/gestor-stock.desktop DESTINATION share/applications)
    if(EXISTS "${CMAKE_SOURCE_DIR}/data/Logo.svg")
        install(FILES data/Logo.svg DESTINATION share/icons/hicolor/scalable/apps)
    else()
        message(WARNING "Falta data/Logo.svg: el programa se instala sin ícono.")
    endif()
endif()
if(GS_MOVIL)
    add_subdirectory(movil)
endif()

if(NOT ANDROID)
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
