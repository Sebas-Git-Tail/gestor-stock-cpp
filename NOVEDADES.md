# Novedades del Gestor de Stock

Lo que cambió en cada versión (la PC y el celular). La app lo muestra la primera vez que se abre una versión
nueva y siempre desde «Novedades» en el menú. Los cambios marcados [Sebastián] solo aparecen en sus equipos.

## 2.11.0 (27/11/2026)
- En la venta, además del descuento, se puede cargar un recargo (por ejemplo, por pagar con tarjeta).
- El descuento y el recargo se pueden poner en pesos o en porcentaje. El porcentaje se calcula sobre el subtotal
  y la cuenta se ve antes de confirmar.
- El historial muestra el recargo de cada venta, y los reportes lo tienen en cuenta.
- Celular: si el descuento es mayor que el total, lo avisa y no deja confirmar.
- Actualizá la PC y el celular juntos: con una versión anterior el total de la venta queda bien, pero no se ve
  el recargo y los reportes por producto salen mal en las ventas que lo tienen.
- [Sebastián] Columna nueva operaciones.recargo (ALTER TABLE, user_version sigue en 4) y campo opcional «rec» en
  «op». Los porcentajes se pasan a pesos al registrar (montoEnPesos en modelos.h); reportes: factor
  total / (total + descuento − recargo).

## 2.10.0 (26/11/2026)
- Historial de ventas y compras (botón «Historial» en la PC, en el menú del celular): cada una con su fecha y lo
  que llevó; se puede filtrar por ventas o compras y por fechas.
- Anular una venta o una compra (sirve para una devolución): el stock vuelve a como estaba y los reportes ya no
  la cuentan. No se borra: queda en el historial como «anulada».
- En la venta se puede anotar el medio de pago, el cliente y un descuento (todo opcional).
- Si el lector de códigos (en la PC) o el escáner (en el celular) lee un código que no tiene ningún producto,
  ofrece «Crear producto con este código».
- Celular: «Copia de seguridad» y «Exportar CSV» en el menú, y «Deshacer» al eliminar un producto.
- Celular: en Reportes se pueden elegir las fechas Desde y Hasta, como en la PC.
- Copia automática de los datos una vez por día (quedan las últimas 7), en la PC y en el celular.
- Actualizá la PC y el celular juntos: con una versión anterior el stock queda bien, pero esa versión no ve las
  anulaciones ni los datos nuevos de la venta.
- [Sebastián] Las columnas nuevas de operaciones (anulada, medio_pago, cliente, descuento) se agregan con ALTER
  TABLE sin cambiar user_version (sigue en 4), así la 2.9.x abre la base. Viajan como campos opcionales de «op»
  (anul, mp, cli, desc); la devolución de stock viaja como ajustes con uuid fijo «anul-<op>-<producto>», así una
  anulación hecha en los dos lados no se duplica. Copias diarias: backups/automatica_AAAA-MM-DD.db.
- [Sebastián] Eliminar en la PC ya no se demora en memoria: borra enseguida y «Deshacer» usa Stock::restaurar.

## 2.9.1 (25/11/2026)
- Celular: ahora necesita Android 11 o más nuevo, como las otras apps. No cambia nada más.

## 2.9.0 (21/11/2026)
- Novedades: la primera vez que abrís una versión nueva te cuenta qué cambió. Siempre están en el menú, en
  «Novedades».
- Si la app se cierra por una falla, manda un informe a Sebastián (sin tus productos ni tus ventas) para que
  la pueda arreglar. Si algo anda mal, también podés mandarlo a mano desde Novedades y contar qué pasó.

## 2.8.0 (18/11/2026)
- Modo claro y oscuro con un botón (se recuerda).

## 2.7.0 (18/11/2026)
- Celular: el menú se abre con las 3 rayitas, que muestran un puntito cuando hay una versión nueva.

## 2.6.0 (17/11/2026)
- Interfaz nueva, igual en todas las apps.
- Al eliminar un producto aparece «Deshacer» por unos segundos.
