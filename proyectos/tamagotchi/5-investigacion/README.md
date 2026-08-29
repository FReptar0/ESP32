# Investigación — Tamagotchi

Lo que hubo que averiguar antes de poder construir, **con fecha y fuente**.

La fecha no es adorno: el hardware y las APIs cambian, y la información en
internet se pudre. Un tutorial de 2019 puede describir una API que ya no existe.
Anotar cuándo lo leíste es lo que después te permite distinguir *"lo implementé
mal"* de *"esto funcionaba y cambió"*.

---

## 🔴 Pendientes, en orden de urgencia

### ~~1. ¿Qué cobertura de letras hay para tu música?~~ ✅ RESUELTO

Verificado el 2026-08-29 → [`lrclib-api.md`](lrclib-api.md). Hay cobertura
suficiente y la API no pide autenticación.

### 2. ¿Cómo se validan los certificados raíz TLS? (futuro ADR-0009)

El spike de memoria usó `setInsecure()`, que no verifica el certificado. El
firmware final sí debe hacerlo. Hay que decidir entre incrustar el certificado
raíz (que **caduca**, y el aparato dejaría de funcionar solo un día) o usar el
paquete de certificados del ESP32.

Es el único riesgo del proyecto donde algo que funcionaba se rompe sin que nadie
lo toque.

---

## ✅ Hallazgos

### 2026-08-29 — Una búsqueda larga dejaba al aparato ciego a los cambios

**Qué pasaba:** al saltar de canción mientras se descargaba una letra, el aparato
se quedaba mostrando la anterior. La causa: `buscar()` encadenaba hasta 5
peticiones (~8 s) **sin comprobar nada entre medias**, y con `precargarSiguiente()`
el núcleo de red podía pasar más de 10 segundos sin preguntarle nada a Spotify.
Como el cambio de canción **solo se detecta preguntando**, era ciego.
**Fuente:** propio — ver [`cancelacion-de-busquedas.md`](cancelacion-de-busquedas.md).
**Confianza:** máxima (reproducido y corregido).
**Consecuencia:** la búsqueda pasa de bloque a **cola de pasos**, un paso por
vuelta del bucle, consultando a Spotify entre medias. Retraso máximo de ~10 s a
~2,4 s.
🔑 **La forma más simple de "cancelar" casi nunca es abortar: es no encadenar.**

### 2026-08-29 — El contexto solo se resolvía para playlists

**Qué pasaba:** al reproducir desde un álbum salía literalmente "Album", y desde
un artista salía "Reproduciendo". Solo las playlists mostraban su nombre real.
**Causa:** descuido propio — solo implementé la búsqueda del nombre para
playlists y para el resto puse una etiqueta fija.
**Consecuencia:** ahora el álbum usa el nombre que **ya viene** en la respuesta de
"reproduciendo ahora" (cero peticiones extra), el artista se pide a
`/v1/artists/{id}`, y cuando no hay contexto se muestra el álbum en vez de un
"Reproduciendo" que no dice nada.


### 2026-08-29 — Cuatro problemas al ver el aparato funcionando

**Qué se encontró y arregló:**

1. **Los acentos no se dibujaban** (`León` → `Len`). Las fuentes integradas de
   TFT_eSPI solo tienen ASCII 32–127 y **descartan el resto en silencio**. La
   búsqueda en LRCLIB sí funcionaba con acentos: el fallo era solo al dibujar.
2. **🐛 Bug propio:** se añadía `...` a frases que sí cabían. Un `break` salía
   del bucle sin vaciar la variable que se comprobaba después.
3. **Recortar es peor que encoger.** Ahora prueba 3 renglones en fuente 4, luego
   fuente 2, y solo entonces recorta.
4. **💥 Desbordamiento de pila** al precargar: un objeto de 6.8 KB como variable
   local, con la pila de Arduino en 8 KB. Había 186 KB libres, pero **de heap,
   que es otro sitio**.

**Fuente:** propio — detalle en [`limitaciones-de-pantalla.md`](limitaciones-de-pantalla.md).
**Confianza:** máxima (todos reproducidos y arreglados).

