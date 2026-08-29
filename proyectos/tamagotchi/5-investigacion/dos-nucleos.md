# Las pausas del desplazamiento: la red bloqueaba el dibujo

- **Fecha:** 2026-08-29
- **Síntoma:** *"la velocidad está bien pero sigue habiendo pausas"*
- **Confianza:** máxima (causa identificada, medida y corregida)

---

## No era el bucle del texto

El primer arreglo atacó el reinicio del bucle: se duplicó el texto para que
nunca hubiera un instante en blanco. Estaba bien hecho, **pero no era la causa**.

Las pausas seguían porque venían de otro sitio.

## La causa: pedirle algo a internet **bloquea**

| Llamada | Cuánto tarda |
|---|---|
| `consultar()` — preguntar a Spotify | **~1.200 ms** |
| `precargarSiguiente()` — cola + letra | **~3.000 ms** |

Y ambas se hacían **dentro del bucle principal**, el mismo que dibuja:

```cpp
void loop() {
  if (toca consultar) consultar();     // <- aquí el programa se para 1,2 s
  ...
  moverBarra();                        // <- esto no se ejecuta mientras tanto
  delay(20);
}
```

El desplazamiento avanza **1 píxel cada 65 ms**. Durante una consulta de 1.200 ms
se pierden **~18 píxeles**. Y se consultaba **cada 2 segundos**.

Resultado: el texto avanzaba dos segundos, se congelaba uno, avanzaba otros dos.
Exactamente las pausas descritas.

🔑 **Un síntoma puede tener varias causas.** El primer arreglo era correcto y no
sobra — simplemente había una segunda causa detrás, más grande.

## La solución: usar los dos núcleos

El ESP32 tiene **dos núcleos**, y hasta ahora solo se usaba uno.

```
        ANTES                          AHORA
   ┌──────────────┐            ┌────────┐   ┌────────┐
   │  Núcleo 1    │            │Núcleo 0│   │Núcleo 1│
   │  red         │            │  red   │   │ dibujo │
   │  + dibujo    │            │ (puede │   │(nunca  │
   │              │            │ tardar)│   │ se para│
   │  Núcleo 0    │            └────────┘   └────────┘
   │  sin usar    │                  ↕ mutex
   └──────────────┘
```

- **Núcleo 0** — hablar con internet. Puede tardar lo que quiera.
- **Núcleo 1** — dibujar, y solo dibujar. Nunca se bloquea.

Así la red puede quedarse esperando tres segundos: **la animación ni se entera**.

### El reparto no es arbitrario

**Todo el dibujo se queda en un solo núcleo, a propósito.** La librería de
pantalla no está preparada para que dos núcleos dibujen a la vez: se corrompería
la imagen. Manteniendo el dibujo en uno solo, ese problema no puede aparecer.

Eso obligó a cambiar algo: antes, la función que consultaba Spotify **también
pintaba** la pantalla al detectar una canción nueva. Ahora solo actualiza los
datos y levanta una bandera; el núcleo que dibuja la ve y repinta.

### El mutex

Lo único que comparten los dos núcleos son los datos. Un **mutex** es un turno:
garantiza que nadie lea a medias lo que el otro está escribiendo.

Sin él podría pasar que, justo cuando el núcleo 0 sustituye la letra por la de la
canción nueva, el núcleo 1 esté leyéndola — y dibujaría medio de una y medio de
la otra, o directamente se bloquearía.

**Detalle importante:** la descarga de la letra se hace **fuera** del mutex. Tarda
segundos, y retener el turno tanto tiempo dejaría al otro núcleo esperando —
volviendo justo al problema que se quería resolver. Se descarga sobre un buffer
que nadie está mirando y **solo el intercambio final** ocurre dentro del turno,
que dura microsegundos.

## Coste

| | |
|---|---|
| Pila de la tarea nueva | 16 KB |
| RAM libre en reposo | 202 KB (antes 218) |
| Mínimo histórico | **71 KB** (antes 97) |

El mínimo baja porque ahora la red y el dibujo pueden coincidir en el tiempo. 71
KB sigue siendo holgado.

🔑 **La lección:** cuando algo "se congela a ratos" en un sistema empotrado,
sospecha de una llamada bloqueante en el mismo bucle que dibuja. Y si el chip
tiene más de un núcleo, ahí está la solución — no en optimizar la animación.
