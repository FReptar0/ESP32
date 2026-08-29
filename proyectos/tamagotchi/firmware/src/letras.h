/*
 * ============================================================
 *  Letras sincronizadas desde LRCLIB  (RF-04, ADR-0003)
 * ============================================================
 *  Separado del resto a propósito: aquí vive TODO lo que sabe
 *  de dónde salen las letras. Si algún día LRCLIB desaparece,
 *  se reescribe este archivo y nada más.
 *
 *  Es la separación "de dónde vienen los datos" contra "cómo
 *  se dibujan" que piden los ADRs 0002, 0003, 0004 y 0008.
 * ============================================================
 */

#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "texto.h"

// Memoria RESERVADA DE UNA VEZ, no pedida y devuelta constantemente.
// En un aparato que va a estar encendido días, pedir y liberar memoria
// miles de veces la va dejando "picada" (fragmentada) hasta que un día
// no cabe nada. Con un array fijo eso no puede pasar.
#define MAX_LINEAS 90
#define MAX_TEXTO  72

struct LineaLetra {
  long ms;                 // en qué milisegundo de la canción va esta línea
  char texto[MAX_TEXTO];
};

enum EstadoLetra { LETRA_NO_BUSCADA, LETRA_OK, LETRA_INSTRUMENTAL, LETRA_NO_HAY };

class Letras {
public:
  LineaLetra lineas[MAX_LINEAS];
  int n = 0;
  EstadoLetra estado = LETRA_NO_BUSCADA;

  void limpiar() { n = 0; estado = LETRA_NO_BUSCADA; }

  // Devuelve el índice de la línea que toca en este milisegundo,
  // o -1 si la canción todavía no llega a la primera línea.
  int lineaEn(long ms) const {
    int r = -1;
    for (int i = 0; i < n; i++) {
      if (lineas[i].ms <= ms) r = i; else break;
    }
    return r;
  }

  bool buscar(const uint8_t* bundleCA, const char* titulo, const char* artista,
              const char* album, long duracionMs);

private:
  bool pedirExacta(const uint8_t* ca, const String& t, const String& a,
                   const String& al, long segs);
  bool pedirPorBusqueda(const uint8_t* ca, const String& t, const String& a, long segs);
  bool pedirPorId(const uint8_t* ca, long id);
  bool procesarRespuesta(HTTPClient& https, WiFiClientSecure& cli);
  void parsearLRC(const char* lrc);
};

/* ============================================================
 *  Lector que entiende "Transfer-Encoding: chunked"
 * ============================================================
 *  LRCLIB (a través de Cloudflare) no dice de antemano cuánto
 *  mide la respuesta. En su lugar la manda por TROZOS, y antes
 *  de cada trozo escribe su tamaño en hexadecimal:
 *
 *      1f4a\r\n{"id":123,"trackName":...   <- trozo de 0x1f4a bytes
 *      \r\n
 *      0\r\n\r\n                          <- trozo vacío = se acabó
 *
 *  El problema: HTTPClient::getStream() del ESP32 entrega el
 *  chorro CRUDO, con esas marcas incluidas. ArduinoJson ve
 *  "1f4a{" y falla, porque eso no es JSON.
 *
 *  Spotify sí manda Content-Length, por eso ahí el mismo código
 *  funcionaba. Fue exactamente esa diferencia la que hizo que
 *  /api/get devolviera HTTP 200 y aun así "no encontrara" nada.
 *
 *  Esta clase se pone en medio y va quitando las marcas, así que
 *  ArduinoJson solo ve JSON limpio — y sin guardar la respuesta
 *  entera en memoria, que es todo el objetivo.
 * ============================================================ */
class LectorChunked : public Stream {
  // ⚠️ Segundo nombre reservado del día: NO llamar a esto "cli".
  //    Arduino define cli() como macro (deshabilitar interrupciones,
  //    heredada de los AVR). Igual que HEX, DEC, BIN, min, max...
  //    Si un nombre corto y común falla de forma rarísima, sospecha
  //    de una macro.
  WiFiClient* _c;
  long restante = 0;      // bytes que quedan del trozo actual
  bool terminado = false;
  int  guardado = -2;     // -2 = no hay byte adelantado

