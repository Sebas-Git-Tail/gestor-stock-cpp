// Lector de códigos de barras: usa el de Google Play Services (GmsBarcodeScanning), que pone su propia
// pantalla y no necesita permiso de cámara. La primera vez Play Services baja el módulo (hace falta internet).
// El resultado vuelve a C++ por el método nativo resultado() (Controlador::recibirCodigo).
package ar.gestorstock.app;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;

import com.google.mlkit.vision.barcode.common.Barcode;
import com.google.mlkit.vision.codescanner.GmsBarcodeScanner;
import com.google.mlkit.vision.codescanner.GmsBarcodeScannerOptions;
import com.google.mlkit.vision.codescanner.GmsBarcodeScanning;

public class Escaner {
    // codigo: lo leído ("" si se canceló). error: vacío si salió bien.
    private static native void resultado(String codigo, String error);

    private static Activity actividad(Context contexto) {
        while (contexto instanceof ContextWrapper) {
            if (contexto instanceof Activity)
                return (Activity) contexto;
            contexto = ((ContextWrapper) contexto).getBaseContext();
        }
        return null;
    }

    public static void escanear(Context contexto) {
        final Activity a = actividad(contexto);
        if (a == null) {
            resultado("", "no hay una pantalla abierta");
            return;
        }
        a.runOnUiThread(() -> {
            try {
                GmsBarcodeScannerOptions opciones = new GmsBarcodeScannerOptions.Builder()
                    .setBarcodeFormats(Barcode.FORMAT_ALL_FORMATS)
                    .enableAutoZoom()
                    .build();
                GmsBarcodeScanner lector = GmsBarcodeScanning.getClient(a, opciones);
                lector.startScan()
                    .addOnSuccessListener(codigo -> resultado(codigo.getRawValue() != null ? codigo.getRawValue() : "", ""))
                    .addOnCanceledListener(() -> resultado("", ""))
                    .addOnFailureListener(e -> resultado("", e.getMessage() != null ? e.getMessage() : "error desconocido"));
            } catch (Exception e) {
                resultado("", e.getMessage() != null ? e.getMessage() : "error desconocido");
            }
        });
    }
}
