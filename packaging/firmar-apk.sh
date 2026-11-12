#!/usr/bin/env bash
# Firma el APK de release (Android no instala APKs sin firmar).
# Uso: packaging/firmar-apk.sh [carpeta-de-compilación]   (por defecto: build-android)
# La primera vez crea la clave en ~/.android/gestor-stock.keystore.
# ¡Guardá una copia de esa clave! Sin ella no se pueden instalar actualizaciones encima.
set -euo pipefail
BUILD=${1:-build-android}
KS=~/.android/gestor-stock.keystore
PASSF=~/.android/gestor-stock.clave
BT=$(ls -d "${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"/build-tools/*/ | sort -V | tail -1)

if [ ! -f "$KS" ]; then
    (umask 077; head -c 18 /dev/urandom | base64 | tr -d '/+=' > "$PASSF")
    keytool -genkeypair -keystore "$KS" -alias gestorstock -keyalg RSA -keysize 2048 \
        -validity 10000 -storepass "$(cat "$PASSF")" -keypass "$(cat "$PASSF")" -dname "CN=Gestor de Stock"
fi

IN=$(find "$BUILD" -name '*-release-unsigned.apk' -print -quit)
[ -n "$IN" ] || { echo "No encontré el APK sin firmar en $BUILD (¿compilaste con --target apk?)"; exit 1; }
VER=$(grep -oP 'android:versionName="\K[^"]+' "$(dirname "$IN")"/../../../../AndroidManifest.xml 2>/dev/null || echo app)
OUT="$BUILD/gestor-stock-movil-$VER.apk"
TMP=$(mktemp --suffix=.apk)
"$BT/zipalign" -f -p 4 "$IN" "$TMP"
"$BT/apksigner" sign --ks "$KS" --ks-key-alias gestorstock --ks-pass file:"$PASSF" --out "$OUT" "$TMP" 2>/dev/null
rm -f "$TMP" "$OUT.idsig"
"$BT/apksigner" verify "$OUT" 2>/dev/null && echo "Listo: $OUT"
