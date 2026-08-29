/*
 * ============================================================
 *  Tamagotchi Spotify — Pasos 5 y 6
 * ============================================================
 *  PASO 5: el ESP32 renueva su propio access_token usando el
 *          refresh_token, sin ayuda de nadie.
 *  PASO 6: pregunta qué canción suena y la interpreta.
 *
 *  Todo con el certificado del servidor VERIFICADO (ADR-0009).
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include "secrets.h"

// El paquete de certificados raíz queda incrustado en la memoria flash.
// Este nombre rarísimo lo genera el compilador a partir de la ruta del
// archivo (data/cert/x509_crt_bundle.bin -> _binary_data_cert_..._bin_start).
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

const char* URL_TOKEN  = "https://accounts.spotify.com/api/token";
const char* URL_CANCION = "https://api.spotify.com/v1/me/player/currently-playing";

const unsigned long CADA_CUANTO_MS = 5000;   // consultar cada 5 s (ver investigación)

String accessToken;
unsigned long tokenCaducaEn = 0;   // millis() en que hay que renovarlo
String ultimaCancion;              // para detectar cambios y no repetir en pantalla

// ---------------------------------------------------------------
//  Prepara un cliente TLS que SÍ verifica al servidor
// ---------------------------------------------------------------
void prepararTLS(WiFiClientSecure& c) {
  // Esta línea es todo el ADR-0009. Sin ella habría que usar
  // setInsecure(), que acepta a cualquiera que diga ser Spotify —
  // y por esta conexión viajan nuestros tokens.
  c.setCACertBundle(rootca_crt_bundle_start);
}

// ---------------------------------------------------------------
//  PASO 5 · Renovar el access_token
// ---------------------------------------------------------------
bool renovarToken() {
  Serial.println("\n[Token] Renovando access_token...");

  // Spotify quiere las credenciales como "Basic <id:secret en base64>".
  // base64 NO es cifrado, solo una forma de escribir bytes con letras.
  // Por eso todo esto va obligatoriamente por HTTPS.
  String credenciales = String(SPOTIFY_CLIENT_ID) + ":" + String(SPOTIFY_CLIENT_SECRET);
  unsigned char b64[256];
  size_t b64Len = 0;
  mbedtls_base64_encode(b64, sizeof(b64), &b64Len,
                        (const unsigned char*)credenciales.c_str(), credenciales.length());
  String basic = String((char*)b64).substring(0, b64Len);

  WiFiClientSecure cliente;
  prepararTLS(cliente);

  HTTPClient https;
  if (!https.begin(cliente, URL_TOKEN)) {
    Serial.println("[Token] ❌ No se pudo iniciar la conexión");
    return false;
  }

  https.addHeader("Authorization", "Basic " + basic);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String cuerpo = "grant_type=refresh_token&refresh_token=" + String(SPOTIFY_REFRESH_TOKEN);
  int codigo = https.POST(cuerpo);

  if (codigo != 200) {
    Serial.printf("[Token] ❌ Spotify respondió %d\n", codigo);
    if (codigo > 0) Serial.println("  " + https.getString());
    if (codigo < 0) Serial.println("  (código negativo = falló la conexión, no la petición)");
    https.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, https.getStream());
  https.end();

  if (err) {
    Serial.printf("[Token] ❌ JSON ilegible: %s\n", err.c_str());
    return false;
  }

  accessToken = doc["access_token"].as<String>();
  long duracion = doc["expires_in"] | 3600;

  // Se renueva 5 minutos ANTES de que caduque. Apurar hasta el último
  // segundo garantizaría fallos justo en el límite.
  tokenCaducaEn = millis() + (duracion - 300) * 1000UL;

  Serial.printf("[Token] ✅ Renovado. Válido %ld s (se renovará en %ld s)\n",
                duracion, duracion - 300);
  return true;
}

// ---------------------------------------------------------------
//  PASO 6 · Preguntar qué suena
// ---------------------------------------------------------------
void consultarCancion() {
  WiFiClientSecure cliente;
  prepararTLS(cliente);

  HTTPClient https;
  if (!https.begin(cliente, URL_CANCION)) return;
  https.addHeader("Authorization", "Bearer " + accessToken);

  int codigo = https.GET();

  // 204 = "no hay nada sonando". No es un error: es una respuesta
  // legítima que hay que distinguir de "se cayó internet" (RF-06 vs RF-07).
  if (codigo == 204) {
    if (ultimaCancion != "") {
      Serial.println("\n⏸  No hay nada sonando");
      ultimaCancion = "";
    }
    https.end();
    return;
  }

  if (codigo == 401) {
    Serial.println("[API] Token caducado, renovando...");
    https.end();
    renovarToken();
    return;
  }

  if (codigo == 429) {
    // Nos pasamos del límite de peticiones. Spotify dice cuánto esperar.
    String espera = https.header("Retry-After");
    Serial.printf("[API] ⚠️ Límite alcanzado. Esperar %s s\n", espera.c_str());
    https.end();
    delay((espera.toInt() + 1) * 1000UL);
    return;
  }

  if (codigo != 200) {
    Serial.printf("[API] ❌ Respuesta %d\n", codigo);
    https.end();
    return;
  }

  // ---- Filtro: la clave para no quedarnos sin memoria ----
  // La respuesta de Spotify trae MUCHÍSIMO más de lo que necesitamos:
  // imágenes, mercados disponibles, enlaces, popularidad... Fácilmente
  // 10 KB o más.
  //
  // Este filtro le dice a ArduinoJson qué campos conservar. Todo lo
  // demás se descarta MIENTRAS se lee, sin llegar a ocupar memoria.
  // Es la diferencia entre gastar ~10 KB y gastar unos cientos de bytes.
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

  if (err) {
    Serial.printf("[API] ❌ JSON ilegible: %s\n", err.c_str());
    return;
  }

  const char* titulo = doc["item"]["name"]  | "(desconocido)";
  const char* album  = doc["item"]["album"]["name"] | "(desconocido)";
  const char* artista = doc["item"]["artists"][0]["name"] | "(desconocido)";
  long progreso = doc["progress_ms"] | 0;
  long duracion = doc["item"]["duration_ms"] | 0;
  bool sonando = doc["is_playing"] | false;

  String cancion = String(titulo) + "|" + String(artista);
  bool esNueva = (cancion != ultimaCancion);
  ultimaCancion = cancion;

  if (esNueva) {
    Serial.println();
    Serial.println("┌──────────────────────────────────────────────");
    Serial.printf("│ 🎵  %s\n", titulo);
    Serial.printf("│ 👤  %s\n", artista);
    Serial.printf("│ 💿  %s\n", album);
    Serial.println("└──────────────────────────────────────────────");
  }

  // Barra de progreso, para ver que progress_ms avanza de verdad —
  // que es el dato que después sincronizará las letras (RF-04).
  if (duracion > 0) {
    int llenos = (progreso * 20) / duracion;
    Serial.printf("\r  %s [", sonando ? "▶" : "⏸");
    for (int i = 0; i < 20; i++) Serial.print(i < llenos ? "=" : " ");
    Serial.printf("] %ld:%02ld / %ld:%02ld   RAM: %lu B  ",
                  progreso / 60000, (progreso / 1000) % 60,
                  duracion / 60000, (duracion / 1000) % 60,
                  (unsigned long)ESP.getFreeHeap());
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);   // LED apagado: sin parpadeo

  Serial.println("\n############################################");
  Serial.println("#  Tamagotchi Spotify — Pasos 5 y 6        #");
  Serial.println("############################################\n");

  Serial.printf("Conectando a %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf(" ✅ %s\n", WiFi.localIP().toString().c_str());

  // ⚠️ IMPRESCINDIBLE antes de verificar certificados.
  //
  // Un certificado dice "soy válido entre tal fecha y tal otra". Para
  // comprobarlo hay que saber qué día es hoy — y el ESP32 arranca creyendo
  // que estamos en 1970. Sin esto, TODOS los certificados parecerían
  // "todavía no válidos" y la conexión fallaría sin explicar por qué.
  Serial.print("Sincronizando la hora por NTP ");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t ahora = 0;
  while (ahora < 1700000000) { delay(500); Serial.print("."); time(&ahora); }
  Serial.printf(" ✅ %s", ctime(&ahora));

  Serial.println("Certificados: paquete de Mozilla incrustado (150 CAs)");

  renovarToken();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WiFi] Reconectando...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  if (accessToken == "" || millis() > tokenCaducaEn) {
    if (!renovarToken()) { delay(30000); return; }
  }

  consultarCancion();
  delay(CADA_CUANTO_MS);
}
