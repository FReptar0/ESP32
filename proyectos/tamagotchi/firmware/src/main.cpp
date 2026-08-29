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
#include "letras.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

TFT_eSPI tft = TFT_eSPI();
Letras letras;

const char* URL_TOKEN   = "https://accounts.spotify.com/api/token";
const char* URL_CANCION = "https://api.spotify.com/v1/me/player/currently-playing";

const unsigned long CADA_CUANTO_MS  = 5000;   // preguntar a Spotify
const unsigned long REFRESCO_MS     = 150;    // repintar la letra

const int CANAL_PWM = 0;
const int BRILLO    = 140;                    // 0-255 (RNF-02)

// --- Colores ---
const uint16_t VERDE_SPOTIFY = 0x1DE9;
const uint16_t GRIS          = 0x8410;
const uint16_t GRIS_OSCURO   = 0x4208;
const uint16_t GRIS_LINEA    = 0x2104;

// --- Distribución de la pantalla (240x240) ---
const int MARGEN = 8, ANCHO = 240 - 16;
const int Y_TITULO = 32, Y_ARTISTA = 52;
const int Y_LETRA = 80, ALTO_LETRA = 100;
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

int  lineaPintada = -2;
unsigned long ultimaConsulta = 0, ultimoRefresco = 0;
String estadoPintado;

long progresoAhora() {
  if (!sonando) return progresoBase;
  long p = progresoBase + (long)(millis() - instanteBase);
  return (duracionMs > 0 && p > duracionMs) ? duracionMs : p;
}

// ============================================================
//  Dibujo
// ============================================================

int partirEnLineas(const String& texto, int fuente, int anchoMax,
                   String salida[], int maxLineas) {
  int n = 0; String resto = texto;
  while (resto.length() > 0 && n < maxLineas) {
    if (tft.textWidth(resto, fuente) <= anchoMax) { salida[n++] = resto; break; }
    int corte = resto.length();
    while (corte > 0 && tft.textWidth(resto.substring(0, corte), fuente) > anchoMax) corte--;
    int esp = resto.lastIndexOf(' ', corte);
    if (esp > 0) corte = esp;                 // cortar por palabra, no a media palabra
    salida[n++] = resto.substring(0, corte);
    resto = resto.substring(corte); resto.trim();
  }
  if (resto.length() > 0 && n > 0) {
    String u = salida[n-1];
    while (u.length() > 1 && tft.textWidth(u + "...", fuente) > anchoMax)
      u = u.substring(0, u.length() - 1);
    salida[n-1] = u + "...";
  }
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
  String l[1];
  partirEnLineas(String(titulo), 2, ANCHO, l, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(l[0], 120, Y_TITULO, 2);
  partirEnLineas(String(artista), 2, ANCHO, l, 1);
  tft.setTextColor(VERDE_SPOTIFY, TFT_BLACK);
  tft.drawString(l[0], 120, Y_ARTISTA, 2);
  tft.drawFastHLine(MARGEN, 74, ANCHO, GRIS_LINEA);
}

// Pinta la zona de la letra: la línea que toca, grande y blanca, con la
// anterior y la siguiente en gris tenue. Ese contexto es lo que hace que
// se siga la canción con la vista sin esfuerzo.
void zonaLetra(int idx) {
  tft.fillRect(0, Y_LETRA, 240, ALTO_LETRA, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);

  if (letras.estado == LETRA_INSTRUMENTAL) {
    tft.setTextColor(GRIS, TFT_BLACK);
    tft.drawString("~ instrumental ~", 120, Y_LETRA + 40, 4);
    return;
  }
  if (letras.estado != LETRA_OK) {
    tft.setTextColor(GRIS_OSCURO, TFT_BLACK);
    tft.drawString(letras.estado == LETRA_NO_HAY ? "sin letra" : "buscando letra...",
                   120, Y_LETRA + 42, 2);
    return;
  }

  // anterior, en gris muy tenue
  if (idx > 0 && letras.lineas[idx-1].texto[0]) {
    String a[1]; partirEnLineas(String(letras.lineas[idx-1].texto), 2, ANCHO, a, 1);
    tft.setTextColor(GRIS_OSCURO, TFT_BLACK);
    tft.drawString(a[0], 120, Y_LETRA + 2, 2);
  }

  // la línea actual: fuente 4, blanca, hasta 2 renglones
  if (idx >= 0 && letras.lineas[idx].texto[0]) {
    String c[2];
    int n = partirEnLineas(String(letras.lineas[idx].texto), 4, ANCHO, c, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int y = Y_LETRA + (n == 1 ? 32 : 22);
    for (int i = 0; i < n; i++) tft.drawString(c[i], 120, y + i * 27, 4);
  }

  // siguiente, en gris
  if (idx + 1 < letras.n && letras.lineas[idx+1].texto[0]) {
    String s[1]; partirEnLineas(String(letras.lineas[idx+1].texto), 2, ANCHO, s, 1);
    tft.setTextColor(GRIS, TFT_BLACK);
    tft.drawString(s[0], 120, Y_LETRA + ALTO_LETRA - 18, 2);
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

void consultar() {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_CANCION)) return;
  https.addHeader("Authorization", "Bearer " + accessToken);
  int cod = https.GET();

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
  instanteBase = millis();

  String clave = String(titulo) + "|" + String(artista);
  if (clave != claveActual) {
    claveActual = clave;
    estadoPintado = ""; lineaPintada = -2;
    tft.fillScreen(TFT_BLACK);
    cabecera(titulo, artista);
    letras.limpiar();
    zonaLetra(-1);                      // muestra "buscando letra..."
    Serial.printf("\n🎵 %s — %s\n", titulo, artista);
    letras.buscar(rootca_crt_bundle_start, titulo, artista, album, duracionMs);
    Serial.printf("[Letra] %d líneas · RAM %lu B\n", letras.n, (unsigned long)ESP.getFreeHeap());
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

  // A Spotify se le pregunta cada 5 s...
  if (ahora - ultimaConsulta >= CADA_CUANTO_MS) { ultimaConsulta = ahora; consultar(); }

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
