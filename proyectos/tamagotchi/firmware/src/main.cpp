/*
 * ============================================================
 *  Tamagotchi Spotify — Paso 8: letras sincronizadas
 * ============================================================
 *  Todo junto: WiFi + NTP + TLS verificado + token propio +
 *  Spotify + pantalla + letras de LRCLIB.
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "mbedtls/base64.h"
#include "secrets.h"
#include "texto.h"
#include "letras.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

TFT_eSPI tft = TFT_eSPI();

Letras letras;      // la letra que se está mostrando
Letras precarga;    // la de la SIGUIENTE canción, ya descargada
String clavePrecarga;

const char* URL_TOKEN   = "https://accounts.spotify.com/api/token";
const char* URL_CANCION = "https://api.spotify.com/v1/me/player/currently-playing";
const char* URL_COLA    = "https://api.spotify.com/v1/me/player/queue";

const unsigned long CADA_CUANTO_MS  = 5000;   // preguntar a Spotify, normal
const unsigned long CERCA_DEL_FIN_MS = 1000;  // ...y más seguido al final de la canción
const unsigned long UMBRAL_FIN_MS   = 15000;  // "final" = últimos 15 s
const unsigned long REFRESCO_MS     = 100;    // repintar la letra

// Ajuste fino del sincronismo de la letra. Positivo = la letra va por
// delante. Si la ves atrasada, súbelo; si adelantada, bájalo.
const long AJUSTE_LETRA_MS = 400;

const int CANAL_PWM = 0;
const int BRILLO    = 140;                    // 0-255 (RNF-02)

// --- Colores ---
const uint16_t VERDE_SPOTIFY = 0x1DE9;
const uint16_t GRIS          = 0x8410;
const uint16_t GRIS_OSCURO   = 0x4208;
const uint16_t GRIS_LINEA    = 0x2104;

// --- Distribución de la pantalla (240x240) ---
const int MARGEN = 8, ANCHO = 240 - 16;
const int Y_TITULO = 30, Y_ARTISTA = 50;
const int Y_LETRA = 76, ALTO_LETRA = 114;
const int Y_BARRA = 196, Y_TIEMPOS = 214;

String accessToken;
unsigned long tokenCaducaEn = 0;
String claveActual;

// Para que la letra avance suave entre consultas: se guarda el progreso
// que reportó Spotify y CUÁNDO lo dijo. Entre consulta y consulta, el
// progreso se calcula sumando el tiempo transcurrido. Sin esto la letra
// saltaría de golpe cada 5 segundos y se vería fatal.
long  progresoBase = 0, duracionMs = 0;
unsigned long instanteBase = 0;
bool  sonando = false;
bool  colaPedida = false;

int  lineaPintada = -2;
unsigned long ultimaConsulta = 0, ultimoRefresco = 0;
String estadoPintado;

long progresoAhora() {
  if (!sonando) return progresoBase;
  long p = progresoBase + (long)(millis() - instanteBase) + AJUSTE_LETRA_MS;
  if (p < 0) p = 0;
  return (duracionMs > 0 && p > duracionMs) ? duracionMs : p;
}

// ============================================================
//  Dibujo
// ============================================================

// Parte un texto en líneas que quepan a lo ancho, cortando por PALABRAS.
// Devuelve cuántas líneas usó; si `cupoTodo` no es nulo, dice si el texto
// entró completo o hubo que recortarlo.
int partirEnLineas(const String& texto, int fuente, int anchoMax,
                   String salida[], int maxLineas, bool* cupoTodo = nullptr) {
  int n = 0;
  String resto = texto;
  resto.trim();

  while (resto.length() > 0 && n < maxLineas) {
    if (tft.textWidth(resto, fuente) <= anchoMax) {
      salida[n++] = resto;
      resto = "";          // 🐛 ESTO FALTABA.
      break;               // Sin limpiar `resto`, la comprobación de más
    }                      // abajo creía que había sobrado texto y añadía
                           // "..." a frases que SÍ cabían completas.
    int corte = resto.length();
    while (corte > 0 && tft.textWidth(resto.substring(0, corte), fuente) > anchoMax) corte--;
    int esp = resto.lastIndexOf(' ', corte);
    if (esp > 0) corte = esp;              // cortar por palabra, no a media palabra
    if (corte <= 0) corte = 1;             // palabra larguísima: cortar donde sea
    salida[n++] = resto.substring(0, corte);
    resto = resto.substring(corte);
    resto.trim();
  }

  if (cupoTodo) *cupoTodo = (resto.length() == 0);
  return n;
}

// Como el anterior, pero si el texto no cabe con la fuente pedida, prueba
// con la más chica antes de rendirse. Recortar con "..." pierde palabras
// de la canción; encoger la letra no pierde nada.
int partirConReserva(const String& texto, int anchoMax, String salida[],
                     int maxLineas, int& fuenteUsada) {
  bool cupo = false;
  fuenteUsada = 4;
  int n = partirEnLineas(texto, 4, anchoMax, salida, maxLineas, &cupo);
  if (cupo) return n;

  fuenteUsada = 2;                       // segunda oportunidad, más chica
  n = partirEnLineas(texto, 2, anchoMax, salida, maxLineas + 1, &cupo);
  if (cupo) return n;

  // Ya no hay más remedio: recortar la última línea e indicarlo.
  String u = salida[n-1];
  while (u.length() > 1 && tft.textWidth(u + "...", 2) > anchoMax)
    u = u.substring(0, u.length() - 1);
  salida[n-1] = u + "...";
  return n;
}

void barraEstado(const String& txt, uint16_t color) {
  if (estadoPintado == txt) return;
  estadoPintado = txt;
  tft.fillRect(0, 0, 240, 26, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(txt, MARGEN, 6, 2);
  tft.drawFastHLine(0, 27, 240, GRIS_LINEA);
}

void cabecera(const char* titulo, const char* artista) {
  tft.fillRect(0, 30, 240, 46, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  // aASCII: sin esto, "León" se vería "Len" (ver texto.h)
  String l[2]; int f;
  partirConReserva(aASCII(String(titulo)), ANCHO, l, 1, f);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(l[0], 120, Y_TITULO, 2);
  partirConReserva(aASCII(String(artista)), ANCHO, l, 1, f);
  tft.setTextColor(VERDE_SPOTIFY, TFT_BLACK);
  tft.drawString(l[0], 120, Y_ARTISTA, 2);
  tft.drawFastHLine(MARGEN, 70, ANCHO, GRIS_LINEA);
}

// Pinta la zona de la letra: la línea que toca, grande y blanca, con la
// anterior y la siguiente en gris tenue. Ese contexto es lo que hace que
// se siga la canción con la vista sin esfuerzo.
// ¿Esta línea es un silencio? Lo es si está vacía o si solo contiene
// símbolos de nota musical (que en los .lrc marcan intros, solos y
// puentes instrumentales).
bool esSilencio(const char* t) {
  if (!t || !*t) return true;
  for (const char* p = t; *p; p++) if (*p != ' ' && *p != '~') return false;
  return true;
}

// Busca la siguiente línea CON TEXTO a partir de `desde`.
int siguienteConTexto(int desde) {
  for (int i = (desde < 0 ? 0 : desde); i < letras.n; i++)
    if (!esSilencio(letras.lineas[i].texto)) return i;
  return -1;
}

int anteriorConTexto(int desde) {
  for (int i = desde; i >= 0; i--)
    if (!esSilencio(letras.lineas[i].texto)) return i;
  return -1;
}

// Dibuja una nota musical con primitivas gráficas. Las fuentes no tienen
// el símbolo ♪, así que se pinta a mano: cabeza, palo y banderita.
void dibujarNota(int x, int y, uint16_t color) {
  tft.fillCircle(x, y, 4, color);
  tft.drawFastVLine(x + 3, y - 15, 15, color);
  tft.drawFastVLine(x + 4, y - 15, 15, color);
  tft.drawLine(x + 4, y - 15, x + 10, y - 11, color);
  tft.drawLine(x + 4, y - 12, x + 10, y - 8,  color);
}

void zonaLetra(int idx) {
  tft.fillRect(0, Y_LETRA, 240, ALTO_LETRA, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);

  if (letras.estado == LETRA_INSTRUMENTAL) {
    tft.setTextColor(GRIS, TFT_BLACK);
    tft.drawString("instrumental", 120, Y_LETRA + 50, 4);
    dibujarNota(90, Y_LETRA + 30, GRIS);
    dibujarNota(140, Y_LETRA + 34, GRIS);
    return;
  }
  if (letras.estado != LETRA_OK) {
    tft.setTextColor(GRIS_OSCURO, TFT_BLACK);
    tft.drawString(letras.estado == LETRA_NO_HAY ? "sin letra" : "buscando letra...",
                   120, Y_LETRA + 48, 2);
    return;
  }

  bool enSilencio = (idx < 0) || esSilencio(letras.lineas[idx].texto);

  // ------------------------------------------------------------------
  //  SILENCIO: intro, solo instrumental o pausa entre estrofas.
  //
  //  Antes aquí se dejaba la pantalla en blanco, y estaba mal: durante
  //  la intro de una canción podías estar 20 segundos mirando un hueco
  //  negro sin saber si el aparato se había colgado.
  //
  //  Ahora se dibuja una nota (para que se vea que ES un tramo sin voz,
  //  no un fallo) y DEBAJO, en gris, la frase que viene. Así se puede
  //  seguir la canción incluso antes de que empiece a cantarse.
  // ------------------------------------------------------------------
  if (enSilencio) {
    int sig = siguienteConTexto(idx < 0 ? 0 : idx + 1);

    dibujarNota(104, Y_LETRA + 26, GRIS_OSCURO);
    dibujarNota(128, Y_LETRA + 30, GRIS_OSCURO);

    if (sig >= 0) {
      String s2[3]; int fs;
      int ns = partirConReserva(String(letras.lineas[sig].texto), ANCHO, s2, 2, fs);
      tft.setTextColor(GRIS, TFT_BLACK);
      int alto = (fs == 4) ? 27 : 19;
      int y = Y_LETRA + ALTO_LETRA - 8 - ns * alto;
      for (int i = 0; i < ns; i++) tft.drawString(s2[i], 120, y + i * alto, fs);
    }
    return;
  }

  // ------------------------------------------------------------------
  //  CANTANDO: la frase actual grande y blanca, con contexto en gris.
  // ------------------------------------------------------------------
  String c[4]; int fuente = 4;
  int n = partirConReserva(String(letras.lineas[idx].texto), ANCHO, c, 3, fuente);

  int alto = (fuente == 4) ? 27 : 19;
  int bloque = n * alto;
  int y = Y_LETRA + (ALTO_LETRA - bloque) / 2;

  // El contexto solo se dibuja si sobra sitio: cuando la frase actual es
  // larga, ella manda.
  bool hayHueco = (bloque + 42) <= ALTO_LETRA;

  if (hayHueco) {
    int ant = anteriorConTexto(idx - 1);
    if (ant >= 0) {
      String a[3]; int fa;
      partirConReserva(String(letras.lineas[ant].texto), ANCHO, a, 1, fa);
      tft.setTextColor(GRIS_OSCURO, TFT_BLACK);
      tft.drawString(a[0], 120, Y_LETRA, 2);
    }
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i < n; i++) tft.drawString(c[i], 120, y + i * alto, fuente);

  if (hayHueco) {
    int sig = siguienteConTexto(idx + 1);
    if (sig >= 0) {
      String sg[3]; int fs;
      partirConReserva(String(letras.lineas[sig].texto), ANCHO, sg, 1, fs);
      tft.setTextColor(GRIS, TFT_BLACK);
      tft.drawString(sg[0], 120, Y_LETRA + ALTO_LETRA - 18, 2);
    }
  }
}

void barraProgreso() {
  if (duracionMs <= 0) return;
  long p = progresoAhora();
  int lleno = (int)((long long)p * ANCHO / duracionMs);
  if (lleno < 0) lleno = 0; if (lleno > ANCHO) lleno = ANCHO;
  tft.fillRect(MARGEN, Y_BARRA, lleno, 5, sonando ? VERDE_SPOTIFY : GRIS);
  tft.fillRect(MARGEN + lleno, Y_BARRA, ANCHO - lleno, 5, GRIS_LINEA);

  char a[12], b[12];
  snprintf(a, sizeof(a), "%ld:%02ld", p / 60000, (p / 1000) % 60);
  snprintf(b, sizeof(b), "%ld:%02ld", duracionMs / 60000, (duracionMs / 1000) % 60);
  tft.fillRect(0, Y_TIEMPOS, 240, 18, TFT_BLACK);
  tft.setTextColor(GRIS, TFT_BLACK);
  tft.setTextDatum(TL_DATUM); tft.drawString(a, MARGEN, Y_TIEMPOS, 2);
  tft.setTextDatum(TR_DATUM); tft.drawString(b, 240 - MARGEN, Y_TIEMPOS, 2);
}

void pantallaMensaje(const char* l1, const char* l2, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  estadoPintado = ""; lineaPintada = -2;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, TFT_BLACK); tft.drawString(l1, 120, 110, 4);
  if (l2) { tft.setTextColor(GRIS, TFT_BLACK); tft.drawString(l2, 120, 145, 2); }
}

// ============================================================
//  Spotify
// ============================================================

bool renovarToken() {
  String cred = String(SPOTIFY_CLIENT_ID) + ":" + String(SPOTIFY_CLIENT_SECRET);
  unsigned char b64[256]; size_t n = 0;
  mbedtls_base64_encode(b64, sizeof(b64), &n,
                        (const unsigned char*)cred.c_str(), cred.length());
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_TOKEN)) return false;
  https.addHeader("Authorization", "Basic " + String((char*)b64).substring(0, n));
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int cod = https.POST("grant_type=refresh_token&refresh_token=" + String(SPOTIFY_REFRESH_TOKEN));
  if (cod != 200) { Serial.printf("[Token] ❌ %d\n", cod); https.end(); return false; }
  JsonDocument doc;
  if (deserializeJson(doc, https.getStream())) { https.end(); return false; }
  https.end();
  accessToken = doc["access_token"].as<String>();
  long dur = doc["expires_in"] | 3600;
  tokenCaducaEn = millis() + (dur - 300) * 1000UL;
  Serial.printf("[Token] ✅ renovado (%ld s)\n", dur);
  return true;
}

// ============================================================
//  Precarga: descargar la letra de la SIGUIENTE canción
// ============================================================
//  Sin esto, al cambiar de canción hay que ir a Spotify Y a LRCLIB
//  antes de poder mostrar nada: dos viajes por internet, y la letra
//  llega tarde aunque la canción ya haya cambiado.
//
//  Spotify sabe qué viene después (/me/player/queue). Así que en
//  cuanto la canción actual está estable, se descarga la letra de la
//  siguiente y se guarda aparte. Cuando llegue el cambio, ya está
//  lista: cero espera.
//
//  Usa el permiso user-read-playback-state, que ya se pidió en el
//  ADR-0010 pensando justo en cosas como esta.
void precargarSiguiente() {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_COLA)) return;
  https.addHeader("Authorization", "Bearer " + accessToken);
  int cod = https.GET();
  if (cod != 200) { https.end(); Serial.printf("[Cola] HTTP %d\n", cod); return; }

  // La cola trae hasta 20 canciones con todos sus datos: son decenas de
  // KB. El filtro se queda solo con la PRIMERA y sus cuatro campos.
  JsonDocument filtro;
  filtro["queue"][0]["name"] = true;
  filtro["queue"][0]["duration_ms"] = true;
  filtro["queue"][0]["album"]["name"] = true;
  filtro["queue"][0]["artists"][0]["name"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, https.getStream(),
                                             DeserializationOption::Filter(filtro));
  https.end();
  if (err) { Serial.printf("[Cola] JSON: %s\n", err.c_str()); return; }

  JsonObject sig = doc["queue"][0];
  if (sig.isNull()) return;

  const char* t = sig["name"] | "";
  const char* a = sig["artists"][0]["name"] | "";
  const char* al = sig["album"]["name"] | "";
  long dur = sig["duration_ms"] | 0;
  if (!*t) return;

  String clave = String(t) + "|" + String(a);
  if (clave == clavePrecarga) return;      // ya la teníamos

  Serial.printf("[Precarga] siguiente: %s — %s\n", t, a);

  // ⚠️ Aquí había un `Letras tmp;` como variable local, y reventaba la
  //    placa con "Stack canary watchpoint triggered".
  //
  //    Un objeto Letras pesa ~6.8 KB (90 líneas x 76 bytes). Las
  //    variables locales viven en la PILA, y la pila de la tarea
  //    principal de Arduino son solo 8 KB — aunque queden 186 KB de
  //    memoria libre, que es de otro sitio (el "montón" o heap).
  //
  //    El "canario" es un valor centinela que el sistema pone al final
  //    de la pila. Si aparece pisado, sabe que hubo desbordamiento y
  //    reinicia a propósito, en vez de seguir con la memoria corrupta.
  //
  //    La solución no es agrandar la pila: es no poner 7 KB en ella.
  //    `precarga` es una variable global, así que vive fuera de la pila.
  precarga.limpiar();
  if (!precarga.buscar(rootca_crt_bundle_start, t, a, al, dur)) {
    precarga.limpiar();
    precarga.estado = LETRA_NO_HAY;
  }
  clavePrecarga = clave;
  Serial.printf("[Precarga] lista (%d líneas) · RAM %lu B\n",
                precarga.n, (unsigned long)ESP.getFreeHeap());
}

void consultar() {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_CANCION)) return;
  https.addHeader("Authorization", "Bearer " + accessToken);

  // Se mide cuánto tarda el viaje de ida y vuelta. El progress_ms que
  // devuelve Spotify corresponde a un instante ANTERIOR a que llegue la
  // respuesta: aproximadamente la mitad del viaje. Sin compensarlo, la
  // letra va sistemáticamente atrasada.
  unsigned long t0 = millis();
  int cod = https.GET();
  unsigned long viaje = millis() - t0;

  if (cod == 204) {                       // nada sonando: NO es un error (RF-06)
    if (claveActual != "__nada__") {
      claveActual = "__nada__"; letras.limpiar(); sonando = false;
      pantallaMensaje("En silencio", "no hay nada sonando", GRIS);
    }
    https.end(); return;
  }
  if (cod == 401) { https.end(); renovarToken(); return; }
  if (cod == 429) {
    int e = https.header("Retry-After").toInt();
    Serial.printf("[API] ⚠️ límite, esperando %d s\n", e);
    https.end(); delay((e + 1) * 1000UL); return;
  }
  if (cod != 200) { Serial.printf("[API] ❌ %d\n", cod); https.end(); return; }

  JsonDocument filtro;
  filtro["is_playing"] = true; filtro["progress_ms"] = true;
  filtro["item"]["name"] = true; filtro["item"]["duration_ms"] = true;
  filtro["item"]["album"]["name"] = true; filtro["item"]["artists"][0]["name"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, https.getStream(),
                                             DeserializationOption::Filter(filtro));
  https.end();
  if (err) { Serial.printf("[API] ❌ JSON: %s\n", err.c_str()); return; }

  const char* titulo  = doc["item"]["name"] | "(sin título)";
  const char* album   = doc["item"]["album"]["name"] | "";
  const char* artista = doc["item"]["artists"][0]["name"] | "";
  progresoBase = doc["progress_ms"] | 0;
  duracionMs   = doc["item"]["duration_ms"] | 0;
  sonando      = doc["is_playing"] | false;
  instanteBase = millis() - viaje / 2;      // compensar el viaje de red

  String clave = String(titulo) + "|" + String(artista);
  if (clave != claveActual) {
    claveActual = clave;
    estadoPintado = ""; lineaPintada = -2;
    colaPedida = false;
    tft.fillScreen(TFT_BLACK);
    cabecera(titulo, artista);
    Serial.printf("\n🎵 %s — %s\n", titulo, artista);

    if (clave == clavePrecarga && precarga.estado != LETRA_NO_BUSCADA) {
      // 🚀 Ya la teníamos descargada: aparece al instante, sin ir a la red.
      letras = precarga;
      clavePrecarga = "";
      precarga.limpiar();
      Serial.printf("[Letra] ⚡ desde precarga (%d líneas)\n", letras.n);
    } else {
      letras.limpiar();
      zonaLetra(-1);                    // "buscando letra..."
      letras.buscar(rootca_crt_bundle_start, titulo, artista, album, duracionMs);
      Serial.printf("[Letra] %d líneas · RAM %lu B\n", letras.n, (unsigned long)ESP.getFreeHeap());
    }
    lineaPintada = -2;                  // forzar repintado con la letra ya cargada
  }
  barraEstado(sonando ? "SONANDO" : "EN PAUSA", sonando ? VERDE_SPOTIFY : GRIS);
}

// ============================================================

void setup() {
  Serial.begin(115200); delay(400);
  ledcSetup(CANAL_PWM, 5000, 8); ledcAttachPin(TFT_BL, CANAL_PWM);
  ledcWrite(CANAL_PWM, BRILLO);
  tft.init(); tft.setRotation(0);

  pantallaMensaje("Tamagotchi", "conectando al WiFi...", TFT_WHITE);
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(300);
  Serial.printf("WiFi ✅ %s\n", WiFi.localIP().toString().c_str());

  // Sin hora correcta no se validan certificados: el ESP32 arranca en 1970.
  pantallaMensaje("Tamagotchi", "sincronizando la hora...", TFT_WHITE);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t t = 0; while (t < 1700000000) { delay(300); time(&t); }

  pantallaMensaje("Tamagotchi", "hablando con Spotify...", TFT_WHITE);
  renovarToken();
  claveActual = "";
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (claveActual != "__sinred__") {
      claveActual = "__sinred__";
      pantallaMensaje("Sin conexion", "reintentando...", TFT_RED);
    }
    WiFi.reconnect(); delay(5000); return;
  }
  if (accessToken == "" || millis() > tokenCaducaEn) {
    if (!renovarToken()) { delay(30000); return; }
  }

  unsigned long ahora = millis();

  // Sondeo ADAPTATIVO: normalmente cada 5 s, pero en los últimos 15
  // segundos de la canción cada 1 s. Así el cambio de canción se detecta
  // casi al instante, sin gastar peticiones el resto del tiempo.
  long falta = duracionMs - progresoAhora();
  unsigned long intervalo = (sonando && duracionMs > 0 && falta < (long)UMBRAL_FIN_MS)
                            ? CERCA_DEL_FIN_MS : CADA_CUANTO_MS;

  if (ahora - ultimaConsulta >= intervalo) { ultimaConsulta = ahora; consultar(); }

  // Una vez la canción está en marcha, se aprovecha para descargar la
  // letra de la siguiente. Se hace a los 8 s para no competir con la
  // carga de la actual.
  if (!colaPedida && sonando && letras.estado != LETRA_NO_BUSCADA
      && progresoAhora() > 8000 && claveActual.length() && claveActual[0] != '_') {
    colaPedida = true;
    precargarSiguiente();
  }

  // ...pero la letra se refresca 6 veces por segundo, calculando el
  // progreso a partir del reloj interno. Así avanza suave en vez de a
  // saltos, y de paso no se gastan peticiones de más.
  if (ahora - ultimoRefresco >= REFRESCO_MS) {
    ultimoRefresco = ahora;
    if (claveActual.length() && claveActual[0] != '_') {
      int idx = (letras.estado == LETRA_OK) ? letras.lineaEn(progresoAhora()) : -1;
      if (idx != lineaPintada) { lineaPintada = idx; zonaLetra(idx); }
      barraProgreso();
    }
  }
  delay(20);
}
