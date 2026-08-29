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
#include "fuentes.h"
#include "letras.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

TFT_eSPI tft = TFT_eSPI();

Letras letras;      // la letra que se está mostrando
Letras precarga;    // la de la SIGUIENTE canción, ya descargada
String clavePrecarga;

const char* URL_TOKEN   = "https://accounts.spotify.com/api/token";
const char* URL_CANCION = "https://api.spotify.com/v1/me/player/currently-playing";
const char* URL_COLA    = "https://api.spotify.com/v1/me/player/queue";

// Cada cuánto se le pregunta a Spotify. Más seguido = detecta antes las
// pausas y los cambios, pero se acerca al límite de peticiones (que se
// mide en una ventana móvil de 30 s). 2 s = 15 peticiones por ventana.
unsigned long cadaCuantoMs = 2000;            // se ajusta solo si hay 429
const unsigned long CERCA_DEL_FIN_MS = 1000;  // al final de la canción
const unsigned long UMBRAL_FIN_MS   = 15000;  // "final" = últimos 15 s
const unsigned long REFRESCO_MS     = 100;    // repintar la letra

// Ajuste fino del sincronismo de la letra. Positivo = la letra va por
// delante. Si la ves atrasada, súbelo; si adelantada, bájalo.
const long AJUSTE_LETRA_MS = 400;

// Alturas de línea de las fuentes generadas (ver tools/gen-fuente.py)
const int ALTO_GRANDE = 23, ALTO_CHICA = 15;

