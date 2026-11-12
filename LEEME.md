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
| `escritorio/*.ui` | Las pantallas: **se editan en Qt Designer** y se convierten en código al compilar |
| `escritorio/ventanaprincipal.cpp` | Tabla, búsqueda, posición recordada, acciones, copiar y pegar |
| `escritorio/dialogooperacion.cpp` | Ventana de venta / compra |
| `escritorio/portapapeles.cpp` | Convierte renglones a texto (para otros programas) y texto a productos |
| `escritorio/db.cpp` | SQLite: productos, precios, operaciones, migraciones, importación |
| `escritorio/util.cpp` | Precios, vencimientos y búsqueda (texto y precio) |
| `escritorio/xlsxlector.cpp`, `importarexcel.cpp` | Importación desde Excel (sin librerías externas) |
| `tests/` | 5 suites de pruebas (QtTest) |

Si renombrás un widget en Designer, hay que cambiarlo también en el `.cpp` (el compilador avisa).

## Se guarda en
- Base de datos: `~/.local/share/gestor-stock/stock.db` (backups en `backups/` al lado)
- Posición, tamaño de ventana, columnas y orden: `~/.config/gestor-stock/gestor-stock.conf`
