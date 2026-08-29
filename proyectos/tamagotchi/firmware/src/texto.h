/*
 * ============================================================
 *  Texto: acentos de verdad
 * ============================================================
 *  POR QUÉ NO SALÍAN LA Ñ NI LAS TILDES
 *
 *  Las fuentes que trae TFT_eSPI (las que se piden por número:
 *  2, 4, 6...) solo contienen ASCII 32-127. Está escrito en su
 *  propio código:
 *
 *      if ((font>1) && (font<9) &&
 *          ((uniCode < 32) || (uniCode > 127))) return 0;
 *
 *  La ñ es el código 241 y la ó el 243: fuera de rango. La
 *  librería las descarta EN SILENCIO — ni error, ni cuadrito.
 *  Antes se convertían a "n" y "o" para al menos poder leerlas.
 *
 *  LA SOLUCIÓN DE VERDAD: TFT_eSPI también admite fuentes
 *  personalizadas (formato Adafruit GFX), y esas SÍ llegan hasta
 *  el código 255 — justo donde vive el español. Se generó una
 *  con tools/gen-fuente.py a partir de una Arial del sistema.
 *
 *  Con esa fuente puesta, aquí ya no hay que quitar acentos:
 *  solo convertir de UTF-8 (2 bytes por letra acentuada) a
 *  Latin-1 (1 byte), que es como está indexada la fuente.
 * ============================================================
 */

#pragma once
#include <Arduino.h>

/* ------------------------------------------------------------
 *  Emojis: por qué NO se guardan como imágenes
 * ------------------------------------------------------------
 *  Un emoji de verdad es un dibujo a color. A 20x20 píxeles
 *  ocuparía ~800 bytes CADA UNO, y existen más de 3.700. Meter
 *  siquiera una parte sería cientos de KB de memoria.
 *
 *  Pero un corazón son dos círculos y un triángulo. Una estrella
 *  son cinco líneas. DIBUJARLOS cuesta código, no datos: unos
 *  cientos de bytes de programa y cero memoria por emoji.
 *
 *  Así que aquí los emojis frecuentes se traducen a marcadores de
 *  un byte, y la pantalla los pinta a mano. Los demás se
 *  convierten en un punto: mejor eso que un hueco raro.
 * ------------------------------------------------------------ */
#define MARCA_NOTA     '\x01'
#define MARCA_COR_ROJO '\x02'
#define MARCA_COR_MOR  '\x03'
#define MARCA_COR_AZUL '\x04'
#define MARCA_COR_VERD '\x05'
#define MARCA_COR_AMAR '\x06'
#define MARCA_COR_NEGR '\x07'
#define MARCA_COR_BLAN '\x08'
#define MARCA_ESTRELLA '\x0B'
#define MARCA_FUEGO    '\x0C'
#define MARCA_PUNTO    '\x0E'    // cualquier emoji que no sepamos dibujar

inline bool esIcono(char c) { return (uint8_t)c >= 1 && (uint8_t)c <= 0x0E; }