inline void usarFuente(bool grande) {
  tft.setFreeFont(grande ? &FuenteGrande : &FuenteChica);
}

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
int partirEnLineas(const String& texto, bool grande, int anchoMax,
                   String salida[], int maxLineas, bool* cupoTodo = nullptr) {
  usarFuente(grande);
  int n = 0;
  String resto = texto;
  resto.trim();

  while (resto.length() > 0 && n < maxLineas) {
    if (tft.textWidth(resto) <= anchoMax) {
      salida[n++] = resto;
      resto = "";          // 🐛 ESTO FALTABA.
      break;               // Sin limpiar `resto`, la comprobación de más
    }                      // abajo creía que había sobrado texto y añadía
                           // "..." a frases que SÍ cabían completas.
    int corte = resto.length();
    while (corte > 0 && tft.textWidth(resto.substring(0, corte)) > anchoMax) corte--;
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
                     int maxLineas, bool& grandeUsada) {
  bool cupo = false;
  grandeUsada = true;
  int n = partirEnLineas(texto, true, anchoMax, salida, maxLineas, &cupo);
  if (cupo) return n;

  grandeUsada = false;                   // segunda oportunidad, más chica
  n = partirEnLineas(texto, false, anchoMax, salida, maxLineas + 1, &cupo);
  if (cupo) return n;

  // Ya no hay más remedio: recortar la última línea e indicarlo.
  usarFuente(false);
  String u = salida[n-1];
  while (u.length() > 1 && tft.textWidth(u + "...") > anchoMax)
    u = u.substring(0, u.length() - 1);
  salida[n-1] = u + "...";
  return n;
}

// Dibuja centrado horizontalmente con la fuente indicada.
inline void escribir(const String& t, int y, bool grande, uint16_t color) {
  usarFuente(grande);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString(t, 120, y);
}

void barraEstado(const String& txt, uint16_t color) {
  if (estadoPintado == txt) return;
  estadoPintado = txt;
  tft.fillRect(0, 0, 240, 26, TFT_BLACK);
  usarFuente(false);
  tft.setTextColor(color, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(txt, MARGEN, 5);
  tft.drawFastHLine(0, 27, 240, GRIS_LINEA);
}

void cabecera(const char* titulo, const char* artista) {
  tft.fillRect(0, 30, 240, 40, TFT_BLACK);
  // aLatin1 conserva los acentos: la fuente generada llega al código 255.
  String l[3]; bool g;
  partirConReserva(aLatin1(String(titulo)), ANCHO, l, 1, g);
  escribir(l[0], Y_TITULO, false, TFT_WHITE);
  partirConReserva(aLatin1(String(artista)), ANCHO, l, 1, g);
  escribir(l[0], Y_ARTISTA, false, VERDE_SPOTIFY);
  tft.drawFastHLine(MARGEN, 70, ANCHO, GRIS_LINEA);
}

// ¿Esta línea es un silencio? Lo es si está vacía o si solo contiene el
// marcador de nota musical (que en los .lrc señala intros, solos y
// puentes instrumentales).
bool esSilencio(const char* t) {
  if (!t || !*t) return true;
  for (const char* p = t; *p; p++) if (*p != ' ' && *p != MARCA_NOTA) return false;
  return true;
}

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

// Dibuja una nota musical con primitivas gráficas: cabeza, palo y
// banderita. Ninguna fuente la trae, así que se pinta a mano.
void dibujarNota(int x, int y, uint16_t color) {
  tft.fillCircle(x, y, 4, color);
  tft.drawFastVLine(x + 3, y - 15, 15, color);
  tft.drawFastVLine(x + 4, y - 15, 15, color);
  tft.drawLine(x + 4, y - 15, x + 10, y - 11, color);
  tft.drawLine(x + 4, y - 12, x + 10, y - 8,  color);
}

// Dos notas, centradas horizontalmente respecto a `cx`.
void dibujarNotas(int cx, int y, uint16_t color) {
  dibujarNota(cx - 14, y, color);
  dibujarNota(cx + 4,  y + 4, color);
}

void zonaLetra(int idx) {
  tft.fillRect(0, Y_LETRA, 240, ALTO_LETRA, TFT_BLACK);

  if (letras.estado == LETRA_INSTRUMENTAL) {
    dibujarNotas(120, Y_LETRA + 44, TFT_WHITE);
    escribir("instrumental", Y_LETRA + 58, false, GRIS);
    return;
  }
  if (letras.estado != LETRA_OK) {
    escribir(letras.estado == LETRA_NO_HAY ? "sin letra" : "buscando letra...",
             Y_LETRA + 48, false, GRIS_OSCURO);
    return;
  }

  bool enSilencio = (idx < 0) || esSilencio(letras.lineas[idx].texto);

  // ------------------------------------------------------------------
  //  SILENCIO: intro, solo instrumental o pausa entre estrofas.
  //  Se dibujan las notas EN BLANCO y CENTRADAS (ocupan el sitio de la
  //  frase que se cantaría), y debajo en gris la frase que viene. Así se
  //  puede seguir la canción antes de que empiece a cantarse, y queda
  //  claro que el aparato está vivo.
  // ------------------------------------------------------------------
  if (enSilencio) {
    int sig = siguienteConTexto(idx < 0 ? 0 : idx + 1);

    String s2[3]; bool g2 = false; int ns = 0;
    if (sig >= 0) ns = partirConReserva(String(letras.lineas[sig].texto), ANCHO, s2, 2, g2);

    // Las notas van donde iría la frase actual: en el centro del bloque.
    int altoSig = ns * (g2 ? ALTO_GRANDE : ALTO_CHICA);
    int yNotas  = Y_LETRA + (ALTO_LETRA - altoSig) / 2 - 6;
    dibujarNotas(120, yNotas, TFT_WHITE);

    for (int i = 0; i < ns; i++)
      escribir(s2[i], Y_LETRA + ALTO_LETRA - 6 - altoSig + i * (g2 ? ALTO_GRANDE : ALTO_CHICA),
               g2, GRIS);
    return;
  }

  // ------------------------------------------------------------------
  //  CANTANDO: la frase actual grande y blanca, con contexto en gris.
  // ------------------------------------------------------------------
  String c[4]; bool grande = true;
  int n = partirConReserva(String(letras.lineas[idx].texto), ANCHO, c, 3, grande);

  int alto = grande ? ALTO_GRANDE : ALTO_CHICA;
  int bloque = n * alto;
  int y = Y_LETRA + (ALTO_LETRA - bloque) / 2;

  // El contexto solo se dibuja si sobra sitio: cuando la frase actual es
  // larga, ella manda.
  bool hayHueco = (bloque + 38) <= ALTO_LETRA;

  if (hayHueco) {
    int ant = anteriorConTexto(idx - 1);
    if (ant >= 0) {
      String a[3]; bool ga;
      partirConReserva(String(letras.lineas[ant].texto), ANCHO, a, 1, ga);
      escribir(a[0], Y_LETRA, false, GRIS_OSCURO);
    }
  }

  for (int i = 0; i < n; i++) escribir(c[i], y + i * alto, grande, TFT_WHITE);

  if (hayHueco) {
    int sig = siguienteConTexto(idx + 1);
    if (sig >= 0) {
      String sg[3]; bool gs;
      partirConReserva(String(letras.lineas[sig].texto), ANCHO, sg, 1, gs);
      escribir(sg[0], Y_LETRA + ALTO_LETRA - ALTO_CHICA, false, GRIS);
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
  usarFuente(false);
  tft.setTextColor(GRIS, TFT_BLACK);
  tft.setTextDatum(TL_DATUM); tft.drawString(a, MARGEN, Y_TIEMPOS);
  tft.setTextDatum(TR_DATUM); tft.drawString(b, 240 - MARGEN, Y_TIEMPOS);
}

void pantallaMensaje(const char* l1, const char* l2, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  estadoPintado = ""; lineaPintada = -2;
  escribir(l1, 105, true, color);
  if (l2) escribir(l2, 140, false, GRIS);
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
    // Spotify no publica su límite exacto, así que si se queja subimos
    // el intervalo y ya no volvemos a molestarle tanto. Se autorregula.
    cadaCuantoMs = min(cadaCuantoMs + 1000UL, 8000UL);
    Serial.printf("[API] ⚠️ límite alcanzado: esperando %d s, intervalo -> %lu ms\n",
                  e, cadaCuantoMs);
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
  // El texto ya viene convertido a Latin-1 (un byte por carácter) desde
  // aLatin1(), así que hay que apagar el decodificador UTF-8 de la
  // librería: si no, intentaría interpretar el byte 0xF1 (ñ) como el
  // principio de una secuencia de varios bytes y se perdería.
  tft.setAttribute(UTF8_SWITCH, false);

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

  // Sondeo ADAPTATIVO: cada 2 s normalmente, y cada 1 s en los últimos 15
  // segundos de la canción. Así se detectan rápido las pausas y los
  // cambios manuales, sin abusar del límite de peticiones.
  long falta = duracionMs - progresoAhora();
  unsigned long intervalo = (sonando && duracionMs > 0 && falta < (long)UMBRAL_FIN_MS)
                            ? CERCA_DEL_FIN_MS : cadaCuantoMs;

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
