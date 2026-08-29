# Requerimientos — Tamagotchi Spotify

- **Versión:** 0.1
- **Fecha:** 2026-08-29
- **Estado:** `Borrador` — falta tu revisión

---

## El proyecto en una frase

Un aparatito de escritorio, basado en ESP32, que muestra qué estás escuchando en
Spotify y —más adelante— la letra de la canción.

## Para qué lo quiero

**Para aprender electrónica e IoT.** Esto no es un detalle: es *el* requerimiento
que manda sobre todos los demás.

Si el objetivo fuera solamente ver la canción en una pantalla, la solución
correcta sería un widget en la Mac y se acabó. Como el objetivo es aprender, el
camino largo es el camino correcto, y decisiones como "que el ESP32 hable directo
con internet" valen la pena aunque sean más difíciles.

Además es un **proyecto de descubrimiento**: se van comprando piezas sobre la
marcha y se mejora conforme se entiende más. Los requerimientos van a cambiar y
eso es esperado, no un problema.

---

## Requerimientos funcionales

| ID | Requerimiento | Prioridad | Criterio de aceptación | Estado |
|---|---|---|---|---|
| RF-01 | Mostrar el **título** de la canción que suena | Debe | Cambio de canción en Spotify → el título nuevo aparece en pantalla en < 10 s, sin tocar nada | 🔲 Pendiente |
| RF-02 | Mostrar el **álbum** al que pertenece | Debe | El nombre del álbum se ve junto al título, sin cortar información crítica | 🔲 Pendiente |
| RF-03 | Mostrar el **artista** | Debería | Se distingue claramente del título | 🔲 Pendiente |
| RF-04 | Mostrar la **letra** sincronizada con la canción | Debería | La línea en pantalla corresponde a lo que se está cantando, con < 2 s de desfase | 🔲 Pendiente |
| RF-05 | Conectarse a WiFi solo, al encender | Debe | Se enchufa y a los < 30 s ya está mostrando algo, sin intervención | 🔲 Pendiente |
| RF-06 | Indicar visualmente cuando **no** hay nada sonando | Debe | Pantalla en reposo distinguible, no la última canción congelada | 🔲 Pendiente |
| RF-07 | Indicar visualmente cuando **perdió la conexión** | Debería | Se distingue "no hay música" de "no hay internet" — son problemas distintos | 🔲 Pendiente |

## Requerimientos no funcionales

| ID | Requerimiento | Valor objetivo | Cómo se mide |
|---|---|---|---|
| RNF-01 | Legible a distancia de escritorio | Se lee cómodo a ~60 cm | Prueba directa, sin acercarse |
| RNF-02 | No molesta de noche | No ilumina la habitación a oscuras | Prueba directa con la luz apagada. Con la GMT130 se resuelve por PWM en el pin `BLK` |
| RNF-03 | Cabe en un escritorio | < 10 × 10 × 10 cm aprox. | Medir |
| RNF-04 | El proyecto se puede recompilar dentro de un año y funciona igual | Versiones fijadas en `platformio.ini` | Ver [ADR-0001](../1-decisiones/ADR-0001-plataforma-de-desarrollo.md) |
| RNF-05 | Las credenciales no acaban en ningún repositorio | 0 secretos en Git | `.gitignore` + revisión antes de cada commit |
| RNF-06 | Funciona indefinidamente sin reiniciarlo a mano | > 7 días encendido sin colgarse | Dejarlo prendido una semana |

---

## Fuera de alcance (por ahora)

Esto es lo que **no** vamos a hacer en la primera versión. La lista existe para
que el proyecto pueda terminarse:

- ❌ **Controlar** Spotify desde el aparato (play, pausa, siguiente). Solo mostrar.
- ❌ Mostrar la **portada** del álbum. La pantalla ya no es el impedimento (la
  GMT130 es a color), pero sigue haciendo falta descargar y decodificar un JPEG
  dentro del ESP32, y eso es un proyecto entero aparte. Queda para después.
- ❌ Funcionar con Apple Music, YouTube Music u otros servicios.
- ❌ Sonido. No emite audio de ningún tipo.
- ❌ Configurar el WiFi desde el propio aparato (portal cautivo). Va compilado.
- ❌ Funcionar con batería. Primera versión enchufada por USB.
- ❌ Carcasa impresa en 3D. Primero funciona, después se ve bonito.
- ❌ La parte "Tamagotchi" propiamente dicha (mascota que reacciona, se alimenta,
  tiene estados de ánimo). **Se llama así por dónde queremos llegar, no por lo que
  hace la v1.**

