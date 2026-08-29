# Hallazgos — Acentos, recortes y desbordamiento de pila

- **Fecha:** 2026-08-29
- **Origen:** cuatro problemas reportados al ver el aparato en marcha

---

## 1. ❌ Los acentos no se dibujaban ("León" → "Len")

**Causa, encontrada en el código de la propia librería:**

```c
// TFT_eSPI.cpp, línea 5228
if ((font>1) && (font<9) && ((uniCode < 32) || (uniCode > 127))) return 0;
```

**Las fuentes integradas de TFT_eSPI solo tienen ASCII 32–127.** Cualquier
carácter fuera de ese rango **se descarta en silencio** — ni error, ni cuadrito,
ni nada. Simplemente no se dibuja.

Y como en UTF-8 la `ó` ocupa **dos bytes**, desaparecía entera: `León` → `Len`.

> Nótese que la búsqueda en LRCLIB **sí funcionaba** con acentos: `Labios Rotos
> — Zoé` se encontró sin problema. El fallo era solo al dibujar, lo que lo hacía
> más confuso de diagnosticar.

**Primera solución (superada):** convertir los acentos a su letra más parecida
(`León` → `Leon`). Funcionaba y se leía bien, pero perdía las tildes.

### ✅ Solución definitiva: una fuente propia con acentos

Resulta que TFT_eSPI **sí** admite fuentes personalizadas en formato Adafruit GFX,
y esas llegan hasta el código **255** — justo donde vive el español. Se ve en su
propio código, que usa el rango declarado por la fuente en vez de un tope fijo:

```c
if ((c >= pgm_read_word(&gfxFont->first)) && (c <= pgm_read_word(&gfxFont->last)))
```

Así que se escribió [`tools/gen-fuente.py`](../tools/gen-fuente.py), que coge una
Arial del sistema, dibuja cada carácter del 32 al 255 y los empaqueta en ese
formato.

| | |
|---|---|
| Fuente grande | 19 px · 3.722 bytes de dibujos |
| Fuente chica | 13 px · 1.715 bytes |
| **Total en flash** | **~8.4 KB** |

Barato para lo que resuelve: **`León`, `Cariño`, `Como Tú` y `The Marías` se ven
como deben**.

**Dos detalles que hubo que atar:**

1. El texto se convierte de UTF-8 a **Latin-1** (de 2 bytes por letra acentuada a
   1), porque así está indexada la fuente. Se hace al cargar la letra, una vez por
   canción, no 6 veces por segundo al dibujar.
2. Hay que **apagar el decodificador UTF-8 de la librería**
   (`tft.setAttribute(UTF8_SWITCH, false)`). Si no, vería el byte `0xF1` de la ñ
   y creería que empieza una secuencia de varios bytes.

## 2. 🐛 Se añadía "..." a frases que sí cabían

Reportado con un caso concreto: *"baby, i'll give you my loving"* cabía completa
y aun así aparecía con `...` al final.

**Era un bug propio, de una línea:**

```cpp
while (resto.length() > 0 && n < maxLineas) {
  if (tft.textWidth(resto, fuente) <= anchoMax) {
    salida[n++] = resto;
    break;                 // ← salía SIN vaciar `resto`
  }
  ...
}
if (resto.length() > 0 && n > 0) { /* añadir "..." */ }
```

Al salir por el `break`, `resto` seguía conteniendo el texto. La comprobación de
después lo interpretaba como "sobró texto" y añadía los puntos suspensivos.

**Arreglo:** `resto = "";` antes del `break`.

🔑 **La lección:** un `break` que sale a mitad de un bucle se salta la
actualización de las variables que el resto del código da por hecha. Es un
descuido clásico y silencioso — el programa no falla, solo hace algo levemente
mal.

## 3. ✂️ Recortar es peor que encoger

Aunque el bug estuviera arreglado, las frases genuinamente largas seguían
perdiendo palabras.

**Nuevo comportamiento, en tres escalones:**

1. Intentar con **fuente 4** en hasta **3 renglones**
2. Si no cabe, reintentar con **fuente 2** (más chica, más texto)
3. Solo si aun así no cabe, recortar con `...`

**Por qué en ese orden:** perder palabras de la canción es peor que perder tamaño
de letra. Con el texto más chico se sigue leyendo; con el texto recortado, la
frase deja de tener sentido.

Además, la línea anterior y la siguiente (el contexto en gris) **solo se dibujan
si sobra sitio**. Cuando la frase actual es larga, ella manda.

## 4. 💥 Desbordamiento de pila al precargar

```
Guru Meditation Error: Core 1 panic'ed (Unhandled debug exception)
Debug exception reason: Stack canary watchpoint triggered (loopTask)
```

**Causa:** una variable local así:

```cpp
Letras tmp;     // 90 líneas x 76 bytes = 6.840 bytes
```

**Las variables locales viven en la pila**, y la pila de la tarea principal de
Arduino es de **8.192 bytes**. Un objeto de 6.8 KB casi la llena él solo.

