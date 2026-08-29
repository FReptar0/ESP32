# Componente — ESP32-S NodeMCU (genérico)

- **ID en inventario:** C-001
- **Categoría:** Microcontrolador
- **Fecha de la ficha:** 2026-08-29
- **Estado de la ficha:** ✅ **Confirmada contra la placa física** (2026-08-29)
- **Datasheet:** [ESP32-WROOM-32 (Espressif)](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf)

---

## Qué hace, en una frase

Es una computadora diminuta con **WiFi y Bluetooth integrados**, que se programa
por USB y tiene pines para conectarle cosas (pantallas, sensores, botones).

Para el Tamagotchi es el cerebro: se conecta a internet, le pregunta a Spotify
qué estás oyendo, y dibuja el resultado en la pantalla.

## ✅ Identidad confirmada

Interrogada directamente con `esptool` el 2026-08-29:

| Dato | Valor |
|---|---|
| Chip | **ESP32-D0WD-V3**, revisión v3.1 |
| Núcleos | 2, a 240 MHz |
| Radio | Wi-Fi 2.4 GHz + Bluetooth |
| Memoria flash | **4 MB** |
| Cristal | 40 MHz |
| **Dirección MAC** | `84:1F:E8:2C:3E:C4` |
| Chip USB-Serie | **CP2102** (Silicon Labs) |
| Puerto en macOS | `/dev/cu.usbserial-0001` |
| Perfil de PlatformIO | `board = esp32dev` |

> **La MAC es el número de serie de la placa.** Es única e irrepetible, así que
> sirve para distinguir tus placas idénticas entre sí. Cuando identifiquemos las
> otras dos, se anota aquí la de cada una.

### Sobre el nombre "ESP32-S NodeMCU"

Confirmado lo que se sospechaba: el **"-S" era marca comercial del vendedor**, no
un chip distinto. El chip real es un **ESP32-D0WD-V3**, o sea el ESP32 clásico —
**no** es un ESP32-S2 ni un ESP32-S3, que son familias diferentes.

Importa porque es lo que determina el `board = esp32dev` del `platformio.ini`.

### Sobre el CP2102

Es el chip que traduce entre el USB de la Mac y el puerto serie del ESP32. Es de
Silicon Labs, y **macOS lo reconoce sin instalar ningún driver**.

Un detalle práctico confirmado: `esptool` terminó con `Hard resetting via RTS
pin`, lo que significa que **el auto-reset funciona**. Traducción: al cargar
código, la placa entra sola en modo programación. **No hay que apretar el botón
BOOT** — que es una molestia clásica en placas donde esto no funciona.

## Alimentación

| Concepto | Valor | Nota |
|---|---|---|
| Alimentación por USB | 5 V | La placa tiene un regulador que baja a 3.3 V |
| Alimentación por pin `VIN` | 5 V | Entrada alternativa |
| Alimentación por pin `3V3` | 3.3 V | ⚠️ Se salta el regulador. Si metes 5 V aquí, mueres |
| **Voltaje lógico** | **3.3 V** | ⚠️ El número que importa |
| Consumo típico | ~80–160 mA | Con WiFi activo |
| Consumo pico | **hasta ~500 mA** | ⚠️ Al transmitir por WiFi |

> ### ⚠️ Los pines del ESP32 son de 3.3 V y no toleran 5 V
>
> Este es el error que más placas quema. Si conectas la salida de un sensor de
> 5 V directo a un pin del ESP32, lo dañas — a veces al instante, a veces poco a
> poco hasta que empieza a fallar de forma rara.
>
> Cuando haya que mezclar 3.3 V y 5 V, hace falta un **conversor de nivel lógico**
> (level shifter), que es una piecita barata que traduce entre los dos voltajes.
> Yo te aviso cada vez que aplique.

> ### ⚠️ El pico de 500 mA explica un bug clásico
>
> Un ESP32 consume poco en reposo, pero al encender la radio WiFi pega un jalón
> de corriente. Si la fuente no lo aguanta (un puerto USB flojo, un cable largo y
> delgado, un hub sobrecargado), el voltaje cae y **la placa se reinicia sola**.
>
> El síntoma es desesperante: el aparato se reinicia justo cuando se conecta a
> internet, y parece un bug del código. No lo es. Es la fuente.
>
> Si el proyecto empieza a reiniciarse sin explicación, esto es lo primero que hay
> que descartar.

## Cómo se comunica

- **Con la Mac:** por **UART** a través de un chip conversor USB-Serie que trae la
  placa. Ese chip es el que crea el `/dev/cu.usbserial-XXXX` en macOS.
  - ⚠️ **Cuál chip trae, sin confirmar.** Los dos comunes son el **CP2102**
    (Silicon Labs) y el **CH340** (WCH). macOS moderno reconoce ambos sin
    instalar nada. Se confirma en cuanto enumere.
- **Con los periféricos:** I2C, SPI, UART, PWM, ADC. Prácticamente cualquier pin
  se puede reasignar por software (a diferencia del Arduino clásico).
- **Con internet:** WiFi **802.11 b/g/n en 2.4 GHz**.
  - ⚠️ **No se conecta a redes de 5 GHz.** Y como muchos routers usan el mismo
    nombre para las dos bandas, este es un clásico de "no conecta y no sé por qué".

## Conexiones

