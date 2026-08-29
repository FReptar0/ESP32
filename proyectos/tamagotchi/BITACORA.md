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

### 🔜 En qué me quedé — siguiente sesión

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
