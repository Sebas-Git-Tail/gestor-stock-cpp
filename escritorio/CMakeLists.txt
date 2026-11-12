find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(ZLIB REQUIRED)    # para leer los .xlsx

# Todo menos main(): así las pruebas usan exactamente el mismo código que el programa
add_library(gsescritorio STATIC
    xlsxlector.h xlsxlector.cpp
    importarexcel.h importarexcel.cpp
    portapapeles.h portapapeles.cpp
    dialogoproducto.h dialogoproducto.cpp dialogo_producto.ui
    dialogooperacion.h dialogooperacion.cpp dialogo_operacion.ui
    dialogocolumnas.h dialogocolumnas.cpp dialogo_columnas.ui
    dialogosincronizacion.h dialogosincronizacion.cpp dialogo_sincronizacion.ui
    ventanaprincipal.h ventanaprincipal.cpp main.ui
)
target_include_directories(gsescritorio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gsescritorio PUBLIC gsnucleo Qt6::Widgets ZLIB::ZLIB)
target_compile_options(gsescritorio PRIVATE -Wall -Wextra)

add_executable(gestor-stock main.cpp)
target_link_libraries(gestor-stock PRIVATE gsescritorio)
target_compile_options(gestor-stock PRIVATE -Wall -Wextra)
