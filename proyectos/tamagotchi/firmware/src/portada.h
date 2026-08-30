/*
 * ============================================================
 *  La portada del álbum (RF-13)
 * ============================================================
 *  Descargar un JPEG y pintarlo en un ESP32 tiene dos problemas,
 *  y los dos son de memoria.
 *
 *  1. LA IMAGEN DESCOMPRIMIDA NO CABE. 240x240 píxeles a 16 bits
 *     son 115 KB, más de la mitad de la RAM libre. Por eso se usa
 *     TJpg_Decoder: saca la imagen por bloques y cada bloque se
 *     manda a la pantalla y se olvida. Nunca existe entera.
 *
 *  2. EL ARCHIVO COMPRIMIDO TAMPOCO, SI TE DESCUIDAS. Medido el
 *     2026-08-29 sobre portadas reales de Spotify:
 *
 *         640x640 -> 161 KB   ❌ con los ~50 KB del TLS son 211 KB
 *         300x300 ->  41 KB   ✅ pico de 91 KB, cabe
 *          64x64  ->   3 KB   (demasiado pequeña para la pantalla)
 *
 *     Así que la de 300 no es una preferencia estética: es la
 *     única que cabe.
 *
 *  🔑 Y 300 a 240 resulta ser una proporción exacta de 5 a 4. Se
 *     reduce tirando UN píxel de cada CINCO, sin recortar nada y
 *     llenando la pantalla entera. Cuando los números salen
 *     redondos, casi siempre hay un camino simple escondido.
 * ============================================================
 */
#pragma once
#include <Arduino.h>
#include <TJpg_Decoder.h>

/* Cuánto mide la imagen que estamos decodificando.
 *
 * ⚠️ NO es constante, y suponerlo fue un error. Las portadas de álbum
 *    vienen en 300x300, pero la imagen de una playlist puede ser de
 *    640x640 —o un mosaico sin dimensiones declaradas—. Con un 300
 *    fijo, de una imagen de 640 se dibujaba solo su esquina superior
 *    izquierda, y nadie lo habría notado hasta ver una carátula
 *    misteriosamente recortada.
 *
 * 🔑 Preguntarle a la imagen cuánto mide cuesta una llamada. Suponerlo
 *    cuesta un fallo que solo aparece con ciertas imágenes.
 */
int PORTADA_ORIGEN = 300;

/* ------------------------------------------------------------
 *  Dónde va la portada
 * ------------------------------------------------------------
 *  La primera versión la ponía a 240x240, a pantalla completa. Se
 *  veía muy bien pero se perdían la playlist y el estado, y cuando
 *  la barra se dibujaba encima pisaba la carátula.
 *
 *  Ahora la barra conserva sus 28 píxeles de arriba y la portada
 *  ocupa el cuadrado más grande que cabe debajo: 212x212, centrado
 *  a lo ancho. Nada se encima con nada.
 * ------------------------------------------------------------ */
const int PORTADA_ALTO_BARRA = 28;

/* El destino NO es constante: la misma máquina sirve para dos cosas
 * con tamaños distintos.
 *
 *   · Modo portada  → 212x212 debajo de la barra, lo más grande que cabe
 *   · Selector      → 150x150, dejando sitio abajo al nombre
 *
 * Se ajustan justo antes de decodificar. Es lo que evita tener dos
 * copias del volcado, que era la alternativa. */
int PORTADA_LADO = 240 - PORTADA_ALTO_BARRA;   // 212 por defecto
int PORTADA_X0   = 14;
int PORTADA_Y0   = PORTADA_ALTO_BARRA;

inline void encuadrarImagen(int lado, int y0) {
  PORTADA_LADO = lado;
  PORTADA_X0   = (240 - lado) / 2;
  PORTADA_Y0   = y0;
}