  int leerCrudo() {
    if (terminado) return -1;
    if (restante == 0) {
      String cab = _c->readStringUntil('\n'); cab.trim();
      if (cab.length() == 0) { cab = _c->readStringUntil('\n'); cab.trim(); }
      restante = strtol(cab.c_str(), nullptr, 16);   // el tamaño va en hexadecimal
      if (restante <= 0) { terminado = true; return -1; }
    }
    unsigned long limite = millis() + 5000;
    while (!_c->available()) {
      if (!_c->connected() || millis() > limite) { terminado = true; return -1; }
      delay(1);
    }
    int b = _c->read();
    if (b >= 0 && --restante == 0) { _c->read(); _c->read(); }  // saltar el \r\n final
    return b;
  }

public:
  explicit LectorChunked(WiFiClient* c) : _c(c) {}
  int read() override  { if (guardado != -2) { int b = guardado; guardado = -2; return b; } return leerCrudo(); }
  int peek() override  { if (guardado == -2) guardado = leerCrudo(); return guardado; }
  int available() override { return (guardado != -2 || !terminado) ? 1 : 0; }
  void flush() override {}
  size_t write(uint8_t) override { return 0; }
};

// --- Codificar para meter texto en una URL ---
// Los títulos traen espacios, tildes y signos. En una URL no pueden ir
// tal cual: hay que escribirlos como %XX. Sin esto, "Chilango Blues"
// rompería la petición.
static String urlenc(const String& s) {
  // ⚠️ No llamarlo HEX: Arduino ya define HEX como macro (=16), la que
  //    se usa en Serial.print(x, HEX). El compilador lo sustituiría y
  //    "HEX[i]" se convertiría en "16[i]", que no significa nada.
  //    Lo mismo pasa con DEC, BIN y OCT.
  static const char* DIGITOS = "0123456789ABCDEF";
  String out; out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { out += '%'; out += DIGITOS[c >> 4]; out += DIGITOS[c & 0x0F]; }
  }
  return out;
}

// --- Convertir "[01:23.45]" a milisegundos ---
static long tiempoLRC(const char* p) {
  int mm = 0, ss = 0, cc = 0;
  if (sscanf(p, "[%d:%d.%d]", &mm, &ss, &cc) < 2) return -1;
  return (long)mm * 60000L + (long)ss * 1000L + (long)cc * 10L;
}

inline void Letras::parsearLRC(const char* lrc) {
  n = 0;
  const char* p = lrc;
  while (*p && n < MAX_LINEAS) {
    if (*p == '[') {
      long ms = tiempoLRC(p);
      const char* cierre = strchr(p, ']');
      if (ms >= 0 && cierre) {
        const char* ini = cierre + 1;
        while (*ini == ' ') ini++;
        const char* fin = strchr(ini, '\n');
        size_t len = fin ? (size_t)(fin - ini) : strlen(ini);

        // Las líneas vacías del LRC son silencios entre estrofas.
        // Se conservan: dejan la pantalla en blanco cuando nadie canta,
        // que es justo lo correcto.
        String bruto;
        bruto.reserve(len + 1);
        for (size_t k = 0; k < len; k++) bruto += ini[k];
        bruto.trim();

        // La conversión UTF-8 -> Latin-1 se hace AQUÍ, una sola vez al
        // cargar la letra, y no cada vez que se dibuja (6 veces por
        // segundo). Los acentos se CONSERVAN: la fuente generada con
        // tools/gen-fuente.py llega hasta el código 255.
        String limpio = aLatin1(bruto);
        size_t l2 = limpio.length();
        if (l2 >= MAX_TEXTO) l2 = MAX_TEXTO - 1;
        memcpy(lineas[n].texto, limpio.c_str(), l2);
        lineas[n].texto[l2] = 0;
        lineas[n].ms = ms;
        n++;
      }
    }
    const char* sig = strchr(p, '\n');
    if (!sig) break;
    p = sig + 1;
  }
}

inline bool Letras::procesarRespuesta(HTTPClient& https, WiFiClientSecure& cli) {
  // Filtro: de los ~8 KB solo nos interesan dos campos.
  JsonDocument filtro;
  filtro["syncedLyrics"] = true;
  filtro["instrumental"] = true;

  JsonDocument doc;
  DeserializationError err;
  if (https.getSize() < 0) {              // -1 = no dijo cuánto mide: viene por trozos
    LectorChunked lector(&cli);
    err = deserializeJson(doc, lector, DeserializationOption::Filter(filtro));
  } else {
    err = deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filtro));
  }
  if (err) { Serial.printf("[Letra] JSON: %s\n", err.c_str()); return false; }

  if (doc["instrumental"] | false) { estado = LETRA_INSTRUMENTAL; return true; }

  const char* lrc = doc["syncedLyrics"] | "";
  if (!lrc || !*lrc) return false;

  parsearLRC(lrc);
  if (n == 0) return false;
  estado = LETRA_OK;
  return true;
}

