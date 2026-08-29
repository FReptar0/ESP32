/*
 * ============================================================
 *  Tamagotchi Spotify — Paso 7: la pantalla de verdad
 * ============================================================
 *  Junta todo lo anterior:
 *    · WiFi + NTP + TLS verificado      (pasos 2, 3, ADR-0009)
 *    · Renovar el token solo            (paso 5)
 *    · Leer qué suena en Spotify        (paso 6)
 *    · Dibujarlo en la GMT130           (este paso, ADR-0004/0008)
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

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

TFT_eSPI tft = TFT_eSPI();

const char* URL_TOKEN   = "https://accounts.spotify.com/api/token";
const char* URL_CANCION = "https://api.spotify.com/v1/me/player/currently-playing";
const unsigned long CADA_CUANTO_MS = 5000;

// --- Brillo del backlight (RNF-02: que no moleste de noche) ---
// PWM = prender y apagar la luz miles de veces por segundo. Si está
// encendida el 55% del tiempo, se ve al 55% de brillo. Sin hardware extra.
const int CANAL_PWM = 0;
const int BRILLO    = 140;      // de 0 a 255

// --- Distribución de la pantalla (240x240) ---
const int Y_ESTADO   = 6;
const int Y_TITULO   = 40;
const int Y_ARTISTA  = 110;
const int Y_ALBUM    = 140;
const int Y_BARRA    = 196;
const int Y_TIEMPOS  = 216;
const int MARGEN     = 8;
const int ANCHO_UTIL = 240 - MARGEN * 2;

String accessToken;
unsigned long tokenCaducaEn = 0;
String ultimaClave;                 // para no redibujar si no cambió nada
String estadoAnterior;

// ============================================================
//  Dibujo
// ============================================================

void barraEstado(const char* texto, uint16_t color) {
  if (estadoAnterior == texto) return;
  estadoAnterior = texto;
  tft.fillRect(0, 0, 240, 26, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(texto, MARGEN, Y_ESTADO, 2);
  tft.drawFastHLine(0, 28, 240, 0x2104);   // gris muy oscuro
}

// Parte un texto en líneas que quepan a lo ancho, cortando por PALABRAS.
// Cortar a mitad de palabra se lee fatal; por eso se busca el último
// espacio que todavía cabe.
int partirEnLineas(const String& texto, int fuente, int anchoMax,
                   String salida[], int maxLineas) {
  int n = 0;
  String resto = texto;
  while (resto.length() > 0 && n < maxLineas) {
    if (tft.textWidth(resto, fuente) <= anchoMax) { salida[n++] = resto; break; }

    int corte = resto.length();
    while (corte > 0 && tft.textWidth(resto.substring(0, corte), fuente) > anchoMax) corte--;

    int espacio = resto.lastIndexOf(' ', corte);
    if (espacio > 0) corte = espacio;

    salida[n++] = resto.substring(0, corte);
    resto = resto.substring(corte);
    resto.trim();
  }
  // Si no cupo todo, se marca con "..." para que se note que falta texto.
  if (resto.length() > 0 && n > 0) {
    String ult = salida[n - 1];
    while (ult.length() > 1 && tft.textWidth(ult + "...", fuente) > anchoMax)
      ult = ult.substring(0, ult.length() - 1);
    salida[n - 1] = ult + "...";
  }
  return n;
}

void dibujarCancion(const char* titulo, const char* artista, const char* album) {
  tft.fillRect(0, 30, 240, 150, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);

  // --- Título: fuente 4 (la elegida en RNF-01), hasta 2 líneas ---
  String lineas[2];
  int n = partirEnLineas(String(titulo), 4, ANCHO_UTIL, lineas, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i < n; i++) tft.drawString(lineas[i], 120, Y_TITULO + i * 30, 4);

  // --- Artista: fuente 4 en verde Spotify, 1 línea ---
  String la[1];
  partirEnLineas(String(artista), 4, ANCHO_UTIL, la, 1);
  tft.setTextColor(0x1DE9, TFT_BLACK);              // verde Spotify aproximado
  tft.drawString(la[0], 120, Y_ARTISTA, 4);

  // --- Álbum: fuente 2, gris. Es lo menos importante de los tres. ---
  String lb[1];
  partirEnLineas(String(album), 2, ANCHO_UTIL, lb, 1);
  tft.setTextColor(0x8410, TFT_BLACK);              // gris medio
  tft.drawString(lb[0], 120, Y_ALBUM, 2);
}

void dibujarProgreso(long progreso, long duracion, bool sonando) {
  if (duracion <= 0) return;

  int ancho = ANCHO_UTIL;
  int lleno = (int)((long long)progreso * ancho / duracion);
  if (lleno < 0) lleno = 0;
  if (lleno > ancho) lleno = ancho;

  tft.fillRect(MARGEN, Y_BARRA, lleno, 6, sonando ? 0x1DE9 : 0x8410);
  tft.fillRect(MARGEN + lleno, Y_BARRA, ancho - lleno, 6, 0x2104);

  char izq[16], der[16];
  snprintf(izq, sizeof(izq), "%ld:%02ld", progreso / 60000, (progreso / 1000) % 60);
  snprintf(der, sizeof(der), "%ld:%02ld", duracion / 60000, (duracion / 1000) % 60);

  tft.fillRect(0, Y_TIEMPOS, 240, 20, TFT_BLACK);
  tft.setTextColor(0x8410, TFT_BLACK);
  tft.setTextDatum(TL_DATUM); tft.drawString(izq, MARGEN, Y_TIEMPOS, 2);
  tft.setTextDatum(TR_DATUM); tft.drawString(der, 240 - MARGEN, Y_TIEMPOS, 2);
}

void pantallaMensaje(const char* linea1, const char* linea2, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  estadoAnterior = "";
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(linea1, 120, 110, 4);
  if (linea2) {
    tft.setTextColor(0x8410, TFT_BLACK);
    tft.drawString(linea2, 120, 145, 2);
  }
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
  tokenCaducaEn = millis() + (dur - 300) * 1000UL;   // renovar 5 min antes
  Serial.printf("[Token] ✅ renovado (%ld s)\n", dur);
  return true;
}

void consultar() {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_CANCION)) return;
  https.addHeader("Authorization", "Bearer " + accessToken);

  int cod = https.GET();

  // 204 = no hay nada sonando. Es una respuesta legítima, no un error:
  // hay que distinguirla de "se cayó la red" (RF-06 contra RF-07).
  if (cod == 204) {
    if (ultimaClave != "__nada__") {
      ultimaClave = "__nada__";
      pantallaMensaje("En silencio", "no hay nada sonando", 0x8410);
      Serial.println("⏸  Nada sonando");
    }
    https.end(); return;
  }
  if (cod == 401) { https.end(); renovarToken(); return; }
  if (cod == 429) {
    int esperar = https.header("Retry-After").toInt();
    Serial.printf("[API] ⚠️ límite, esperando %d s\n", esperar);
    https.end(); delay((esperar + 1) * 1000UL); return;
  }
  if (cod != 200) { Serial.printf("[API] ❌ %d\n", cod); https.end(); return; }

  // Filtro: la respuesta de Spotify trae >10 KB (imágenes, mercados,
  // enlaces...). Esto descarta todo lo que no pedimos MIENTRAS se lee,
  // así que nunca llega a ocupar memoria.
  JsonDocument filtro;
  filtro["is_playing"] = true;
  filtro["progress_ms"] = true;
  filtro["item"]["name"] = true;
  filtro["item"]["duration_ms"] = true;
  filtro["item"]["album"]["name"] = true;
  filtro["item"]["artists"][0]["name"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, https.getStream(),
                                             DeserializationOption::Filter(filtro));
  https.end();
  if (err) { Serial.printf("[API] ❌ JSON: %s\n", err.c_str()); return; }

  const char* titulo  = doc["item"]["name"] | "(sin título)";
  const char* album   = doc["item"]["album"]["name"] | "";
  const char* artista = doc["item"]["artists"][0]["name"] | "";
  long progreso = doc["progress_ms"] | 0;
  long duracion = doc["item"]["duration_ms"] | 0;
  bool sonando  = doc["is_playing"] | false;

  String clave = String(titulo) + "|" + String(artista);
  if (clave != ultimaClave) {
    // Solo redibujamos el texto cuando cambia la canción. Redibujar
    // todo cada 5 s haría parpadear la pantalla sin ninguna necesidad.
    ultimaClave = clave;
    estadoAnterior = "";
    tft.fillScreen(TFT_BLACK);
    dibujarCancion(titulo, artista, album);
    Serial.printf("\n🎵 %s — %s [%s]\n", titulo, artista, album);
  }

  barraEstado(sonando ? "SONANDO" : "EN PAUSA", sonando ? 0x1DE9 : 0x8410);
  dibujarProgreso(progreso, duracion, sonando);
}

// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  // Backlight con brillo regulado por PWM (RNF-02)
  ledcSetup(CANAL_PWM, 5000, 8);
  ledcAttachPin(TFT_BL, CANAL_PWM);
  ledcWrite(CANAL_PWM, BRILLO);

  tft.init();
  tft.setRotation(0);
  pantallaMensaje("Tamagotchi", "conectando al WiFi...", TFT_WHITE);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(300);
  Serial.printf("WiFi ✅ %s\n", WiFi.localIP().toString().c_str());

  // Sin hora correcta no se pueden validar certificados: el ESP32
  // arranca en 1970 y todos le parecerían "aún no válidos".
  pantallaMensaje("Tamagotchi", "sincronizando la hora...", TFT_WHITE);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t t = 0;
  while (t < 1700000000) { delay(300); time(&t); }

  pantallaMensaje("Tamagotchi", "hablando con Spotify...", TFT_WHITE);
  renovarToken();
  ultimaClave = "";
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (ultimaClave != "__sinred__") {
      ultimaClave = "__sinred__";
      pantallaMensaje("Sin conexion", "reintentando...", TFT_RED);
    }
    WiFi.reconnect(); delay(5000); return;
  }
  if (accessToken == "" || millis() > tokenCaducaEn) {
    if (!renovarToken()) { delay(30000); return; }
  }
  consultar();
  delay(CADA_CUANTO_MS);
}
