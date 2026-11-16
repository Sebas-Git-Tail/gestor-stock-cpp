package com.sebastian.comun;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;

import java.io.File;

// Instalar APKs (la actualización de la propia app u otra app del catálogo) y saber qué versión
// de una app está instalada. Lo llama apps/Actualizador.cpp de comun-qt.
//
// Cada app necesita en su AndroidManifest.xml:
//   - el permiso REQUEST_INSTALL_PACKAGES (la primera vez Android pide permitir «instalar apps desconocidas»)
//   - el proveedor com.sebastian.comun.ProveedorArchivos con authorities="<paquete>.archivos" y
//     res/xml/rutas_archivos.xml (comparte solo la carpeta «actualizacion» de la caché)
//   - <queries> con los paquetes de las otras apps (Android 11+ no deja ver las demás si no)
public class Instalador
{
    public static String carpeta(Context contexto)
    {
        File carpeta = new File(contexto.getCacheDir(), "actualizacion");
        carpeta.mkdirs();
        // Se borran los APK de instalaciones anteriores
        File[] viejos = carpeta.listFiles();
        if (viejos != null)
            for (File f : viejos)
                f.delete();
        return carpeta.getAbsolutePath();
    }

    public static void instalar(Context contexto, String ruta)
    {
        Uri uri = ProveedorArchivos.getUriForFile(contexto, contexto.getPackageName() + ".archivos", new File(ruta));
        Intent intento = new Intent(Intent.ACTION_VIEW);
        intento.setDataAndType(uri, "application/vnd.android.package-archive");
        intento.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
        contexto.startActivity(intento);
    }

    // versionName de un paquete instalado, o "" si no está
    public static String version(Context contexto, String paquete)
    {
        try {
            PackageInfo info = contexto.getPackageManager().getPackageInfo(paquete, 0);
            return info.versionName != null ? info.versionName : "";
        } catch (PackageManager.NameNotFoundException e) {
            return "";
        }
    }
}
