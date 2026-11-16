# comun-qt

Código que comparten mis programas en C++ / Qt 6, para no tener la misma copia en cada proyecto
(antes había que hacer el mismo arreglo tres veces).

| Carpeta | Qué tiene | Lo usan |
|---|---|---|
| `descargas/` | Diálogo de pistas de audio, subtítulos y calidad (`DialogoPistas`), lectura de `yt-dlp -J` y opciones para bajar en MKV (`ConsultaVideo`), resumen de una simulación de rsync (`ResumenSimulacion`) | Arch-Linux-Manager, Android-Manager, Windows-Manager |
| `red/` | Pedidos HTTP(S) que andan igual en la PC y en Android (`ClienteHttp`), datos del servidor desde `servidor.json` (`ConfigServidor`), aviso de versión nueva desde GitHub (`Actualizaciones`) | Android-Manager, Windows-Manager, Bodrio Supremo |
| `sinc/` | Sincronización PC ↔ celular **cifrada** (`ServidorSincBase`, `ClienteSincBase`) | gestor-stock-cpp, ingresos-gastos |
| `apps/` | Catálogo de los programas, **actualizarse sola** e **instalar las otras apps** (`Actualizador`, `ModeloApps` para QML y Widgets, `PanelApps`/`DialogoApps`, `avisarActualizaciones`) | todos |
| `terceros/monocypher/` | [Monocypher](https://monocypher.org) 4.0.2 (CC0 / BSD-2): X25519, Elligator, BLAKE2b y XChaCha20-Poly1305, sin OpenSSL | `sinc/` |

## Cómo se usa en un proyecto
Cada proyecto lo trae en su carpeta `comun/` con `git subtree` (así cada repo se puede clonar y
compilar solo, sin pasos extra):

```bash
git subtree add  --prefix=comun https://github.com/Sebas-Git-Tail/comun-qt main --squash   # la primera vez
git subtree pull --prefix=comun https://github.com/Sebas-Git-Tail/comun-qt main --squash   # para actualizar
```

y en su `CMakeLists.txt`:

```cmake
set(COMUN_DESCARGAS ON)        # o COMUN_RED / COMUN_SINC
add_subdirectory(comun)        # ../comun si el CMakeLists está en gui/
target_link_libraries(MiPrograma PRIVATE comun_descargas)
```

Los cambios se hacen **acá** y después se trae la versión nueva a cada proyecto con `subtree pull`
(en esta máquina: `~/Repo/Scripts/actualizar-comun.sh`, que lo hace en todos).

Para Android, la app tiene que incluir `red/android/com/sebastian/comun/ClienteHttp.java` en su
carpeta `android/src` (los proyectos tienen un enlace a ese archivo).

## Apps: actualizarse e instalar las otras
- `apps/CatalogoApps.cpp` dice de dónde se baja cada programa (release de GitHub o carpeta del servidor),
  qué archivo corresponde a cada sistema y cómo se ve la versión instalada (pacman -Q, PackageManager o
  el registro de Inno Setup). **Carina y Bodrio Supremo están ocultas**: se actualizan solas pero nunca
  aparecen en la lista para instalar.
- Instalar: Linux `pkexec pacman -U`, Android el instalador del sistema, Windows el instalador de Inno
  Setup (la propia app, en silencio y se vuelve a abrir).
- CMake: `set(COMUN_APPS_WIDGETS ON)` y `comun_apps_widgets` (o `COMUN_APPS` y `comun_apps` en QML).
- Android: las clases Java (`Instalador`, `ProveedorArchivos`) están en `red/android/com/sebastian/comun`, al lado de
  `ClienteHttp.java`, así que entran con el mismo enlace de `android/src/com/sebastian/comun`. Además hay que enlazar
  `apps/android/res/xml/rutas_archivos.xml` en `android/res/xml` y declarar en el manifiesto  el permiso
  `REQUEST_INSTALL_PACKAGES`, el proveedor `com.sebastian.comun.ProveedorArchivos`
  (`android:authorities="${applicationId}.archivos"`) y `<queries>` con los paquetes de las otras apps.
- Para publicar una versión alcanza con subir el archivo con el nombre de siempre al release de GitHub
  (o dejarlo en la carpeta del servidor, para Bodrio y Carina).

## Sincronización (versión 2)
- **Emparejar** con un código de 6 números: intercambio de claves CPace (X25519 + Elligator). El código
  no viaja; escuchando la red no se puede averiguar, y quien se haga pasar por la PC o por el celular
  tiene un intento por conexión (la PC acepta 5 por código). Queda un secreto de 256 bits en los dos.
- **Sincronizar**: X25519 con claves nuevas cada vez, mezclado con ese secreto; los dos se prueban que lo
  tienen y después todo va cifrado y autenticado (XChaCha20-Poly1305, con contador: no se puede leer,
  cambiar, repetir ni reordenar nada).
- Los equipos emparejados con la versión 1 tienen que **emparejarse de nuevo** una vez (el programa lo avisa).

## Pruebas
```bash
cmake -S . -B build -G Ninja -DCOMUN_PRUEBAS=ON && cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```
`tst_sinc` empareja y sincroniza dos bases de juguete y comprueba, con un intermediario que copia el
tráfico, que por la red no pasa nada legible (ni los datos, ni el código, ni el secreto).
