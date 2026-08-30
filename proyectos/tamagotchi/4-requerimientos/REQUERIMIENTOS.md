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

### 🎁 Y va a ser un regalo

Decidido a mitad del proyecto, y **cambia las prioridades**: el aparato va a estar
en manos de alguien que no puede depurarlo ni reiniciarlo a conciencia.

Consecuencias prácticas, todas ya aplicadas:

- **La estabilidad pesa más que las funciones nuevas.** Un reinicio ocasional es
  aceptable en un prototipo propio; en un regalo, no.
- **Todo fallo debe degradar, no romper.** Sin letra se sigue viendo la canción;
  sin red se avisa en rojo; si la memoria bajara, el hilo de letras se aparta solo.
- **Los detalles visuales importan.** El desplazamiento sin tirones, la transición
  entre frases, los acentos y el color del corazón no son adornos: son lo único
  que va a ver quien lo reciba.

---

## Requerimientos funcionales

| ID | Requerimiento | Prioridad | Criterio de aceptación | Estado |
|---|---|---|---|---|
| RF-01 | Mostrar el **título** de la canción que suena | Debe | Cambio de canción en Spotify → el título nuevo aparece en pantalla en < 10 s, sin tocar nada | ✅ **Hecho** |
| RF-02 | Mostrar el **álbum** al que pertenece | Debe | El nombre del álbum se ve junto al título, sin cortar información crítica | ✅ **Hecho** |
| RF-03 | Mostrar el **artista** | Debería | Se distingue claramente del título | ✅ **Hecho** |
| RF-04 | Mostrar la **letra** sincronizada con la canción | Debería | La línea en pantalla corresponde a lo que se está cantando, con < 2 s de desfase | ✅ **Hecho** — sincronía confirmada mirando |
| RF-05 | Conectarse a WiFi solo, al encender | Debe | Se enchufa y a los < 30 s ya está mostrando algo, sin intervención | ✅ **Hecho** |
| RF-06 | Indicar visualmente cuando **no** hay nada sonando | Debe | Pantalla en reposo distinguible, no la última canción congelada | ✅ **Hecho** — verificado mirando |
| RF-07 | Indicar visualmente cuando **perdió la conexión** | Debería | Se distingue "no hay música" de "no hay internet" — son problemas distintos | ✅ **Hecho** — verificado mirando |
| RF-08 | **Pausar y reanudar** la reproducción desde el aparato | **Debe** *(sube: ya hay hardware)* | Pulsar el joystick pausa o reanuda lo que suena, y se ve reflejado en pantalla en < 2 s | ✅ **Cumplido** 2026-08-29 — el icono cambia al instante (optimista) y la orden llega en ~1,5 s de mediana |
| RF-09 | **Saltar** a la canción siguiente o anterior | **Debe** *(sube: ya hay hardware)* | Izquierda y derecha del joystick saltan a la anterior o la siguiente en < 2 s | 🟡 **Parcial** 2026-08-29 — mediana 1,5 s ✅, pero con picos de hasta 5 s al chocar con otra petición. Mitigado con el spinner |
| RF-10 | **Ajustar el volumen** desde el aparato | Podría | Un encoder o botones cambian el volumen del aparato que reproduce | ⬜ Descartado en v2 — el joystick (C-005) no tiene eje libre para volumen |
| RF-11 | **Cuatro pantallas en bucle** con arriba/abajo del joystick | Debe | Arriba y abajo recorren: sin letra → con letra → portada → elegir playlist → y vuelta a empezar. El modo elegido sobrevive a un reinicio | ✅ **Cumplido** 2026-08-30 — los cuatro modos rotan en bucle con fundido. El modo se guarda en NVS 3 s tras el último cambio. 👀 Falta mirar si el fundido (~110 ms + 200 ms) estorba al cambiar rápido |
| RF-12 | **Modo sin letra**: canción y artista en grande | Debe | Título y artista ocupan el centro con tipografía mayor que hoy. La barra de la playlist se conserva arriba, y la pausa se sigue viendo | ✅ **Cumplido** 2026-08-30 — título centrado y artista en verde debajo. 👀 Falta mirar si se lee bien a distancia de escritorio |
| RF-13 | **Modo portada**: la carátula del álbum a pantalla completa | Debe | Al cambiar de canción, la portada aparece en < 6 s. Si no se puede descargar, se degrada al modo con letra sin quedarse en negro | ✅ **Cumplido** 2026-08-30 — se usa la de 300 px ([ADR-0011](../1-decisiones/ADR-0011-imagenes-en-pantalla.md)): 26–57 KB, dibujar 170–437 ms. 👀 Falta mirar si el congelón de ~400 ms del texto de arriba molesta |
| RF-14 | **Elegir playlist desde el aparato** | Debe | Se ven las playlists con su imagen; izquierda/derecha navega y el clic la reproduce en < 3 s | ✅ **Cumplido** 2026-08-30 — suena al pulsar. Las 3 playlists se encuentran por prefijo de nombre y la lista se refresca sola (5 min). 👀 Falta mirar la imagen de 150 px y el contador |
| RF-15 | **Pantalla oculta de estado** | Podría | Una combinación poco probable del joystick la abre. Muestra el estado del aparato (RAM, WiFi, versión, tiempo encendido) | ✅ **Cumplido** 2026-08-30 — se abre con `↑↑↓↓` ([ADR-0012](../1-decisiones/ADR-0012-combinacion-secreta.md)). Muestra WiFi, señal, RAM libre, bloque contiguo, versión y tiempo encendido; los cuatro que se mueven se refrescan cada segundo. **Verificado mirando.** La ventana entre gestos se midió en la placa: 1200 ms era demasiado corta (huecos reales de hasta 1047 ms), subida a 2000 |
| RF-16 | **Que encontrarla sea un regalo, no un susto** | Debe | Al abrirse, una animación de sorpresa y un texto que tranquiliza: que no se preocupe, que basta un clic para salir y puede seguir usándolo. Al salir muestra **"Te quiero!!"** | ✅ **Cumplido** 2026-08-30 — tres ondas que salen del centro y un corazón que crece; debajo del título, *"no rompiste nada"* y *"un clic y vuelves"*. Al salir, corazón y **"Te quiero!!"** 1,6 s. **Verificado mirando**, incluso con la música en pausa — que era el caso que fallaba |

