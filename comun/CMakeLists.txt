# comun-qt: código que comparten los programas de Sebastián. Cada proyecto lo trae en su carpeta
# comun/ con «git subtree» y lo agrega con add_subdirectory. Antes de add_subdirectory, cada uno
# elige qué partes usa:
#   COMUN_DESCARGAS  diálogo de pistas, consulta a yt-dlp y resumen de rsync  (los Manager)
#   COMUN_RED        pedidos HTTP, servidor.json y aviso de versiones nuevas  (Android/Windows-Manager, Bodrio)
#   COMUN_SINC       sincronización cifrada PC ↔ celular                      (gestor de stock, ingresos y gastos)
#   COMUN_APPS       actualizarse sola e instalar las otras apps (prende COMUN_RED)      (todas)
#   COMUN_APPS_WIDGETS  la página/ventana «Apps» y el aviso de versión nueva con Widgets (todas menos las de QML)
# Para probarlo solo: cmake -S . -B build -G Ninja -DCOMUN_PRUEBAS=ON && cmake --build build && ctest --test-dir build
cmake_minimum_required(VERSION 3.21)
project(comun-qt LANGUAGES C CXX)

option(COMUN_DESCARGAS "Diálogo de pistas y yt-dlp" OFF)
option(COMUN_RED "Pedidos HTTP y configuración del servidor" OFF)
option(COMUN_SINC "Sincronización PC ↔ celular" OFF)
option(COMUN_APPS "Actualizarse sola e instalar las otras apps" OFF)
option(COMUN_APPS_WIDGETS "Página «Apps» con Widgets" OFF)
option(COMUN_PRUEBAS "Compilar las pruebas (prende todo)" OFF)
if(COMUN_PRUEBAS)
    set(COMUN_DESCARGAS ON)
    set(COMUN_RED ON)
    set(COMUN_SINC ON)
    set(COMUN_APPS ON)
    set(COMUN_APPS_WIDGETS ON)
endif()
if(COMUN_APPS_WIDGETS)
    set(COMUN_APPS ON)
endif()
if(COMUN_APPS)
    set(COMUN_RED ON)
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

if(COMUN_APPS)
    find_package(Qt6 REQUIRED COMPONENTS Core Network)
    add_library(comun_apps STATIC
        apps/CatalogoApps.cpp apps/CatalogoApps.h
        apps/Actualizador.cpp apps/Actualizador.h
        apps/ModeloApps.cpp apps/ModeloApps.h)
    set_target_properties(comun_apps PROPERTIES AUTOMOC ON)
    target_include_directories(comun_apps PUBLIC apps)
    target_link_libraries(comun_apps PUBLIC comun_red Qt6::Core Qt6::Network)
    if(WIN32)
        target_link_libraries(comun_apps PRIVATE shell32)
    endif()
    # En Android: Instalador.java y ProveedorArchivos.java están en red/android (como ClienteHttp.java);
    # la app enlaza apps/android/res/xml/rutas_archivos.xml y declara permiso, proveedor y <queries>
    # (ver red/android/com/sebastian/comun/Instalador.java). Necesita androidx.core.
endif()

if(COMUN_APPS_WIDGETS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_library(comun_apps_widgets STATIC apps/PanelApps.cpp apps/PanelApps.h)
    set_target_properties(comun_apps_widgets PROPERTIES AUTOMOC ON)
    target_link_libraries(comun_apps_widgets PUBLIC comun_apps Qt6::Widgets)
endif()

if(COMUN_PRUEBAS)
    enable_testing()
    add_subdirectory(tests)
endif()
