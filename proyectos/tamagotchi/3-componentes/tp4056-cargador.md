# Cargador TP4056 con protección (C-007)

- **Estado:** 🟡 **`B+`/`B-` soldadas · `OUT+`/`OUT-` libres** (2026-08-30)
- **Inventario:** [C-007](../../../inventario/INVENTARIO.md)
- **Alimentación:** [`6-esquemas/alimentacion-por-bateria.md`](../6-esquemas/alimentacion-por-bateria.md)

---

## Qué hace

Dos cosas distintas que conviene no mezclar:

1. **Carga la batería** cuando enchufas su USB. Sube la celda hasta 4,2 V y ahí
   se detiene sola. Un LED avisa mientras carga y otro cuando termina.
2. **La protege** mientras se usa: corta si se descarga demasiado (por debajo de
   ~2,4 V), si se pide demasiada corriente, o si hay un cortocircuito.

## Las cuatro pastillas, y por qué son cuatro y no dos

Es lo más importante de esta ficha:

| Pastilla | A qué va | Estado |
|---|---|---|
| `B+` | Cable **rojo** de la batería | ✅ soldada |
| `B-` | Cable **negro** de la batería | ✅ soldada |
| `OUT+` | **Positivo hacia el aparato** | 🔲 libre |
| `OUT-` | **Negativo hacia el aparato** | 🔲 libre |

Podría parecer que `OUT+` es lo mismo que `B+` con otro nombre. **No lo es**, y
esa diferencia es toda la protección:

```
batería ──> B+ ──> [protección] ──> OUT+ ──> aparato
```

Entre `B` y `OUT` hay dos transistores que **cortan el paso** si algo va mal. Si
conectaras el aparato a `B+` te saltarías la protección: la batería se seguiría
cargando bien, pero podría descargarse hasta estropearse.

🔑 **Todo lo que consuma va a `OUT`. A `B` solo va la batería.** Si un módulo
TP4056 solo tiene dos pastillas, es la versión **sin** protección — esa es otra
pieza y no debe usarse así.

## ⚠️ El malentendido que rompe el proyecto

> **`OUT+` no da 5 V. Da lo que tenga la batería: entre 3,0 y 4,2 V.**

El módulo **no eleva la tensión**. Su USB es solo una *entrada* para cargar; no
hay un camino de 5 V hacia la salida. Es el error más común con esta pieza,
porque la placa tiene un USB y uno da por hecho que "sale USB".

Consecuencia directa, y está desarrollada en el esquema: **no se puede conectar
`OUT+` ni al pin `VIN` ni al pin `3V3` del ESP32.** Falta una pieza en medio.

## Cargar y usar a la vez

Sí se puede: mientras el USB del cargador esté enchufado, alimenta al aparato y
carga la batería con lo que sobre. Es la razón de poner el interruptor en `OUT+`
y no entre la batería y el módulo — así **apagar el aparato no impide cargarlo**.

## Corriente de carga

Estos módulos vienen de fábrica a **1 A**, fijado por una resistencia de la
placa. Para una celda de 1000 mAh eso es cargar a "1C", que es aceptable pero va
al máximo recomendado.

⚠️ **Sin verificar:** hay que leer la resistencia (rotulada `R3` o similar,
`122` = 1,2 kΩ = 1 A; `472` = 4,7 kΩ = ~260 mA). Si se quiere cargar más suave,
se cambia esa resistencia. **No es urgente**, pero conviene saberlo antes de
dejar el aparato cargando toda la noche sin vigilancia.
