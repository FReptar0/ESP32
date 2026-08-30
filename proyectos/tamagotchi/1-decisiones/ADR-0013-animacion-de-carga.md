# ADR-0013 — Guardar la animación como fotogramas de 1 bit, no reproducir el GIF

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-30
- **Reemplaza a:** —
- **Relacionado con:** [ADR-0011](ADR-0011-imagenes-en-pantalla.md) (imágenes en
  pantalla), [ADR-0004](ADR-0004-pantalla-gmt130.md) (la pantalla),
  `5-investigacion/portada-cache-descartada.md`

---

## Contexto

El aparato es **un regalo**, y a quien lo va a recibir le gusta Snoopy. La
pantalla de arranque decía `Tamagotchi / conectando al WiFi...` en texto sobre
negro durante unos 6 segundos. La idea es poner ahí a Snoopy bailando.

El usuario trajo un GIF: 488×275, 7 cuadros, 110 ms cada uno, con fondo (una
pared mostaza con una franja naranja y un suelo gris).

Las restricciones de partida no son negociables:

- La pantalla es de **240×240** y 16 bits de color ([ADR-0004](ADR-0004-pantalla-gmt130.md)).
- **Un fotograma a pantalla completa en color son 115.200 B.** Siete son 806 KB.
- La partición de programa tenía **137 KB libres** de 1,25 MB.
- El montón está **muy troceado**: guardar 50 KB de forma continuada ya costó una
  sesión entera y se acabó descartando (`portada-cache-descartada.md`). El bloque
  contiguo mayor baja a 45 KB.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- Snoopy es **línea negra sobre blanco**: dos tonos, sin degradados. Medido en el
  GIF: su tinta tiene saturación 11 de mediana.
- El fondo del GIF es plano y **saturado** (la franja, entre 69 y 135), así que se
  puede separar de la tinta sin recortar por posición.
- El arranque dura ~6 s hasta la primera canción: da para ~8 vueltas de un bucle
  de 770 ms.
- La flash de la placa es de **4 MB** y `default.csv` reserva 1,25 MB para una
  segunda copia del programa (`app1`) que **este proyecto no usa**: no hay ni una
  llamada a OTA en el firmware.

**Lo que asumimos sin verificar:**

- Que 7 cuadros bastan para que el baile se lea como un baile. *(Verificado
  después mirándolo: sí.)*
- Que los umbrales medidos sobre **este** GIF sirvan para otro. Casi seguro que
  no: habría que volver a medirlos.

**Lo que quedó pendiente de averiguar:**

- Si conviene cambiar a `huge_app.csv`. No hace falta para esto, pero deja el
  programa al 91,4 % de su sitio.

## Opciones consideradas

### Opción A — Descomprimir y reproducir el GIF en la placa

- **A favor:** el archivo entra tal cual; cambiar la animación sería copiar otro.
- **En contra:** hay que descomprimir LZW **en marcha**, con un buffer de
  fotograma en un montón que ya demostró no aguantar bloques grandes. Y añade una
  librería a un firmware que va al 91 % de la flash.
- **Costo:** alto, y en el punto exacto donde este proyecto ya se rompió una vez.

### Opción B — Fotogramas en color (RGB565), reducidos

- **A favor:** conserva el color; conversión trivial.
- **En contra:** a 120×120 son 28.800 B por cuadro, 202 KB los siete: **no cabe**
  en los 137 KB libres. Y el color no aporta nada: el dibujo es de dos tonos.
- **Costo:** no cabe.

### Opción C — Dibujarlo con figuras, como el corazón del `"Te quiero!!"`

- **A favor:** coste de memoria cero.
- **En contra:** Snoopy tiene una forma muy reconocible y **un Snoopy mal dibujado
  se nota inmediatamente**. Sería peor que no ponerlo.
- **Costo:** bajo en bytes, inaceptable en resultado.

### Opción D — Fotogramas de 1 bit por píxel, convertidos en el ordenador *(elegida)*

- **A favor:** 3.496 B por cuadro, **24.472 B los siete**. Cabe sin tocar las
  particiones. Cero trabajo de descompresión en la placa: los cuadros se leen
  directos de la flash, sin pasar por el montón.
- **En contra:** se pierde el color (no importa) y el fondo (mejor así). Y hace
  falta escribir un conversor.
- **Costo:** el conversor, `tools/gen-animacion.py`.

## Decisión

**Elegimos la opción D.**

El factor que decidió no fue el tamaño, sino **dónde vive el coste**. Reproducir
un GIF pone el trabajo en la placa, en marcha, sobre un montón troceado — el sitio
más frágil de este aparato. Convertirlo pone el trabajo en el ordenador, una vez,
donde la memoria no es un problema. En la placa solo queda leer bytes de la flash.

Y el personaje juega a favor: **Snoopy ya está dibujado en blanco y negro.** Los
16 bits de color no representaban información, solo ocupaban sitio. Pasar a 1 bit
no es una concesión de calidad: es dejar de guardar lo que no había.

**Fondo blanco y tinta negra**, al revés que el resto del aparato. Se probó
invertido y **se ve mal**: su oreja negra sale blanca y el conjunto parece un
negativo. La pantalla de arranque es una viñeta de cómic; el resto del aparato, no.

## Consecuencias

**Lo que esto nos habilita:**

- Cualquier otra animación de línea entra por el mismo camino: cambiar el GIF y
  volver a pasar el conversor.
- La animación **es un diagnóstico**: si se para, el arranque se atascó. Un texto
  fijo no distingue "esperando" de "colgado".

**Lo que esto nos cierra o nos cuesta:**

- Nada con color o sombreado servirá. Para eso habría que volver a esta decisión.
- La flash queda al **91,4 %**: 112 KB libres. La siguiente cosa que ocupe espacio
  obliga a pasar a `huge_app.csv`.
- Los umbrales del conversor están medidos **sobre este GIF**. Otro necesitará
  volver a medirlos, y el conversor imprime lo que hace falta para hacerlo.

**Lo que hay que vigilar:**

- 🚩 Que la flash pase del 95 %. Entonces toca cambiar las particiones (hay 1,25 MB
  desperdiciados en `app1`, que no se usa) y eso merece su propio ADR.
- 🚩 Que el fondo blanco moleste al arrancar a oscuras. Verificado mirándolo de
  día; **de noche no se ha probado**.

## Cómo revertir

**Barato.** Quitar `#include "snoopy.h"` y devolver las tres llamadas a
`pantallaMensaje()` en `setup()`. El archivo generado se puede borrar y son 24 KB
de flash que vuelven.

Cambiar de animación no es revertir: es volver a ejecutar
`tools/gen-animacion.py` con otro GIF, y **medir de nuevo los umbrales** — que es
donde estuvo todo el trabajo, no en el formato.