## Requerimientos no funcionales

| ID | Requerimiento | Valor objetivo | Cómo se mide |
|---|---|---|---|
| RNF-01 | ~~Legible a distancia de escritorio~~ | ✅ **Resuelto** — fuente 4 (26 px) se lee cómodo a 60 cm | Probado mirando la pantalla |
| RNF-02 | ~~No molesta de noche~~ | No ilumina la habitación a oscuras | ✅ **Verificado** a oscuras con el brillo al 55% |
| RNF-03 | Cabe en un escritorio | < 10 × 10 × 10 cm aprox. | Medir |
| RNF-04 | El proyecto se puede recompilar dentro de un año y funciona igual | Versiones fijadas en `platformio.ini` | Ver [ADR-0001](../1-decisiones/ADR-0001-plataforma-de-desarrollo.md) |
| RNF-05 | ~~Las credenciales no acaban en ningún repositorio~~ | 0 secretos en Git | ✅ **Verificado** — comprobación automatizada en la skill `/cerrar-sesion` |
| RNF-06 | Funciona indefinidamente sin reiniciarlo a mano | > 7 días encendido sin colgarse | 🟢 **Sin fuga confirmado** con el firmware completo: plano en 5 min, cero fragmentación, 97 KB de margen en el peor instante ([medición](../5-investigacion/memoria-en-regimen-2.md)) |

---

## Fuera de alcance (por ahora)

Esto es lo que **no** vamos a hacer en la primera versión. La lista existe para
que el proyecto pueda terminarse:

