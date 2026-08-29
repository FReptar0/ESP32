# Bitácora — Tamagotchi Spotify

Diario de trabajo. Lo más nuevo arriba. Aquí va **el trayecto**: los intentos que
fallaron, las horas perdidas, las sospechas sin confirmar. Las *decisiones* van en
[`1-decisiones/`](1-decisiones/).

---

## 2026-08-29 — Sesión 1: arranque del taller, y la caza del cable

### Qué se hizo

- Se armó la estructura completa del taller y la metodología de 6 fases.
- Se creó el inventario general con su bitácora.
- Se escribieron los primeros 3 ADRs y el borrador de requerimientos.
- Se mapeó la arquitectura end-to-end.
- Se intentó detectar el ESP32 desde la Mac.

### ❌ El ESP32 no fue detectado *(al principio)*

Esta fue la sorpresa de la sesión. Dejo el diagnóstico completo porque, si el
problema resulta ser otro, este registro es lo que va a permitir descartar
hipótesis rápido.

**Qué se revisó, y qué salió:**

| Comprobación | Resultado |
|---|---|
| `ls /dev/cu.*` | Solo Bluetooth, `debug-console` y `wlan-debug`. **Ningún puerto USB-Serie** |
| Árbol USB completo (`ioreg -p IOUSB`) | Hub VIA Labs, USB Billboard, webcam C920, segundo hub, **dock DisplayLink** e "instalador de driver" del dock. **Nada más** |
| Drivers serie cargados (`kextstat`) | Ninguno relacionado con CP2102, CH340 ni FTDI |
| Puertos serie registrados en el kernel | Los 3 internos de macOS. Ninguno USB |

**Un falso positivo que vale la pena documentar:**

Apareció un dispositivo del fabricante `wch.cn` (VID `0x1A86`) — que es
justamente la empresa que fabrica los chips **CH340**, el conversor USB-Serie más
común en placas ESP32 baratas. Por un momento pareció que era la placa.

No lo era. Al montarlo resultó ser un disco USB falso de 197 KB llamado *"Driver
Install"*, cuyo contenido apunta a los drivers de **DisplayLink/Synaptics**:

```
/Volumes/NO NAME/macOS Driver.url
  → https://www.synaptics.com/products/displaylink-graphics/downloads/macos
```

Es decir: el dock DisplayLink usa un controlador WCH para su propio instalador de
drivers. Coincidencia pura de fabricante. **No tiene nada que ver con el ESP32.**

Lo anoto porque la próxima vez que aparezca `wch.cn` en la lista, ya sabemos que
no es la placa.

**Conclusión:** la placa no está enumerando. Descartado que sea un problema de
driver faltante (macOS 26 trae soporte nativo para CP2102 y CH340) — el sistema
operativo ni siquiera ve un dispositivo al que asignarle driver. El problema está
antes: en el cable, en la conexión o en la placa.

### 🎯 Giro: al levantar el inventario apareció la hipótesis buena

Después del diagnóstico declaraste el material real, y salieron dos datos que
cambian todo:

- Los ESP32 genéricos (C-001, C-002) tienen **puerto USB-C**.
- Hay una tercera placa, la **C-003 (DEVKIT v1)**, con **mini-USB**.

**Hipótesis principal ahora: faltan las resistencias CC de 5.1 kΩ.**

El estándar USB-C exige dos resistencias de 5.1 kΩ en las líneas CC para que un
aparato le diga a la computadora "aquí estoy, dame corriente". Muchísimas placas
ESP32 baratas con USB-C **las omiten**, porque el fabricante probó con un cable
USB-A → USB-C y funcionó (un puerto USB-A entrega 5 V sin preguntar nada).

Pero un puerto USB-C **no entrega nada hasta que el otro lado se identifica**.
Sin resistencias, la placa nunca se identifica:

| Cable | Resultado |
|---|---|
| USB-A → USB-C | ✅ Funciona |
| USB-C → USB-C | ❌ Muerta: ni corriente, ni enumeración |

Encaja perfecto con lo observado: **cero dispositivos en el árbol USB**, como si
no hubiera nada conectado. Y desde el punto de vista de la Mac, literalmente no
hay nada conectado. Tu Mac es Apple Silicon (puertos USB-C), así que lo natural
es haber usado un cable C-a-C.

Explicación completa en la [ficha del ESP32](3-componentes/esp32-d0wd-v3.md).

### Hipótesis, reordenadas

1. 🥇 **Cable USB-C → USB-C sin resistencias CC en la placa.** Se arregla con un
   cable USB-A → USB-C. Prácticamente gratis.
2. 🥈 **El cable es de solo carga.** Muchos cables (los de power banks,
   audífonos) llevan corriente pero no datos. La placa enciende, el LED prende, y
   la compu no la ve. Frustrante porque *parece* que todo está bien.
3. 🥉 **Está conectada al dock DisplayLink** y no directo a la Mac.
4. **No está conectada ahora mismo.**
5. **La placa está dañada.** Lo menos probable. Y ahora hay forma de descartarlo
   sin dudas: si C-002 o C-003 sí aparecen con el mismo cable, C-001 está mal.

### ✅ RESUELTO — eran cables USB-C de solo carga

