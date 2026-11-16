// Carga la traducción de Qt al español, para que los botones estándar (Aceptar, Cancelar, Sí, No,
// Cerrar...) no salgan en inglés. En Linux usa la del sistema (qt6-translations) si está; si no, la que
// viene adentro del programa (Android y Windows no la traen). Llamar en main() después de crear la app.
#pragma once

namespace apps {
void cargarTraduccionesQt();
}
