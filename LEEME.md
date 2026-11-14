# Gestor de Stock — versión C++ / Qt6

Port a C++ del gestor de stock en Python, más lo pedido en *Actualizar Gestor Stock.md*.
Usa **la misma base de datos** (`~/.local/share/gestor-stock/stock.db`): se abre directo, sin perder nada.

## Construir y probar
```bash
sudo pacman -S cmake ninja qt6-base          # una sola vez (zlib ya viene con el sistema)
cmake -S . -B build -G Ninja
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure    # pruebas
./build/gestor-stock                                                   # ejecutar
```

## Instalar como paquete de Arch
```bash
cd packaging && makepkg -si
```
Poné tu `Logo.svg` en `data/` antes de construir (el `.desktop` usa `Icon=Logo`).

## Qué se agregó
| Pedido | Cómo quedó |
|---|---|
| Producto con su stock, precio y vencimiento | Se ven en la tabla y en cada línea de la venta/compra |
| Venta y compra al seleccionar productos | Botones **Venta…** y **Compra…** (o clic derecho). Se pueden elegir varios productos |
| Venta: baja el stock y suma el importe | Cada producto lleva su presentación/precio y cantidad; muestra subtotales y el **Total**. Todo o nada: si falta stock de uno, no se toca ninguno |
| Compra: sube el stock | Solo cantidades |
| Buscador que no vuelve al principio | Al terminar la búsqueda queda en lo elegido (o donde estaba); al cerrar y abrir también |
| Buscar por precio | `4000` · `2000-5000` · `<3000` · `>=2000` · `$ 1.500`; se puede mezclar con texto (`avena 1800`) |

Además: la búsqueda ignora mayúsculas y tildes (`azucar` encuentra `AZÚCAR`), y cada venta/compra queda
guardada en la base (tablas `operaciones` y `operacion_items`) con fecha y total.

## Versión 2.4.0
| Pedido | Cómo quedó |
|---|---|
| Calendario de vencimientos que empiece el domingo (2.3.1) | El vencimiento y las fechas Desde/Hasta de Reportes empiezan el domingo |
| Poner el porcentaje de ganancia al editar el producto | Campo **Ganancia %** al editar (PC y celular): cuánto se le suma al costo (con 30, lo que cuesta $100 se vende a $130). Reportes calcula la ganancia de lo vendido con ese porcentaje; si el producto no lo tiene, usa el costo de las compras como antes |

«Ganancia %» es una columna propia de números con uuid fijo `col-ganancia`: por eso la 2.3.x también la muestra y la sincroniza sin perderla. Arranca oculta en la tabla (se puede mostrar desde Columnas). Si se elimina, no vuelve sola.

## Versión 2.3.0
| Pedido | Cómo quedó |
|---|---|
| Reportes | ☰ → **Reportes y avisos…** (en el celular: Opciones → Reportes y avisos): ventas, compras y **ganancia estimada** de hoy, la semana, el mes, el mes anterior, el año o las fechas que elijas; lo **más vendido**, ventas **por mes** y **por día** |
| Ganancia | En la **compra** se puede cargar el **costo** de cada uno (opcional). La ganancia es lo vendido menos lo que costó, con el costo promedio de las compras de cada producto. Si algo se vendió sin costo cargado, se avisa y no entra en la cuenta |
| Avisos de stock bajo y vencimientos | Arriba de la tabla (y arriba de la lista en el celular) aparece «⚠ 2 con poco stock · 1 por vencer · Ver» cuando un producto está en su **stock mínimo** o por debajo, está **vencido** o **vence en 30 días** (si queda alguno) |
| Código de barras en el celular | Botón **Escanear** arriba (busca el producto) y en la venta/compra (lo agrega directo). Usa el lector de Google Play Services: no pide permiso de cámara; la primera vez lo baja (hace falta internet). En la PC, un lector USB escribe el código en el buscador como si fuera un teclado |
| Sincronización cifrada | El emparejamiento ya no deja adivinar el código escuchando la red y todo viaja **cifrado**. **Hay que emparejar de nuevo una vez** el celular con la PC (el celular lo avisa): Opciones → Sincronizar con la PC → Olvidar esta PC, y emparejar con un código nuevo. Actualizar **los dos** (PC y celular) |

## Versión 2.2.1
| Pedido | Cómo quedó |
|---|---|
| En venta/compra, el buscador mostraba nombre y categoría: con varios tamaños había que probar hasta acertar | Ahora muestra **nombre — presentación y precio** (`Avena — 500 g: $ 4.000 · 1 kg: $ 7.500  (stock 5)`), en la PC y en el celular. La categoría se sigue pudiendo buscar |

## Versión 2.2.0
| Pedido | Cómo quedó |
|---|---|
| Columnas y filas editables | **Columnas de la tabla…** (menú ☰) o clic derecho sobre un título: agregar columnas propias (texto, número o fecha), renombrar, ocultar, ordenar (también arrastrando los títulos) y eliminar. Nombre, Precios y Cantidad no se pueden eliminar; las demás del programa se quitan y se pueden volver a poner con sus datos. Las filas (productos) se agregan, editan y eliminan como siempre |
| Versión para Android | App del celular (`movil/`): buscar, agregar, editar y eliminar productos, administrar columnas, registrar ventas y compras |
| Sincronización PC ↔ celular | ☰ → **Sincronizar con el celular…** en la PC y, en el celular, Opciones → Sincronizar con la PC. Se emparejan una vez con un código de 6 números. Las cantidades se concilian **sumando los movimientos** (ventas, compras y correcciones), así que vender en un lado y comprar en el otro antes de sincronizar no se pisa |