Cuatro rondas de diagnóstico para llegar aquí. Vale la pena el registro completo,
porque el razonamiento sirve para la próxima vez.

**Cómo se fue estrechando el cerco:**

| Ronda | Qué se probó | Resultado | Qué descartó |
|---|---|---|---|
| 1 | Placa USB-C por el hub | Nada en el árbol USB | Descartó driver: sin driver igual aparecería como "no reconocido" |
| 2 | C-003 (mini-USB) por el hub | **Ni LED** | Descartó la hipótesis de las resistencias CC — mini-USB no puede tenerla |
| 3 | Placa USB-C directo a la Mac | **LED sí, datos no** | 🎯 La combinación decisiva |
| 4 | Rotar cables C-a-C | Uno funcionó | Confirmado |

**La pista que lo resolvió fue la ronda 3.** LED encendido + cero dispositivos en
el árbol USB solo puede significar una cosa: **la corriente pasa y los datos no.**
Eso es, por definición, un cable de solo carga. Por fuera son idénticos a uno
normal; por dentro les faltan los hilos de datos.

Y de paso quedó descartada mi hipótesis de las resistencias CC: la placa **sí**
enumeró conectada directo al puerto USB-C de la Mac, así que negocia bien. Era
100% el cable.

**Identificación de la placa** (`esptool`):

```
Chip:      ESP32-D0WD-V3 (revisión v3.1)
Núcleos:   2 @ 240 MHz     Flash: 4 MB
Radio:     Wi-Fi 2.4 GHz + Bluetooth
MAC:       84:1F:E8:2C:3E:C4
USB-Serie: CP2102 (Silicon Labs)
Puerto:    /dev/cu.usbserial-0001
```

Confirmado además que **el auto-reset funciona** (`Hard resetting via RTS pin`):
no habrá que apretar el botón BOOT para programarla. También quedó claro que el
`-S` de "ESP32-S" era marca del vendedor: es el ESP32 clásico, no un S2 ni un S3.

**La lección, para la próxima:** ante cualquier "no me detecta la placa", el
primer paso es conectar ese mismo cable a un celular. Si la compu no ve el
celular, el cable no pasa datos y no hay nada más que investigar. Cuesta 20
segundos y habría ahorrado las cuatro rondas.

### ✅ Primer firmware cargado y corriendo

`[SUCCESS] Took 672.46 seconds` — de los cuales casi todo fue descargar el
toolchain de ESP32 (1.5 GB, solo la primera vez). Las siguientes compilaciones
son de segundos.

**Lo que reportó la placa:**

```
Chip:      ESP32-D0WD-V3, 2 núcleos @ 240 MHz, 4 MB flash
MAC:       84:1F:E8:2C:3E:C4
RAM libre: 310.744 bytes (303.5 KB)   ← línea base
Redes:     5 encontradas, todas 2.4 GHz
           INFINITUMEB00   −39 dBm  canal 11  ← excelente
```

Dos supuestos resueltos de un tiro:

- **S-06 ✅** — el ESP32 solo escanea 2.4 GHz, así que que la red aparezca *es*
  la prueba de que tiene esa banda. Y a −39 dBm, con señal inmejorable.
- **S-04 🔄** — línea base de 303.5 KB establecida. Falta medirla con TLS activo,
  que es el punto de control del ADR-0002.

### ✅ Investigación de la API de Spotify (resuelve el riesgo #1)

`GET /me/player/currently-playing` sigue vivo, devuelve todo lo que necesitamos
(incluido el `progress_ms` que habilita las letras sincronizadas) y **quedó fuera**
de los recortes de nov-2024 y de las restricciones de dev mode de feb-2026.

Apareció y se cerró un prerequisito nuevo: Spotify exige que el dueño de la app
tenga **Premium**. Confirmado — plan Familiar como titular.

Detalle completo en [`5-investigacion/spotify-api-2026.md`](5-investigacion/spotify-api-2026.md).

### 🎯 Decisión de arquitectura: directo, sin puente

Se cerró el [ADR-0002](1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md)
con la **opción A**: el ESP32 habla directo con Spotify.

Yo había recomendado empezar por un puente en la Mac. Se descartó, y con buen
motivo: el objetivo del proyecto es aprender IoT, y la capa que yo proponía mover
a la Mac —WiFi, TLS, OAuth, JSON con memoria contada— es justamente lo que hay
que aprender.

### ✅ Pasos 2 y 3 completados

**Paso 2 — WiFi:** conectó al primer intento, en menos de un segundo.
IP `192.168.1.96`, puerta de enlace `192.168.1.254`, señal **−45 dBm**.

**Paso 3 — el punto de control del proyecto:** conexión HTTPS real contra
`accounts.spotify.com:443`, midiendo la memoria en cada etapa.

```
Antes de nada        251.6 KB libres   bloque mayor 110.580 B
Objeto TLS creado    249.1 KB          bloque mayor 110.580 B
🔑 TLS conectado     208.8 KB          bloque mayor 110.580 B
Respuesta leída      247.8 KB          bloque mayor 110.580 B
Tras cerrar          250.1 KB          bloque mayor 110.580 B

MÍNIMO HISTÓRICO:    199.4 KB   (umbral de "sobra": 100 KB)
```

