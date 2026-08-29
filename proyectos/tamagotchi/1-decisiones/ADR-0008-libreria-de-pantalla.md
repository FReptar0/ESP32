# ADR-0008 — Usar `TFT_eSPI`, configurada desde `platformio.ini`

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-29
- **Relacionado con:** [ADR-0004](ADR-0004-pantalla-gmt130.md), RF-04, RNF-01, RNF-04

---

## Contexto

Hay que elegir con qué librería se dibuja en la GMT130 (ST7789, 240×240, SPI, sin
pin `CS`). Las dos candidatas para ST7789 en ESP32 son `TFT_eSPI` de Bodmer y el
par `Adafruit_ST7789` + `Adafruit_GFX`.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- El controlador es **ST7789**, confirmado en la propia plaquita.
- El módulo **no tiene `CS`** (7 pines). Ambas librerías lo soportan pasando `-1`.
- El proyecto va a mostrar **letras sincronizadas** (RF-04), que implican
  redibujar texto varias veces por minuto sin que parpadee.

**Lo que asumimos sin verificar:**

- Que 40 MHz de SPI son estables con el cableado en protoboard. Si hubiera
  artefactos visuales, se baja y listo.

## Opciones consideradas

### Opción A — `Adafruit_ST7789` + `Adafruit_GFX`

- **A favor:** los pines se pasan en el constructor, a la vista en tu propio
  código: `Adafruit_ST7789 tft(-1, DC, RST)`. Muy transparente para aprender.
  Documentación excelente y muchísimos tutoriales.
- **En contra:** notablemente más lenta. No trae *sprites* (dibujar primero en
  memoria y volcar de golpe), que es lo que evita el parpadeo al redibujar.
- **Costo:** bajo al inicio, cobra factura en el RF-04.

### Opción B — `TFT_eSPI` ← *elegida*

- **A favor:**
  - Mucho más rápida: usa el hardware SPI del ESP32 y DMA.
  - Trae **sprites**: se dibuja en un buffer de memoria y se vuelca de una vez.
    Es *la* técnica para que el texto de las letras cambie sin parpadeo.
  - Es el estándar de facto para ST7789 en ESP32.
- **En contra:** ⚠️ **su configuración es su peor defecto.** Por defecto, los
  pines se definen editando `User_Setup.h`, un archivo **dentro de la carpeta de
  la librería**. Eso significa que la configuración del proyecto vive fuera del
  proyecto, no se versiona, y **se pierde al reinstalar la librería**. Es la
  causa número uno de "a mí me funcionaba y ahora no".
- **Costo:** medio al inicio, bajo después.

## Decisión

**`TFT_eSPI`, pero con toda la configuración en `platformio.ini`.**

El defecto de `TFT_eSPI` no es la librería: es *dónde* guarda su configuración.
Y eso tiene solución. Definiendo `USER_SETUP_LOADED=1` junto con todos los pines
como `build_flags`, la librería deja de buscar su archivo interno y usa lo que le
damos.

Resultado: **toda la configuración vive en el proyecto y queda en Git.** Se puede
clonar el repositorio en otra máquina, compilar, y funciona igual — que es
exactamente el [RNF-04](../4-requerimientos/REQUERIMIENTOS.md).

Con eso, la única desventaja real de `TFT_eSPI` desaparece, y quedan solo sus
ventajas: velocidad y sprites, que es justo lo que va a hacer falta para las
letras.

### Configuración aplicada

```ini
-D USER_SETUP_LOADED=1   ; "no busques User_Setup.h, yo te lo doy"
-D ST7789_DRIVER=1
-D TFT_WIDTH=240
-D TFT_HEIGHT=240
-D CGRAM_OFFSET          ; los paneles 240x240 llevan desplazamiento
-D TFT_MOSI=23  -D TFT_SCLK=18  -D TFT_CS=-1
-D TFT_DC=16    -D TFT_RST=4    -D TFT_BL=32
-D SPI_FREQUENCY=40000000
```

## Consecuencias

**Lo que esto nos habilita:**

- Redibujar rápido y sin parpadeo → el RF-04 (letras) es realista.
- La configuración es reproducible y versionada.
- Fuentes de varios tamaños ya cargadas, para resolver el RNF-01 (que se lea a
  60 cm) probando en vez de adivinando.

**Lo que esto nos cuesta:**

- Un bloque de `build_flags` largo en `platformio.ini`. Está comentado línea por
  línea para que se entienda.
- Los tutoriales de `TFT_eSPI` dirán "edita User_Setup.h". Aquí **no hay que
  hacerlo**: si se edita ese archivo, no pasará nada, porque `USER_SETUP_LOADED`
  hace que se ignore.

**Lo que hay que vigilar:**

- Si aparecen artefactos o líneas raras, bajar `SPI_FREQUENCY` a 27 o 20 MHz.
  Los cables largos de protoboard no siempre aguantan 40 MHz.
- Si los colores salen invertidos (cian/magenta/amarillo en vez de
  rojo/verde/azul), añadir `-D TFT_INVERSION_ON` o `_OFF` según toque.
- Si falta un borde o hay una franja de basura, ajustar el desplazamiento del
  panel.

## Cómo revertir

**Medio.** La API de `TFT_eSPI` y la de Adafruit se parecen pero no son iguales
(`drawString` contra `print`, distinto manejo de fuentes). Migrar implicaría
reescribir la capa de dibujo — pero **nada** de la lógica de red ni de datos.

Es la misma separación "de dónde vienen los datos" contra "cómo se dibujan" que
ya piden los ADRs [0002](ADR-0002-como-obtener-datos-de-spotify.md),
[0003](ADR-0003-fuente-de-las-letras.md) y [0004](ADR-0004-pantalla-gmt130.md).
Cuatro razones independientes apuntando a la misma estructura.
