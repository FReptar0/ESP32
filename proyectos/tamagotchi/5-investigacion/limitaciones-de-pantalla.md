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

**Solución aplicada:** convertir los acentos a su letra más parecida **al cargar
la letra**, no al dibujarla — así se hace una vez por canción y no 6 veces por
segundo. `León` → `Leon`.

Se pierde la tilde. Es un compromiso consciente: en una pantalla de 1.3" lo que
importa es leer la palabra.

**La alternativa, para el futuro:** TFT_eSPI soporta fuentes *smooth* (`.vlw`),
que sí manejan Unicode completo. Requieren generar el archivo de fuente con una
herramienta aparte y guardarlo en la memoria del ESP32. Es la vía si algún día
los acentos importan de verdad.

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
