# Inventario general de componentes

> **Este archivo es de uso general.** Todos los proyectos leen de aquí.
> Ningún proyecto tiene su propio inventario: si algo se usa en un proyecto,
> se marca aquí en la columna `Proyecto`.

**Última actualización:** 2026-08-29
**Reglas de uso:** ver [README.md](README.md) · **Historial de movimientos:** ver [LOG.md](LOG.md)

---

## Cómo leer la columna `Estado`

| Estado | ¿Cuenta como disponible para planear? | Significado corto |
|---|---|---|
| `Disponible` | ✅ Sí | Está en el cajón, libre |
| `Reservado` | ⚠️ Sí, pero apartado | Apartado para un proyecto, aún no montado |
| `En uso` | ❌ No | Montado y funcionando en un proyecto |
| `A reparar` | 🔜 Sí (a futuro) | Está dañado **pero** en el LOG dijiste que lo repararías |
| `A reponer` | 🔜 Sí (a futuro) | Está dañado **pero** en el LOG dijiste que comprarías otro |
| `Por comprar` | 🔜 Sí (a futuro) | Identificado como necesario, aún no comprado |
| `En tránsito` | 🔜 Sí (a futuro) | Ya comprado, viene en camino |
| `Prestado` | ❌ No | Se lo prestaste a alguien |
| `Dañado` | ❌ **No — ignorar** | Solo existe para tener el registro histórico |
| `Baja` | ❌ No | Se tiró, se perdió o se regaló. Registro histórico |

> **Regla dura:** un componente en estado `Dañado` **no existe** para efectos de
> planeación. Solo pasa a `A reparar` o `A reponer` si tú lo declaras en el
> [LOG.md](LOG.md). Yo nunca cambio ese estado por mi cuenta.

---

## Componentes

| ID | Componente | Categoría | Estado | Cant. | Lógica | Proyecto | Specs clave | Ubicación | Notas |
|---|---|---|---|---|---|---|---|---|---|
| C-001 | ESP32-S NodeMCU (genérico) | Microcontrolador | Disponible | 1 | 3.3 V | — | ESP32-WROOM-32, WiFi+BT, 38 pines (por confirmar), USB-Serial por confirmar | — | ⚠️ Aún **no detectado** por la Mac — ver LOG 2026-08-29 |

---

## Lista de compras derivada

Todo lo que esté en `Por comprar` o `A reponer` aparece aquí automáticamente
cuando actualicemos el inventario.

| ID | Componente | ¿Para qué proyecto? | Prioridad | Precio aprox. | Notas |
|---|---|---|---|---|---|
| — | *(vacío por ahora — se llena cuando definamos la pantalla del Tamagotchi)* | | | | |

---

## Categorías válidas

Usa **solo** estas categorías para que las búsquedas y los filtros funcionen:

| Categoría | Ejemplos |
|---|---|
| `Microcontrolador` | ESP32, ESP8266, Arduino Nano, Raspberry Pi Pico |
| `Pantalla` | OLED SSD1306, TFT ST7789, e-ink, LCD 16x2 |
| `Sensor` | DHT22 (temp/humedad), LDR (luz), HC-SR04 (distancia), MPU6050 |
| `Actuador` | Servo SG90, motor DC, buzzer, relé, vibrador |
| `Entrada` | Botón, encoder rotatorio, joystick, potenciómetro, touch |
| `Alimentación` | Batería LiPo, cargador TP4056, regulador AMS1117, power bank |
| `Comunicación` | Módulo LoRa, RFID RC522, nRF24L01, transceptor |
| `Pasivo` | Resistencias, capacitores, inductores, diodos, cristales |
| `Semiconductor` | Transistores, MOSFETs, ICs, optoacopladores |
| `Prototipado` | Protoboard, jumpers, headers, PCB perforada, cable |
| `Mecánico` | Carcasa, tornillería, filamento 3D, imanes, standoffs |
| `Herramienta` | Multímetro, cautín, fuente de poder, pinzas, osciloscopio |
| `Consumible` | Estaño, flux, termorretráctil, cinta Kapton |

---

## Sobre la columna `Lógica` (importante si estás aprendiendo)

Es el **voltaje al que trabajan los pines de datos** del componente. Es la causa
número uno de que la gente queme placas.

- El **ESP32 es 3.3 V**. Sus pines *no toleran 5 V*.
- Si un componente dice `5 V` en esta columna y lo conectas directo al ESP32,
  probablemente lo dañes → y acabará en estado `Dañado` en esta tabla.
- Cuando quieras mezclar 3.3 V y 5 V necesitas un **level shifter** (conversor de
  nivel lógico). Yo te aviso cada vez que esto aplique.

`5 V (tolera 3.3)` significa que el componente se *alimenta* con 5 V pero entiende
señales de 3.3 V — eso sí es seguro.
