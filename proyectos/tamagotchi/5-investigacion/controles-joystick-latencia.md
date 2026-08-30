# Cuánto tarda el aparato en hacerte caso — y por qué el spinner lo arregla

- **Fecha:** 2026-08-29
- **Contexto:** RF-08 (pausa) y RF-09 (saltar) con el joystick C-005
- **Confianza:** alta — 20+ órdenes medidas en la placa, con el tiempo desglosado en tres tramos

---

## Dos hipótesis mías, las dos falsas

Vale la pena dejarlas escritas, porque las dos sonaban razonables.

**Hipótesis 1: "las órdenes se pierden".** El primer buzón entre núcleos tenía
una sola ranura, y un gesto pisaba al anterior. **Era cierto** y se arregló con
una cola de 4 — pero no era la causa del retraso.

**Hipótesis 2: "la precarga de letras acapara la red".** Se silenció la precarga
durante la navegación. **Empeoró:** la mediana subió de 2,9 s a 4,4 s. Y la
prueba siguiente lo remató: hubo 30 segundos sin una sola descarga de letra y las
órdenes seguían tardando 5 s.

🔑 **Dos hipótesis plausibles seguidas, las dos desmentidas por la medición.**
Lo que las tumbó no fue pensarlo mejor: fue instrumentar el camino completo y
mirar el desglose.

## La medición que sí sirvió

Se partió el trayecto de cada orden en tres tramos —esperar en la cola, esperar
el turno de red, y la petición— y se registraron los tres:

```
[⏱] siguiente:  cola  427 ms + turno    0 ms + red 1098 ms = 1525 ms
[⏱] play/pausa: cola   99 ms + turno 3950 ms + red 1003 ms = 5052 ms
[⏱] anterior:   cola 2863 ms + turno    0 ms + red 1003 ms = 3866 ms
```

| Tramo | Media | Máximo |
|---|---|---|
| Esperando en la cola | 577 ms | 2643 ms |
| Esperando el turno de red | 802 ms | **6023 ms** |
| **La petición HTTPS** | **1007 ms** | 1227 ms |

### Lo que revela

1. **Hay un suelo de ~1 segundo.** La petición a Spotify cuesta 1000 ms clavados
   (894–1227 en 20 muestras). Es el viaje de ida y vuelta: no hay código que lo
   arregle.

2. **El caso típico ya cumple.** En 15 de 20 órdenes el turno fue `0 ms` y el
   total 1–1,6 s, dentro del RF-09.

3. **Los picos son colisiones, no lentitud.** 6 segundos es más que una descarga
   de letra: es que `consultar()` puede encadenar **dos peticiones dentro del
   mismo turno** —la canción y el nombre de la playlist— sin soltar el mutex.

## La decisión: no seguir tocando la red

Se podía partir `consultar()` para que soltara el turno entre sus dos
peticiones. Se decidió **no hacerlo por ahora**: es tocar el reparto de red, que
es exactamente donde ese mismo día se rompió el aparato y hubo que revertir. Y el
aparato es un regalo.

En su lugar se atacó **la percepción**, que era el problema real.

## El spinner

Al empujar la palanca aparece un spinner en el hueco del icono de reproducción,
**en menos de 20 ms**, porque no depende de la red para nada: solo de que el
gesto se leyó. Gira mientras la orden viaja y, al completarse, vuelve el icono.

🔑 **La latencia que molesta no es la que tarda: es la que no avisa.** Un segundo
con respuesta se siente instantáneo; uno sin respuesta se siente roto — y uno
vuelve a empujar, que además encola otra orden y lo empeora.

Verificado mirando la pantalla: *"sí, y se siente mejor"*.

### El otro truco: adelantarse en la pausa

Para play/pausa el icono cambia **antes** de que Spotify conteste, porque
sabemos qué pedimos. Si la orden fallara, `ok` es falso y no se toca nada.

🔑 En una interfaz, ir por delante del servidor es legítimo cuando sabes lo que
pediste y puedes corregirte después.

## Lo que sí se dejó puesto

- **Cola de 4 órdenes** en vez de una ranura. Un buzón de una sola nota no es un
  buzón: es una pizarra, y escribir en ella es borrar lo anterior.
- **Las letras se callan mientras navegas** (2,5 s desde el último gesto). Tras
  este cambio el turno de red fue `0 ms` en 8 de 9 órdenes. Nadie lee la letra
  mientras salta canciones.
- **El desglose de tiempos sigue en el firmware.** Solo escribe cuando hay una
  orden, o sea cuando alguien toca el aparato. Sin esos tres números esta
  investigación no habría salido de las suposiciones.

## Pendiente

- Partir `consultar()` para que no encadene dos peticiones sin soltar el turno.
  Quitaría los picos de 5–6 s. **Medir antes y después**, y solo si hay tiempo de
  verificarlo bien.
- El `cola` llega a 2,6 s porque el núcleo 0 está dentro de su propio sondeo y no
  puede atender la cola. Mismo origen, misma solución.
