# ADR-0012 — La pantalla oculta se abre con `↑ ↑ ↓ ↓`, la mitad del Código Konami

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-30
- **Reemplaza a:** —
- **Relacionado con:** RF-15 (pantalla oculta de estado), RF-16 (que encontrarla
  sea un regalo), [ADR-0007](ADR-0007-joystick-como-control.md) (el joystick es
  el único control)

---

## Contexto

El aparato va a ser **un regalo**. La idea de RF-15/RF-16 es que además de las
cuatro pantallas visibles haya **una escondida**: muestra el estado interno (RAM,
WiFi, versión, tiempo encendido) y, al abrirse, da una pequeña sorpresa y un
`"Te quiero!!"` al salir.

Para que exista una pantalla oculta hace falta primero decidir **cómo se abre**. Y
esa combinación tiene que cumplir dos cosas que tiran en direcciones opuestas:

1. **Improbable sin querer.** Si la abre un manotazo, deja de ser un secreto y
   pasa a ser un susto: alguien que no sabe qué es creerá que rompió el aparato.
2. **Fácil de repetir cuando la conoces.** Es un regalo; quien lo reciba tiene
   que poder enseñárselo a otro sin practicar.

La restricción de material está fijada desde el [ADR-0007](ADR-0007-joystick-como-control.md):
**el joystick es el único control**. No hay botones extra. Lo único disponible
son cuatro direcciones y un pulsador (clic corto y clic largo).

Y hay una restricción de diseño heredada, que resultó ser la que decidió: en el
firmware actual **arriba y abajo cambian de pantalla**, e **izquierda y derecha
saltan de canción**. Cualquier combinación que se elija va a ir *haciendo cosas*
mientras se escribe.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- Los seis gestos ya están implementados y medidos: `GESTO_ARRIBA`, `ABAJO`,
  `IZQUIERDA`, `DERECHA`, `CLIC` y `CLIC_LARGO` (`firmware/src/joystick.h`).