Handshake TLS en 1.088 ms. Respuesta `HTTP/1.1 307` — esperada, la raíz de
`accounts.spotify.com` redirige; lo que importa es que **hubo respuesta**.

**Tres cosas buenas, no una:**

1. **199.4 KB de mínimo** — el doble del umbral. TLS cuesta ~50 KB temporales.
2. **Sin fugas** — la memoria vuelve íntegra al cerrar (1.520 B de diferencia, que
   es ruido). Crítico: el aparato va a abrir ~17.000 conexiones al día. Una fuga
   de 2 KB por conexión lo mataría en minutos, y de madrugada, cuando nadie mira.
3. **Sin fragmentación** — el bloque contiguo mayor no se movió de 110.580 B en
   ninguna de las cinco mediciones. Tener 200 KB libres no garantiza poder
   reservar 50 KB de golpe si están partidos en pedacitos; aquí no lo están.

**El ADR-0002 queda confirmado.** No hay que reabrirlo.

Detalle completo en [`5-investigacion/spike-memoria-tls.md`](5-investigacion/spike-memoria-tls.md).

### ⚠️ Hallazgo sobre el Redirect URI de Spotify

`localhost` **ya no se acepta**. Hay que usar la IP explícita:
`http://127.0.0.1:8888/callback`.

Se empezó a aplicar el **9 de abril de 2025**, así que todos los tutoriales
anteriores a esa fecha están mal. Buen ejemplo de por qué la fase 5 anota fechas.

### 🔧 Dos arreglos sobre la marcha

**El script de OAuth fallaba con `CERTIFICATE_VERIFY_FAILED`.** No era el código:
el Python de python.org no usa el llavero de macOS y su paquete de certificados
nunca se enlazó (`.../etc/openssl/cert.pem` no existe). El script ahora apunta a
`certifi` explícitamente, así que funciona sin depender de la configuración de la
máquina.

📌 Es un anticipo exacto del ADR-0009: la pregunta *"¿en quién confío para
verificar este certificado?"* es la misma que tendrá el ESP32, solo que él no
tiene llavero de ningún tipo.

**Se quitó el parpadeo del LED.** Cumplió su función en el paso 1 —demostrar que
la placa ejecutaba nuestro código— y a partir de ahí solo molesta en un aparato
de escritorio.

### 🎉 Pasos 4, 5 y 6 completados — el ESP32 lee Spotify

```
┌──────────────────────────────────────────────
│ 🎵  Fashion Drunk
│ 👤  Otis Stacks
│ 💿  Otis Stacks
└──────────────────────────────────────────────
  ▶ [==============      ] 2:26 / 3:26   RAM: 201012 B
```

La placa renueva su propio `access_token` con el `refresh_token`, consulta
`/me/player/currently-playing`, interpreta el JSON y saca canción, artista, álbum
y progreso. Sola, sin la Mac de por medio. **Confirma el ADR-0002.**

**Detalle técnico que vale la pena:** el JSON de Spotify trae 10 KB o más
(imágenes, mercados, enlaces, popularidad). En vez de guardarlo entero se usa un
**filtro de ArduinoJson**, que descarta lo que no interesa *mientras lee*. Gasto:
unos cientos de bytes en vez de 10 KB.

### 🔐 ADR-0009 cerrado: certificados verificados de verdad

```
[esp_crt_bundle.c] esp_crt_verify_callback(): Certificate validated
```

En cada conexión. Se descartó incrustar un certificado suelto (frágil: caduca o
Spotify cambia de autoridad y el aparato muere solo un día) a favor del **paquete
completo de 150 CAs de Mozilla**, generado desde `certifi` con un script propio
porque el core no lo trae. 66.8 KB de flash.

**Y una pieza que casi se nos escapa:** verificar un certificado exige saber qué
día es hoy, y el ESP32 arranca en 1970 — todos los certificados le parecerían
"aún no válidos". El firmware sincroniza por NTP y **espera** antes de intentar
TLS.

### 🔍 Falsa alarma de fuga de memoria (y cómo se resolvió)

Las primeras 5 muestras bajaban ~230 B cada una. A ese ritmo la placa moriría en
una hora, y el RNF-06 sería imposible.

Se midió con 31 muestras en vez de 5:

```
201448 ... 198320 198120  ← baja 16 veces
198116 198088 198128 ...  ← y se aplana
```

**No es fuga: es el calentamiento del heap.** Los buffers de TLS y de red crecen
hasta su tamaño de trabajo y ahí se quedan.

🔑 **La lección:** una fuga es **lineal e infinita**; el calentamiento es
**asintótico**. Cinco muestras no distinguen una cosa de la otra; treinta sí. Con
5 habríamos rediseñado algo que no estaba roto.

Detalle en [`5-investigacion/memoria-en-regimen.md`](5-investigacion/memoria-en-regimen.md).

### 🎉 Paso 7 completado — se ve en la pantalla

```
🎵 Chilango Blues — Mon Laferte [Chilango Blues]
```

**RF-01, RF-02, RF-03 y RF-05 cerrados.** El aparato hace lo que se pidió al
arrancar el proyecto: muestra la canción y el disco al que pertenece.

