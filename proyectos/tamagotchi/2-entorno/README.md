# Entorno de desarrollo — Tamagotchi

Decisión de fondo: [ADR-0001](../1-decisiones/ADR-0001-plataforma-de-desarrollo.md)
(PlatformIO + framework Arduino).

---

## Qué significa "entorno aislado" aquí

Que este proyecto tenga **sus propias herramientas y sus propias versiones**, sin
compartir nada con los demás proyectos del taller.

La analogía: es como tener una caja de herramientas por proyecto en vez de un
taller comunitario. Si en otro proyecto cambias un destornillador por uno nuevo,
este ni se entera.

El aislamiento tiene dos capas:

### Capa 1 — Entorno virtual de Python (`.venv/`)

Una carpeta dentro del proyecto que contiene su propia copia de Python y las
herramientas de compilación. Al "activarlo", el comando `pio` que se ejecuta es
el de **este** proyecto, no uno global del sistema.

Sin esto, actualizar PlatformIO para un proyecto nuevo podría romper este.

### Capa 2 — `platformio.ini`

El archivo que fija **qué placa, qué framework y qué versión exacta de cada
librería** usa el proyecto. Se ve así:

```ini
[env:esp32dev]
platform = espressif32@6.5.0    ; ← versión clavada, no "la última"
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
    bblanchon/ArduinoJson@6.21.3    ; ← también clavada
```

Ese `@6.21.3` es lo que hace que el proyecto se pueda recompilar en un año y
funcione idéntico. Sin la versión, PlatformIO bajaría "la más reciente" — que
para entonces será otra, con otra API, y el código no compilará.

Es exactamente el requerimiento
[RNF-04](../4-requerimientos/REQUERIMIENTOS.md#requerimientos-no-funcionales).

---

## Estado

| Paso | Estado |
|---|---|
| Entorno virtual creado (`.venv/`) | ✅ |
| PlatformIO instalado en el entorno | ✅ |
| `esptool` instalado (diagnóstico de la placa) | ✅ |
| `platformio.ini` escrito | ✅ `board = esp32dev` (ESP32-D0WD-V3 confirmado) |
| Placa detectada | ✅ `/dev/cu.usbserial-0001` (CP2102) |
| Primera compilación y carga | ✅ `[SUCCESS]` — 672 s (la mayor parte, descarga del toolchain) |
| Prueba de vida ejecutándose en la placa | ✅ |

> El `platformio.ini` se escribió **después** de confirmar la placa con `esptool`,
> no antes. Ese orden importa: poner un `board` equivocado compila igual pero
> luego falla de formas confusas. Ver [ficha del ESP32](../3-componentes/esp32-d0wd-v3.md).

---

## Cómo usarlo

Todos los comandos se corren desde `proyectos/tamagotchi/`.

**Activar el entorno** (una vez por cada terminal nueva):

```bash
source 2-entorno/.venv/bin/activate
```

Sabrás que funcionó porque el prompt cambia y aparece `(.venv)` al inicio.

**Salir:**

```bash
deactivate
```

### Comandos del día a día

| Comando | Qué hace |
|---|---|
| `pio run` | Compila. No toca la placa |
| `pio run -t upload` | Compila **y carga** el programa al ESP32 |
| `pio device list` | Lista los puertos serie — **el primer diagnóstico cuando algo falla** |
| `pio device monitor` | Abre el monitor serie: lo que el ESP32 imprime, lo ves aquí |
| `pio run -t clean` | Borra lo compilado y empieza de cero |
| `pio pkg install` | Instala las librerías del `platformio.ini` |

Para salir del monitor serie: `Ctrl+C`.

### Diagnóstico de la placa

```bash
esptool --port /dev/cu.usbserial-0001 chip-id
```

> Ojo: en versiones viejas el comando era `esptool.py`. Desde la v5 (la que
> instalamos) es `esptool`, sin el `.py`. Los tutoriales viejos usan la forma
> antigua.

Esto interroga directamente al chip. Si responde, la placa está viva y comunica —
independientemente de si tu código funciona o no. Es la prueba definitiva.

---

## Notas de instalación

**2026-08-29** — Entorno creado con Python 3.13.2 en macOS 26.5.2 (Apple Silicon).
Versiones instaladas: **PlatformIO Core 6.1.19**, **esptool 5.3.1**.

Verificación tras instalar: en el primer intento `pio device list` solo mostraba
los 3 puertos internos de macOS. Tras cambiar el cable USB apareció
`/dev/cu.usbserial-0001`. La causa del bloqueo está documentada en la
[bitácora](../BITACORA.md): eran cables USB-C de solo carga.

Se instalaron dos paquetes:

- **`platformio`** — el que compila y carga. Es la herramienta central según el
  [ADR-0001](../1-decisiones/ADR-0001-plataforma-de-desarrollo.md).
- **`esptool`** — utilidad oficial de Espressif para hablarle al chip a bajo
  nivel. Sirve para diagnosticar: preguntarle al ESP32 quién es, leer su MAC,
  borrarlo por completo. PlatformIO la usa por dentro, pero tenerla suelta permite
  distinguir "la placa no responde" de "mi código está mal" — que en este momento
  del proyecto es justo lo que hace falta.

> La primera vez que corras `pio run`, PlatformIO va a descargar el compilador
> para ESP32 (~1 GB). Eso va a `~/.platformio/`, fuera del proyecto, y se comparte
> entre proyectos porque es una herramienta, no una dependencia. Tarda un rato la
> primera vez y nunca más.
