# Dos hilos de red: los reinicios y la letra equivocada

- **Fecha:** 2026-08-29
- **Síntomas:** *"se reinicia mucho"* · *"toma la letra de una canción pasada"* · frases largas recortadas
- **Confianza:** máxima (causas identificadas en el log del crash y corregidas)

---

## 1. 💥 Por qué se reiniciaba

El log del propio ESP32 lo dijo con todas las letras:

```
E task_wdt: Task watchdog got triggered. The following tasks did not
            reset the watchdog in time:
E task_wdt:  - IDLE0 (CPU 0)
abort() was called at PC 0x400f21ad on core 0
```

**El vigilante del sistema mató la placa.** Su trabajo es detectar programas
atascados: vigila que la tarea "ociosa" de cada núcleo llegue a ejecutarse de vez
en cuando. Si un núcleo pasa **5 segundos al 100%**, asume que algo se colgó y
reinicia.

### La causa: un handshake TLS no espera, CALCULA

Aquí estaba mi error de concepto. Yo trataba las peticiones de red como
operaciones de **espera** — el procesador estaría libre mientras llegan los datos.

Para la transferencia es cierto. **Para el handshake no.** Establecer una conexión
cifrada es criptografía pura: intercambio de claves, verificación de firmas,
aritmética con números enormes. En un ESP32 son **uno o dos segundos de CPU
real**, no de espera.

Puse los dos hilos de red en el mismo núcleo. Dos handshakes a la vez lo
saturaban varios segundos seguidos → la tarea ociosa nunca llegaba a ejecutarse →
reinicio.

🔑 **"Esperando por la red" y "calculando" se parecen desde fuera y son
completamente distintos por dentro.** Uno deja el procesador libre; el otro lo
acapara. Si dos tareas hacen lo segundo a la vez en el mismo núcleo, lo saturan.

### Por qué un tercer hilo lo empeoraría

Era la pregunta natural: *¿abrimos otro hilo?* No. **Sería un tercer handshake
compitiendo por el mismo procesador.** El problema no era falta de hilos, sino
demasiado cálculo simultáneo.

### La solución: un turno para la radio

Un **mutex de red**: solo una conexión cifrada a la vez.

| | Antes | Ahora |
|---|---|---|
| Handshakes simultáneos | 2 | **1** |
| Reinicios en 150 s | 11 | **0** |
| Mínimo de RAM libre | 59 KB | **89 KB** |

Y no se pierde lo que se buscaba: el hilo de Spotify espera **como mucho UNA
petición de letra** (~1,2 s), nunca una cadena. La detección de cambio de canción
sigue siendo independiente en la práctica.

De regalo, **30 KB más de memoria libre** en el peor caso: nunca hay dos búferes
de cifrado abiertos.

---

## 2. 🔀 La letra de la canción anterior

**Síntoma:** al saltar varias canciones seguidas, aparecía la letra de una
anterior. El usuario lo diagnosticó exactamente: *"pareciera ser que termina la
descarga de la letra previa"*.

Justo eso.

### Por qué la comprobación que había no servía

El código comprobaba, al terminar la descarga, si la canción seguía siendo la
misma:

```cpp
if (claveActual == claveObj) { publicar; }
```

Parece correcto. No lo es:

```
1. Empieza la descarga de la letra de A     claveObj = A
2. Cambias a B  ->  consultar() escribe     claveObj = B, claveActual = B
3. Termina la descarga (que era de A)
4. Comprueba: claveActual == claveObj  ->  B == B  ✅  ¡pasa!
5. Publica la letra de A como si fuera de B
```

**La variable que decía "qué estamos buscando" ya había cambiado.** Comparar dos
valores que se actualizan juntos nunca detecta nada.

### La solución: marcar al empezar

Un contador de **generación** que sube con cada cambio de canción. Al empezar una
descarga se apunta el número; al terminar se compara con el actual:

```cpp
uint32_t miGen = generacion;    // al EMPEZAR
... descarga ...
if (generacion != miGen) { descartar; }   // al TERMINAR
```

Y el objetivo (título, artista, álbum) se **copia** al empezar, en vez de leerse
al final. Así la descarga trabaja siempre con datos coherentes entre sí.

🔑 **Para saber si un resultado llega tarde, hay que marcarlo al EMPEZAR y
comprobar la marca al terminar.** Mirar solo el estado final no distingue "sigue
igual" de "cambió mientras tanto".

Es el mismo principio que un número de pedido: no preguntas *"¿esto es lo que
pedí?"* mirando lo que hay ahora en la mesa — comparas contra el resguardo que te
dieron al hacer el pedido.

---

## 3. ✂️ Frases largas recortadas — y no era lo que parecía

Primero subí el número de renglones permitidos (de 3/4 a 4/5, que es lo que
físicamente cabe). **Y seguía cortándose.**

En vez de seguir suponiendo, instrumenté el reparto para que imprimiera qué
estaba pasando de verdad:

```
[Frase] 71 car, ancho=645 (max 224), 3 lineas, fuente GRANDE
        texto: "Honey, honey get the kids in the car now, put the cash in the trunk, ge"
        [0] w=198 "Honey, honey get the"
        [1] w=214 "kids in the car now, put"
        [2] w=222 "the cash in the trunk, ge"
```

**El texto llegaba ya cortado: 71 caracteres, terminando en "ge".** El reparto en
renglones funcionaba perfecto — los tres caben de sobra en 224 px. El problema
estaba **mucho antes**, al guardar la letra.

🔑 **Cuando el síntoma aparece al final de una cadena, hay que instrumentar la
cadena entera.** Yo llevaba dos intentos arreglando el último eslabón, que era el
único que funcionaba bien.

### La causa: un cajón fijo por línea

```c
#define MAX_TEXTO 72          // 71 caracteres + fin de cadena
char texto[MAX_TEXTO];        // uno por línea
```

Medido sobre 10 canciones reales:

| | Máximo | Media |
|---|---|---|
| Líneas por canción | 56 | 42 |
| Caracteres por línea | **90** | 61 |
| **Texto total** | **2.237 B** | 1.422 B |

El reparto en cajones fijos **desperdiciaba y se quedaba corto a la vez**: 90
líneas × 72 bytes = 6.480 bytes reservados, y aun así una línea de 90 caracteres
no cabía.

### La solución: un almacén compartido

Todas las líneas seguidas en un solo buffer, y cada una apunta a dónde empieza:

```c
long     ms[160];        //   640 B — el tiempo de cada línea
uint16_t inicio[160];    //   320 B — dónde empieza su texto
char     pool[6000];     // 6.000 B — todas las líneas, una tras otra
```

| | Antes | Ahora |
|---|---|---|
| Memoria | 6.840 B | **6.960 B** (+120) |
| Líneas máximas | 90 | **160** |
| Caracteres por línea | 71 | **sin límite** |

Por 120 bytes más: **78% más líneas y ninguna se corta**. Y el texto de la canción
más larga medida ocupa 2.237 de los 6.000 disponibles — margen de 2,7×.

Verificado con la frase que fallaba:

```
[Frase] 90 car, 4 lineas
        "Honey, honey get the kids in the car now, put the cash in the trunk, get the keys let's go"
        [0] "Honey, honey get the"      [2] "the cash in the trunk,"
        [1] "kids in the car now, put"  [3] "get the keys let's go"
```

🔑 **Cuando los elementos son de tamaño muy variable, darle a cada uno un hueco
fijo desperdicia y se queda corto A LA VEZ.** Un almacén común se adapta solo: lo
que no gasta una línea corta queda disponible para una larga.