**Decisiones de diseño de la pantalla:**

- **Título** en fuente 4 blanca, hasta 2 líneas, cortando **por palabras** (no a
  mitad de palabra) y con `...` si no cabe. El corte se calcula con
  `tft.textWidth()`, midiendo de verdad en vez de contar caracteres — con una
  fuente de ancho variable, "IIIII" y "WWWWW" no ocupan lo mismo.
- **Artista** en fuente 4 verde Spotify · **álbum** en fuente 2 gris. La
  jerarquía por tamaño y color es lo que una OLED monocroma no habría permitido.
- **Solo se redibuja el texto cuando cambia la canción.** Redibujar los 240×240
  cada 5 segundos haría parpadear la pantalla sin ninguna necesidad. La barra de
  progreso sí se actualiza siempre, pero es un `fillRect` diminuto.
- **Brillo por PWM al 55%** (RNF-02), tal como se había previsto en el plano
  físico: `BLK` está en un pin con PWM justamente para esto.
- Pantallas propias para "en silencio" (RF-06) y "sin conexión" (RF-07), que
  son estados distintos y hay que poder distinguirlos.

### 🔤 Descubrimiento: las fuentes 6, 7 y 8 solo tienen números

Al probar la pantalla apareció un "6" naranja suelto donde debía decir
"Fuente 6". No era un fallo: **en TFT_eSPI las fuentes 6, 7 y 8 solo incluyen
`0-9 . : - a p m`**, porque están pensadas para relojes y medidores. De
`"Fuente 6"` solo se dibujó el `6`.

Consecuencia práctica: **la fuente 4 es la más grande con alfabeto completo** de
las integradas. Refuerza la elección del RNF-01. Para títulos más grandes habría
que usar fuentes externas (`LOAD_GFXFF`, ya habilitado).

### 🎤 Paso 8 — letras sincronizadas funcionando

```
[Letra] buscando: 4Me 4Me — Malcolm Todd (210s)
[Letra] exacta -> HTTP 200 ✅
[Letra] 30 líneas · RAM 193004 B
```

**Cómo se ve:** la línea que toca en fuente 4 blanca, centrada, con la anterior
en gris tenue arriba y la siguiente en gris abajo. Ese contexto es lo que permite
seguir la canción con la vista sin esfuerzo.

**El detalle que hace que se vea bien:** a Spotify se le pregunta cada 5 s, pero
la letra se refresca **6 veces por segundo**, calculando el progreso a partir del
reloj interno del ESP32 (`progresoBase + tiempo transcurrido`). Sin eso la letra
saltaría de golpe cada 5 segundos y quedaría fatal — y encima gastaría peticiones
de más.

### 🐛 Dos errores propios que valió la pena documentar

**1. `Transfer-Encoding: chunked`** — LRCLIB devolvía **HTTP 200** y el firmware
decía "no encontrada". La causa: LRCLIB manda la respuesta por trozos, con el
tamaño de cada uno en hexadecimal antes del contenido, y
`HTTPClient::getStream()` entrega eso **crudo**. ArduinoJson veía `1f4a{"id":...`
y fallaba. Spotify manda `Content-Length`, por eso el mismo código funcionaba ahí
y no aquí.

Se escribió `LectorChunked`, que quita las marcas al vuelo y mantiene el
streaming — sin cargar los 91 KB en memoria, que era todo el objetivo del
ADR-0003.

🔑 **Un HTTP 200 solo dice que el servidor respondió bien, no que tú lo hayas
entendido bien.** Ante un fallo con 200, sospechar del parseo, no de la petición.

**2. Macros de Arduino: `HEX` y `cli`** — dos errores de compilación seguidos con
mensajes que no señalaban el problema real. `HEX` es una macro con valor 16 (la
de `Serial.print(x, HEX)`), así que `HEX[i]` se convertía en `16[i]`. Y `cli()`
es la de deshabilitar interrupciones.

🔑 **Si un nombre corto y común da un error sin sentido, sospecha de una macro.**

Detalle en [`5-investigacion/chunked-encoding.md`](5-investigacion/chunked-encoding.md)
y [`macros-de-arduino.md`](5-investigacion/macros-de-arduino.md).

### 🔧 Cuatro arreglos tras ver el aparato funcionando

**1. Los acentos.** `León` se veía `Len`. Las fuentes integradas de TFT_eSPI solo
tienen ASCII 32–127 y descartan el resto **en silencio** — está en su código:
`if ((font>1) && (font<9) && ((uniCode<32)||(uniCode>127))) return 0;`. Y como la
`ó` ocupa dos bytes en UTF-8, desaparecía entera.

Ahora se convierten a su letra más parecida (`León` → `Leon`) **al cargar la
letra**, no al dibujarla: una vez por canción en vez de 6 veces por segundo.
Se pierde la tilde; la alternativa real serían las fuentes *smooth* `.vlw`.

Curioso: la búsqueda en LRCLIB **sí funcionaba** con acentos (`Zoé`, `México`).
El fallo era solo al dibujar, lo que despistaba.

**2. 🐛 El `...` en frases que sí cabían.** Bug propio, de una línea: el `break`
salía del bucle **sin vaciar `resto`**, y la comprobación de después lo tomaba
por texto sobrante. 🔑 Un `break` a media función se salta lo que el resto del
código da por hecho.

