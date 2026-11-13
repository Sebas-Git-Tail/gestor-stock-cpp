// Pedido HTTPS con el cliente del sistema: Qt en Android no trae OpenSSL y no puede hacer HTTPS
// por su cuenta. Lo llaman ConfigServidor (comun-qt) y CatalogoServidor (Bodrio), desde un hilo aparte porque este pedido se queda esperando.
package com.sebastian.comun;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

public class ClienteHttp {
    // Devuelve {código HTTP, cuerpo}. El código es 0 si no hubo conexión.
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
}
