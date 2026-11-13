# comun-qt: código que comparten los programas de Sebastián. Cada proyecto lo trae en su carpeta
# comun/ con «git subtree» y lo agrega con add_subdirectory. Antes de add_subdirectory, cada uno
# elige qué partes usa:
#   COMUN_DESCARGAS  diálogo de pistas, consulta a yt-dlp y resumen de rsync  (los Manager)
#   COMUN_RED        pedidos HTTP, servidor.json y aviso de versiones nuevas  (Android/Windows-Manager, Bodrio)
#   COMUN_SINC       sincronización cifrada PC ↔ celular                      (gestor de stock, ingresos y gastos)
# Para probarlo solo: cmake -S . -B build -G Ninja -DCOMUN_PRUEBAS=ON && cmake --build build && ctest --test-dir build
cmake_minimum_required(VERSION 3.21)
project(comun-qt LANGUAGES C CXX)

option(COMUN_DESCARGAS "Diálogo de pistas y yt-dlp" OFF)
option(COMUN_RED "Pedidos HTTP y configuración del servidor" OFF)
option(COMUN_SINC "Sincronización PC ↔ celular" OFF)
option(COMUN_PRUEBAS "Compilar las pruebas (prende todo)" OFF)
if(COMUN_PRUEBAS)
    set(COMUN_DESCARGAS ON)
    set(COMUN_RED ON)
    set(COMUN_SINC ON)
endif()

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(COMUN_DESCARGAS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_library(comun_descargas STATIC
        descargas/DialogoPistas.cpp descargas/DialogoPistas.h descargas/DialogoPistas.ui
        descargas/ConsultaVideo.cpp descargas/ConsultaVideo.h
        descargas/ResumenSimulacion.cpp descargas/ResumenSimulacion.h)
    set_target_properties(comun_descargas PROPERTIES AUTOMOC ON AUTOUIC ON)
    target_include_directories(comun_descargas PUBLIC descargas)
    target_link_libraries(comun_descargas PUBLIC Qt6::Widgets)
endif()

if(COMUN_RED)
    find_package(Qt6 REQUIRED COMPONENTS Core Network)
    add_library(comun_red STATIC
        red/ClienteHttp.cpp red/ClienteHttp.h
        red/ConfigServidor.cpp red/ConfigServidor.h
        red/Actualizaciones.cpp red/Actualizaciones.h)
    set_target_properties(comun_red PROPERTIES AUTOMOC ON)
    target_include_directories(comun_red PUBLIC red)
    target_link_libraries(comun_red PUBLIC Qt6::Core Qt6::Network)
endif()

if(COMUN_SINC)
    find_package(Qt6 REQUIRED COMPONENTS Core Network)
    add_library(comun_monocypher STATIC terceros/monocypher/monocypher.c terceros/monocypher/monocypher.h)
    target_include_directories(comun_monocypher PUBLIC terceros/monocypher)
    set_target_properties(comun_monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(comun_sinc STATIC
        sinc/base.h sinc/cripto.cpp sinc/cripto.h
        sinc/sincronizacion_comun.cpp sinc/sincronizacion_comun.h)
    set_target_properties(comun_sinc PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
    target_include_directories(comun_sinc PUBLIC sinc)
    target_link_libraries(comun_sinc PUBLIC Qt6::Core Qt6::Network PRIVATE comun_monocypher)
endif()

if(COMUN_PRUEBAS)
    enable_testing()
    add_subdirectory(tests)
endif()