Lo desconcertante del caso: **había 186 KB de memoria libre**. Pero esa memoria
es de otro sitio.

| | Dónde vive | Tamaño aquí | Para qué |
|---|---|---|---|
| **Pila** (*stack*) | Bloque fijo por tarea | **8 KB** | Variables locales, llamadas a funciones |
| **Montón** (*heap*) | El resto de la RAM | ~186 KB | Lo que se pide con `new`, los `String`, ArduinoJson |

Tener el montón vacío no ayuda si lo que se llena es la pila.

**El "canario"** es un valor centinela que el sistema coloca al final de la pila.
Si lo encuentra pisado, sabe que hubo desbordamiento y **reinicia a propósito**,
en vez de seguir funcionando con la memoria corrompida — que daría fallos
absurdos e imposibles de rastrear. El reinicio es la parte buena de la historia.

**Arreglo:** usar el objeto global `precarga` en lugar de una copia local. La
solución **no** era agrandar la pila: era no poner 7 KB en ella.

🔑 **La lección:** ante un `Stack canary watchpoint triggered`, buscar variables
locales grandes — arrays, estructuras, buffers. La memoria libre que reporta
`ESP.getFreeHeap()` no dice nada sobre la pila.


---

## 5. 🎵 El símbolo de nota musical dejaba un hueco

**Qué pasaba:** los archivos `.lrc` usan `♪` (y `♩ ♫ ♬`) para marcar los tramos
**sin voz**: intros, solos, puentes instrumentales. No son texto que falte — son
información sobre la canción.

Pero al no existir en las fuentes integradas, `aASCII` los mandaba al caso
`default`, que los convertía en un espacio. Resultado: un hueco raro sin
explicación.

**Solución en dos partes:**

1. Los cuatro símbolos (`U+2669`–`U+266C`) se traducen a `~`.
2. La pantalla **reconoce ese marcador** y, en su lugar, dibuja una nota musical
   de verdad con primitivas gráficas — cabeza, palo y banderita. Como las fuentes
   no la traen, se pinta a mano.

## 6. 🕳️ La zona de la letra se quedaba vacía en los silencios

**Reportado así:** *"al cambiar de canción, hasta que no entra la primera frase
el espacio de las letras queda vacío"* y *"cuando llega a las pausas donde no hay
frase desaparece todo el texto"*.

**Era una decisión de diseño mía, y estaba mal.** El código decía literalmente:

```cpp
if (idx < 0 || !letras.lineas[idx].texto[0]) {
  return;    // "silencio entre estrofas: la pantalla se queda limpia"
}
```

El razonamiento era *"si nadie canta, no hay nada que mostrar"*. Suena lógico y
en la práctica es horrible: durante la intro de una canción puedes pasar 20
segundos mirando un hueco negro, **sin saber si el aparato se colgó**.

🔑 **Una pantalla vacía es indistinguible de un aparato roto.** Es la misma razón
por la que existen el RF-06 y el RF-07 (distinguir "no hay música" de "no hay
internet"): el aparato nunca debe dejar al usuario adivinando.

**Comportamiento nuevo durante un silencio:**

```
        ♪ ♪            <- dibujado, para que se vea que ES un tramo sin voz
   la frase que viene   <- en gris, debajo
```

Así se puede seguir la canción **antes** de que empiece a cantarse, y queda claro
que el aparato está vivo y sabe lo que hace.

Además, la búsqueda de la línea anterior y la siguiente ahora **salta los
silencios**: busca la anterior y la siguiente *con texto*, en vez de mostrar
huecos como contexto.


---

## 7. 🎵 Las notas de los silencios: centradas y en blanco

Al ver el resultado, las notas quedaban **arriba y en gris**, cuando lo natural
es que ocupen el sitio de la frase que se cantaría: **centradas y en blanco**,
con la frase que viene en gris debajo.

Ahora se calcula primero cuánto va a ocupar la frase siguiente, y con eso se
centra el bloque de notas en el hueco que queda. El resultado es que durante una
intro la pantalla se ve equilibrada en vez de descolgada hacia arriba.

## 8. ⏱️ Detectar antes las pausas y los cambios manuales

Se preguntaba a Spotify **cada 5 segundos**, así que darle a pausa o saltar de
canción tardaba hasta 5 segundos en notarse. Se sentía lento.

**Ahora cada 2 segundos**, que son 15 peticiones por ventana de 30 s.

⚠️ El riesgo es el límite de peticiones, que Spotify **no publica** para el modo
desarrollo. Por eso se añadió un **freno automático**: si alguna vez responde
`429`, el intervalo sube un segundo (hasta 8 s como máximo) y ya no se le vuelve
a molestar tanto.

🔑 **Cuando un límite no está documentado, el diseño tiene que autorregularse.**
Adivinar un número fijo es apostar; reaccionar a la respuesta del servidor, no.
