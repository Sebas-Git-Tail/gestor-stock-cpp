# comun-qt: código que comparten los programas de Sebastián. Cada proyecto lo trae en su carpeta
# comun/ con «git subtree» y lo agrega con add_subdirectory. Antes de add_subdirectory, cada uno
# elige qué partes usa:
#   COMUN_DESCARGAS  diálogo de pistas, consulta a yt-dlp y resumen de rsync  (los Manager con Widgets)
#   COMUN_DESCARGAS_NUCLEO  lo mismo sin el diálogo (sin Widgets): opciones de pistas, consulta y resumen  (Android-Manager, QML)
#   COMUN_RED        pedidos HTTP, servidor.json y aviso de versiones nuevas  (Android/Windows-Manager, Bodrio)
#   COMUN_SINC       sincronización cifrada PC ↔ celular                      (gestor de stock, ingresos y gastos)
#   COMUN_APPS       actualizarse sola e instalar las otras apps (prende COMUN_RED), novedades e informe de
#                    fallas (soporte/)                                                       (todas)
#   COMUN_APPS_WIDGETS  la página/ventana «Apps», el aviso de versión nueva y la ventana de novedades con
#                    Widgets (todas menos las de QML)
#   COMUN_UI         tema oscuro común, íconos SVG, tarjetas, avisos y estado vacío con Widgets  (las de Widgets)
#   COMUN_UI_QML     lo mismo para QML: módulo «Comun.Ui» (Tema, Tarjeta, Chip, Avisos, Dialogo...)  (las de QML)
# Para probarlo solo: cmake -S . -B build -G Ninja -DCOMUN_PRUEBAS=ON && cmake --build build && ctest --test-dir build
cmake_minimum_required(VERSION 3.21)
project(comun-qt LANGUAGES C CXX)

option(COMUN_DESCARGAS "Diálogo de pistas y yt-dlp" OFF)
option(COMUN_DESCARGAS_NUCLEO "Consulta a yt-dlp y opciones de pistas, sin Widgets" OFF)
option(COMUN_RED "Pedidos HTTP y configuración del servidor" OFF)
option(COMUN_SINC "Sincronización PC ↔ celular" OFF)
option(COMUN_APPS "Actualizarse sola e instalar las otras apps" OFF)
option(COMUN_APPS_WIDGETS "Página «Apps» con Widgets" OFF)
option(COMUN_UI "Tema y piezas de interfaz con Widgets" OFF)
option(COMUN_UI_QML "Tema y piezas de interfaz en QML (módulo Comun.Ui)" OFF)
option(COMUN_PRUEBAS "Compilar las pruebas (prende todo)" OFF)
if(COMUN_PRUEBAS)
    set(COMUN_DESCARGAS ON)
    set(COMUN_RED ON)
    set(COMUN_SINC ON)
    set(COMUN_APPS ON)
    set(COMUN_APPS_WIDGETS ON)
    set(COMUN_UI ON)
endif()
if(COMUN_APPS_WIDGETS)
    set(COMUN_APPS ON)
    set(COMUN_UI ON)
endif()
if(COMUN_APPS)
    set(COMUN_RED ON)
endif()

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(COMUN_DESCARGAS)
    set(COMUN_DESCARGAS_NUCLEO ON)
endif()

if(COMUN_DESCARGAS_NUCLEO)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    add_library(comun_descargas_nucleo STATIC
        descargas/Pistas.h
        descargas/OpcionesPistas.cpp descargas/OpcionesPistas.h
        descargas/ConsultaVideo.cpp descargas/ConsultaVideo.h
        descargas/ResumenSimulacion.cpp descargas/ResumenSimulacion.h)
    target_include_directories(comun_descargas_nucleo PUBLIC descargas)
    target_link_libraries(comun_descargas_nucleo PUBLIC Qt6::Core)
endif()

if(COMUN_DESCARGAS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_library(comun_descargas STATIC
        descargas/DialogoPistas.cpp descargas/DialogoPistas.h descargas/DialogoPistas.ui)
    set_target_properties(comun_descargas PROPERTIES AUTOMOC ON AUTOUIC ON)
    target_link_libraries(comun_descargas PUBLIC comun_descargas_nucleo Qt6::Widgets)
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
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Network)
    add_library(comun_apps STATIC
        apps/CatalogoApps.cpp apps/CatalogoApps.h
        apps/Actualizador.cpp apps/Actualizador.h
        apps/ModeloApps.cpp apps/ModeloApps.h
        apps/Traducciones.cpp apps/Traducciones.h apps/traducciones/traducciones.qrc
        soporte/Novedades.cpp soporte/Novedades.h
        soporte/Fallas.cpp soporte/Fallas.h)
    set_target_properties(comun_apps PROPERTIES AUTOMOC ON AUTORCC ON)
    target_include_directories(comun_apps PUBLIC apps soporte)
    target_link_libraries(comun_apps PUBLIC comun_red Qt6::Core Qt6::Gui Qt6::Network)
    if(WIN32)
        target_link_libraries(comun_apps PRIVATE shell32)
    endif()
    # Mete el NOVEDADES.md de la app en el programa como «:/novedades/NOVEDADES.md» (lo lee soporte::Novedades):
    #   comun_novedades(MiApp ${CMAKE_CURRENT_SOURCE_DIR}/../NOVEDADES.md)
    function(comun_novedades destino archivo)
        set_source_files_properties("${archivo}" PROPERTIES QT_RESOURCE_ALIAS NOVEDADES.md)
        qt_add_resources(${destino} comun_novedades_${destino} PREFIX /novedades FILES "${archivo}")
    endfunction()
    # En Android: Instalador.java y ProveedorArchivos.java están en red/android (como ClienteHttp.java);
    # la app enlaza apps/android/res/xml/rutas_archivos.xml y declara permiso, proveedor y <queries>
    # (ver red/android/com/sebastian/comun/Instalador.java). Necesita androidx.core.
endif()

if(COMUN_UI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
    add_library(comun_ui STATIC
        ui/Tema.cpp ui/Tema.h
        ui/Piezas.cpp ui/Piezas.h
        ui/iconos/iconos.qrc)
    set_target_properties(comun_ui PROPERTIES AUTOMOC ON AUTORCC ON)
    target_include_directories(comun_ui PUBLIC ui)
    target_link_libraries(comun_ui PUBLIC Qt6::Widgets Qt6::Svg)
endif()

if(COMUN_UI_QML)
    add_subdirectory(ui/qml)
endif()

if(COMUN_APPS_WIDGETS)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_library(comun_apps_widgets STATIC apps/PanelApps.cpp apps/PanelApps.h
        soporte/VentanaNovedades.cpp soporte/VentanaNovedades.h)
    set_target_properties(comun_apps_widgets PROPERTIES AUTOMOC ON)
    target_link_libraries(comun_apps_widgets PUBLIC comun_apps comun_ui Qt6::Widgets)
endif()

if(COMUN_PRUEBAS)
    enable_testing()
    add_subdirectory(tests)
endif()