⚠️ Pendiente hasta definir la pantalla. Ver
[`6-esquemas/arquitectura-end-to-end.md`](../6-esquemas/arquitectura-end-to-end.md).

## Precauciones

- ⚠️ **Nunca 5 V en un pin de datos.** Es lo que más placas mata.
- ⚠️ **Nunca 5 V en el pin `3V3`.** Ese pin va después del regulador.
- ⚠️ **Fuente que aguante 500 mA de pico**, o se reiniciará solo.
- ⚠️ **Hay pines que no se pueden usar libremente.** Algunos deben estar en cierto
  nivel al arrancar o la placa no enciende; otros son solo de entrada; algunos
  están conectados a la memoria interna y usarlos hace que la placa no arranque.
  La lista exacta depende del modelo → se documenta al confirmarlo.
- ⚠️ **Desconecta el USB antes de recablear.** Suena obvio; se olvida siempre.

## Cómo probarlo aislado

Antes de conectarle absolutamente nada:

1. Conectar solo el cable USB a la Mac. Nada más en los pines.
2. Verificar que aparece un puerto: `ls /dev/cu.*` debe mostrar un
   `cu.usbserial-*` o `cu.wchusbserial-*` nuevo.
3. Cargar el ejemplo "Blink" (parpadear el LED de la placa).
4. Abrir el monitor serie e imprimir un `Hola` desde el código.

**Cómo se ve el éxito:** el LED parpadea y el `Hola` aparece en el monitor. Con
eso queda demostrado que la placa vive, que el USB comunica en ambos sentidos, y
que la cadena de compilar → cargar funciona completa.

Solo después de eso tiene sentido conectar algo.

## ✅ Estado actual: funcionando

**2026-08-29** — Detectada, identificada y respondiendo. La cadena
`editar → compilar → cargar → ejecutar` quedó verificada.

### La causa del bloqueo: cables USB-C de solo carga

Costó cuatro rondas de diagnóstico. Vale la pena dejar por qué, porque es la
trampa más común y la vas a volver a encontrar.

**Lo que pasaba:** los cables C-a-C que se probaron primero **llevan corriente
pero no datos**. Por fuera son idénticos a uno normal. El LED de la placa
encendía —así que "obviamente estaba conectada"— pero la Mac no veía absolutamente
nada.

**La pista que lo resolvió:** el LED encendido + cero dispositivos en el árbol
USB. Esa combinación solo puede significar una cosa: **la corriente llega y los
datos no.** Si hubiera sido falta de driver, el aparato igual habría aparecido
como "dispositivo no reconocido". No apareció nada en absoluto.

**Cómo se ve el mismo síntoma con causas distintas:**

| Qué observas | Qué significa |
|---|---|
| LED encendido + nada en el árbol USB | 🎯 **Cable de solo carga** ← era esto |
| Ni LED ni nada | No llega corriente: cable roto, puerto muerto o placa dañada |
| Aparece en el árbol pero sin puerto `/dev/cu.*` | Falta el driver (raro en macOS moderno) |
| Aparece y hay puerto, pero `esptool` no conecta | Problema de auto-reset: hay que apretar BOOT |

**Lección para el futuro:** ante cualquier "no me detecta la placa", el primer
paso es probar ese mismo cable con un celular. Si la compu no ve el celular, el
cable no pasa datos, y no hay nada más que investigar.

> 💡 Se descartó también la hipótesis de las resistencias CC (que muchas placas
> ESP32 con USB-C omiten): esta placa enumeró conectada **directo al puerto
> USB-C** de la Mac, lo que demuestra que sí negocia bien. Era el cable.

> ### ⚠️ Las resistencias que le faltan a muchas placas ESP32 con USB-C
>
> **Esta es la hipótesis principal de por qué no se detecta.**
>
> El estándar USB-C exige que todo aparato que reciba corriente lleve **dos
> resistencias de 5.1 kΩ** en unas líneas llamadas **CC** (*Configuration
> Channel*). Sirven para que el aparato le diga a la computadora *"aquí hay algo,
> y necesita energía"*.
>
> Un montón de placas ESP32 baratas con USB-C **no traen esas resistencias**. Los
> fabricantes las omiten porque en su momento probaron con un cable USB-A → USB-C
> y funcionó — y funciona, porque un puerto USB-A **siempre** entrega 5 V sin
> preguntar nada.
>
> Pero un puerto USB-C **no entrega nada hasta que el otro lado se identifica**.
> Sin las resistencias, la placa nunca se identifica. Resultado:
>
> | Cable | Qué pasa |
> |---|---|
> | USB-A → USB-C | ✅ Funciona perfecto |
> | USB-C → USB-C | ❌ **Muerta. Ni corriente, ni enumeración, ni nada** |
>
> Y el síntoma coincide **exactamente** con lo observado: cero dispositivos en el
> árbol USB, como si no hubiera nada conectado. Desde el lado de la Mac, en efecto,
> no hay nada conectado.
>
> Tu Mac es Apple Silicon: sus puertos son USB-C, así que lo natural es haber
> agarrado un cable C-a-C. Encaja.
>
> **Cómo confirmarlo:** conectar la **C-003 (DEVKIT v1)**, que tiene mini-USB y
> por tanto usa por fuerza un cable A-a-mini. Si esa sí aparece, la hipótesis
> queda confirmada y el arreglo cuesta un cable, no una placa.

## Notas y sorpresas

*(Se va llenando conforme lo usemos.)*