🔑 **Tres lecciones:** los acentos no existen en las fuentes integradas · un
`break` a media función se salta lo que el resto del código da por hecho ·
`ESP.getFreeHeap()` **no dice nada sobre la pila**.

### 2026-08-29 — Precarga: la letra aparece al instante al cambiar de canción

**Qué se hizo:** el ESP32 pregunta a Spotify qué canción viene después
(`/me/player/queue`) y **descarga esa letra por adelantado**. Además sondea cada
1 s en los últimos 15 segundos de la canción, en vez de cada 5 s.
**Fuente:** propio — ver [`precarga-de-letras.md`](precarga-de-letras.md).
**Por qué importa:** antes hacían falta dos viajes por internet al cambiar de
canción. Ahora es una copia en memoria.
**Confianza:** máxima (verificado: `[Letra] ⚡ desde precarga`).
**Consecuencia:** usa `user-read-playback-state`, que se había pedido en el
[ADR-0010](../1-decisiones/ADR-0010-permisos-de-spotify.md) sin saber para qué.


### 2026-08-29 — `Transfer-Encoding: chunked` rompía la lectura de LRCLIB

**Qué se averiguó:** LRCLIB devolvía HTTP 200 y el firmware decía "no
encontrada". La causa: LRCLIB manda la respuesta **por trozos**, y
`HTTPClient::getStream()` del ESP32 entrega el chorro crudo **con las marcas de
tamaño incluidas**. ArduinoJson veía `1f4a{"id":...` y fallaba. Spotify manda
`Content-Length`, por eso ahí el mismo código sí funcionaba.
**Fuente:** diagnóstico propio — ver [`chunked-encoding.md`](chunked-encoding.md).
**Por qué importa:** 🔑 **un HTTP 200 solo dice que el servidor respondió bien,
no que tú lo hayas entendido bien.** Ante un fallo con 200, sospechar del parseo.
**Confianza:** máxima (arreglado y verificado).
**Consecuencia:** se escribió `LectorChunked`, que quita las marcas al vuelo y
mantiene el streaming — sin cargar los 91 KB en memoria.

### 2026-08-29 — Arduino define macros con nombres muy comunes (`HEX`, `cli`…)

**Qué se averiguó:** dos errores de compilación seguidos, con mensajes que no
señalaban el problema. `HEX` es una macro con valor 16 y `cli()` es la de
deshabilitar interrupciones.
**Fuente:** propio — ver [`macros-de-arduino.md`](macros-de-arduino.md).
**Por qué importa:** 🔑 **si un nombre corto y común da un error sin sentido,
sospecha de una macro.** El preprocesador cambió el código antes de compilarlo.
**Confianza:** máxima.


### 2026-08-29 — ✅ LRCLIB sirve, y la duración es la clave del cruce (resuelve S-05)

**Qué se averiguó:** LRCLIB no pide autenticación, es gratis y tiene buena
cobertura. La canción que parecía faltar (`If`, Red Hot Chili Peppers) **sí
estaba**, con 8 versiones y letra sincronizada — el 404 lo causaba una **duración
equivocada**, no el nombre del álbum.
⚠️ Descubierto además: `/api/search` devuelve **91–115 KB** porque incluye la
letra completa de 20 resultados. `/api/get` exacto son solo **5 KB**.
**Fuente:** consultas reales a la API — ver [`lrclib-api.md`](lrclib-api.md).
**Por qué importa:** era el último supuesto abierto de los nueve. Y la duración,
que es el dato decisivo, es justo el que Spotify da con precisión.
**Confianza:** máxima (medido, no leído).
**Consecuencia:** [ADR-0003](../1-decisiones/ADR-0003-fuente-de-las-letras.md)
aceptado, con búsqueda en dos pasos para no cargar nunca los 91 KB.


### 2026-08-29 — La fuente legible a 60 cm es la 4, y eso recorta el texto disponible

