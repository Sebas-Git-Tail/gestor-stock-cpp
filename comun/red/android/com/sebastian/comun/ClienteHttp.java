// Pedido HTTPS con el cliente del sistema: Qt en Android no trae OpenSSL y no puede hacer HTTPS
// por su cuenta. Lo llaman ConfigServidor (comun-qt) y CatalogoServidor (Bodrio), desde un hilo aparte porque este pedido se queda esperando.
package com.sebastian.comun;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.io.OutputStream;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public class ClienteHttp {
    // Rutas de las descargas que hay que cortar (cancelar())
    private static final Set<String> cancelados = ConcurrentHashMap.newKeySet();

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
        cancelados.remove(ruta);
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
                while ((leidos = entrada.read(bloque)) > 0) {
                    if (cancelados.remove(ruta))
                        throw new IOException("cancelado");
                    salida.write(bloque, 0, leidos);
                }
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

    // Corta la descarga en curso a esa ruta (descargar() devuelve 0 y borra lo bajado)
    public static void cancelar(String ruta) {
        cancelados.add(ruta);
    }

    // POST con un cuerpo JSON. Devuelve {código HTTP, cuerpo}; el cuerpo llega también con los
    // errores (4xx/5xx), porque ahí viene el mensaje del servidor. Código 0 si no hubo conexión.
    public static String[] enviar(String direccion, String json, int segundos) {
        HttpURLConnection conexion = null;
        try {
            conexion = (HttpURLConnection) new URL(direccion).openConnection();
            conexion.setConnectTimeout(20000);
            conexion.setReadTimeout(segundos * 1000);
            conexion.setRequestMethod("POST");
            conexion.setDoOutput(true);
            conexion.setRequestProperty("User-Agent", "Android-Manager");
            conexion.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            byte[] cuerpo = json.getBytes(StandardCharsets.UTF_8);
            conexion.setFixedLengthStreamingMode(cuerpo.length);
            try (OutputStream salida = conexion.getOutputStream()) {
                salida.write(cuerpo);
            }
            int codigo = conexion.getResponseCode();
            InputStream entrada = codigo < 400 ? conexion.getInputStream() : conexion.getErrorStream();
            if (entrada == null)
                return new String[] {Integer.toString(codigo), ""};
            try (InputStream e = entrada) {
                ByteArrayOutputStream respuesta = new ByteArrayOutputStream();
                byte[] bloque = new byte[16384];
                int leidos;
                while ((leidos = e.read(bloque)) > 0)
                    respuesta.write(bloque, 0, leidos);
                return new String[] {Integer.toString(codigo), respuesta.toString("UTF-8")};
            }
        } catch (IOException error) {
            return new String[] {"0", ""};
        } finally {
            if (conexion != null)
                conexion.disconnect();
        }
    }
}