> Esa última merece un comentario: el nombre del proyecto apunta a la mascota
> virtual que reacciona a tu música. Es un objetivo genial y llegaremos ahí. Pero
> la v1 es "una pantalla que muestra la canción", que ya tiene suficiente
> dificultad (WiFi, HTTPS, OAuth, pantalla). Mezclar las dos cosas ahora
> garantizaría no terminar ninguna.

---

## Supuestos

| # | Supuesto | ¿Verificado? | Si es falso, ¿qué se rompe? |
|---|---|---|---|
| S-01 | ~~Al menos una placa ESP32 funciona~~ | ✅ **Resuelto** — C-001 identificada y respondiendo | — |
| S-02 | ~~La API de Spotify permite leer "reproduciendo ahora"~~ | ✅ **Resuelto** — sí, ver [investigación](../5-investigacion/spotify-api-2026.md) | — |
| S-03 | ~~Hay una pantalla disponible~~ | ✅ **Resuelto** — 4× GMT130 240×240 en inventario | — |
| S-04 | El ESP32 tiene RAM suficiente para HTTPS + JSON a la vez | ❌ No | Obligaría a la arquitectura de puente del [ADR-0002](../1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md) |
| S-05 | Existe una fuente gratuita de letras sincronizadas para tu música | ❌ No | Solo RF-04. El resto sobrevive |
| S-06 | El WiFi de casa es 2.4 GHz | 🔄 En curso — el firmware de prueba escanea las redes | RF-05. ⚠️ **El ESP32 no se conecta a redes de 5 GHz** |
| S-07 | ~~La GMT130 es 3.3 V y ST7789~~ | ✅ **Resuelto** — confirmado en la placa | — |
| S-08 | ~~Faltan las resistencias CC en las placas USB-C~~ | ✅ **Descartado** — C-001 enumeró directo en el puerto USB-C. Era el cable |
| S-09 | **Tienes una cuenta Spotify Premium** | 🔴 **No — bloqueante** | **Todo.** Spotify lo exige para apps en modo desarrollo. No hay rodeo posible |

> S-06 merece atención: muchos routers modernos usan el mismo nombre de red para
> 2.4 GHz y 5 GHz. El ESP32 solo entiende 2.4 GHz, y este es un clásico de
> "no me conecta y no sé por qué". Lo verificamos cuando llegue el momento.

## Riesgos conocidos

| Riesgo | Probabilidad | Impacto | Plan si ocurre |
|---|---|---|---|
| ~~La placa no se detecta~~ | — | — | ✅ Resuelto 2026-08-29: eran cables de solo carga |
| ~~Spotify cierra el endpoint~~ | — | — | ✅ Verificado 2026-08-29: sigue activo y fuera de los recortes |
| **Sin cuenta Premium, la app no funciona** | ❓ Por confirmar | **Muy alto** | Requisito duro de Spotify. Sin rodeo técnico |
| Superar el límite de peticiones (429) | Media | Bajo | Spotify no publica el número. Consultar cada 5 s y respetar siempre `Retry-After` |
| El texto se ve nítido pero demasiado chico para leerlo a 60 cm | Media | Medio | 240 px en 1.3" = píxeles diminutos. Se resuelve con fuentes más grandes, sacrificando caracteres. Ver [ADR-0004](../1-decisiones/ADR-0004-pantalla-gmt130.md) |
| El ESP32 se queda sin memoria con TLS | Media | Alto | Migrar a arquitectura de puente ([ADR-0002](../1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md) opción B) |
| El alcance crece y el proyecto no termina | **Alta** | Medio | La lista de "fuera de alcance" de arriba. Revisarla cada vez que aparezca una idea nueva |

---

## Historial de cambios

| Fecha | Cambio | Motivo |
|---|---|---|
| 2026-08-29 | Versión inicial | Arranque del proyecto |
| 2026-08-29 | S-02 resuelto (la API sirve); nuevo S-09 (Premium) como bloqueante | Investigación de la API de Spotify. El riesgo #1 se cierra y aparece un prerequisito de cuenta |
| 2026-08-29 | S-01, S-07 y S-08 resueltos; S-06 en curso; riesgos de hardware cerrados | La placa fue identificada (ESP32-D0WD-V3) y la GMT130 confirmada. El bloqueante era cables USB-C de solo carga |
| 2026-08-29 | S-03 resuelto; RF-04 sube de `Podría` a `Debería`; riesgos reordenados | Apareció el inventario real: 4× TFT 240×240 a color en vez de la OLED 128×64 supuesta. Con esa resolución las letras dejan de ser una apuesta ([ADR-0004](../1-decisiones/ADR-0004-pantalla-gmt130.md)) |
