# Metodología de desarrollo del taller

Todo proyecto de este taller pasa por las mismas 6 fases. No son burocracia: cada
una existe para evitar un error caro concreto.

La estructura de carpetas **es** la metodología — las carpetas están numeradas
para que el orden se vea solo:

```
proyectos/<nombre>/
├── 1-decisiones/       ← ADRs: por qué hicimos lo que hicimos
├── 2-entorno/          ← el entorno aislado y cómo levantarlo
├── 3-componentes/      ← qué es cada pieza y cómo se comporta
├── 4-requerimientos/   ← qué tiene que hacer el aparato
├── 5-investigacion/    ← lo que hubo que averiguar antes de construir
├── 6-esquemas/         ← el mapa: físico y lógico, de punta a punta
├── firmware/           ← el código
└── BITACORA.md         ← qué pasó cada sesión de trabajo
```

Las fases **no son una cascada**. Vas a saltar entre ellas todo el tiempo: una
investigación cambia un requerimiento, un componente que llega distinto obliga a
una decisión nueva. Eso está bien. Lo que no está bien es que ese ida y vuelta no
quede escrito.

---

## Fase 1 — Decisiones (ADR)

**Carpeta:** `1-decisiones/`

Un **ADR** (*Architecture Decision Record*, registro de decisión de arquitectura)
es un documento corto, de una página, que captura **una** decisión: qué se
decidió, qué alternativas había, y sobre todo **por qué**.

### Por qué te va a servir tanto

Dentro de dos meses vas a mirar tu propio proyecto y pensar *"¿por qué usé este
pin y no aquel?"*. Sin ADR, la respuesta se perdió. Con ADR, la lees en 30
segundos.

Y más importante para ti que estás aprendiendo: los ADRs registran lo que **no
sabías** al decidir. Cuando algo falle, vas a poder distinguir *"me equivoqué al
decidir"* de *"decidí bien con la información que tenía, y la información cambió"*.
Son problemas muy distintos.

### Un ADR nunca se borra ni se reescribe

Esta es la parte que hace que el sistema funcione. Los proyectos evolucionan y
las decisiones se revierten — pero **el registro de la decisión vieja se
conserva**.

Cuando cambias de opinión no editas el ADR viejo: escribes uno nuevo que lo
reemplaza.

```
ADR-0003  Estado: Reemplazado por ADR-0007
             ↑ (el ADR-0003 sigue ahí, íntegro, con su razonamiento original)

ADR-0007  Estado: Aceptado
          Reemplaza a: ADR-0003
          "El ADR-0003 asumía que el OLED sería suficiente. Al probar
           las letras nos dimos cuenta de que 128x64 no alcanza..."
```

Así conservas la cadena completa de razonamiento. Puedes hacer *backtrack*: ver
en qué punto exacto el proyecto tomó cada camino, y qué sabías tú en ese momento.

### Estados posibles de un ADR

| Estado | Significa |
|---|---|
| `Propuesto` | Está sobre la mesa, aún no se decide |
| `Aceptado` | Es la decisión vigente |
| `Reemplazado por ADR-XXXX` | Ya no aplica; hay una decisión más nueva |
| `Rechazado` | Se consideró y se descartó (útil: evita volver a discutirlo) |
| `Obsoleto` | Dejó de ser relevante (p. ej. se eliminó esa parte del proyecto) |

**Plantilla:** [`plantillas/ADR-plantilla.md`](plantillas/ADR-plantilla.md)

---

## Fase 2 — Configuración del entorno

**Carpeta:** `2-entorno/`

Cada proyecto tiene su **entorno aislado**: sus propias herramientas, sus propias
versiones de librerías, sin compartir nada con los demás proyectos.

### Por qué aislado

Imagina que el Tamagotchi funciona con la versión 6.21 de una librería de
pantallas. Meses después, otro proyecto necesita la versión 7. Si las librerías
fueran compartidas, actualizar rompería el Tamagotchi sin que te enteres — y lo
descubrirías el día que quieras cambiarle algo.

Con entornos aislados, cada proyecto congela sus versiones. Puedes volver a un
proyecto de hace un año, compilarlo, y funciona igual que el último día.

En la práctica el aislamiento tiene dos capas:

1. Un **entorno virtual de Python** (`.venv`) por proyecto, con las herramientas
   de compilación y flasheo fijadas a una versión.
2. Un archivo **`platformio.ini`** que fija la placa, el framework y cada librería
   con su versión exacta.

**Qué va en esta carpeta:** las notas de cómo se levanta el entorno, qué se
instaló y por qué, y cualquier problema que hayas resuelto para dejarlo andando.

---

## Fase 3 — Entendimiento de los componentes

