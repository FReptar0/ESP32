/*
 * ============================================================
 *  Texto: acentos y partido en líneas
 * ============================================================
 *  PROBLEMA: las fuentes integradas de TFT_eSPI solo tienen
 *  ASCII 32-127. Se ve en su propio código:
 *
 *      if ((font>1) && (font<9) &&
 *          ((uniCode < 32) || (uniCode > 127))) return 0;
 *
 *  O sea: cualquier carácter acentuado se DESCARTA EN SILENCIO.
 *  Por eso "León" se veía como "Len" — la ó no se dibujaba, y
 *  como en UTF-8 ocupa dos bytes, desaparecía entera.
 *
 *  SOLUCIÓN: convertir los acentos a su letra más parecida
 *  antes de dibujar. "León" -> "Leon". Se pierde la tilde pero
 *  se lee bien, que es lo que importa en una pantalla de 1.3".
 *
 *  (La alternativa real serían las fuentes "smooth" .vlw de
 *   TFT_eSPI, que sí soportan Unicode completo. Requieren
 *   generar el archivo de fuente con una herramienta aparte y
 *   guardarlo en la memoria del ESP32. Queda anotado como
 *   mejora futura.)
 * ============================================================
 */

#pragma once
#include <Arduino.h>

// UTF-8 en dos palabras: los caracteres normales ocupan 1 byte.
// Los acentuados ocupan 2, y el primero siempre empieza por 110xxxxx.
// Aquí se leen esos pares y se traducen a una letra simple.
inline String aASCII(const String& in) {
  String out;
  out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in[i];

    if (c < 0x80) { out += (char)c; continue; }        // ASCII normal

    if ((c & 0xE0) == 0xC0 && i + 1 < in.length()) {   // secuencia de 2 bytes
      uint16_t u = ((c & 0x1F) << 6) | ((uint8_t)in[i+1] & 0x3F);
      i++;
      switch (u) {
        case 0xE1: case 0xE0: case 0xE2: case 0xE4: case 0xE3: out += 'a'; break;
        case 0xC1: case 0xC0: case 0xC2: case 0xC4: case 0xC3: out += 'A'; break;
        case 0xE9: case 0xE8: case 0xEA: case 0xEB:            out += 'e'; break;
        case 0xC9: case 0xC8: case 0xCA: case 0xCB:            out += 'E'; break;
        case 0xED: case 0xEC: case 0xEE: case 0xEF:            out += 'i'; break;
        case 0xCD: case 0xCC: case 0xCE: case 0xCF:            out += 'I'; break;
        case 0xF3: case 0xF2: case 0xF4: case 0xF6: case 0xF5: out += 'o'; break;
        case 0xD3: case 0xD2: case 0xD4: case 0xD6: case 0xD5: out += 'O'; break;
        case 0xFA: case 0xF9: case 0xFB: case 0xFC:            out += 'u'; break;
        case 0xDA: case 0xD9: case 0xDB: case 0xDC:            out += 'U'; break;
        case 0xF1: out += 'n'; break;                  // ñ
        case 0xD1: out += 'N'; break;                  // Ñ
        case 0xE7: out += 'c'; break;                  // ç
        case 0xC7: out += 'C'; break;
        case 0xFD: case 0xFF: out += 'y'; break;
        case 0xDF: out += "ss"; break;                 // ß
        case 0xBF: out += '?'; break;                  // ¿
        case 0xA1: out += '!'; break;                  // ¡
        case 0xAB: case 0xBB: out += '"'; break;       // « »
        case 0xB0: out += 'o'; break;                  // °
        default:   out += ' '; break;                  // lo desconocido, un espacio
      }
      continue;
    }

    if ((c & 0xF0) == 0xE0 && i + 2 < in.length()) {   // secuencia de 3 bytes
      uint16_t u = ((c & 0x0F) << 12) | (((uint8_t)in[i+1] & 0x3F) << 6)
                 | ((uint8_t)in[i+2] & 0x3F);
      i += 2;
      switch (u) {
        case 0x2018: case 0x2019: out += '\''; break;  // comillas curvas ' '
        case 0x201C: case 0x201D: out += '"';  break;  // " "
        case 0x2013: case 0x2014: out += '-';  break;  // guiones largos – —
        case 0x2026: out += "..."; break;              // puntos suspensivos …
        default:     out += ' ';  break;
      }
      continue;
    }

    // 4 bytes (emojis y demás): se saltan enteros
    if ((c & 0xF8) == 0xF0) { i += 3; out += ' '; continue; }
  }
  out.trim();
  return out;
}