**3. Recortar es peor que encoger.** Ahora hay tres escalones: fuente 4 en hasta
3 renglones → fuente 2 → y solo entonces `...`. Perder palabras de la canción es
peor que perder tamaño de letra.

**4. 💥 Desbordamiento de pila.** `Stack canary watchpoint triggered (loopTask)`.
Un `Letras tmp;` local pesa **6.840 bytes** y la pila de Arduino son **8.192**.
Lo desconcertante: había **186 KB libres** — pero de *heap*, que es otro sitio.
🔑 `ESP.getFreeHeap()` no dice nada sobre la pila.

El "canario" es un centinela al final de la pila: si aparece pisado, el sistema
reinicia **a propósito** en vez de seguir con memoria corrupta. El reinicio era
la parte buena.

### ⚡ Precarga de la siguiente canción

```
[Precarga] siguiente: Price I'll Pay — Thee Sacred Souls
[Precarga] lista (41 líneas)
🎵 Price I'll Pay — Thee Sacred Souls
[Letra] ⚡ desde precarga (41 líneas)      <- cero espera
```

Dos mejoras combinadas:

- **Sondeo adaptativo:** cada 5 s normalmente, **cada 1 s en los últimos 15
  segundos** de la canción. El ESP32 sabe la duración y el progreso, así que
  puede predecir el final.
- **Precarga:** a los 8 s de empezar una canción, pregunta a Spotify qué sigue
  (`/me/player/queue`) y descarga esa letra por adelantado.

Usa `user-read-playback-state`, pedido en el ADR-0010 sin saber para qué. Pedir
todos los permisos de una vez resultó buena idea.

**Latencia:** se compensa la mitad del viaje de red (`instanteBase = millis() -
viaje/2`) más un ajuste manual `AJUSTE_LETRA_MS = 400`, para afinar mirando.

Detalle en [`5-investigacion/limitaciones-de-pantalla.md`](5-investigacion/limitaciones-de-pantalla.md)
y [`precarga-de-letras.md`](5-investigacion/precarga-de-letras.md).

### 🎵 Dos arreglos más: notas musicales y silencios

**El símbolo `♪` dejaba un hueco.** Los `.lrc` lo usan para marcar los tramos sin
voz (intros, solos, puentes). Al no existir en las fuentes, se convertía en un
espacio en blanco sin explicación.

Ahora se traduce a `~`, la pantalla lo reconoce, y **dibuja una nota musical con
primitivas gráficas** — cabeza, palo y banderita. Como la fuente no la trae, se
pinta a mano.

**La zona de la letra se quedaba vacía en los silencios.** Y esto era una
decisión de diseño mía, mal tomada. El código decía:

```cpp
if (idx < 0 || !letras.lineas[idx].texto[0]) {
  return;    // "si nadie canta, no hay nada que mostrar"
}
```

Suena lógico y en la práctica es horrible: durante una intro puedes pasar 20
segundos mirando un hueco negro **sin saber si el aparato se colgó**.

🔑 **Una pantalla vacía es indistinguible de un aparato roto.** Es exactamente la
razón por la que existen el RF-06 y el RF-07 — el aparato nunca debe dejar al
usuario adivinando. Lo tenía escrito en los requerimientos y aun así lo hice mal
aquí.

Ahora, durante un silencio se ve:

```
        ♪ ♪            <- dibujado: es un tramo sin voz, no un fallo
   la frase que viene   <- en gris, debajo
```

Y la búsqueda de contexto **salta los silencios**: busca la anterior y la
siguiente *con texto*, en vez de mostrar huecos.

### ✅ Acentos de verdad: una fuente propia

La solución anterior (quitar las tildes: `León` → `Leon`) se quedó corta. Al
revisar el código de TFT_eSPI apareció que **sí** admite fuentes personalizadas en
formato Adafruit GFX, y esas llegan hasta el código 255 — donde vive el español:

```c
if ((c >= pgm_read_word(&gfxFont->first)) && (c <= pgm_read_word(&gfxFont->last)))
```

Se escribió `tools/gen-fuente.py`: coge una Arial del sistema, dibuja cada
carácter del 32 al 255 y los empaqueta en ese formato. **8.4 KB** las dos fuentes
(19 px y 13 px).

Resultado verificado con casos reales: `León Larregui`, `Como Tú`, `Cariño`,
`The Marías`.

Dos detalles que hubo que atar: el texto se convierte a **Latin-1** (1 byte por
letra) al cargar la letra, y hay que **apagar el decodificador UTF-8 de la
librería** — si no, vería el byte `0xF1` de la ñ y creería que empieza una
secuencia de varios bytes.

### 🎵 Notas centradas y sondeo más ágil

- Las notas de los silencios ahora van **centradas y en blanco**, ocupando el
  sitio de la frase que se cantaría, con la siguiente en gris debajo.
- Se pregunta a Spotify **cada 2 s** en vez de cada 5, para notar antes las pausas
  y los saltos de canción. Con **freno automático**: si Spotify responde `429`,
  el intervalo sube solo (hasta 8 s).

