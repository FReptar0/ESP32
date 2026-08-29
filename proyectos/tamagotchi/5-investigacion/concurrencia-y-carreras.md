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

## 3. ✂️ Frases largas recortadas

*"And I know you're really mad at me, but you're on your way to some kind of
life"* aparecía cortada.

La zona de la letra mide **112 píxeles**. Con la fuente grande (28 px) caben
**4 renglones** justos; con la chica (20 px), **5**.

Estaban limitados a 3 y 4. Una frase de ~78 caracteres no cabía ni encogiendo, y
se recortaba con `...` — perdiendo el final, que muchas veces es lo que le da
sentido a la frase.

Subidos a 4 y 5, que es lo que físicamente cabe.
