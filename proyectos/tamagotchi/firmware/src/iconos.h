/*
 * ============================================================
 *  Iconos dibujados a mano
 * ============================================================
 *  Ninguna fuente trae emojis, y guardarlos como imágenes sería
 *  carísimo: un 💜 a 20x20 píxeles a color son ~800 bytes, y
 *  existen más de 3.700 emojis.
 *
 *  Un corazón, en cambio, son dos círculos y un triángulo.
 *  Dibujarlo cuesta CÓDIGO, no datos: unos cientos de bytes de
 *  programa en total, y cero memoria por emoji.
 *
 *  Todas las funciones reciben `TFT_eSPI&`. Como TFT_eSprite
 *  hereda de esa clase, el mismo código sirve para pintar
 *  directo en la pantalla o dentro de un sprite.
 * ============================================================
 */

#pragma once
#include <TFT_eSPI.h>
#include "texto.h"

const int ANCHO_ICONO = 15;

inline uint16_t colorDeIcono(char c, uint16_t porDefecto) {
  switch (c) {
    case MARCA_COR_ROJO: return TFT_RED;
    case MARCA_COR_MOR:  return 0x915C;   // morado
    case MARCA_COR_AZUL: return 0x1C9F;
    case MARCA_COR_VERD: return 0x2E6A;
    case MARCA_COR_AMAR: return 0xFD20;   // ámbar
    case MARCA_COR_NEGR: return 0x4208;   // gris muy oscuro (negro no se vería)
    case MARCA_COR_BLAN: return TFT_WHITE;
    case MARCA_ESTRELLA: return 0xFE60;
    case MARCA_FUEGO:    return 0xFB00;
    default:             return porDefecto;
  }
}

inline bool esCorazon(char c) {
  return c >= MARCA_COR_ROJO && c <= MARCA_COR_BLAN;
}

// Un corazón: dos círculos arriba y un triángulo abajo.
inline void pintarCorazon(TFT_eSPI& g, int x, int y, uint16_t c) {
  g.fillCircle(x + 3, y - 1, 3, c);
  g.fillCircle(x + 8, y - 1, 3, c);
  g.fillTriangle(x, y, x + 11, y, x + 5, y + 6, c);
}

// Un destello de cuatro puntas: dos triángulos alargados cruzados.
inline void pintarEstrella(TFT_eSPI& g, int x, int y, uint16_t c) {
  int cx = x + 5, cy = y + 1;
  g.fillTriangle(cx, cy - 6, cx - 3, cy, cx + 3, cy, c);
  g.fillTriangle(cx, cy + 6, cx - 3, cy, cx + 3, cy, c);
  g.fillTriangle(cx - 6, cy, cx, cy - 3, cx, cy + 3, c);
  g.fillTriangle(cx + 6, cy, cx, cy - 3, cx, cy + 3, c);
}

// Una llama: un círculo con una punta encima.
inline void pintarFuego(TFT_eSPI& g, int x, int y, uint16_t c) {
  g.fillCircle(x + 5, y + 2, 4, c);
  g.fillTriangle(x + 1, y + 1, x + 9, y + 1, x + 5, y - 7, c);
}

inline void pintarNotaPeq(TFT_eSPI& g, int x, int y, uint16_t c) {
  g.fillCircle(x + 3, y + 3, 3, c);
  g.drawFastVLine(x + 5, y - 6, 9, c);
  g.drawFastVLine(x + 6, y - 6, 9, c);
  g.drawLine(x + 6, y - 6, x + 10, y - 3, c);
}

// Dibuja un icono. `y` es la línea base del texto que lo rodea.
inline void pintarIcono(TFT_eSPI& g, char c, int x, int y, uint16_t colorTexto) {
  uint16_t col = colorDeIcono(c, colorTexto);
  int cy = y + 7;                       // centrado respecto a la altura de la letra
  if (esCorazon(c))                pintarCorazon(g, x + 1, cy, col);
  else if (c == MARCA_ESTRELLA)    pintarEstrella(g, x + 1, cy, col);
  else if (c == MARCA_FUEGO)       pintarFuego(g, x + 1, cy, col);
  else if (c == MARCA_NOTA)        pintarNotaPeq(g, x + 1, cy, col);
  else                             g.fillCircle(x + 6, cy + 1, 2, col);  // punto genérico
}

// Ancho de una cadena contando los iconos.
inline int anchoMixto(TFT_eSPI& g, const String& s) {
  int w = 0; String bloque;
  for (size_t i = 0; i < s.length(); i++) {
    if (esIcono(s[i])) { w += g.textWidth(bloque) + ANCHO_ICONO; bloque = ""; }
    else bloque += s[i];
  }
  return w + g.textWidth(bloque);
}

// Dibuja texto que puede llevar iconos intercalados. Devuelve el ancho.
// Va alternando: trozo de texto normal, icono, trozo de texto...
inline int dibujarMixto(TFT_eSPI& g, const String& s, int x, int y, uint16_t color) {
  int cx = x; String bloque;
  g.setTextDatum(TL_DATUM);
  g.setTextColor(color);
  for (size_t i = 0; i < s.length(); i++) {
    if (esIcono(s[i])) {
      if (bloque.length()) { g.drawString(bloque, cx, y); cx += g.textWidth(bloque); bloque = ""; }
      pintarIcono(g, s[i], cx, y, color);
      cx += ANCHO_ICONO;
    } else bloque += s[i];
  }
  if (bloque.length()) { g.drawString(bloque, cx, y); cx += g.textWidth(bloque); }
  return cx - x;
}