/* ------------------------------------------------------------
 *  Reducir de 300 a 212 — ya no sale una cuenta redonda
 * ------------------------------------------------------------
 *  Con la pantalla completa la proporción era 5 a 4 exacta: bastaba
 *  tirar uno de cada cinco píxeles. De 300 a 212 no hay atajo, así
 *  que se hace lo general: cada píxel de origen sabe a qué píxel de
 *  destino le toca, y se conserva solo el PRIMERO que cae en cada
 *  destino. Los demás se descartan.
 *
 *  Es lo mismo que hace cualquier programa al reducir una foto, en
 *  su versión más simple. Pierde algo de nitidez —no promedia, elige—
 *  pero cuesta dos divisiones por píxel y no necesita ni un byte de
 *  memoria extra, que aquí es lo que manda.
 * ------------------------------------------------------------ */
inline int aDestino(int s) { return s * PORTADA_LADO / PORTADA_ORIGEN; }
inline bool seConserva(int s) { return s == 0 || aDestino(s) != aDestino(s - 1); }

/* ------------------------------------------------------------
 *  El volcado de un bloque, reduciendo al vuelo
 * ------------------------------------------------------------
 *  El decodificador llama a esto una vez por bloque (16x16 o 8x8).
 *  Aquí se decide qué píxeles sobreviven y se mandan a la pantalla.
 *
 *  Se envía una FILA entera de golpe en vez de píxel a píxel: cada
 *  envío por SPI tiene un coste fijo, y una docena de píxeles en una
 *  llamada cuestan casi lo mismo que uno solo.
 * ------------------------------------------------------------ */
inline bool volcarBloquePortada(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bmp) {
  if (y >= PORTADA_ORIGEN) return false;   // false = deja de decodificar

  uint16_t fila[32];
  for (uint16_t j = 0; j < h; j++) {
    int sy = y + j;
    if (sy >= PORTADA_ORIGEN) break;
    if (!seConserva(sy)) continue;

    int n = 0, dx0 = -1;
    for (uint16_t i = 0; i < w && n < 32; i++) {
      int sx = x + i;
      if (sx >= PORTADA_ORIGEN) break;
      if (!seConserva(sx)) continue;
      if (dx0 < 0) dx0 = PORTADA_X0 + aDestino(sx);
      fila[n++] = bmp[j * w + i];
    }
    if (n > 0) tft.pushImage(dx0, PORTADA_Y0 + aDestino(sy), n, 1, fila);
  }
  return true;
}

// Se llama una vez, al arrancar.
inline void iniciarPortada() {
  TJpgDec.setJpgScale(1);                  // 1:1 — la reducción la hacemos aquí
  TJpgDec.setSwapBytes(true);              // el orden de bytes que espera la pantalla
  TJpgDec.setCallback(volcarBloquePortada);
}

/* ------------------------------------------------------------
 *  Preparar el encuadre mirando la imagen de verdad
 * ------------------------------------------------------------
 *  Dos cosas a la vez:
 *
 *  1. Averiguar cuánto mide, para que la reducción sea correcta.
 *
 *  2. Elegir la escala del decodificador. TJpg sabe decodificar a
 *     1/2, 1/4 u 1/8 mucho más rápido que a tamaño completo, así que
 *     conviene usar la mayor reducción que TODAVÍA quede por encima
 *     del tamaño de destino. El resto lo afina nuestro volcado.
 *
 *     Ejemplo: una de 640 hacia un hueco de 212 se decodifica a 1/2
 *     (320 px, cuatro veces menos trabajo) y de ahí se baja a 212.
 *     A tamaño completo serían cuatro veces más bloques para tirar
 *     la mayoría.
 * ------------------------------------------------------------ */
inline bool prepararEncuadre(const uint8_t* datos, uint32_t largo, int lado, int y0) {
  uint16_t w = 0, h = 0;
  if (TJpgDec.getJpgSize(&w, &h, datos, largo) != 0 || w == 0) {
    Serial.println("[Imagen] no se pudo leer el tamano");
    return false;
  }
  int escala = 1;
  while (escala < 8 && (w / (escala * 2)) >= (uint16_t)lado) escala *= 2;
  TJpgDec.setJpgScale(escala);
  PORTADA_ORIGEN = w / escala;
  encuadrarImagen(lado, y0);
  if (escala > 1)
    Serial.printf("[Imagen] %dx%d -> 1/%d = %d -> %d px\n", w, h, escala, PORTADA_ORIGEN, lado);
  return true;
}