🔑 **Cuando un límite no está documentado, el diseño tiene que autorregularse.**
Spotify no publica el suyo para el modo desarrollo, así que adivinar un número
fijo sería apostar. Reaccionar a su respuesta, no.

### 🎨 La playlist arriba, y fuente estilo Spotify

**La barra de arriba muestra ahora la playlist** en vez de "SONANDO", que era
redundante: si suena o no ya se ve en el color de la barra de progreso.

El endpoint de "reproduciendo ahora" trae un campo `context` con el origen, pero
**solo el identificador, no el nombre**. Hay que pedirlo aparte a
`/v1/playlists/{id}?fields=name` — y se guarda, porque mientras sigas en la misma
playlist no cambia. Ese `?fields=name` es importante: sin él, Spotify mandaría la
playlist entera con todas sus canciones.

Verificado: `[Contexto] playlist -> "for you" (HTTP 200)`.

⚠️ Si alguna playlist privada no resuelve el nombre, hace falta el permiso
`playlist-read-private`. Ya se añadió al perfil `esp32` del script de OAuth;
basta con volver a correrlo.

Se añadieron también **símbolos de play y pausa dibujados a mano** (triángulo y
dos barras) a la derecha de la barra, porque ninguna fuente los trae.

**Fuente cambiada a Avenir Next Demi Bold**, que es lo más parecido en macOS a la
*Circular* de Spotify (propietaria, no distribuible). Misma familia geométrica.
8.8 KB las dos.

### 📊 Sobre el "tiempo real"

Se documentó el análisis completo en
[`5-investigacion/cuan-en-tiempo-real.md`](5-investigacion/cuan-en-tiempo-real.md).
Resumen: la barra de progreso y la letra **ya son tiempo real** porque se calculan
con el reloj interno. Lo que cuesta son los *eventos* (pausa, saltos), porque la
API de Spotify es solo de consulta: no hay forma de que avise.

🔑 El techo real no es el límite de peticiones, sino que **cada consulta abre una
conexión TLS nueva** (~1 s y ~50 KB de pico). A 2 segundos de intervalo, medio
tiempo se va en negociar cifrado.

### ✅ CORRECCIÓN: no había fuga de memoria

Durante la sesión avisé de que la RAM bajaba sin parar (193 → 186 → 181 → 176 →
173 KB) y podía ser una fuga. **Era una falsa alarma, por dos errores míos de
medición:**

**1. Comparaba firmwares distintos.** Cada número venía de una versión con más
cosas dentro: la precarga añadió 13,7 KB de buffers, las fuentes propias 8,8 KB,
el sprite de la barra 8 KB. Cada número era correcto para su versión; la
*tendencia* no existía. 🔑 Es como pesarse cambiando de báscula.

**2. Medía en el momento equivocado del ciclo.** Esos números se imprimían justo
después de descargar una letra, con la conexión TLS aún ocupando memoria. Al
medir en reposo salen 218 KB.

Se añadió un **latido de diagnóstico** cada 30 s y se midió 5 minutos con el
firmware completo:

```
t= 30s   libre 219.876   minimo 97.104   bloque mayor 110.580
t=301s   libre 218.712   minimo 97.076   bloque mayor 110.580

primera mitad -1.136 B    segunda mitad -28 B    fragmentacion: cero
```

Las últimas seis muestras varían **30 bytes entre sí**. Eso es ruido, no
tendencia. **RNF-06 cerrado.**

Y el número que de verdad importa es el **mínimo histórico: 97 KB** — el peor
instante, durante el handshake TLS. Sigue holgado.

🔑 **Antes de diagnosticar una tendencia, asegúrate de que las muestras son
comparables.** Mismo binario, mismo punto del ciclo, suficientes muestras.
Detalle en [`5-investigacion/memoria-en-regimen-2.md`](5-investigacion/memoria-en-regimen-2.md).

### ⛔ Al saltar de canción durante una búsqueda, se atascaba

`buscar()` encadenaba hasta 5 peticiones (~8 s) **sin comprobar nada entre
medias**, y con `precargarSiguiente()` el núcleo de red podía pasar **más de 10
segundos** sin preguntarle nada a Spotify. Como el cambio de canción solo se
detecta preguntando, durante ese rato el aparato era **ciego**.

No había ninguna cancelación: una vez empezaba la búsqueda, iba hasta el final.

**Solución: la búsqueda pasa de bloque a cola de pasos**, y el bucle de red hace
un paso por vuelta, consultando a Spotify entre medias:

```
consultar() → paso exacta → consultar() → paso candidatos → consultar() → ...
```

Si la canción cambió: `[Letra] ⛔ cancelada: cambió la canción`. Retraso máximo de
~10 s a ~2,4 s.

🔑 **La forma más simple de "cancelar" casi nunca es abortar de verdad: es no
encadenar.** Hacer un trozo, mirar si sigue teniendo sentido, hacer el siguiente.

Tres detalles que hicieron falta: el resultado solo se publica si la canción no
cambió; la precarga no se lanza si hay una búsqueda en curso; y tras la precarga
se fuerza una consulta inmediata.

### 🏷️ El contexto solo funcionaba con playlists

