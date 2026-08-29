# Hallazgo — `Transfer-Encoding: chunked` rompe `getStream()` en el ESP32

- **Fecha:** 2026-08-29
- **Síntoma:** LRCLIB devolvía **HTTP 200** y aun así el firmware decía "no encontrada"
- **Confianza:** máxima (causa identificada y arreglada, verificado funcionando)

---

## El síntoma, que era engañoso

```
[Letra] exacta -> HTTP 200        <- ¡el servidor SÍ encontró la canción!
[Letra] ❌ no encontrada
```

Un HTTP 200 significa que todo salió bien. Y aun así el código descartaba la
respuesta. Comprobado desde la Mac: esas mismas canciones **sí** tenían letra
sincronizada en LRCLIB.

Así que el fallo no estaba ni en la red, ni en LRCLIB, ni en la búsqueda.
Estaba en **cómo se leía la respuesta**.

## La causa

Al mirar las cabeceras que devuelve LRCLIB:

```
Content-Type: application/json
Transfer-Encoding: chunked        <- aquí
Server: cloudflare
```

**`chunked` significa que el servidor no sabe de antemano cuánto va a medir la
respuesta**, así que la manda por trozos y antes de cada uno escribe su tamaño
en hexadecimal:

```
1f4a\r\n                              <- "ahí van 0x1f4a bytes"
{"id":123,"trackName":"...","syncedLyrics":"[00:12.51] Something inside..."}
\r\n
0\r\n\r\n                             <- trozo de tamaño 0 = se acabó
```

El problema: **`HTTPClient::getStream()` del ESP32 entrega el chorro crudo, con
esas marcas incluidas.** ArduinoJson recibe `1f4a{"id":...` y falla al primer
carácter, porque eso no es JSON válido.

### Por qué con Spotify no pasaba

Spotify manda `Content-Length`: dice de entrada cuánto mide la respuesta, así que
no hace falta trocearla y `getStream()` entrega JSON limpio.

**El mismo código funcionaba en un servidor y fallaba en el otro** — y la
diferencia no estaba en el código ni en los datos, sino en una cabecera HTTP.
Es el tipo de fallo que puede costar una tarde si no se miran las cabeceras.

## La solución

Se podría haber usado `https.getString()`, que sí decodifica los trozos. Pero eso
carga **toda** la respuesta en memoria, y la búsqueda amplia de LRCLIB pesa 91 KB
— justo lo que el [ADR-0003](../1-decisiones/ADR-0003-fuente-de-las-letras.md)
quería evitar.

En su lugar se escribió **`LectorChunked`**, una clase que se pone entre el
socket y ArduinoJson y va quitando las marcas según pasan:

```
socket crudo  ->  LectorChunked  ->  ArduinoJson
"1f4a\r\n{...}"    quita marcas       ve solo "{...}"
```

Así ArduinoJson recibe JSON limpio **sin que la respuesta se guarde entera en
memoria**. Se conserva el streaming, que era el punto.

El firmware elige solo: si `https.getSize()` devuelve `-1` (el servidor no dijo
el tamaño), usa el lector; si no, va directo.

## Resultado

```
[Letra] exacta -> HTTP 200 ✅
[Letra] 30 líneas · RAM 193004 B
```

## Lo que hay que llevarse de aquí

🔑 **Un HTTP 200 solo dice que el servidor respondió bien. No dice que tú lo hayas
entendido bien.** Cuando el código falle con un 200, sospecha del *parseo*, no de
la petición.

🔑 **Mira las cabeceras cuando algo funcione en un servidor y no en otro.**
`Transfer-Encoding`, `Content-Encoding` y `Content-Length` cambian cómo hay que
leer la respuesta, y no se ven en el cuerpo.
