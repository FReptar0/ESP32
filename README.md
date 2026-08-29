# 🔧 Taller de electrónica

Espacio de trabajo para proyectos de electrónica e IoT. Cada proyecto tiene su
carpeta, su entorno aislado y sigue la misma metodología de 6 fases.

**Objetivo:** aprender electrónica e IoT construyendo cosas, dejando registro de
por qué se hizo cada cosa para poder volver sobre los pasos.

---

## Mapa del taller

```
ESP32/
├── metodologia/        📐 Las 6 fases + plantillas
├── inventario/         📦 Qué material hay, en qué estado (compartido por todos)
├── proyectos/          🔨 Un proyecto por carpeta
│   └── tamagotchi/
└── herramientas/       ⚙️ Scripts del taller
```

| Carpeta | Para qué |
|---|---|
| [`metodologia/`](metodologia/) | Cómo se trabaja aquí. **Léelo primero.** |
| [`inventario/`](inventario/) | Inventario general de componentes + su bitácora — **3 ESP32 y 4 pantallas TFT** |
| [`proyectos/`](proyectos/) | Los proyectos |
| [`herramientas/`](herramientas/) | `nuevo-proyecto.sh` y utilidades |

---

## Proyectos

| Proyecto | Qué es | Estado |
|---|---|---|
| [🐣 tamagotchi](proyectos/tamagotchi/) | Aparato con ESP32 que muestra qué suena en Spotify | 🟢 **Funcionando** · muestra canción, artista, álbum y progreso |

---

## La metodología, en corto

Seis fases, y la estructura de carpetas de cada proyecto **es** la metodología:

| # | Fase | Carpeta | Para qué existe |
|---|---|---|---|
| 1 | **Decisiones (ADR)** | `1-decisiones/` | Que dentro de un año sepas *por qué* hiciste cada cosa |
| 2 | **Entorno** | `2-entorno/` | Que el proyecto se pueda recompilar en un año y funcione igual |
| 3 | **Componentes** | `3-componentes/` | No quemar piezas por conectarlas sin leer qué esperaban |
| 4 | **Requerimientos** | `4-requerimientos/` | Saber cuándo terminaste |
| 5 | **Investigación** | `5-investigacion/` | Distinguir "lo hice mal" de "esto cambió" |
| 6 | **Esquemas** | `6-esquemas/` | Tener el mapa antes de tocar un cable |

Detalle completo en [`metodologia/README.md`](metodologia/README.md).

**No es una cascada.** Vas a saltar entre fases todo el tiempo — una
investigación cambia un requerimiento, un componente que llega distinto obliga a
una decisión nueva. Eso está bien. Lo que no está bien es que ese ida y vuelta
no quede escrito.

---

## Empezar un proyecto nuevo

```bash
./herramientas/nuevo-proyecto.sh nombre-del-proyecto
```

Crea la estructura de 6 fases, copia las plantillas y prepara el entorno aislado.

O simplemente dime *"quiero empezar un proyecto de X"* y yo lo armo.

---

## Dos reglas que se aplican siempre

**El inventario es uno solo, compartido.** No hay inventario por proyecto. El
ESP32 es físicamente uno: si el Tamagotchi lo está usando, no está disponible
para otro proyecto, y un inventario por proyecto te haría creer que sí.

**Lo dañado no cuenta.** Un componente marcado `Dañado` se ignora por completo al
planear. La única forma de que vuelva a contar es que tú declares en el
[`LOG.md`](inventario/LOG.md) que lo vas a reparar o reponer. Esa decisión es
tuya, no mía.

---

## Convenciones

- **Todo en español**, incluidos comentarios del código.
- **Fechas absolutas** (`2026-08-29`), nunca "la semana pasada".
- **Diagramas en Mermaid**, no imágenes. Al ser texto, se versionan y se puede
  ver qué cambió entre dos versiones de un diagrama.
- **Nada de secretos en el repositorio.** Contraseñas de WiFi y tokens van en
  archivos ignorados por Git.
- **Los ADRs no se borran ni se reescriben.** Cuando una decisión cambia, se
  escribe uno nuevo que reemplaza al viejo.
