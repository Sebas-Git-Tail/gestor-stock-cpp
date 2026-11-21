# comun-qt

Código que comparten mis programas en C++ / Qt 6, para no tener la misma copia en cada proyecto
(antes había que hacer el mismo arreglo tres veces).

| Carpeta | Qué tiene | Lo usan |
|---|---|---|
| `descargas/` | Diálogo de pistas de audio, subtítulos y calidad (`DialogoPistas`), lectura de `yt-dlp -J` y opciones para bajar en MKV (`ConsultaVideo`), resumen de una simulación de rsync (`ResumenSimulacion`) | Arch-Linux-Manager, Android-Manager, Windows-Manager |
| `red/` | Pedidos HTTP(S) que andan igual en la PC y en Android (`ClienteHttp`), datos del servidor desde `servidor.json` (`ConfigServidor`), aviso de versión nueva desde GitHub (`Actualizaciones`) | Android-Manager, Windows-Manager, Bodrio Supremo |
| `sinc/` | Sincronización PC ↔ celular **cifrada** (`ServidorSincBase`, `ClienteSincBase`) | gestor-stock-cpp, ingresos-gastos |
| `apps/` | Catálogo de los programas, **actualizarse sola** e **instalar las otras apps** (`Actualizador`, `ModeloApps` para QML y Widgets, `PanelApps`/`DialogoApps`, `avisarActualizaciones`) | todos |
| `soporte/` | **Novedades** de cada versión (lo «[Sebastián]» solo en sus equipos) e **informe de fallas** al servidor (`Novedades`, `Fallas`, `VentanaNovedades`; en QML `DialogoNovedades`). Va dentro de `comun_apps` | todos |
| `ui/` | **Tema e interfaz comunes.** Widgets (`COMUN_UI`, `comun_ui`): `tema::aplicar(app, acento)` (oscuro, con los colores del sitio), íconos SVG de trazo pintados del color que se pida (`tema::icono("buscar")`, en `ui/iconos/`), botones destacados/peligro, chips de estado, `TarjetaAccion`, `EstadoVacio` y avisos flotantes con «Deshacer» (`tema::avisar`). QML (`COMUN_UI_QML`, módulo `Comun.Ui`): `Tema`, `Icono`, `Seccion`, `Chip`, `Segmentos`, `Hoja`, `Dialogo`, `TarjetaHerramienta`, `TarjetaEstado`, `Avisos`, `EstadoVacio`, `AvisoFlotante` y la página `PaginaApps` | todos |
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

## Interfaz común (ui/)
- **Widgets:** `set(COMUN_UI ON)` (lo prende también `COMUN_APPS_WIDGETS`) y `comun_ui`. En `main`, antes de crear ventanas:
  `tema::aplicar(app)` (celeste del sitio) o `tema::aplicar(app, QColor("#e8a838"))` (otro acento). Marcas por propiedad:
  `tema::marcarPrimario(boton)`, `tema::marcarPeligro(boton)`, `tema::marcarChip(etiqueta, tema::Estado::Ok)`,
  `tema::mostrarEstado(etiqueta, texto, estado)` (texto de color en vez de ✓/✗), `lista->setProperty("lateral", true)`.
  **Modo claro/oscuro:** `tema::botonModo(this)` (botón de sol/luna) en la barra de la ventana; se recuerda en
  `tema/oscuro` (la primera vez, oscuro). Estilo propio de la app con `tema::agregarEstilo([](const tema::Colores &c) {…})`
  (no `app.setStyleSheet(app.styleSheet() + …)`, que se perdería al cambiar de modo) y lo pintado a mano con colores
  (hojas de estilo de un widget, pixmaps) se rehace en `tema::alCambiarModo(this, [this] {…})`. Los íconos de
  `tema::icono()` siguen solos al color del tema.
  Ventana principal: `tema::recordarVentana(&ventana)` antes de `show()` (vuelve a abrir con el mismo ancho y alto, y
  maximizada o en pantalla completa si lo estaba; se guarda en `ventana/geometria` de QSettings).
- **QML:** `set(COMUN_UI_QML ON)`, enlazar `comun_ui_qmlplugin`, en `main.cpp`
  `#include <QtQml/qqmlextensionplugin.h>` + `Q_IMPORT_QML_PLUGIN(Comun_UiPlugin)` (también en las pruebas que carguen QML) y en
  QML `import Comun.Ui`. El acento de cada app: `Tema.acento = "#2e7d32"`. `PaginaApps { conBarra: true }` para apps donde cada
  página trae su barra. Las páginas del módulo se abren con un `Component`, no por nombre de archivo.