inline bool Letras::pedirExacta(const uint8_t* ca, const String& t, const String& a,
                                const String& al, long segs) {
  String url = "https://lrclib.net/api/get?track_name=" + urlenc(t) +
               "&artist_name=" + urlenc(a) + "&album_name=" + urlenc(al) +
               "&duration=" + String(segs);
  WiFiClientSecure cli; cli.setCACertBundle(ca);
  HTTPClient https;
  if (!https.begin(cli, url)) return false;
  https.addHeader("User-Agent", "tamagotchi-esp32");
  int cod = https.GET();
  bool ok = (cod == 200) && procesarRespuesta(https, cli);
  https.end();
  Serial.printf("[Letra] exacta -> HTTP %d %s\n", cod, ok ? "✅" : "");
  return ok;
}

inline bool Letras::pedirPorId(const uint8_t* ca, long id) {
  WiFiClientSecure cli; cli.setCACertBundle(ca);
  HTTPClient https;
  if (!https.begin(cli, "https://lrclib.net/api/get/" + String(id))) return false;
  https.addHeader("User-Agent", "tamagotchi-esp32");
  int cod = https.GET();
  bool ok = (cod == 200) && procesarRespuesta(https, cli);
  https.end();
  Serial.printf("[Letra] por id %ld -> HTTP %d %s\n", id, cod, ok ? "✅" : "");
  return ok;
}

inline bool Letras::pedirPorBusqueda(const uint8_t* ca, const String& t,
                                     const String& a, long segs) {
  // ⚠️ Esta respuesta pesa ~91 KB: trae la letra COMPLETA de 20 resultados.
  // Cargarla entera sería la mitad de la memoria libre del ESP32.
  //
  // El filtro hace que ArduinoJson lea el chorro de datos y DESCARTE las
  // letras según pasan, quedándose solo con id y duration de cada uno.
  // Unos cientos de bytes en vez de 91 KB.
  String url = "https://lrclib.net/api/search?track_name=" + urlenc(t) +
               "&artist_name=" + urlenc(a);
  WiFiClientSecure cli; cli.setCACertBundle(ca);
  HTTPClient https;
  if (!https.begin(cli, url)) return false;
  https.addHeader("User-Agent", "tamagotchi-esp32");
  int cod = https.GET();
  if (cod != 200) { https.end(); Serial.printf("[Letra] búsqueda -> HTTP %d\n", cod); return false; }

  JsonDocument filtro;
  filtro[0]["id"] = true;
  filtro[0]["duration"] = true;

  JsonDocument doc;
  DeserializationError err;
  if (https.getSize() < 0) {
    LectorChunked lector(&cli);
    err = deserializeJson(doc, lector, DeserializationOption::Filter(filtro));
  } else {
    err = deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filtro));
  }
  https.end();
  if (err) { Serial.printf("[Letra] búsqueda JSON: %s\n", err.c_str()); return false; }

  // Elegir el resultado cuya duración más se parezca a la nuestra.
  long mejorId = -1, mejorDif = 999999;
  for (JsonObject o : doc.as<JsonArray>()) {
    long d = (long)(o["duration"] | 0.0);
    long dif = labs(d - segs);
    if (dif < mejorDif) { mejorDif = dif; mejorId = o["id"] | -1; }
  }
  Serial.printf("[Letra] mejor coincidencia: id=%ld (±%ld s)\n", mejorId, mejorDif);

  // Más de 5 segundos de diferencia casi seguro es otra versión.
  if (mejorId < 0 || mejorDif > 5) return false;
  return pedirPorId(ca, mejorId);
}

inline bool Letras::buscar(const uint8_t* ca, const char* titulo, const char* artista,
                           const char* album, long duracionMs) {
  limpiar();
  long segs = duracionMs / 1000;
  Serial.printf("\n[Letra] buscando: %s — %s (%lds)\n", titulo, artista, segs);

  // Paso 1: la exacta. ~5 KB, y funciona porque Spotify nos da la duración.
  if (pedirExacta(ca, titulo, artista, album, segs)) return true;

  // Paso 2: solo si falla. Versiones remasterizadas, deluxe, en vivo...
  if (pedirPorBusqueda(ca, titulo, artista, segs)) return true;

  estado = LETRA_NO_HAY;
  Serial.println("[Letra] ❌ no encontrada");
  return false;
}