Descuido mío: solo implementé la búsqueda del nombre para playlists. Un álbum
mostraba literalmente "Album" y un artista, "Reproduciendo".

Ahora: el **álbum** usa el nombre que ya viene en la respuesta de "reproduciendo
ahora" (cero peticiones extra), el **artista** se pide a `/v1/artists/{id}`, y
cuando **no hay contexto** se muestra el álbum en vez de un "Reproduciendo" que no
dice nada. Verificado: `[Contexto] HTTP 200 -> "Liked Songs"`.

### 📐 Mapa de memoria

Documentado en [`6-esquemas/mapa-de-memoria.md`](6-esquemas/mapa-de-memoria.md):
flash contra RAM, las tres zonas de la RAM (estático, pilas, montón), qué se
reserva en el arranque y qué se pide y devuelve sobre la marcha, y por qué las
letras usan un tamaño fijo que **nunca se libera porque nunca se reserva**.

### ⏱️ El reloj daba saltos + transición entre frases

**El tiempo transcurrido retrocedía.** Dos causas:

1. **La corrección de golpe.** Entre consultas el progreso se calcula con el reloj
   interno, y nunca coincide exacto con lo que reporta Spotify. Al llegar la
   respuesta se sobrescribía de golpe: si íbamos 300 ms adelantados, el número
   **retrocedía** (`1:24 → 1:23 → 1:24`).
   Ahora se distingue: diferencia **> 1,5 s** = alguien movió la canción, se
   resincroniza de golpe; diferencia pequeña = deriva, se corrige **un 20% cada
   vez**. 🔑 Es lo que hacen los relojes de red: no se ajustan de un tirón, se
   arrastran. Un salto atrás se nota mucho más que un error de 300 ms.

2. **El parpadeo.** El bloque de tiempos se borraba y redibujaba 10 veces por
   segundo aunque el texto fuera idéntico. Ahora solo se repinta cuando cambia.

**La transición entre frases** es un fundido de gris oscuro a blanco en ~170 ms,
y **no cuesta memoria**. El enfoque obvio (un buffer para mezclar) serían 53 KB
para la zona de la letra. No hace falta: las letras son de **un solo bit**, así
que dibujar el mismo texto encima con un gris más claro solo aclara esos píxeles.
Sin borrado no hay parpadeo.

🔑 **Un efecto visual no siempre necesita un buffer.** Si el contenido no cambia
entre fotogramas —solo su color, y siempre hacia más claro— basta con dibujar
encima.

**Y un arreglo que salió observando:** una entrada de LRCLIB puede existir y dar
`HTTP 200` pero **sin letra sincronizada**. Rendirse con el primer candidato
desperdicia otras versiones que sí la tienen. Ahora se prueban los **tres**
mejores por duración.

### 🧠 Las pausas del desplazamiento: eran la RED, no el texto

Tras arreglar el bucle del texto, las pausas seguían. La causa real era otra:

| Llamada | Bloquea |
|---|---|
| `consultar()` | ~1.200 ms |
| `precargarSiguiente()` | ~3.000 ms |

Ambas se hacían **dentro del mismo bucle que dibuja**. El desplazamiento avanza 1
píxel cada 65 ms, así que una consulta de 1.200 ms se comía **18 píxeles** — y se
consultaba cada 2 segundos.

🔑 **Un síntoma puede tener varias causas.** El arreglo del bucle era correcto y
no sobra; simplemente había una segunda causa detrás, más grande.

**Solución: usar los dos núcleos del ESP32**, que hasta ahora solo usaba uno.

```
Núcleo 0  ->  internet (puede tardar lo que quiera)
Núcleo 1  ->  dibujar, y solo dibujar (nunca se bloquea)
```

El reparto no es arbitrario: **todo el dibujo se queda en un núcleo a propósito**,
porque la librería de pantalla no soporta que dos dibujen a la vez. Eso obligó a
cambiar la función que consultaba Spotify — antes también pintaba; ahora solo
actualiza datos y levanta una bandera.

**El mutex** protege los datos compartidos. Y un detalle que importa: la descarga
de la letra se hace **fuera** del mutex. Tarda segundos, y retener el turno tanto
tiempo dejaría al otro núcleo esperando — volviendo al problema original. Se
descarga sobre un buffer que nadie mira, y solo el intercambio final ocurre
dentro del turno.

Coste: 16 KB de pila para la tarea nueva. El mínimo histórico de RAM baja de 97 a
71 KB, porque ahora red y dibujo pueden coincidir. Sigue holgado.

Detalle en [`5-investigacion/dos-nucleos.md`](5-investigacion/dos-nucleos.md).

### 💜 Emojis, descripción de playlist y barra que se desplaza

**Los emojis SÍ se pueden mostrar, pero no como imágenes.** Un emoji a 20×20 px
a color son ~800 bytes, y existen más de 3.700: guardarlos todos serían ~3 MB, y
el ESP32 tiene 4 MB de flash **en total** con 1,1 MB ya ocupados.

Pero un corazón son **dos círculos y un triángulo**. Eso es código, no datos:
~120 bytes de programa para **todos** los corazones, porque el color es un
parámetro. 💜💙💚💛🖤🤍 son el mismo dibujo.