**Qué se averiguó:** de las tres fuentes probadas en la GMT130, **la fuente 4
(26 px de alto) es la que se lee cómodo a distancia de escritorio**. La 2 se ve
nítida pero cuesta leerla; la 6 se lee genial pero ocupa demasiado.
**Fuente:** prueba propia — mirando la pantalla a ~60 cm.
**Confianza:** máxima. Es una medición perceptual, y el criterio del RNF-01 es
justamente perceptual.

**Por qué importa — y aquí está la sorpresa:**

Las estimaciones anteriores usaban una fuente de 8×16 px y daban **~30 caracteres
× 15 líneas**. Con la fuente 4 (26 px de alto, ancho variable) la realidad es
más bien:

| | Estimado antes (fuente 8×16) | Real con fuente 4 |
|---|---|---|
| Caracteres por línea | ~30 | **~17** |
| Líneas | ~15 | **~9** |

O sea: **casi la mitad** de lo que se había calculado.

Es exactamente el riesgo que estaba anotado en el
[ADR-0004](../1-decisiones/ADR-0004-pantalla-gmt130.md): *"240 píxeles en 1.3
pulgadas son píxeles diminutos; el texto puede verse nítido y aun así no
leerse"*. Lo que cabe **técnicamente** y lo que se **lee** son cosas distintas, y
solo se distinguen mirando.

**Consecuencia:**

- ✅ **RNF-01 resuelto** — fuente 4 para el texto principal.
- ⚠️ **El RF-04 (letras) vuelve a estar apretado.** Con ~17 caracteres por línea,
  un verso normal necesita dos o tres renglones. Sigue siendo viable —una OLED de
  0.96" daba 20 caracteres × 4 líneas—, pero hay menos margen del que parecía.
- 📌 **Pendiente medirlo de verdad:** los números de arriba son estimaciones. El
  ESP32 puede decirnos el ancho exacto con `tft.textWidth(texto, 4)`. Conviene
  medirlo con títulos de canciones reales antes de diseñar la pantalla.


### 2026-08-29 — 🎉 El ESP32 lee lo que suena en Spotify, con certificado verificado

**Qué se logró:** la placa renueva su propio `access_token`, consulta
`/me/player/currently-playing`, interpreta el JSON e imprime canción, artista,
álbum y progreso. Todo con `Certificate validated` en cada conexión.
**Fuente:** prueba propia, firmware pasos 5 y 6.
**Confianza:** máxima — funcionando en la placa.
**Consecuencia:** RF-01, RF-02, RF-03 y RF-05 quedan resueltos a nivel de datos.
Falta llevarlos a la pantalla (paso 7).

### 2026-08-29 — No hay fuga de memoria en el ciclo de consultas

**Qué se averiguó:** la RAM baja 3.3 KB durante las primeras 16 consultas y
**se estabiliza** en ~198.1 KB, oscilando ±130 B sin tendencia. Es calentamiento
del heap, no una fuga.
**Fuente:** prueba propia — ver [`memoria-en-regimen.md`](memoria-en-regimen.md).
**Por qué importa:** con solo 5 muestras la conclusión habría sido "fuga" y
habríamos rediseñado algo que no estaba roto. 🔑 Una fuga es **lineal e
infinita**; el calentamiento es **asintótico**.
**Confianza:** alta. Pendiente una prueba de 24 h.
**Consecuencia:** respalda el RNF-06.

### 2026-08-29 — Verificar certificados exige saber la fecha

**Qué se averiguó:** un certificado declara "soy válido entre tal y tal fecha".
El ESP32 arranca creyendo que estamos en 1970, así que **todos** los certificados
le parecerían "aún no válidos" y toda conexión TLS fallaría.
**Fuente:** conocido del protocolo, aplicado en el firmware.
**Por qué importa:** el fallo se vería como un `connect()` que devuelve false,
sin ninguna pista de la causa real.
**Confianza:** máxima.
**Consecuencia:** el firmware sincroniza por NTP y **espera** a que la hora sea
creíble antes de cualquier TLS. Documentado en el
[ADR-0009](../1-decisiones/ADR-0009-certificados-raiz.md).


