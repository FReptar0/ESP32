# Mejora — Precargar la letra de la siguiente canción

- **Fecha:** 2026-08-29
- **Problema:** *"cambia casi al instante qué canción se reproduce, pero tarda la letra"*

---

## El problema

Al cambiar de canción hacían falta **dos viajes por internet** antes de poder
mostrar nada:

```
cambia la canción → preguntar a Spotify → preguntar a LRCLIB → dibujar
                    (hasta 5 s de espera)   (~1 s)
```

El primero era el caro: se preguntaba a Spotify **cada 5 segundos**, así que en
el peor caso el cambio tardaba 5 segundos en detectarse.

## Las dos soluciones, combinadas

### 1. Sondeo adaptativo — detectar el cambio antes

El ESP32 **sabe cuánto dura la canción y en qué segundo va**. Con eso puede
predecir cuándo va a terminar:

```
Durante casi toda la canción  →  preguntar cada 5 s
En los últimos 15 segundos    →  preguntar cada 1 s
```

Detecta el cambio casi al instante, y **sin gastar peticiones el resto del
tiempo** — que importa, porque el límite de Spotify se calcula sobre una ventana
móvil de 30 segundos.

### 2. Precarga — tener la letra lista de antemano

Spotify sabe qué viene después: el endpoint `/me/player/queue` devuelve la cola
de reproducción.

Así que a los 8 segundos de empezar una canción, cuando ya no hay prisa, el
ESP32 pregunta qué sigue y **descarga esa letra por adelantado**, guardándola
aparte. Cuando llega el cambio:

```
[Precarga] siguiente: Price I'll Pay — Thee Sacred Souls
[Precarga] lista (41 líneas)
🎵 Price I'll Pay — Thee Sacred Souls
[Letra] ⚡ desde precarga (41 líneas)      <- cero espera
```

**Ese `⚡` es una copia en memoria, no una petición de red.**

> Esto usa el permiso `user-read-playback-state`, que se pidió en el
> [ADR-0010](../1-decisiones/ADR-0010-permisos-de-spotify.md) sin saber todavía
> para qué serviría. Pedir los permisos de una vez resultó buena idea: no hubo
> que repetir el flujo de OAuth.

## El coste

Dos juegos de letras en memoria en vez de uno: **13.7 KB** de los ~180 KB
disponibles. Barato para lo que se gana.

⚠️ **Pendiente de vigilar:** la RAM bajó de 186 KB a 180 KB en tres canciones
seguidas. Puede ser el mismo calentamiento del heap que ya se midió antes —que se
aplana— o algo que no se libera del todo. Hay que dejarlo corriendo un rato y
volver a medir.

## La latencia de la letra

Aparte, la letra iba **ligeramente atrasada**. Dos causas y dos arreglos:

**1. El viaje de red.** El `progress_ms` que devuelve Spotify corresponde al
momento en que Spotify procesó la petición, no a cuando llega la respuesta. Ahora
se mide el viaje de ida y vuelta y se compensa la mitad:

```cpp
unsigned long t0 = millis();
int cod = https.GET();
unsigned long viaje = millis() - t0;
...
instanteBase = millis() - viaje / 2;
```

**2. Un ajuste manual**, porque Spotify también reporta con algo de retraso
propio:

```cpp
const long AJUSTE_LETRA_MS = 400;   // + = la letra va por delante
```

Es un número para afinar mirando. Si la letra se ve atrasada, se sube; si va
adelantada, se baja.
