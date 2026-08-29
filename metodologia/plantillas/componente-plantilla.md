# Componente — Nombre del componente

- **ID en inventario:** C-XXX
- **Categoría:** <!-- ver categorías válidas en inventario/INVENTARIO.md -->
- **Fecha de la ficha:** AAAA-MM-DD
- **Datasheet:** <!-- enlace. Si no hay datasheet oficial, dilo — es un riesgo -->

---

## Qué hace, en una frase

<!-- En lenguaje normal. "Es una pantalla chiquita en blanco y negro que se
     controla con solo dos cables de datos." -->

## Alimentación

| Concepto | Valor | Nota |
|---|---|---|
| Voltaje de alimentación | | Con cuánto se le da vida |
| **Voltaje lógico** | | ⚠️ Con cuánto hablan sus pines de datos |
| Consumo típico | mA | En operación normal |
| Consumo pico | mA | ⚠️ El que importa para dimensionar la fuente |

> **Alimentación ≠ lógica.** Un componente puede alimentarse con 5 V pero hablar
> a 3.3 V. Son dos números distintos y confundirlos quema placas. El ESP32 habla
> a **3.3 V** y sus pines **no toleran 5 V**.

**¿Necesita conversor de nivel lógico con el ESP32?** Sí / No — y por qué.

## Cómo se comunica

- **Protocolo:** I2C / SPI / UART / analógico / digital simple / PWM
- **Dirección o parámetros:** <!-- p. ej. dirección I2C 0x3C, o 115200 baudios -->
- **Velocidad:**

<!-- Traducción rápida de protocolos:
     I2C  → 2 cables de datos, varios dispositivos comparten el mismo par. Lento pero muy simple.
     SPI  → 4 cables, más rápido. Se usa cuando hay que mover muchos datos (pantallas grandes).
     UART → 2 cables, comunicación tipo "puerto serie". Es lo que usa el USB para hablarle al ESP32.
-->

## Conexiones

| Pin del componente | Va a (pin ESP32) | Función | Nota |
|---|---|---|---|
| | | | |

**¿Requiere componentes extra?** (resistencias pull-up, capacitor de desacoplo,
diodo de protección, resistencia limitadora…)

## Precauciones

<!-- Lo que lo puede dañar. Sé específico y directo. -->

- ⚠️
- ⚠️

## Cómo probarlo aislado

El plan mínimo para confirmar que la pieza funciona **antes** de integrarla al
resto del proyecto.

<!-- Esto ahorra muchísimo tiempo. Si conectas cinco cosas a la vez y no funciona,
     tienes cinco sospechosos. De uno en uno, siempre sabes quién falló. -->

1.
2.

**Cómo se ve el éxito:** <!-- qué tiene que pasar exactamente -->

## Librería a usar

- **Nombre y versión:**
- **Por qué esta y no otra:**

## Notas y sorpresas

<!-- Lo que descubriste usándolo y no venía en el datasheet.
     Esta sección crece con el tiempo y suele ser la más valiosa. -->
