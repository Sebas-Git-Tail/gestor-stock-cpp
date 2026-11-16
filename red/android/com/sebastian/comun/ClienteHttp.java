// Pedido HTTPS con el cliente del sistema: Qt en Android no trae OpenSSL y no puede hacer HTTPS
// por su cuenta. Lo llaman ConfigServidor (comun-qt) y CatalogoServidor (Bodrio), desde un hilo aparte porque este pedido se queda esperando.
package com.sebastian.comun;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

public class ClienteHttp {
    // Devuelve {código HTTP, cuerpo}. El código es 0 si no hubo conexión.
    // El cuerpo vuelve como TEXTO (UTF-8): sirve para JSON y páginas, NO para archivos binarios
    // (un APK se rompe). Para bajar archivos está descargar().
    public static String[] pedir(String direccion) {
        HttpURLConnection conexion = null;
        try {
            conexion = (HttpURLConnection) new URL(direccion).openConnection();
            conexion.setConnectTimeout(20000);
            conexion.setReadTimeout(20000);
            conexion.setRequestProperty("User-Agent", "Bodrio-Supremo");
            int codigo = conexion.getResponseCode();
            if (codigo != HttpURLConnection.HTTP_OK)
                return new String[] {Integer.toString(codigo), ""};
            try (InputStream entrada = conexion.getInputStream()) {
                ByteArrayOutputStream cuerpo = new ByteArrayOutputStream();
                byte[] bloque = new byte[16384];
                int leidos;
                while ((leidos = entrada.read(bloque)) > 0)
                    cuerpo.write(bloque, 0, leidos);
                return new String[] {"200", cuerpo.toString("UTF-8")};
            }
        } catch (IOException error) {
            return new String[] {"0", ""};
        } finally {
            if (conexion != null)
                conexion.disconnect();
        }
    }

    // Baja un archivo directo al disco, sin pasarlo por texto (APK, instaladores). Devuelve el código
    // HTTP (0 si no hubo conexión o no se pudo escribir). Si falla, borra lo que haya bajado a medias.
    public static int descargar(String direccion, String ruta) {
        HttpURLConnection conexion = null;
        File archivo = new File(ruta);
        try {
            conexion = (HttpURLConnection) new URL(direccion).openConnection();
            conexion.setConnectTimeout(20000);
            conexion.setReadTimeout(60000);
            conexion.setRequestProperty("User-Agent", "Bodrio-Supremo");
            int codigo = conexion.getResponseCode();
            if (codigo != HttpURLConnection.HTTP_OK)
                return codigo;
            try (InputStream entrada = conexion.getInputStream();
                 FileOutputStream salida = new FileOutputStream(archivo)) {
                byte[] bloque = new byte[65536];
                int leidos;
                while ((leidos = entrada.read(bloque)) > 0)
                    salida.write(bloque, 0, leidos);
            }
            return 200;
        } catch (IOException error) {
            archivo.delete();
            return 0;
        } finally {
            if (conexion != null)
                conexion.disconnect();
        }
    }
}
