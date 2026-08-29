# ADR-0003 — De dónde salen las letras de las canciones

- **Estado:** `Propuesto` ⚠️ *(no se decide hasta la fase 5 — falta verificar)*
- **Fecha:** 2026-08-29
- **Reemplaza a:** —
- **Relacionado con:** RF-04, ADR-0002

---

## Contexto

El requerimiento a futuro es mostrar la letra de la canción que está sonando.

Aquí hay una sorpresa que conviene saber desde ahora, porque cambia el plan:

> **La API pública de Spotify no da letras.**

Las letras que ves en la app de Spotify vienen de un proveedor externo
(Musixmatch), a través de un endpoint interno que no forma parte de la API
pública ni está permitido usar desde fuera de la app.

O sea: aunque el ESP32 logre hablar con Spotify perfectamente, **de ahí no van a
salir las letras**. Hace falta una segunda fuente, distinta, y hay que cruzar la
canción de Spotify con la letra de esa otra fuente.

Este ADR existe para que esa sorpresa esté documentada desde el día uno y no te
la encuentres a mitad del proyecto.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- La Web API pública de Spotify no expone letras.
- Las letras vienen en dos sabores muy distintos:
  - **Letra plana** — el texto completo, sin marcas de tiempo.
  - **Letra sincronizada** (formato `.lrc`) — cada línea lleva su marca de
    tiempo: `[01:23.45] línea de la canción`. Esto es lo que permite que la
    pantalla vaya cambiando de línea al ritmo de la canción.
- Spotify **sí** devuelve el `progress_ms` (en qué milisegundo va la
  reproducción). Ese dato es justamente lo que hace falta para sincronizar una
  letra `.lrc`. Así que la mitad del rompecabezas ya la tenemos.

**Lo que asumimos sin verificar (todo esto es la fase 5):**

- ⚠️ Que LRCLIB sigue en línea, es gratis y no pide autenticación.
- ⚠️ Qué tanta cobertura tiene para la música que escuchas. Para música muy
  nicho puede simplemente no haber letra.
- ⚠️ Que se puede cruzar una canción de Spotify con la letra correcta usando
  artista + título + álbum + duración, sin equivocarse de versión (en vivo,
  remaster, edición deluxe…).
- Que una letra completa cabe en la memoria del ESP32. Una canción puede tener
  40+ líneas; probablemente haya que ir pidiendo por trozos.

**Lo que quedó pendiente de averiguar:**

- Si la sincronización se siente bien en la práctica o se desfasa.
- Cuántos caracteres caben realmente en la pantalla que acabemos usando. **Este
  es un problema mayor de lo que parece** — ver "Lo que hay que vigilar".

## Opciones consideradas

### Opción A — LRCLIB (`lrclib.net`)

- **A favor:** gratis, abierto, **sin autenticación** (no hay OAuth que
  implementar, que es justo la parte más pesada para un ESP32). Devuelve letra
  plana **y** sincronizada. Se busca por artista + título + álbum + duración, que
  es exactamente lo que Spotify entrega.
- **En contra:** cobertura desigual, es un proyecto comunitario. Depende de que
  el servicio siga en pie.
- **Costo:** bajo.

### Opción B — API oficial de Musixmatch

- **A favor:** es la fuente que usa Spotify. Cobertura enorme y de calidad.
- **En contra:** requiere API key. Su plan gratuito históricamente devuelve solo
  un **fragmento** de la letra (no la completa), y la letra sincronizada es de
  pago. Para el caso de uso, un fragmento no sirve.
- **Costo:** medio o alto, según el plan.

### Opción C — Genius

- **A favor:** cobertura excelente, sobre todo en hip-hop, y anotaciones.
- **En contra:** su API oficial **no devuelve el texto de la letra** — solo
  metadatos y la URL de la página. Obtener el texto implicaría hacer scraping de
  la página web, lo que va contra sus términos de servicio. Y no tiene marcas de
  tiempo, así que no se puede sincronizar.
- **Costo:** descartada por los términos de servicio.

### Opción D — Letras precargadas a mano

Guardar en el ESP32 las letras de un puñado de canciones favoritas.

- **A favor:** cero dependencias externas. Funciona siempre.
- **En contra:** no escala más allá de unas pocas canciones. Pero **es una
  excelente opción para la primera prueba**: permite resolver el problema difícil
  (cómo se ve una letra en una pantalla chiquita y cómo se sincroniza) sin
  depender de que ninguna API funcione.
- **Costo:** muy bajo.

## Decisión

⚠️ **Pendiente hasta la fase 5.** No se decide todavía porque la información
clave no está verificada.

**La dirección propuesta, en dos pasos:**

1. **Primero, opción D** — una o dos letras metidas a mano en el código, para
   resolver el problema visual: cuántos caracteres caben, cómo se corta el texto,
   cómo se sincroniza con `progress_ms`, si se lee cómodo. Este es el problema
   difícil de verdad y no necesita ninguna API para atacarse.
2. **Después, opción A (LRCLIB)** — una vez que sepamos que la letra *se ve
   bien*, conectarla a una fuente real.

El motivo de partirlo así es el mismo del ADR-0002: no pelear dos problemas
difíciles a la vez. "Conseguir la letra" y "mostrar la letra bien en una pantalla
diminuta" son problemas independientes, y el segundo se puede resolver primero
sin depender de nada.

## Consecuencias

**Lo que esto nos habilita:**

- Se puede avanzar en la parte de letras **sin haber resuelto la API**, lo que
  desbloquea trabajo en paralelo.

**Lo que hay que vigilar:**

- ⚠️ **El tamaño de la pantalla va a ser el cuello de botella real, no la API.**
  En una OLED típica de 0.96" (128×64 píxeles) caben aproximadamente 4 líneas de
  ~20 caracteres. Un verso de una canción rara vez cabe en 20 caracteres. Esto
  puede terminar decidiendo qué pantalla comprar — y por eso conviene tener este
  ADR **antes** de comprarla.
- La cobertura de LRCLIB para tu música en concreto. Vale la pena probar a mano,
  desde el navegador, con 5 canciones que sí escuches, antes de programar nada.
- Que el desfase de sincronización se mantenga tolerable. El `progress_ms` de
  Spotify llega con retraso de red, así que va a haber algo de desfase siempre.

## Cómo revertir

**Barato**, si el código se estructura bien. Si la parte que dibuja la letra
recibe una lista de líneas con sus tiempos, sin saber de dónde salió, cambiar de
fuente es cambiar una función. Es la misma separación que propone el ADR-0002:
*de dónde vienen los datos* separado de *cómo se dibujan*.
