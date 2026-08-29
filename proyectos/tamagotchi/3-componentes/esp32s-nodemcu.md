# Componente — ESP32-S NodeMCU (genérico)

- **ID en inventario:** C-001
- **Categoría:** Microcontrolador
- **Fecha de la ficha:** 2026-08-29
- **Estado de la ficha:** ⚠️ **Incompleta** — la placa aún no responde, faltan los
  datos que solo se pueden confirmar con ella conectada
- **Datasheet:** [ESP32-WROOM-32 (Espressif)](https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf)

---

## Qué hace, en una frase

Es una computadora diminuta con **WiFi y Bluetooth integrados**, que se programa
por USB y tiene pines para conectarle cosas (pantallas, sensores, botones).

Para el Tamagotchi es el cerebro: se conecta a internet, le pregunta a Spotify
qué estás oyendo, y dibuja el resultado en la pantalla.

## Sobre el nombre "ESP32-S NodeMCU"

Vale la pena aclararlo porque genera confusión y afecta lo que hay que comprar:

- **ESP32** — la familia de chips de Espressif.
- **ESP32-WROOM-32** — el módulo concreto que traen la mayoría de las placas
  genéricas: el chip + memoria + antena, todo en una latita metálica.
- **NodeMCU** — el formato de la placa (el rectángulo con pines a los lados y un
  puerto USB). Originalmente era para el ESP8266 y el nombre se heredó.
- **"ESP32-S"** — ⚠️ **ojo aquí.** En las placas genéricas suele ser solo una
  marca comercial del vendedor, sinónimo de "ESP32 en formato NodeMCU". **No es
  lo mismo que el ESP32-S2 ni el ESP32-S3**, que son chips distintos y más nuevos.

Esto importa porque al configurar PlatformIO hay que decirle exactamente qué
placa es. Elegir mal implica que el código compila pero no funciona, o que los
pines no corresponden. **Se confirma cuando la placa responda.**

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

## Estado actual: ❌ no detectado

**2026-08-29** — La Mac no ve la placa. Diagnóstico completo en
[`../BITACORA.md`](../BITACORA.md) y en
[`inventario/LOG.md`](../../../inventario/LOG.md).

Resumen: no aparece ningún chip USB-Serie en el árbol USB del sistema, y no existe
ningún `/dev/cu.usbserial*`. Todo el hardware USB visible pertenece al dock
DisplayLink.

## Notas y sorpresas

*(Se va llenando conforme lo usemos.)*
