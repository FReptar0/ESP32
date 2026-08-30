# Joystick analógico de 2 ejes (C-005) — tipo KY-023

- **Estado:** ✅ **Conectado y verificado** el 2026-08-29
- **Inventario:** [C-005](../../../inventario/INVENTARIO.md)
- **Cableado:** [`6-esquemas/conexiones-joystick.md`](../6-esquemas/conexiones-joystick.md)

---

## Qué es por dentro

Dos **potenciómetros de 10 kΩ** (uno por eje) y un **pulsador**. Nada más. No
lleva chip ni lógica: es un componente pasivo.

Cada potenciómetro es un divisor de tensión — un carril de resistencia con un
cursor que se desliza. Entrega una fracción de lo que le entra, según dónde esté
la palanca.

> **Por eso funciona a 3,3 V igual que a 5 V.** Solo cambia el tamaño de la tarta
> que reparte; el centro sigue siendo el centro. Y al ESP32 solo le importa la
> proporción.

## Conexión (resumen)

| Pin | ESP32 | Nota |
|---|---|---|
| `GND` | `GND` | |
| `+5V` | **`3V3`** | ⚠️ El rótulo miente. Nunca a 5 V |
| `VRx` | `GPIO34` | ADC1. ⚠️ Es el eje **vertical** ↕, no el horizontal |
| `VRy` | `GPIO35` | ADC1. ⚠️ Es el eje **horizontal** ↔ |
| `SW` | `GPIO33` | Necesita `INPUT_PULLUP` |

---

## 📏 Medido en esta unidad — 2026-08-29

**No son cifras de catálogo: salen de esta palanca concreta**, leídas con
[`pruebas/joystick/`](../pruebas/joystick/).

| Medida | Eje X (horizontal) | Eje Y (vertical) |
|---|---|---|
| **Reposo** | **1737** | **1760** |
| Desviación del centro teórico (2048) | **−311** | **−288** |
| Ruido en reposo | **±4 cuentas** | **±4 cuentas** |
| Recorrido total | `0 … 4095` | `0 … 4095` |
| Recorrido hacia el positivo | 2358 | 2335 |
| Recorrido hacia el negativo | 1737 | 1760 |

Pulsador: `1` en reposo, `0` pulsado. ✅ Verificado.

### 🔑 Hallazgo 1: el reposo NO es el centro, y por bastante

Se desvía unas **300 cuentas**, un 7 % del rango. Si el firmware diera por hecho
que el reposo es 2048, creería que la palanca está permanentemente empujada hacia
abajo y a la izquierda — y el aparato saltaría canciones solo.

> **Por eso el firmware debe medir el reposo al arrancar**, promediando unas
> decenas de lecturas, en vez de usar una constante. Y por eso tampoco sirve
> clavar en el código el 1737 de hoy: otra unidad daría otro número, y esta misma
> puede derivar con la temperatura o el tiempo.

### 🔑 Hallazgo 2: el recorrido es asimétrico, y eso se nota al usarlo

Como el reposo está por debajo del centro, hay **2358 cuentas hacia un lado y
solo 1737 hacia el otro**. El lado corto tiene un 74 % del recorrido del largo.

Con un umbral fijo —digamos 600 cuentas— haría falta empujar un **25 %** del
recorrido hacia un lado y un **35 %** hacia el otro. Se siente raro sin saber por
qué: parece que la palanca "va dura" en una dirección.

> **La solución:** convertir cada eje a una escala de −1,0 a +1,0 **usando el
> recorrido real de cada dirección por separado**, y luego aplicar un único
> umbral sobre ese número.
>
> 🔑 Cuando el hardware es asimétrico, la simetría se pone en el software.
> Compensar en el número hace que la mano perciba algo parejo aunque el
> componente no lo sea.

### 🔑 Hallazgo 3: el ruido es bajísimo, y eso es un diagnóstico

**±4 cuentas** sobre 4095 es prácticamente nada. Confirma tierra común sólida y
buen contacto en los tres cables de señal.

Sirve de referencia: **si algún día se ve ruido mucho mayor, no es "cosa del
componente" — es un cable flojo o una tierra mal hecha.**

### La zona muerta que sale de aquí

La regla práctica (4 × el ruido) daría ~16 cuentas, pero eso es *lo mínimo para
no captar ruido*, no *lo necesario para que el gesto sea intencionado*. Con 16
cuentas, un roce sin querer cambiaría de canción.

Para un aparato que se regala, el gesto debe ser deliberado: **umbral sobre el
valor normalizado, no sobre cuentas crudas.** Queda por afinar probándolo con la
mano — es de las cosas que no se deciden en una tabla.

---

## Trampas de este componente

| Trampa | Síntoma | Realidad |
|---|---|---|
| El rótulo `+5V` | — | ⚠️ Alimentar a 3,3 V. Los pines del ESP32 no toleran 5 V |
| **Creer que `VRx` es el eje horizontal** | Empujas a los lados y no pasa nada, aunque el gesto se detecta | Los rótulos son los ejes DEL MÓDULO. Aquí `VRx` resultó ser el **vertical**. Se deriva empujando y viendo qué eje se mueve |
| Usar pines del **ADC2** | Funciona en una prueba suelta y falla al añadir WiFi | El driver de WiFi se apropia del ADC2. **Solo ADC1**: GPIO 32–36 y 39 |
| Suponer que el reposo es 2048 | El aparato "se mueve solo" | Esta unidad reposa en 1737/1760 |
| Esperar recorrido simétrico | Una dirección "va dura" | 2358 hacia un lado, 1737 hacia el otro |
| Olvidar el pull-up en `SW` | El botón se dispara solo | `GPIO34`–`GPIO39` **no tienen** pull-up interno. Por eso `SW` va al `GPIO33` |
| Leer `SW` al revés | El botón parece invertido | Con pull-up, **pulsado es `0`** |
