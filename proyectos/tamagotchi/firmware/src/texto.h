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

// Marcador interno para los símbolos de nota musical de los .lrc.
// La pantalla lo reconoce y dibuja una nota a mano.
#define MARCA_NOTA '\x01'

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
        default: out += ' '; break;
      }
      continue;
    }

    if ((c & 0xF8) == 0xF0) { i += 3; out += ' '; continue; }   // emojis
  }
  out.trim();
  return out;
}