### 2026-08-29 — El Python de python.org no usa el llavero de macOS

**Qué se averiguó:** el intercambio de tokens falló con
`SSLCertVerificationError: unable to get local issuer certificate`.
No era culpa de Spotify ni del código: **Python no tenía ningún paquete de
certificados con el que verificar a nadie**.

```
ssl.get_default_verify_paths().openssl_cafile
  -> /Library/Frameworks/Python.framework/Versions/3.13/etc/openssl/cert.pem
¿existe? -> False
```

**Causa:** el Python instalado desde python.org **no usa el llavero del
sistema**. Trae `certifi` incluido, pero hay que enlazarlo una vez ejecutando
`/Applications/Python 3.13/Install Certificates.command` — un paso que casi nadie
hace y que rompe *cualquier* conexión HTTPS hasta que se corre.

**Fuente:** diagnóstico propio sobre el error real.

**Por qué importa — más allá del arreglo:** 🔑 **es exactamente el problema del
futuro [ADR-0009](../1-decisiones/README.md).** La pregunta *"¿en quién confío
para verificar este certificado?"* es la misma en los dos lados:

| | ¿De dónde saca en quién confiar? |
|---|---|
| Navegador | Del llavero del sistema operativo |
| Python de python.org | De `certifi`… **si alguien lo enlazó** ← aquí falló |
| **ESP32** | **De nada. Hay que dárselo nosotros.** |

Verlo primero en la Mac, donde se puede leer el error completo, vale mucho: el
mismo fallo en el ESP32 aparecería como un `connect()` que devuelve false, sin
más explicación.

**Solución aplicada:** el script apunta explícitamente a `certifi` en vez de
depender de la configuración de la máquina. Así funciona en cualquier Mac, se
haya corrido el instalador o no. Y añade un mensaje de error que explica el
problema en vez de escupir el traceback.


### 2026-08-29 — ❌ El Web Playback SDK no puede correr en el ESP32