- **Modo claro/oscuro (QML):** `Material.theme: Tema.oscuro ? Material.Dark : Material.Light` y en el menú
  `ItemMenu { text: Tema.textoAlternar; icono: Tema.iconoAlternar; onClicked: Tema.alternar() }`. Se guarda en
  `tema.ini` (AppConfigLocation). Hasta que se elige, sigue al sistema; una app oscura de entrada pone
  `Tema.oscuroPredeterminado = true` (`Tema.oscuro` ya no se escribe).
- **Menú de 3 rayitas (Android):** `BotonMenu { punto: Apps.versionNueva !== ""; onClicked: menu.open() }` en la barra
  (puntito naranja cuando hay versión nueva) y en el `Drawer` cada opción con `ItemMenu { text: …; icono: … }`; la de
  «Apps y actualizaciones» con `destacado: Apps.versionNueva !== ""` (naranja + «Nueva versión»). La pregunta «¿La
  instalo?» va en `Connections { target: Apps; function onAvisoVersionNueva() {…} }` (una vez por día); `versionNueva`
  queda puesta cada vez que se abre la app (se busca siempre).
- **Íconos:** SVG de 24×24 con trazo `#000000` (se reemplaza por el color pedido). Para agregar uno: el .svg en `ui/iconos/` y
  una línea en `ui/iconos/iconos.qrc` (QML los toma solo, con un glob).
- No usar emojis ni símbolos raros (☰ ⚠ 🔄) en la interfaz: en algunos celulares y en Windows salen como cuadraditos o con
  un alto de línea enorme. Para eso están los íconos.

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

## Novedades e informe de fallas (soporte/)
- **NOVEDADES.md** en la raíz de cada app, metido en el programa con `comun_novedades(<ejecutable> <ruta>/NOVEDADES.md)`
  (después de `add_subdirectory(comun)`). Formato: `## 1.9.0 (21/11/2026)` y debajo `- cambio` (puede seguir en
  la línea de abajo, con sangría). Un cambio que empieza con **`[Sebastián]`** solo se ve en sus equipos; una
  versión sin nada para mostrar no aparece. Al sacar una versión: anotar ahí lo que cambió (para quien lo usa,
  sin jerga) **antes** de compilar.
- **Equipos de Sebastián:** PC con el archivo `~/.config/comun-qt/desarrollador` (Windows: `%LOCALAPPDATA%\comun-qt\desarrollador`;
  Arch-Linux-Manager lo crea en pc e instituto); Android: dejar apretado «Novedades» en el menú (se guarda en la app).
  `COMUN_DESARROLLADOR=1/0` manda sobre todo (pruebas).
- **Al abrir:** si la versión es más nueva que la última vista (`novedades/vista` en QSettings), muestra lo de las
  versiones entre las dos (si nunca vio ninguna, solo la actual). Widgets: `soporte::novedadesAlAbrir(ventana)` y
  `soporte::botonNovedades(this)` (regalito al lado del de modo) o `soporte::mostrarNovedades(this)` desde un menú.
  QML: propiedad de contexto `Novedades` (`new soporte::Novedades(&app)`), `DialogoNovedades { id: novedades }` en
  Main.qml e `ItemMenu { text: "Novedades"; onClicked: novedades.abrirTodas(); onPressAndHold: novedades.alternarDesarrollador() }`.
- **Fallas:** `soporte::iniciarFallas()` en main, después de setApplicationName/Version. Guarda un registro (qInfo y
  más graves, limpio: carpeta personal → `~`, usuario, URLs solo con el sitio, correos) y arma un informe cuando:
  la app se cerró sin terminar bien (marca `proceso-<pid>.marca`; con la señal o la excepción si la hubo), qFatal,
  qCritical, errores de QML (TypeError, ReferenceError…) o `soporte::informar(contexto, detalle)`. El mismo error una
  vez por versión, 8 por día como mucho; los pendientes quedan en `<AppLocalData>/fallas/pendientes` y se mandan a
  `<servidor>/fallas/api/informe` (fallas_web.py en ~/Scripts/Claude/servidor-web). En Android, que el sistema cierre
  la app en segundo plano no cuenta. Si el QML no carga: `Fallas::instancia().informarYSalir("qml", …, 1)`.
  Pruebas: `COMUN_FALLAS_SIN_RED=1` (no manda) y `COMUN_FALLAS_URL` (adónde).
- **Ver los informes:** http://100.101.213.44:8080/fallas/ (solo Tailscale o la red de casa; por Cloudflare/Funnel da 404).
  Los archivos: `~/.local/share/fallas-apps/` en el servidor.

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