- ⏩ ~~Controlar Spotify desde el aparato~~ → **entró al alcance** como RF-08 a
  RF-10 (2026-08-29). No es de la v1, pero los permisos ya se piden desde ahora
  para no repetir el flujo de OAuth. Ver
  [ADR-0010](../1-decisiones/ADR-0010-permisos-de-spotify.md).
- ❌ **Reproducir audio** desde el propio Tamagotchi. El ESP32 no puede: el Web
  Playback SDK es JavaScript de navegador y necesita descifrado de DRM. El
  aparato es un control remoto, no un altavoz.
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
| S-04 | ~~El ESP32 tiene RAM suficiente para HTTPS~~ | ✅ **Resuelto** — mínimo 199.4 KB con TLS activo, sin fugas ni fragmentación | — |
| S-05 | ~~Existe una fuente gratuita de letras sincronizadas~~ | ✅ **Resuelto** — LRCLIB, sin autenticación, buena cobertura ([investigación](../5-investigacion/lrclib-api.md)) | — |
| S-06 | ~~El WiFi de casa es 2.4 GHz~~ | ✅ **Resuelto** — el ESP32 ve `INFINITUMEB00` a −39 dBm, canal 11 | — |
| S-07 | ~~La GMT130 es 3.3 V y ST7789~~ | ✅ **Resuelto** — confirmado en la placa | — |
| S-08 | ~~Faltan las resistencias CC en las placas USB-C~~ | ✅ **Descartado** — C-001 enumeró directo en el puerto USB-C. Era el cable |
| S-09 | ~~Tienes una cuenta Spotify Premium~~ | ✅ **Resuelto** — plan Familiar, como titular | — |

> S-06 merece atención: muchos routers modernos usan el mismo nombre de red para
> 2.4 GHz y 5 GHz. El ESP32 solo entiende 2.4 GHz, y este es un clásico de
> "no me conecta y no sé por qué". Lo verificamos cuando llegue el momento.

## Riesgos conocidos

| Riesgo | Probabilidad | Impacto | Plan si ocurre |
|---|---|---|---|
| ~~La placa no se detecta~~ | — | — | ✅ Resuelto 2026-08-29: eran cables de solo carga |
| ~~Spotify cierra el endpoint~~ | — | — | ✅ Verificado 2026-08-29: sigue activo y fuera de los recortes |
| ~~Sin cuenta Premium la app no funciona~~ | — | — | ✅ Resuelto: plan Familiar como titular |
| Superar el límite de peticiones (429) | Media | Bajo | Spotify no publica el número. Consultar cada 5 s y respetar siempre `Retry-After` |
| El texto se ve nítido pero demasiado chico para leerlo a 60 cm | Media | Medio | 240 px en 1.3" = píxeles diminutos. Se resuelve con fuentes más grandes, sacrificando caracteres. Ver [ADR-0004](../1-decisiones/ADR-0004-pantalla-gmt130.md) |
| ~~El ESP32 se queda sin memoria con TLS~~ | — | — | ✅ Medido 2026-08-29: 199.4 KB de margen, sin fugas |
| ~~Los certificados raíz TLS caducan~~ | Muy baja | Alto | ✅ Mitigado por el [ADR-0009](../1-decisiones/ADR-0009-certificados-raiz.md): paquete de 150 CAs de Mozilla, no un certificado suelto |
| El alcance crece y el proyecto no termina | **Alta** | Medio | La lista de "fuera de alcance" de arriba. Revisarla cada vez que aparezca una idea nueva |

---

## Historial de cambios