**Carpeta:** `3-componentes/`

Una ficha por cada pieza física del proyecto. **Antes** de conectarla.

### Por qué esta fase existe

Casi todo el daño en electrónica de hobby viene de conectar algo sin haber leído
qué esperaba ese algo. Voltaje equivocado, pin equivocado, resistencia que
faltaba. La ficha es el filtro que atrapa eso.

Cada ficha responde, como mínimo:

- **¿A qué voltaje trabaja?** Alimentación y voltaje lógico son cosas distintas.
  El ESP32 es 3.3 V y sus pines **no toleran 5 V**.
- **¿Cuánta corriente consume?** Si el pico supera lo que tu fuente da, el
  aparato se reinicia solo y vas a perder horas buscando un bug de software que
  no existe.
- **¿Cómo se comunica?** I2C, SPI, UART, analógico, digital simple.
- **¿Qué pines del ESP32 necesita?** Y de esos, ¿alguno es de los "especiales"?
  (Hay pines del ESP32 que se comportan raro al arrancar o que directamente no
  sirven como salida.)
- **¿Necesita algo extra?** Resistencias pull-up, capacitor de desacoplo, etc.

**Plantilla:** [`plantillas/componente-plantilla.md`](plantillas/componente-plantilla.md)

---

## Fase 4 — Recabar requerimientos

**Carpeta:** `4-requerimientos/`

Qué tiene que hacer el aparato, escrito de forma que se pueda **verificar**.

### La diferencia que importa

> ❌ *"Que muestre la canción"*
> ✅ *"Al cambiar de canción en Spotify, la pantalla muestra el nuevo título en
>    menos de 5 segundos, sin que yo toque nada."*

El segundo se puede probar. El primero no: no sabes si ya terminaste.

Cada requerimiento lleva un ID (`RF-01` funcional, `RNF-01` no funcional) y una
prioridad. Los IDs sirven para que el código, los esquemas y los ADRs puedan
apuntar al requerimiento que justifican.

También se registra explícitamente lo que **no** se va a hacer. En un proyecto de
descubrimiento como el Tamagotchi, esa lista es la que evita que el alcance
crezca para siempre y nunca termines nada.

**Plantilla:** [`plantillas/requerimientos-plantilla.md`](plantillas/requerimientos-plantilla.md)

---

## Fase 5 — Investigación

**Carpeta:** `5-investigacion/`

Lo que hubo que averiguar antes de poder construir, con las fuentes.

### Por qué se escribe en vez de solo buscarlo cuando haga falta

Porque el hardware y las APIs cambian, y la información en internet se pudre. Un
tutorial de 2019 puede describir una API que ya no existe. Si anotas *qué* leíste,
*dónde* y *cuándo*, cuando algo se rompa vas a poder distinguir *"lo implementé
mal"* de *"esto funcionaba y cambió".*

Cada nota de investigación incluye la fecha y el enlace. La fecha no es adorno:
en tres meses es lo que te dice si esa información sigue siendo confiable.

Aquí también viven los **spikes**: pruebas rápidas y desechables para responder
una pregunta concreta *(¿el ESP32 aguanta una conexión HTTPS a Spotify sin
quedarse sin memoria?)*. El código del spike se tira; la respuesta se queda.

---

## Fase 6 — Mapas y esquemas

**Carpeta:** `6-esquemas/`

El proyecto mapeado de punta a punta, en dos planos:

**El plano físico** — qué está conectado a qué. Diagrama de conexiones, tabla de
asignación de pines, cómo se alimenta cada cosa. Es lo que consultas antes de
tocar un cable, y lo que te salva de armarlo mal a las 11 de la noche.

**El plano lógico** — cómo fluye la información. De dónde salen los datos, qué
los transforma, dónde acaban. Es lo que consultas cuando algo *funciona* pero
muestra lo que no debe.

Los dos se escriben con **Mermaid**, que es texto plano que se dibuja solo. Eso
importa: al ser texto, vive junto al código, se versiona con Git, y se puede ver
exactamente qué cambió entre una versión y otra del diagrama. Un `.png` exportado
de una herramienta de dibujo no te da nada de eso.

---

## La bitácora

**Archivo:** `BITACORA.md` (en la raíz de cada proyecto)

Diario de trabajo. Una entrada por sesión: qué hiciste, qué funcionó, qué no, y
en qué te quedaste.

Es distinta de los ADRs: el ADR captura **una decisión**, la bitácora captura
**el trayecto**. Los intentos fallidos, las horas perdidas por un cable malo, la
sospecha que aún no confirmas. Cuando retomes el proyecto después de dos semanas,
la última entrada de la bitácora es lo primero que vas a querer leer.
