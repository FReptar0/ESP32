# La canción se atascaba al saltar durante una búsqueda

- **Fecha:** 2026-08-29
- **Reportado:** *"si cambias mientras aún no carga la letra, la canción se atora en la anterior"*

---

## Cuánto bloqueaba

| Operación | Peticiones | Tiempo |
|---|---|---|
| `consultar()` | 1 | ~1,2 s |
| Letra, camino normal | 1 | ~1,2 s |
| Letra, camino lento | 1 + 1 + 3 | **~8 s** |
| `precargarSiguiente()` | 2 | ~3 s |

Todo eso ocurría **encadenado, sin comprobar nada entre medias**. En el peor caso
el núcleo de red pasaba **más de 10 segundos** sin preguntarle nada a Spotify.

Y como el cambio de canción **solo se detecta preguntando**, durante ese rato el
aparato era ciego: seguía mostrando la canción anterior con toda naturalidad.

## No había ninguna cancelación

Confirmado buscando en el código: `buscar()` no tenía ni una sola comprobación de
si lo que estaba haciendo seguía interesando. Una vez empezaba, iba hasta el
final.

Es un descuido fácil de cometer: en un ordenador, 8 segundos de trabajo de fondo
no molestan a nadie. En un aparato con **un solo hilo de red**, monopolizan el
único canal que tiene para enterarse de las cosas.

## La solución: una cola de pasos, no un bloque

**No se puede abortar una petición HTTP a medias** — o mejor dicho, se puede, pero
complica mucho el código. La clave está en otro sitio: **no encadenar peticiones
sin comprobar entre ellas**.

La búsqueda pasa de ser un bloque a ser una **cola de pasos**, y el bucle de red
hace **un paso por vuelta**:

```mermaid
graph LR
    C1["consultar()"] --> P1["paso: exacta"]
    P1 --> C2["consultar()"]
    C2 --> P2["paso: buscar<br/>candidatos"]
    P2 --> C3["consultar()"]
    C3 --> P3["paso: probar<br/>candidato 1"]
    P3 --> C4["consultar()"]
    C4 --> P4["..."]

    style C1 fill:#2d4a2d,color:#fff
    style C2 fill:#2d4a2d,color:#fff
    style C3 fill:#2d4a2d,color:#fff
    style C4 fill:#2d4a2d,color:#fff
```

Entre cada paso vuelve a preguntarle a Spotify. Si la canción cambió:

```
[Letra] ⛔ cancelada: cambió la canción
```

y lo que quedaba se descarta.

**Nunca hay más de UNA petición sin comprobar si sigue interesando.** El retraso
máximo pasa de ~10 s a ~2,4 s.

## Tres detalles que hicieron falta

**1. El resultado solo se publica si sigue valiendo.** Aunque un paso termine
bien, antes de mostrarlo se comprueba que la canción no haya cambiado. Si cambió,
el resultado se tira: es la letra de otra canción.

**2. La precarga no se lanza si hay una búsqueda en curso.** Si no, se
encadenarían dos operaciones lentas seguidas — volviendo al problema original.

**3. Tras la precarga se fuerza una consulta inmediata.** La precarga sí bloquea
varios segundos (y ahí es aceptable: la canción actual ya está en pantalla), así
que al terminar se pregunta enseguida por si hubo un cambio.

---

## ⚠️ El primer arreglo estaba a medias

Con lo anterior implementado, el problema **seguía**. Y la razón es que solo
partí en pasos **la mitad** del trabajo.

La búsqueda de la canción actual quedó bien. Pero `precargarSiguiente()` seguía
siendo un bloque entero:

```
1 petición (cola de reproducción)   ~1,2 s
+ buscar() COMPLETA                 hasta ~8 s
                                   ---------
                                     ~9,2 s de ceguera
```

Y lo peor: **se lanza justo después de cargar la letra de la canción nueva** — o
sea, exactamente en el momento en que uno tiende a saltar otra vez.

Yo lo había razonado así: *"la precarga puede bloquear, porque la canción actual
ya está en pantalla"*. El razonamiento era falso. **Da igual para qué sea la
petición: mientras el hilo de red esté ocupado, el aparato es ciego.**

🔑 **Al arreglar un bloqueo hay que mirar TODAS las operaciones lentas del mismo
hilo, no solo la que se estaba investigando.** Dejar una sin tocar deja el
problema entero.

## La solución definitiva: una sola máquina de pasos

Ahora **todas** las peticiones lentas —la letra de la canción actual y la
precarga de la siguiente— pasan por la misma máquina de estados, y cada vuelta
del bucle hace **exactamente una petición**:

```mermaid
stateDiagram-v2
    [*] --> NADA
    NADA --> EXACTA: cambió la canción
    NADA --> COLA: toca precargar
    COLA --> EXACTA: ya sé cuál es la siguiente
    EXACTA --> [*]: encontrada ✅
    EXACTA --> CANDIDATOS: no está con esa duración
    CANDIDATOS --> PROBAR: hay candidatos
    CANDIDATOS --> [*]: ninguno
    PROBAR --> PROBAR: probar el siguiente
    PROBAR --> [*]: encontrada o agotados
```

Entre cada paso, el bucle vuelve a consultar a Spotify. **El retraso máximo pasa
a ser el de una sola petición.**

Verificado saltando de canción varias veces seguidas:

```
🎵 Staring — Tipling Rock      → ✅ 54 líneas
🎵 Sue Me — Sabrina Carpenter  → ✅ 58 líneas
🎵 HOLIDAY — Lil Nas X         → ✅ 51 líneas
```

Ninguna se quedó atascada.

## La lección

🔑 **Cualquier operación larga en el mismo hilo que detecta cambios tiene que
poder cancelarse — o partirse en trozos.**

Y la forma más simple de conseguirlo casi nunca es "cancelar" de verdad, sino
**no encadenar**: hacer un trozo, mirar si sigue teniendo sentido, hacer el
siguiente. Sale mucho más barato que abortar una petición a medias, y el efecto
es el mismo.