| Fecha | Cambio | Motivo |
|---|---|---|
| 2026-08-29 | Versión inicial | Arranque del proyecto |
| 2026-08-29 | RF-04 implementado: letras sincronizadas desde LRCLIB | Búsqueda en dos pasos + lector de respuestas troceadas. Falta verificarlo mirando |
| 2026-08-29 | S-05 resuelto — **los 9 supuestos cerrados**. ADR-0003 aceptado (LRCLIB) | La API verificada con consultas reales. La canción que parecía faltar sí estaba: el fallo era la duración |
| 2026-08-29 | **RF-06 y RF-07 ✅ HECHOS · RNF-02 verificado.** Registrado que el proyecto será un regalo | Todas las verificaciones visuales pasaron |
| 2026-08-29 | **RF-04 ✅ HECHO** — letras sincronizadas funcionando y confirmadas mirando. RNF-05 verificado | Cierre de sesión |
| 2026-08-30 | **Añadidos RF-11 a RF-16** (cuatro modos, sin letra, portada, selector de playlists, pantalla oculta y su sorpresa) | Nuevos requerimientos tras conseguir el joystick |
| 2026-08-30 | **RF-08 y RF-09 suben de "Podría" a "Debe"** | Ya hay hardware de entrada: el joystick C-005 |
| 2026-08-30 | **RF-10 (volumen) ⬜ DESCARTADO** | Los dos ejes del joystick están comprometidos. Ver [ADR-0007](../1-decisiones/ADR-0007-joystick-como-control.md) |
| 2026-08-30 | **RF-08, RF-11, RF-12, RF-13, RF-14 ✅ HECHOS.** RF-09 🟡 parcial (mediana 1,5 s, picos de 5 s) | Sesión del joystick y los modos |
| 2026-08-30 | **RF-15 y RF-16 implementados** con la combinación `↑↑↓↓`. Quedan 🟡 hasta verlos en pantalla | Sesión de la pantalla oculta ([ADR-0012](../1-decisiones/ADR-0012-combinacion-secreta.md)) |
| 2026-08-30 | **RF-15 y RF-16 ✅ verificados mirando.** Con ellos, los 16 requerimientos quedan resueltos | Fin de la sesión 3 |
| 2026-08-30 | Corregidas las filas de **RF-11 a RF-14**, que seguían diciendo `🔲 Pendiente` pese a estar hechas desde la sesión anterior | Desfase encontrado al abrir sesión: el historial de este mismo documento ya las daba por hechas |
| 2026-08-29 | RF-01, RF-02, RF-03 y RF-05 ✅ HECHOS | La pantalla muestra canción, artista, álbum y progreso, en vivo |
| 2026-08-29 | RNF-01 resuelto (fuente 4). El texto disponible baja a ~17 car. × 9 líneas | Medición perceptual en la pantalla real. Casi la mitad de lo estimado con fuente 8×16: aprieta el RF-04 |
| 2026-08-29 | RF-01/02/03/05/06 con datos funcionando; riesgo de certificados mitigado | El ESP32 lee Spotify con certificado verificado. ADR-0009 cerrado |
| 2026-08-29 | Controlar Spotify pasa de fuera de alcance a RF-08/09/10 | Decisión del usuario. Se piden los permisos desde ya para no repetir OAuth ([ADR-0010](../1-decisiones/ADR-0010-permisos-de-spotify.md)) |
| 2026-08-29 | S-04 resuelto — el punto de control del ADR-0002 pasó con 199.4 KB de margen | Spike de memoria con TLS contra el servidor real de Spotify. La arquitectura directa se confirma |
| 2026-08-29 | S-06 resuelto; S-04 con línea base medida | Primer firmware cargado con éxito: 303.5 KB de RAM libre y red de 2.4 GHz confirmada a −39 dBm |
| 2026-08-29 | S-02 resuelto (la API sirve); nuevo S-09 (Premium) como bloqueante | Investigación de la API de Spotify. El riesgo #1 se cierra y aparece un prerequisito de cuenta |
| 2026-08-29 | S-01, S-07 y S-08 resueltos; S-06 en curso; riesgos de hardware cerrados | La placa fue identificada (ESP32-D0WD-V3) y la GMT130 confirmada. El bloqueante era cables USB-C de solo carga |
| 2026-08-29 | S-03 resuelto; RF-04 sube de `Podría` a `Debería`; riesgos reordenados | Apareció el inventario real: 4× TFT 240×240 a color en vez de la OLED 128×64 supuesta. Con esa resolución las letras dejan de ser una apuesta ([ADR-0004](../1-decisiones/ADR-0004-pantalla-gmt130.md)) |
