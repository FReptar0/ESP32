# ADR-0007 — Un joystick analógico de 2 ejes como único control

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-30
- **Relacionado con:** RF-08, RF-09, RF-10, RF-11, RF-14, RF-15 · [C-005](../../../inventario/INVENTARIO.md)

---

## Contexto

El aparato mostraba información pero no se podía tocar. Los requerimientos de
control (RF-08 pausar, RF-09 saltar, RF-10 volumen) estaban en "Podría" y sin
hardware, y aparecieron cuatro pantallas entre las que hay que navegar además de
elegir playlist.

Hay un joystick analógico de 2 ejes (tipo KY-023) disponible: dos potenciómetros
y un pulsador. Cinco pines: `GND`, `+5V`, `VRx`, `VRy`, `SW`.

Las restricciones que pesan:

- **El ESP32 es de 3,3 V** y sus pines no toleran 5 V.
- **El WiFi está encendido siempre**, y el driver de radio se apropia del **ADC2**.
- Ya hay cinco pines ocupados por la pantalla, entre ellos el `GPIO32`.
- El aparato es un **regalo**: quien lo reciba no va a leer instrucciones.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- El módulo es pasivo: dos potenciómetros y un botón. No lleva lógica.
- Los pines `GPIO34`–`GPIO39` son **solo de entrada** — inútiles para la pantalla,
  perfectos para leer.
- El permiso `user-modify-playback-state` ya estaba pedido en el
  [ADR-0010](ADR-0010-permisos-de-spotify.md), sin saber todavía para qué.

**Lo que asumimos sin verificar (y luego se midió):**

- ~~Que el reposo estaría cerca del centro (2048).~~ **Falso:** esta unidad
  reposa en ~1750, unas 300 cuentas por debajo. Un firmware que supusiera 2048
  creería que la palanca está permanentemente empujada.
- ~~Que `VRx` sería el eje horizontal.~~ **Falso:** los rótulos son los ejes *del
  módulo* y dependen del montaje. Aquí están cruzados.
- ~~Que el recorrido sería simétrico.~~ **Falso:** 2358 cuentas hacia un lado y
  1737 hacia el otro.

**Lo que quedó pendiente de averiguar:**

- Si el reposo deriva con la temperatura a lo largo de semanas.

---

## Decisión

**Un joystick analógico de 2 ejes, alimentado a 3,3 V, con los dos ejes en pines
del ADC1 y el pulsador en un pin con pull-up interno.**

| Pin | ESP32 | Función |
|---|---|---|
| `+5V` | **`3V3`** | ⚠️ Nunca a 5 V |
| `VRx` | `GPIO34` | Eje **vertical** ↕ — cambiar de modo |
| `VRy` | `GPIO35` | Eje **horizontal** ↔ — canción anterior / siguiente |
| `SW` | `GPIO33` | Pausa · seleccionar |

Y tres decisiones de software que van con ella:

1. **El reposo se mide al arrancar**, promediando 64 lecturas. No se clava en el
   código: otra unidad daría otro número.
2. **Cada eje se normaliza con su propio recorrido**, a una escala de −1 a +1.
   Cuando el hardware es asimétrico, la simetría se pone en el software.
3. **Histéresis**: una línea para disparar (0,55) y otra más abajo para volver a
   armarse (0,25). Con un solo umbral, el ruido dispararía ráfagas.

### Y una decisión de NO hacer: el volumen (RF-10) queda descartado

Los dos ejes están comprometidos —horizontal para canciones, vertical para
modos— y no queda por dónde meter el volumen sin pisar algo. Meterlo con un
gesto extra (mantener pulsado y mover) lo haría **imposible de descubrir sin
instrucciones**, que es justo lo que un regalo no puede permitirse.

Si algún día se quiere volumen, hace falta hardware de entrada adicional.

## Consecuencias

**A favor:**

- Un solo componente resuelve seis requerimientos.
- Usa `GPIO34` y `GPIO35`, que estaban descartados para todo lo demás por ser
  solo de entrada. No quita ningún pin útil.
- Consumo despreciable: ~0,33 mA. No hay que revisar la alimentación.

**En contra:**

- **Sin volumen**, y no hay forma de añadirlo sin más hardware.
- Los rótulos del módulo no coinciden con los ejes reales del montaje: si algún
  día se remonta girado, hay que volver a derivar el mapeo. Está explicado en
  [`conexiones-joystick.md`](../6-esquemas/conexiones-joystick.md).
- Es un componente analógico barato: cada unidad tiene su desviación, y por eso
  el firmware **tiene** que calibrar al arrancar.

## Alternativas descartadas

| Alternativa | Por qué no |
|---|---|
| **Botones sueltos** | Harían falta cinco o seis para cubrir modos + transporte + selección, y cinco pines más. La pantalla ya ocupa cinco |
| **Encoder rotatorio** | Bueno para recorrer listas y para volumen, pero no da direcciones. Habría que sumarle botones |
| **Táctil capacitivo del ESP32** | No necesita pines extra, pero es propenso a falsos toques y no da una sensación clara de "he pulsado". En un regalo, la respuesta física importa |