**Qué se averiguó:** el Web Playback SDK es una librería **JavaScript de
navegador**. La documentación dice que *"no puede correr en dispositivos
embebidos ni microcontroladores"*, y requiere soporte de medios cifrados (DRM) y
Premium (no los planes solo-móvil).
**Fuente:** [documentación oficial del Web Playback SDK](https://developer.spotify.com/documentation/web-playback-sdk).
**Por qué importa:** el ESP32 **no puede reproducir audio de Spotify**. Pero sí
puede **controlar** la reproducción de otro aparato, vía los endpoints de player
de la Web API (`user-modify-playback-state` + Premium). El Tamagotchi es un
control remoto, no un altavoz.
**Confianza:** máxima (fuente primaria).
**Consecuencia:** [ADR-0010](../1-decisiones/ADR-0010-permisos-de-spotify.md) ·
RF-08 a RF-10 · "reproducir audio" queda fuera de alcance de forma permanente.

### 2026-08-29 — ~~El scope mínimo para este proyecto es uno solo~~ *(superado)*

**Qué se averiguó:** `/me/player/currently-playing` requiere únicamente
**`user-read-currently-playing`**. No hace falta `user-read-playback-state` ni,
mucho menos, `user-modify-playback-state`.
**Fuente:** documentación oficial del endpoint.
**Por qué importa:** `user-modify-playback-state` daría permiso para **controlar**
la reproducción (play, pausa, saltar). Está fuera del alcance del proyecto por
decisión propia, y además **este token va a vivir dentro del ESP32**: si el
aparato se pierde o se presta, ese permiso se va con él. Pidiendo lo mínimo, en
el peor caso alguien vería qué música escuchas, no manipularía la cuenta.
**Confianza:** máxima.
**Consecuencia:** ⏩ **Superado por el [ADR-0010](../1-decisiones/ADR-0010-permisos-de-spotify.md)**:
el alcance del proyecto creció para incluir control de reproducción, así que se
piden 3 permisos en vez de 1. El razonamiento de mínimo privilegio sigue siendo
válido y se conserva — solo cambió qué es "lo mínimo necesario".


### 2026-08-29 — ✅ La memoria alcanza de sobra (resuelve S-04)

**Qué se averiguó:** con TLS activo contra `accounts.spotify.com`, el mínimo
histórico de RAM libre fue **204.168 B (199.4 KB)** — el doble del umbral de
"sobra". TLS cuesta ~50 KB temporales. **Sin fugas** (la memoria se recupera
íntegra al cerrar) y **sin fragmentación** (el bloque contiguo mayor no se movió
de 110.580 B en ninguna medición).
**Fuente:** prueba propia — ver [`spike-memoria-tls.md`](spike-memoria-tls.md).
**Por qué importa:** era el riesgo #1 tras cerrar el S-02, y el punto de control
del ADR-0002. De haber salido mal, habría obligado a rediseñar la arquitectura.
**Confianza:** máxima (medición directa contra el servidor real).
**Consecuencia:** S-04 resuelto ✅ · el ADR-0002 se confirma, no se reabre ·
queda pendiente el ADR-0009 sobre certificados.

### 2026-08-29 — ⚠️ Spotify ya no acepta `localhost` como Redirect URI

**Qué se averiguó:** hay que usar la IP explícita: `http://127.0.0.1:8888/callback`.
La documentación dice literalmente *"localhost is not allowed as redirect URI"*.
HTTP sin S solo se permite para direcciones de loopback como esta.
**Fuente:** [documentación oficial de Redirect URIs](https://developer.spotify.com/documentation/web-api/concepts/redirect_uri).
**Por qué importa:** se empezó a aplicar el **9 de abril de 2025**, así que
**todos los tutoriales anteriores a esa fecha dicen `localhost` y fallan**. Es el
ejemplo perfecto de por qué esta carpeta anota fechas.
**Confianza:** máxima (fuente primaria).
**Consecuencia:** valor fijado para el paso 4 (OAuth).


### 2026-08-29 — RAM base del ESP32: 303.5 KB libres

**Qué se averiguó:** recién arrancado, con WiFi inicializado pero sin conexión
activa, el ESP32 reporta **310.744 bytes libres (303.5 KB)**.
**Fuente:** prueba propia — `ESP.getFreeHeap()` en el firmware de prueba.
**Por qué importa:** es la **línea base** contra la que se mide todo lo demás.
TLS suele consumir entre 30 y 50 KB, así que a primera vista hay margen — pero
eso hay que medirlo (supuesto S-04), no suponerlo.
**Confianza:** máxima (medición directa).
**Consecuencia:** habilita el paso 3 del
[ADR-0002](../1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md), que es el
punto de control de toda la arquitectura.

### 2026-08-29 — ✅ Hay red de 2.4 GHz con señal excelente (resuelve S-06)

**Qué se averiguó:** el escaneo encontró 5 redes. La más fuerte, **`INFINITUMEB00`
en el canal 11, a −39 dBm** — señal excelente. Las otras 4 son de vecinos, entre
−67 y −96 dBm.
**Fuente:** prueba propia — `WiFi.scanNetworks()` desde el ESP32.
**Por qué importa:** el ESP32 **solo escanea 2.4 GHz**. Que la red aparezca en
este listado *es* la demostración de que tiene banda de 2.4 GHz. Se acabó el
clásico "no me conecta y no sé por qué".
**Confianza:** máxima. Lo dice el propio ESP32, no el router.
**Consecuencia:** S-06 resuelto ✅. Queda confirmar con el usuario que
`INFINITUMEB00` es efectivamente su red.

> 💡 **Cómo leer el dBm**, que va a salir otra vez: es la potencia de señal, y es
> un número **negativo** — cuanto más cerca de cero, mejor.
> `−30 a −55` excelente · `−55 a −70` buena · `−70 a −80` débil (aquí empiezan
> las desconexiones) · peor de `−80` casi inutilizable.


### 2026-08-29 — ✅ La API de Spotify sí sirve para este proyecto (resuelve S-02)

**Qué se averiguó:** `GET /me/player/currently-playing` sigue activo, devuelve
título, álbum, artistas, duración y **`progress_ms`**, y **no** fue afectado ni
por los recortes de noviembre de 2024 ni por las restricciones de febrero de 2026.
Scope necesario: `user-read-currently-playing`.
⚠️ **Pero el Modo Desarrollo exige que el dueño de la app tenga Spotify Premium.**
**Fuente:** documentación oficial — ver [`spotify-api-2026.md`](spotify-api-2026.md).
**Por qué importa:** era el riesgo #1 del proyecto, el único que no se podía
resolver programando. Queda cerrado.
**Confianza:** alta (fuentes primarias).
**Consecuencia:** S-02 resuelto ✅ · se abre el supuesto S-09 (Premium) 🔴 ·
se desbloquea el [ADR-0002](../1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md).


### 2026-08-29 — La placa es un ESP32-D0WD-V3 con USB-Serie CP2102

**Qué se averiguó:** `ESP32-D0WD-V3` rev v3.1, 2 núcleos @ 240 MHz, 4 MB de
flash, Wi-Fi + BT. Chip USB-Serie **CP2102** (Silicon Labs), reconocido por macOS
sin instalar drivers. MAC `84:1F:E8:2C:3E:C4`.
**Fuente:** prueba propia — `esptool chip-id` y `flash-id`.
**Por qué importa:** fija el `board = esp32dev` del `platformio.ini`. Confirma
también que el "-S" del nombre comercial no era un ESP32-S2 ni S3.
**Confianza:** máxima — se lo preguntamos al chip directamente.
**Consecuencia:** desbloqueó la fase 2 (entorno) y la fase 6 (plano físico).

### 2026-08-29 — Los cables USB-C de solo carga son indistinguibles a simple vista

**Qué se averiguó:** varios cables C-a-C llevan corriente pero **no datos**. El
síntoma es LED encendido + cero dispositivos en el árbol USB.
**Fuente:** prueba propia — 4 rondas de diagnóstico.
**Por qué importa:** costó todo el arranque del proyecto. Y se va a repetir.
**Confianza:** máxima.
**Consecuencia:** 🔖 **Regla del taller:** ante un "no me detecta la placa",
probar ese cable con un celular antes que nada. 20 segundos contra horas.

### 2026-08-29 — La API pública de Spotify no da letras

**Qué se averiguó:** las letras que aparecen en la app de Spotify vienen de un
proveedor externo (Musixmatch) por un endpoint interno que no forma parte de la
API pública.

**Por qué importa:** aunque el ESP32 logre hablar con Spotify perfectamente, de
ahí **no van a salir las letras**. Hace falta una segunda fuente y cruzar la
canción con ella. Esto cambia el plan del RF-04 por completo.

**Confianza:** alta, pero conviene reconfirmarlo al llegar a esa fase.

**Consecuencia:** se escribió el
[ADR-0003](../1-decisiones/ADR-0003-fuente-de-las-letras.md).

### 2026-08-29 — El ESP32 no fue detectado por la Mac

Diagnóstico completo en la [bitácora](../BITACORA.md). Verificado por dos vías
independientes (árbol USB del sistema y `pio device list`).

---

## Cómo se anota un hallazgo

```markdown
### AAAA-MM-DD — Título de lo que se averiguó

**Qué se averiguó:** el hecho, en una o dos frases.
**Fuente:** URL, o "prueba propia".
**Por qué importa:** qué decisión o requerimiento afecta.
**Confianza:** alta / media / baja — y por qué.
**Consecuencia:** qué ADR o requerimiento cambia a raíz de esto.
```

## Sobre los spikes

Un **spike** es una prueba rápida y **desechable** para responder una pregunta
concreta. *"¿El ESP32 aguanta una conexión HTTPS sin quedarse sin memoria?"* →
escribes 30 líneas feas, mides, anotas el número aquí, **borras el código**.

La clave está en lo de desechable. Un spike no se hace bien, se hace rápido, y su
único producto es una respuesta escrita en este archivo. Si el código del spike
sobrevive y acaba en el proyecto final, dejó de ser un spike y se convirtió en
deuda técnica.