inline String aLatin1(const String& in) {
  String out;
  out.reserve(in.length());

  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in[i];

    if (c < 0x80) { out += (char)c; continue; }         // ASCII, tal cual

    // Dos bytes: aquí viven á é í ó ú ñ ü ¿ ¡ y compañía.
    // En UTF-8 ocupan 2 bytes; en Latin-1, 1. La fuente está
    // indexada por Latin-1, así que se convierte.
    if ((c & 0xE0) == 0xC0 && i + 1 < in.length()) {
      uint16_t u = ((c & 0x1F) << 6) | ((uint8_t)in[i+1] & 0x3F);
      i++;
      if (u >= 0xA0 && u <= 0xFF) out += (char)(uint8_t)u;   // ✅ acento conservado
      else out += ' ';
      continue;
    }

    // Tres bytes: comillas curvas, guiones largos, notas musicales.
    // Estos NO están en Latin-1, así que se traducen a su
    // equivalente más cercano.
    if ((c & 0xF0) == 0xE0 && i + 2 < in.length()) {
      uint16_t u = ((c & 0x0F) << 12) | (((uint8_t)in[i+1] & 0x3F) << 6)
                 | ((uint8_t)in[i+2] & 0x3F);
      i += 2;
      switch (u) {
        case 0x2018: case 0x2019: out += '\''; break;   // ' '
        case 0x201C: case 0x201D: out += '"';  break;   // " "
        case 0x2013: case 0x2014: out += '-';  break;   // – —
        case 0x2026: out += "..."; break;               // …
        // ♩ ♪ ♫ ♬ marcan los tramos sin voz de los .lrc
        case 0x2669: case 0x266A: case 0x266B: case 0x266C:
          out += MARCA_NOTA; break;

        case 0x2764: out += MARCA_COR_ROJO; break;      // ❤
        case 0x2B50: case 0x2728: out += MARCA_ESTRELLA; break;  // ⭐ ✨

        // Selector de variante y unión de ancho cero: son marcas
        // INVISIBLES que acompañan a los emojis (❤️ es ❤ + selector).
        // Si se convirtieran en espacio, cada emoji dejaría un hueco
        // fantasma al lado.
        case 0xFE0F: case 0x200D: break;

        default: out += ' '; break;
      }
      continue;
    }

    // Cuatro bytes: aquí viven los emojis modernos (💜 🔥 ✨ 🎵...).
    if ((c & 0xF8) == 0xF0 && i + 3 < in.length()) {
      uint32_t u = ((uint32_t)(c & 0x07) << 18)
                 | ((uint32_t)((uint8_t)in[i+1] & 0x3F) << 12)
                 | ((uint32_t)((uint8_t)in[i+2] & 0x3F) << 6)
                 |  (uint32_t)((uint8_t)in[i+3] & 0x3F);
      i += 3;
      switch (u) {
        case 0x1F3B5: case 0x1F3B6: out += MARCA_NOTA;     break;  // 🎵 🎶
        case 0x1F49C: out += MARCA_COR_MOR;  break;                // 💜
        case 0x1F499: out += MARCA_COR_AZUL; break;                // 💙
        case 0x1F49A: out += MARCA_COR_VERD; break;                // 💚
        case 0x1F49B: case 0x1F9E1: out += MARCA_COR_AMAR; break;  // 💛 🧡
        case 0x1F5A4: out += MARCA_COR_NEGR; break;                // 🖤
        case 0x1F90D: out += MARCA_COR_BLAN; break;                // 🤍
        case 0x1F493: case 0x1F495: case 0x1F496: case 0x1F497:
        case 0x1F498: case 0x1F49D: case 0x1F49E: case 0x1F49F:
                      out += MARCA_COR_ROJO; break;                // 💓💕💖💗...
        case 0x1F31F: out += MARCA_ESTRELLA; break;                // 🌟
        case 0x1F525: out += MARCA_FUEGO;    break;                // 🔥
        default:      out += MARCA_PUNTO;    break;
      }
      continue;
    }
  }
  out.trim();
  return out;
}


/* ------------------------------------------------------------
 *  Las descripciones de playlist vienen con HTML
 * ------------------------------------------------------------
 *  Spotify las devuelve tal cual las escribió el usuario, y eso
 *  incluye entidades HTML (&amp; por &, &#x27; por ') y a veces
 *  etiquetas <a href="...">. Sin limpiarlas, en la pantalla se
 *  leería literalmente "&#x27;" en vez de un apóstrofo.
 * ------------------------------------------------------------ */
inline String limpiarHTML(const String& in) {
  String s = in;

  // Fuera las etiquetas: todo lo que va entre < y >
  String sinTags; bool dentro = false;
  for (size_t i = 0; i < s.length(); i++) {
    if (s[i] == '<') dentro = true;
    else if (s[i] == '>') dentro = false;
    else if (!dentro) sinTags += s[i];
  }
  s = sinTags;

  s.replace("&amp;", "&");   s.replace("&lt;", "<");    s.replace("&gt;", ">");
  s.replace("&quot;", "\""); s.replace("&#x27;", "'");  s.replace("&#39;", "'");
  s.replace("&apos;", "'");  s.replace("&nbsp;", " ");

  s = aLatin1(s);
  s.trim();
  return s;
}
