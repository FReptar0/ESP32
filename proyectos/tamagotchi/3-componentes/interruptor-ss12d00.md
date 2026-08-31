# Interruptor deslizante SPDT (C-008) — tipo SS12D00

- **Estado:** 🟡 **Cable soldado a la pata del medio, sin instalar** (2026-08-30)
- **Inventario:** [C-008](../../../inventario/INVENTARIO.md)
- **Alimentación:** [`6-esquemas/alimentacion-por-bateria.md`](../6-esquemas/alimentacion-por-bateria.md)

---

## Qué es "SPDT" y por qué tiene tres patas

**SPDT** quiere decir *un contacto, dos destinos*. La pata del medio es el
**común**, y el deslizador la une a una de las dos de los extremos — nunca a las
dos, nunca a ninguna:

```
   [1]  [C]  [2]        deslizador a la izquierda:  C unido a 1
    │    │    │         deslizador a la derecha:    C unido a 2
```

🔑 **Para encender y apagar solo hacen falta dos patas: la del medio y UNA de los
extremos.** La otra se queda al aire. No es un desperdicio: es que el componente
sirve también para conmutar entre dos cosas, y aquí solo se usa la mitad.

**Cuál de las dos elijas decide hacia qué lado se enciende.** Si al montarlo
queda al revés de lo que esperabas, no hay que resoldar: se pasa el cable a la
otra pata del extremo.

## Cómo está

El **cable rojo ya está soldado a la pata del medio** (la común). Falta el
segundo cable, a una de las de los extremos.

## ⚠️ La corriente: aquí va justo

Los `SS12D00` se venden típicamente para **0,5 A**. Y el ESP32 llega a **500 mA
de pico** al transmitir por WiFi — exactamente el límite.

Peor aún: si el interruptor va del lado de la batería (3,7 V) y el aparato
consume a 3,3 V, la corriente por el interruptor es **mayor** que la que consume
el aparato, porque la conversión mantiene la potencia:

```
200 mA a 3,3 V  ≈  0,66 W  ≈  215 mA a 3,7 V   (y más con las pérdidas)
```

En picos de 500 mA se pasaría de los 0,5 A nominales.

**Dos formas de resolverlo**, y la segunda es mejor:

| | Qué se hace | Corriente por el interruptor |
|---|---|---|
| A | El interruptor corta la línea `OUT+` | **toda la del aparato** — va al límite |
| B | El interruptor gobierna el pin `EN` del convertidor | **microamperios** |

La **B** es la buena: el interruptor no lleva la corriente, solo *da la orden* de
encender. Depende de que el convertidor que se compre tenga pin `EN`, así que hay
que mirarlo al elegirlo.

🔑 Un interruptor no tiene por qué estar en el camino de la corriente. Muchas
veces basta con que **mande sobre quien sí la lleva** — y así nunca se calienta,
nunca se desgasta y no limita el diseño.

⚠️ **Sin verificar:** la corriente máxima real de esta unidad. La cifra de 0,5 A
es la típica del tipo, no está medida ni viene rotulada en la pieza.