🔑 **La diferencia entre guardar la respuesta y guardar la receta.** Una imagen
es la respuesta ya hecha, una por cada caso. El dibujo es la receta: sirve para
todos.

Se dibujan corazones (7 colores), destellos, llamas y notas. Cualquier otro emoji
queda como un **punto discreto** — un hueco vacío parece un fallo, un punto se lee
como "aquí había algo".

**Detalle que casi se escapa:** `❤️` no es un carácter, son dos: el corazón más un
"selector de variante" **invisible**. Si ese invisible se convirtiera en espacio,
cada emoji dejaría un hueco fantasma al lado. Se descartan explícitamente.

Verificado con una playlist real: `Alo 💜` resuelto y el corazón dibujado en
morado.

**La descripción de la playlist** se pide junto al nombre y va en gris tras un
guion. ⚠️ Viene con **HTML**: entidades (`&#x27;`) y a veces etiquetas `<a>`. Sin
limpiarla se leería `&#x27;` literalmente.

**Si no cabe, se desplaza** — con un *sprite*, que es un trozo de pantalla que se
dibuja en memoria y se vuelca de golpe. Dibujando directo se vería el borrado
entre fotogramas. Cuesta 8 KB y es una de las razones del ADR-0008.

Detalle en [`5-investigacion/emojis-e-iconos.md`](5-investigacion/emojis-e-iconos.md).

### 🔜 En qué me quedé — siguiente sesión

**Verificaciones pendientes de mirar:**
- ¿Se ve bien el corazón morado de la playlist `Alo 💜`?
- ¿La descripción se desplaza suave o da tirones?
- ¿La fuente Avenir Next convence, o probamos Futura?
- **La letra: ¿va sincronizada con lo que se canta?** Es lo único que no puedo
  comprobar yo
- ¿Qué pasa con una canción sin letra en LRCLIB? Debe decir "sin letra" y seguir
  mostrando todo lo demás
- Pausar Spotify → debe aparecer "EN PAUSA" y la barra ponerse gris (RF-06)
- Apagar el WiFi → debe aparecer "Sin conexión" en rojo (RF-07)
- Ver la pantalla a oscuras y ajustar el brillo (RNF-02)

**Lo siguiente de verdad:** las letras (RF-04). Antes de programar nada, la
prueba barata: buscar en LRCLIB 5 canciones que escuches de verdad. Si tres no
aparecen, hay que replantear el requerimiento (S-05).

**Mejora pendiente:** títulos largos se cortan con `...`. Un desplazamiento
horizontal se vería mucho mejor, y `TFT_eSPI` trae *sprites* para hacerlo sin
parpadeo — fue una de las razones del ADR-0008.

### 📜 Historial anterior

**Paso 7: la pantalla.** Es lo único que falta para cerrar RF-01, RF-02 y RF-03
— los datos ya están, solo hay que dibujarlos. Toca:

1. Cablear la GMT130 siguiendo [`6-esquemas/conexiones-fisicas.md`](6-esquemas/conexiones-fisicas.md)
2. Decidir el ADR-0008 (`TFT_eSPI` vs `Adafruit_ST7789`)
3. Diseñar cómo se ve: qué va grande, qué chico, qué colores

**Pendientes menores:**
- Prueba de resistencia de 24 h para cerrar el RNF-06 del todo.
- Timeout en la sincronización NTP: hoy, si el router bloquea el puerto 123, el
  arranque se queda esperando para siempre.
- S-05: probar a mano en LRCLIB 5 canciones que escuches, para saber si el RF-04
  (letras) es viable antes de invertirle tiempo.

### 📜 Historial anterior de la sesión

**Paso 4 — OAuth.** Falta terminar de crear la app en el dashboard de Spotify
(con ese Redirect URI) y hacer el baile de autorización una vez desde la Mac,
para obtener el `refresh_token`.

**Riesgo #1 actual: los certificados raíz TLS** (futuro ADR-0009). El spike usó
`setInsecure()`, que no valida el certificado — válido para medir memoria, no
para el firmware final. Es el único riesgo donde el aparato deja de funcionar
solo, un día, sin que nadie haya tocado nada.

**En paralelo, sin depender de nada:** armar el cableado de la pantalla siguiendo
[`6-esquemas/conexiones-fisicas.md`](6-esquemas/conexiones-fisicas.md).

### Material declarado

- 2× ESP32-S NodeMCU genérico, USB-C (C-001, C-002)
- 1× ESP32 DEVKIT v1, mini-USB (C-003)
- 4× TFT IPS 1.3" GMT130-v1.0, 240×240 (C-004)

**La pantalla resultó ser mucho mejor de lo supuesto.** El plan asumía una OLED
de 0.96" (128×64, monocromática). La GMT130 es 240×240 a color: caben ~15 líneas
de ~30 caracteres contra ~4 líneas de ~20. Eso mueve el RF-04 (letras) de
"prácticamente inviable" a "hay que probarlo". Se escribió el
[ADR-0004](1-decisiones/ADR-0004-pantalla-gmt130.md).

### Decisión que sigue pendiente

- [ADR-0002](1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md) — ¿el ESP32
  habla directo con Spotify, o pasa por un puente en la Mac? Sigue sin decidir.
