# El aparato se colgaba en silencio: pedía el mismo turno dos veces

- **Fecha:** 2026-08-30
- **Contexto:** el aparato se quedaba tieso —sin pantalla, sin música, sin
  responder al joystick— y solo revivía desenchufándolo
- **Confianza:** alta — reproducido tres veces y confirmado con la línea exacta

---

## El síntoma, y por qué costaba tanto

Nada. Literalmente nada:

```
[Joystick] ARRIBA
[Modo] -> portada
                     <- y aquí se acaba el registro
```

**Sin `rst:0x`. Sin `Guru Meditation`. Sin `Backtrace`.** Los tres rastros que
deja un ESP32 cuando algo va mal, y ninguno estaba.

🔑 **Esa ausencia es información, no falta de información.** Un reinicio deja
`rst:`, un fallo de programa deja `Guru Meditation`. Si no hay ninguno de los
dos, el aparato **no se rompió: se quedó esperando.** Eso reduce los sospechosos
a uno, y hay que leerlo así desde el principio en vez de buscar un fallo que no
existe.

## Dos hipótesis, y cómo se separaron

| | Hipótesis | Qué la apoyaba |
|---|---|---|
| 1 | **Abrazo mortal** entre los dos núcleos | Los dos se callaban a la vez |
| 2 | **Memoria pisada** por lo justa que iba | Bloque contiguo de 45.044 B pidiendo imágenes de 55.456 B |

Las dos se ven **idénticas desde fuera**. Pensar más no las separa; hay que medir.

### La medición que las separó

El turno de datos se pedía con espera infinita:

```c
xSemaphoreTake(mutexDatos, portMAX_DELAY)
```

Se le puso un plazo de 3 s **que no cambia el comportamiento**: si se agota,
escribe quién espera y **sigue esperando igual que antes**. El aparato se
comporta idéntico y de paso habla antes de callarse.

Al tercer intento saltó:

```
[TURNO] datos: 3s esperando · nucleo 1 · linea 901
[TURNO] datos: 3s esperando · nucleo 0 · linea 499
[TURNO] datos: 3s esperando · nucleo 0 · linea 520
```

Ninguno llegó a imprimir su `por fin`. **Hipótesis 1 confirmada, y con la línea
exacta.**

## La causa

```c
mutexDatos = xSemaphoreCreateMutex();     // NO reentrante
```

Un mutex no reentrante pedido **dos veces por la misma tarea** se bloquea contra
sí mismo. Y eso pasaba por una cadena de tres saltos donde no se veía:

```
loop()  [núcleo 1]
 └─ CON_DATOS({                        ← coge el turno
      pintarContenidoModo()
       └─ pintarPortada()
           └─ imagenQueTocaAhora()
               └─ CON_DATOS({ ... })   ← lo vuelve a pedir  ⛔
```

El núcleo 1 se cerraba la puerta por dentro. El núcleo 0 llegaba después a por el
mismo turno y se quedaba fuera. **Los dos, para siempre.**

🔑 **La petición anidada no estaba a la vista: estaba tres llamadas más abajo.**
Ninguno de los dos sitios que se colgaban menciona un turno en su código. Por eso
la revisión a ojo no lo encuentra — hay que seguir las llamadas.

## Por qué tardó semanas en aparecer

`pintarPortada()` solo entra en la parte peligrosa si se cumple:

```c
if (portadaLista && bufPortada) {                    // hay imagen cargada
    if (portadaEnBuffer != imagenQueTocaAhora()) {   // ← aquí muere
```

Hace falta **entrar en el modo portada teniendo ya una imagen recién bajada**.
Con un uso tranquilo no pasa nunca. Solo salió cuando alguien recorrió las
pantallas rápido, sin esperar a que la carátula terminara de cargar.

## El arreglo

**La portada se dibuja FUERA del turno.** `pintarContenidoModo()` ya no la pinta;
los dos sitios que la llamaban lo hacen después de soltar:

```c
CON_DATOS({ pintarContenidoModo(); ... });
if (modo == MODO_PORTADA) pintarPortada();     // fuera del turno
```

Se descartaron dos alternativas:

- **Turno reentrante** (`xSemaphoreCreateRecursiveMutex`): un cambio más pequeño,
  pero *tolera* el anidamiento en vez de impedirlo. Esconde el próximo.
- **Una versión sin turno de `imagenQueTocaAhora()`**: dos funciones casi iguales
  donde hay que acertar con cuál se llama. Es una trampa nueva.

## Lo que se dejó puesto para que no vuelva

**1. El plazo del turno se queda.** Ya no hay espera infinita muda: si algún día
vuelve a atascarse, el aparato lo dice con núcleo y línea antes de callarse.
Cuesta cero y convierte un silencio en un diagnóstico.

**2. Una comprobación automática de anidamientos.** Sigue las llamadas de forma
transitiva y avisa si alguien vuelve a pedir un turno dentro de otro:

```
=== PIDEN EL TURNO SIN QUE SE VEA (a traves de otra funcion) ===
   pintarContenidoModo
   pintarPortada
   volcarImagenSiLlego

=== ANIDAMIENTOS = CUELGUE GARANTIZADO ===
  linea 2409 -> pintarContenidoModo()
  linea 2467 -> pintarContenidoModo()
```

Después del arreglo da **cero**. Es lo que confirma que no quedaron más: había
exactamente dos, y los dos eran el mismo fallo.

## Dos errores míos por el camino, para no repetirlos

**1. Culpé al cambio que estaba haciendo.** Al colgarse justo después de cargar
una función nueva, lo lógico parecía que fuera culpa suya. **Lo era la anterior.**
Se descartó cargando el firmware viejo y reproduciéndolo: mismo cuelgue, misma
firma. Sin esa prueba habría estado horas buscando en el sitio equivocado.

🔑 **Que un fallo aparezca justo después de un cambio no prueba que sea del
cambio.** Prueba que el cambio te hizo usar el aparato de otra manera.

**2. Tomé un artefacto por una prueba.** El registro traía una línea partida y
repetida 5.485 veces, y lo di como señal de memoria pisada — *"un texto no se
parte solo"*. **Aparecía justo después de la basura de arranque, no cerca del
cuelgue.** En marcha normal el registro sale limpio. Era ruido del reinicio.

🔑 **Antes de usar una anomalía como prueba, mirar CUÁNDO ocurre.** La que sale
siempre al arrancar no explica algo que pasa a los tres minutos.

## Lo que sigue sin resolver

- **La memoria sigue justa.** No era la causa, pero es real: el bloque contiguo
  baja a 45.044 B mientras se piden imágenes de hasta 55.456 B. Todavía no ha
  fallado ninguna reserva (`[Portada] ❌ sin memoria` no ha salido nunca), pero
  el margen es estrecho.
- **Salir del modo portada con una descarga en marcha deja la imagen sin
  dibujar y sin liberar** hasta que se vuelve a entrar. No cuelga nada —eso era
  el turno anidado— pero son ~55 KB retenidos sin motivo. **Supuesto, no
  medido:** que se libere siempre al volver a entrar.