La base se actualiza sola la primera vez (queda una copia en `backups/`). **Ya no se puede abrir con la 2.1.0.**

### Android
```bash
cmake -S . -B build-android -G Ninja -DCMAKE_BUILD_TYPE=Release -DQT_HOST_PATH=$HOME/Qt/6.10.0/gcc_64 \
    -DANDROID_SDK_ROOT=$HOME/Android/Sdk -DANDROID_NDK_ROOT=$HOME/Android/Sdk/ndk/<versión>   # con qt-cmake del kit android_arm64_v8a
JAVA_HOME=/usr/lib/jvm/java-17-openjdk cmake --build build-android --target apk
packaging/firmar-apk.sh build-android      # la clave queda en ~/.android/gestor-stock.keystore: guardá una copia
```

## Versión 2.1.0
| Pedido | Cómo quedó |
|---|---|
| Error: en la venta, a veces la cantidad arrancaba en 2 | Corregido. Pasaba al elegir el producto de la lista del buscador con las flechas y Enter: el Enter contaba dos veces. Además, al elegirlo con un **clic** entraba **otro producto** (el de esa posición en la lista completa); también corregido |
| Copiar y pegar renglones | En la tabla de productos: seleccionar renglones y **Ctrl+C** (o clic derecho → Copiar renglones). Se pegan en Excel, LibreOffice, WhatsApp, etc. **Ctrl+V** en la tabla (o clic derecho → Pegar) los agrega como productos nuevos, con una confirmación |

Al pegar:
- Lo copiado desde el mismo programa entra como **copia**: con todos los datos (también notas y todos los precios), pero con **cantidad 0**, porque es un producto nuevo.
- Texto copiado de una planilla: reconoce los títulos de las columnas en cualquier orden (`Nombre`/`Producto`, `Precio(s)`, `Cantidad`, `Código`, `Categoría`, `Proveedor`, `Vencimiento`/`Vto`, `Medida`, `Stock mín.`, `Notas`); sin títulos, usa el orden de la tabla del programa. Acá la cantidad **sí** se respeta.
- Si el código ya existe (es único), el producto pegado queda sin código.
- Los productos pegados quedan marcados en la tabla; si fue un error, se borran juntos con **Eliminar**.

## Dónde está cada cosa
| Archivo | Qué hace |
|---|---|
| `nucleo/` | Lo que comparten la PC y el celular |
| `nucleo/db.cpp` | SQLite: apertura, migraciones, columnas y productos |
| `nucleo/db_operaciones.cpp` | Ventas, compras, reportes y avisos |
| `nucleo/db_importacion.cpp` | Importación desde Excel y copia de seguridad |
| `nucleo/db_sinc.cpp` | Lo que se intercambia al sincronizar y los equipos emparejados |
| `nucleo/db_interno.h` | Ayudantes que comparten esos cuatro archivos |
| `nucleo/sincronizacion.h` | Puertos y nombre para la sincronización; el protocolo (cifrado) está en `comun/sinc` |
| `nucleo/util.cpp` | Precios, vencimientos y búsqueda (texto y precio) |
| `comun/` | Código compartido con otros programas ([comun-qt](https://github.com/Sebas-Git-Tail/comun-qt), traído con `git subtree`): **no editar acá**, sino en comun-qt |
| `escritorio/*.ui` | Las pantallas de la PC: **se editan en Qt Designer** y se convierten en código al compilar |
| `escritorio/ventanaprincipal.cpp` | Tabla, búsqueda, posición recordada, avisos, acciones, copiar y pegar |
| `escritorio/dialogooperacion.cpp` | Ventana de venta / compra |
| `escritorio/dialogoreportes.cpp` | Reportes y avisos |
| `escritorio/portapapeles.cpp` | Convierte renglones a texto (para otros programas) y texto a productos |
| `escritorio/xlsxlector.cpp`, `importarexcel.cpp` | Importación desde Excel (sin librerías externas) |
| `movil/qml/*.qml` | Las pantallas del celular |
| `movil/controlador.cpp` | Lo que usan las pantallas del celular (en QML se llama `App`) |
| `movil/android/` | Íconos, `build.gradle` (con el lector de códigos de Google) y `Escaner.java` |
| `tests/` | 7 suites de pruebas (QtTest); `tst_movil` se compila con `-DGS_MOVIL=ON` |

Si renombrás un widget en Designer, hay que cambiarlo también en el `.cpp` (el compilador avisa).

## Se guarda en
- Base de datos: `~/.local/share/gestor-stock/stock.db` (backups en `backups/` al lado)
- Posición, tamaño de ventana, columnas y orden: `~/.config/gestor-stock/gestor-stock.conf`
