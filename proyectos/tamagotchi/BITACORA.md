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

### Lo que se avanzó después

- `platformio.ini` escrito con la placa ya confirmada (`board = esp32dev`).
- Primer firmware: parpadeo del LED + info del chip + **escaneo de redes WiFi**
  (que de paso verifica el supuesto S-06, si tu red tiene banda de 2.4 GHz).
- Fase 6 desbloqueada: plano físico completo en
  [`6-esquemas/conexiones-fisicas.md`](6-esquemas/conexiones-fisicas.md).

### 🔜 En qué me quedé — siguiente sesión

1. Ver el resultado del escaneo WiFi → resuelve el supuesto S-06.
2. Armar el cableado de la pantalla siguiendo el plano físico.
3. **Sin hardware, en paralelo:** verificar el supuesto S-02 (que la API de
   Spotify siga dando "reproduciendo ahora"). Sigue siendo el riesgo #1 y no
   depende de la placa.

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