- El joystick exige **volver al centro entre gesto y gesto** ("un gesto por
  empujón"), así que una secuencia son empujones deliberados, no un barrido.
- Las cuatro pantallas están en un bucle de longitud 4 (`N_MODOS`). Por lo tanto
  **subir dos y bajar dos devuelve exactamente a la pantalla de partida.**
- Saltar de canción **sí** cuesta red: mediana 1,5 s con picos de 5–6 s, medido
  en [`controles-joystick-latencia.md`](../5-investigacion/controles-joystick-latencia.md).
  Cambiar de pantalla **no** cuesta red: es puro dibujo.

**Lo que asumimos sin verificar:**

- Que **1200 ms entre gesto y gesto** separa bien "escribir un código con ritmo"
  de "navegar mirando cada pantalla". La intuición es que quien navega *se para a
  mirar*, y quien escribe un código *no se para*. **Sin medir con una persona
  usándolo.**
- Que quien reciba el regalo reconozca el guiño. Si no lo reconoce no se pierde
  nada —sigue siendo una combinación cómoda—, pero se pierde la gracia.

**Lo que quedó pendiente de averiguar:**

- Si `↑↑↓↓` se dispara sin querer en uso normal. El caso realista es **pasarse
  dos pantallas y volver dos**, que produce esa secuencia exacta. Hay que usarlo
  un rato y ver si pasa.

## Opciones consideradas

### Opción A — Clic largo (~1 s) en una pantalla concreta

- **A favor:** `GESTO_CLIC_LARGO` ya está implementado y probado. Coste cero.
- **En contra:** el clic corto es **pausar**. Alguien que pause con la mano
  pesada abre la pantalla oculta sin querer, y ese es justo el susto que RF-16
  quiere evitar. Además no tiene ninguna gracia.
- **Costo:** ninguno, pero falla el requisito 1.

### Opción B — El Código Konami completo: `↑↑↓↓←→←→` + clic

- **A favor:** el guiño entero e inconfundible. Prácticamente imposible por azar.
- **En contra:** los `←→←→` **saltan cuatro canciones mientras lo escribes**, y
  cada salto es una petición a Spotify de 1,5–5 s. Para evitarlo habría que hacer
  que el firmware detectara "estoy a mitad de secuencia" y **no obedeciera** los
  gestos — es decir, tocar el camino de las órdenes de red.
- **Costo:** alto, y **en el sitio equivocado**. El reparto de red es exactamente
  donde este proyecto ya se rompió una vez y hubo que revertir entero
  ([`cancelacion-de-busquedas.md`](../5-investigacion/cancelacion-de-busquedas.md)).
  Nueve gestos también son muchos para "enséñaselo a tu madre".

### Opción C — Mantener izquierda + clic

- **A favor:** un solo gesto compuesto, difícil de lograr por azar.
- **En contra:** hay que leer **dos entradas simultáneas**, y el joystick está
  construido sobre la premisa contraria: "solo gana un eje" y "un gesto por
  empujón". Habría que abrir esa lógica, que hoy es simple y está medida.
- **Costo:** medio, y ensucia el componente mejor aislado del firmware.

### Opción D — `↑ ↑ ↓ ↓`, la mitad del Código Konami *(elegida)*

- **A favor:** el guiño se reconoce igual (es el arranque del código, la parte
  que todo el mundo recuerda). Cuatro gestos se hacen en dos segundos. Y sobre
  todo: **solo usa arriba/abajo, que no tocan la red.**
- **En contra:** más fácil de conseguir por accidente que las otras. El caso
  concreto: pasarse dos pantallas y corregir volviendo dos.
- **Costo:** bajo. No toca el joystick ni el reparto de red: solo mira los gestos
  que ya llegan.

## Decisión

**Elegimos la opción D: `↑ ↑ ↓ ↓`.**

El factor que pesó más que todos los demás no fue la elegancia del guiño, sino
**qué parte del firmware hay que tocar para conseguirlo.** Arriba y abajo son los
únicos gestos que no piden nada a la red: se resuelven enteros en el núcleo del
dibujo. Una combinación hecha solo con ellos se puede detectar mirando los gestos
que ya pasan por ahí, **sin abrir el reparto de red ni la lógica del joystick** —
las dos partes frágiles y ya medidas de este aparato.

Y hay un regalo escondido en la aritmética: como el bucle de pantallas tiene
longitud 4, **subir dos y bajar dos deja la pantalla exactamente donde estaba.**
La secuencia se escribe sin dejar rastro. Quien la haga sin querer no acaba en un
sitio raro ni pierde lo que estaba viendo.

Sobre el riesgo del disparo accidental: se acepta a propósito. **La consecuencia
de equivocarse aquí es benigna** — se abre una pantalla que dice explícitamente
que no ha roto nada y que se sale con un clic. Eso es literalmente el RF-16. Un
falso positivo en este aparato no cuesta nada; un secreto imposible de repetir sí.

## Consecuencias

**Lo que esto nos habilita:**

- Cerrar RF-15 y RF-16 sin tocar la red ni `joystick.h`.
- Un patrón reutilizable: si algún día hace falta otra combinación, el detector
  de secuencias ya está y solo cambia la tabla.

**Lo que esto nos cierra o nos cuesta:**

- El guiño queda a medias. Si alguien quisiera el código completo habría que
  resolver antes el problema de los `←→` (ver opción B).
- Aparece un parámetro de tiempo (`MS_ENTRE_PASOS_SECRETOS`, 1200 ms) que es un
  **supuesto sin medir**. Es la pieza más frágil de esta decisión.

**Lo que hay que vigilar:**

- 🚩 **Que la pantalla oculta se abra sola durante el uso normal.** Si pasa más de
  una vez en una tarde, la ventana de 1200 ms es demasiado ancha: bajarla a
  ~800 ms antes de plantearse cambiar la combinación entera.
- 🚩 **Que cueste abrirla a propósito.** Si hace falta más de un intento, la
  ventana es demasiado estrecha: subirla.
- 🚩 Si algún día se cambia la navegación de pantallas (por ejemplo, añadir una
  quinta), **`↑↑↓↓` deja de volver al punto de partida.** La invisibilidad de la
  secuencia depende de que `N_MODOS` sea 4. Está anotado en el código.

## Cómo revertir

**Barato: una tabla de cuatro elementos en `main.cpp`.**

```c
const Gesto COMBINACION_SECRETA[] = { GESTO_ARRIBA, GESTO_ARRIBA,
                                      GESTO_ABAJO,  GESTO_ABAJO };
```

Cambiar la combinación es cambiar esa lista (y `MS_ENTRE_PASOS_SECRETOS` si el
ritmo no encaja). El detector no sabe qué secuencia está buscando: funciona con
cualquiera que use gestos que no pidan red.

Pasar al Código Konami completo **no** es revertir esto: es el trabajo de la
opción B, y necesitaría su propio ADR porque toca el camino de las órdenes.
