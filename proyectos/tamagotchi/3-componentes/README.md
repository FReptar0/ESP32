# Componentes — Tamagotchi

Una ficha por cada pieza física, escrita **antes** de conectarla.

Casi todo el daño en electrónica de hobby viene de conectar algo sin haber leído
qué esperaba. La ficha es el filtro que atrapa eso.

| Componente | ID | Cant. | Ficha | Estado |
|---|---|---|---|---|
| ESP32-S NodeMCU (USB-C) | C-001, C-002 | 2 | [ficha](esp32s-nodemcu.md) | 🟡 No detectadas — ver hipótesis USB-C |
| ESP32 DEVKIT v1 (mini-USB) | C-003 | 1 | [ficha](esp32s-nodemcu.md) | 🎯 **Sin probar — es la prueba decisiva** |
| TFT IPS 1.3" GMT130 240×240 | C-004 | 4 | [ficha](gmt130-tft-ips-240x240.md) | 🟡 Por confirmar voltaje y pines |

## ⚠️ Lo que hay que verificar antes de conectar nada

1. **En la plaquita de la GMT130:** ¿dice 3.3 V o 5 V? Si dice 5 V, **no la
   conectes** — avísame primero.
2. **Cuenta los pines del conector de la GMT130:** ¿7 u 8? Si son 7, no tiene
   `CS` y acapara el bus SPI.
3. **El ESP32 debe funcionar solo** antes de conectarle la pantalla. Un
   componente a la vez: si conectas todo junto y falla, tienes varios sospechosos.

Plantilla: [`metodologia/plantillas/componente-plantilla.md`](../../../metodologia/plantillas/componente-plantilla.md)
Inventario general: [`inventario/INVENTARIO.md`](../../../inventario/INVENTARIO.md)
