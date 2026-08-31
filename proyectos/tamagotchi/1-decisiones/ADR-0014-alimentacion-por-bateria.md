# ADR-0014 — Elevar la batería a 5 V y entrar por `VIN`, no saltarse el regulador

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-30
- **Reemplaza a:** —
- **Relacionado con:** [C-006](../../../inventario/INVENTARIO.md) batería,
  C-007 cargador, C-008 interruptor,
  [`6-esquemas/alimentacion-por-bateria.md`](../6-esquemas/alimentacion-por-bateria.md),
  RNF-02 (brillo), y el criterio de **regalo** de `CLAUDE.md`

---

## Contexto

El Tamagotchi vive enchufado al USB del ESP32. Como **va a ser un regalo**, tiene
que funcionar sin cable: se busca que se encienda con un interruptor, se cargue
por USB y aguante una tarde.

El usuario aporta una **celda LiPo de 1000 mAh** ya soldada a un **TP4056 con
protección**, y un **interruptor deslizante** con el cable ya puesto en la pata
común.

Y aparece el problema que obliga a decidir: **ninguna de las dos conexiones
directas funciona.**

```
        4,2 V ┤ batería recién cargada
        3,7 V ┤ batería la mayor parte de su vida
        3,3 V ┤ ····· lo que el ESP32 necesita ·····
        3,0 V ┤ batería vacía
```

Los 3,3 V caen **en medio del rango**. La batería empieza por encima y acaba por
debajo.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- **`OUT+` del TP4056 no da 5 V.** Da la tensión de la celda, 3,0–4,2 V. El
  módulo no eleva nada: su USB es entrada de carga.
- La placa monta un **`AMS1117-3.3`**. ✅ **Leído en el chip el 2026-08-30**
  (`1117 3.3 BWQ53`), después de confundirlo dos veces con los transistores
  `J3Y` del auto-reset, que están al lado.
- Un `AMS1117` necesita **1,1 V de margen**: para dar 3,3 V quiere **4,4 V** a la
  entrada. La batería **nunca** los alcanza.
- El pin `3V3` aguanta **3,6 V como máximo**. Meterle los 4,2 V de la batería
  destruye el chip.
- El ESP32 pica a **500 mA** al transmitir (ya estaba medido y avisado).

**Lo que asumimos sin verificar:**

- Que el consumo medio del conjunto ronda **150 mA a 3,3 V**. De ahí salen las
  autonomías, y **son cálculo, no medida**.
- Que el interruptor aguante los picos. Se venden para 0,5 A y el pico del lado
  de la batería sale ~750 mA. **Sin medir.**

**Lo que quedó pendiente de averiguar:**

- La autonomía real, con el aparato montado.
- Si el `AMS1117` se calienta lo bastante como para que importe.

## Opciones consideradas

### Opción A — Elevador a 5 V (MT3608) hacia `VIN` *(elegida)*

- **A favor:** la batería entra **por donde entra el USB**. No se toca nada de la
  placa y todas sus protecciones siguen en su sitio. El módulo es el más común y
  barato que existe. Se puede usar el aparato mientras carga.
- **En contra:** doble conversión. El elevador rinde ~90 %, pero el `AMS1117` es
  **lineal** y quema como calor todo lo que baja de 5 V a 3,3 V: rinde 66 %. Total
  ~60 %. Autonomía estimada **4–5 h**.
- **Costo:** un módulo barato y ~40 % de la batería en calor.

### Opción B — Buck-boost a 3,3 V directo al pin `3V3`

- **A favor:** ~90 % de rendimiento y **6–8 h** estimadas. Menos calor.
- **En contra:** alimenta el chip **sin ninguna protección en medio**. Si el
  convertidor falla alto, mata el ESP32 — y un regalo no se puede depurar. Y
  obliga a no conectar nunca el USB del ESP32 a la vez, porque serían dos fuentes
  peleando por el mismo nodo. Módulo menos común y más caro.
- **Costo:** más autonomía a cambio de quitar la red de seguridad.

### Opción C — Conectar `OUT+` directamente

- Incluida porque es lo que uno intenta primero. A `3V3` **rompe la placa**; a
  `VIN` da un aparato que arranca y **se reinicia solo** al bajar de 4,4 V.
- **Costo:** no funciona, y una de las dos variantes destruye material.

### Opción D — Cambiar el `AMS1117` por uno de bajo margen

- **A favor:** cero componentes añadidos y máximo rendimiento.
- **En contra:** desoldar un chip de superficie de 3 patas y soldar otro, en la
  única placa que hoy funciona, por alguien que está aprendiendo.
- **Costo:** riesgo de quedarse sin el aparato entero para ahorrar un módulo.

## Decisión

**Elegimos la opción A.**

El factor que pesó por encima de todo es que **esto es un regalo**, y la regla del
proyecto ya lo dice: *la estabilidad pesa más que las funciones nuevas, y todo
fallo debe degradar, no romper*.

La opción B da un 60 % más de autonomía, y aun así se descarta: **entrar por
`3V3` es quitar la única protección que queda entre el convertidor y el chip.**
Cuando el aparato esté en manos de alguien que no puede diagnosticarlo, un fallo
que "solo" apaga el aparato es recuperable; uno que lo mata, no.

Entrar por `VIN` significa que la batería usa **exactamente el mismo camino que
el USB**, que lleva meses funcionando. No se estrena ningún camino nuevo hacia el
chip.

El precio es honesto y está aceptado: **cuatro o cinco horas en vez de seis u
ocho**, y un regulador tibio.

## Consecuencias

**Lo que esto nos habilita:**

- El aparato funciona sin cable y se carga por USB, incluso encendido.
- Nada cambia en la placa: se puede volver al USB en cualquier momento.

**Lo que esto nos cierra o nos cuesta:**

- **~40 % de la batería se va en calor** dentro del `AMS1117`.
- Autonomía estimada de 4–5 h. Si resultara insuficiente, la salida no es cambiar
  de convertidor sino **bajar el brillo** (RNF-02) o espaciar las consultas a
  Spotify, que hoy van cada 2 s.

**Lo que hay que vigilar:**

- 🚩 ⚠️ **El MT3608 es AJUSTABLE y no viene a 5 V de fábrica.** Hay que ponerlo a
  5 V con el polímetro y **con nada conectado a la salida**, antes de acercarlo al
  ESP32. Puede llegar ajustado a 20 V, y eso destruye la placa. Es el único paso
  de este montaje que puede matar material.
- 🚩 **El interruptor.** Se venden para 0,5 A y el pico del lado de la batería
  sale ~750 mA. Si se calienta al tacto, hay que cambiarlo por uno de más
  corriente. Los módulos MT3608 corrientes **no sacan el pin `EN`**, así que el
  interruptor tiene que llevar la corriente: la alternativa elegante no está
  disponible.
- 🚩 **Que el `AMS1117` queme.** Tibio es normal; que no se pueda tocar, no.

## Cómo revertir

**Muy barato.** Desconectar el elevador y volver a alimentar por el USB del
ESP32. No se ha modificado la placa ni se ha soldado nada en ella: todo el
montaje cuelga de `OUT+`/`OUT-` del cargador.

Pasarse a la opción B tampoco es revertir esto: es cambiar el convertidor y el
punto de entrada, y necesitaría su propio ADR.
