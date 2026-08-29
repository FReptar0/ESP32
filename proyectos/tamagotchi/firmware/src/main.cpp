/*
 * ============================================================
 *  Tamagotchi Spotify — Pasos 2 y 3
 * ============================================================
 *  PASO 2: conectarse a la red WiFi.
 *  PASO 3: hacer una conexión HTTPS real y MEDIR LA MEMORIA.
 *
 *  El paso 3 es el punto de control de todo el proyecto
 *  (ADR-0002, supuesto S-04). La pregunta que responde es:
 *
 *      ¿Le queda al ESP32 memoria suficiente para hablar
 *       con Spotify por su cuenta, o necesitamos un puente
 *       corriendo en la Mac?
 *
 *  Si la respuesta es "no alcanza", hay que reabrir el ADR-0002
 *  ANTES de escribir una sola línea de lógica de Spotify.
 *  Por eso se mide primero.
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

const int LED = 2;

// Nos conectamos al servidor REAL de Spotify (el de los tokens),
// no a un sitio cualquiera. Medir contra el destino verdadero
// da un número que sí sirve.
const char* HOST = "accounts.spotify.com";
const int   PORT = 443;

// -------- Utilidad para imprimir la memoria en cada etapa --------
void memoria(const char* etapa) {
  Serial.printf("  %-28s libre: %6lu B (%5.1f KB) | bloque mayor: %6lu B\n",
                etapa,
                (unsigned long)ESP.getFreeHeap(),
                ESP.getFreeHeap() / 1024.0,
                (unsigned long)ESP.getMaxAllocHeap());
}

// ================= PASO 2 =================
bool conectarWiFi() {
  Serial.println("=== PASO 2 · Conectando a WiFi ===");
  Serial.printf("  Red: %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // 20 segundos de paciencia. Sin timeout, un error de contraseña
  // dejaría la placa colgada para siempre sin decir por qué.
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("  ❌ No se pudo conectar.");
    Serial.println("     Causas típicas, en orden:");
    Serial.println("       1. La contraseña en secrets.h está mal");
    Serial.println("       2. La red es de 5 GHz (el ESP32 solo ve 2.4 GHz)");
    Serial.println("       3. El nombre de la red tiene un typo");
    return false;
  }

  Serial.println("  ✅ Conectado");
  Serial.printf("     IP asignada : %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("     Puerta enlace: %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("     Señal        : %d dBm\n", WiFi.RSSI());
  return true;
}

// ================= PASO 3 =================
void probarHTTPS() {
  Serial.println();
  Serial.println("=== PASO 3 · Prueba de HTTPS y medición de memoria ===");
  Serial.println("  (Esto responde el supuesto S-04, riesgo #1 del proyecto)");
  Serial.println();

  memoria("Antes de nada");

  // El objeto seguro se crea en el heap: aquí empieza el gasto.
  WiFiClientSecure* cliente = new WiFiClientSecure;

  // ⚠️ setInsecure() = NO verificar el certificado del servidor.
  //    Para ESTA medición está bien: el handshake TLS ocurre igual
  //    y es lo que consume la memoria. Pero para el firmware final
  //    hay que validar el certificado — eso es el futuro ADR-0009.
  cliente->setInsecure();
  memoria("Objeto TLS creado");

  Serial.printf("\n  Conectando a %s:%d ...\n", HOST, PORT);
  unsigned long t0 = millis();

  if (!cliente->connect(HOST, PORT)) {
    Serial.println("  ❌ Falló la conexión TLS.");
    memoria("Tras el fallo");
    delete cliente;
    return;
  }

  unsigned long ms = millis() - t0;
  Serial.printf("  ✅ Handshake TLS completado en %lu ms\n\n", ms);
  memoria("TLS conectado  <-- CLAVE");

  // Petición HTTP mínima, solo para confirmar que fluyen datos.
  cliente->printf("GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", HOST);

  // Leemos solo la primera línea: nos basta para saber que respondió.
  String primeraLinea = cliente->readStringUntil('\n');
  Serial.printf("\n  Respuesta del servidor: %s\n", primeraLinea.c_str());

  size_t bytes = 0;
  while (cliente->connected() || cliente->available()) {
    if (cliente->available()) { cliente->read(); bytes++; }
    else break;
  }
  Serial.printf("  Bytes recibidos: %u\n\n", (unsigned)bytes);
  memoria("Con la respuesta leída");

  cliente->stop();
  delete cliente;
  delay(100);
  memoria("Tras cerrar y liberar");

  // ---- El veredicto ----
  Serial.println();
  Serial.println("  ---------------- VEREDICTO ----------------");
  Serial.printf("  Mínimo histórico de RAM libre: %lu B (%.1f KB)\n",
                (unsigned long)ESP.getMinFreeHeap(), ESP.getMinFreeHeap() / 1024.0);
  Serial.println("  ^ Este es EL número: el peor momento por el que pasó");
  Serial.println("    la placa, aunque durara un instante.");
  Serial.println();
  Serial.println("  Regla de lectura:");
  Serial.println("    > 100 KB  -> sobra. Adelante con el plan directo.");
  Serial.println("    50-100 KB -> alcanza, pero hay que cuidar el JSON.");
  Serial.println("    < 50 KB   -> apretado. Reabrir el ADR-0002.");
  Serial.println("  -------------------------------------------");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(LED, OUTPUT);

  Serial.println();
  Serial.println("############################################");
  Serial.println("#  Tamagotchi Spotify — Pasos 2 y 3        #");
  Serial.println("############################################");
  Serial.println();

  if (conectarWiFi()) {
    probarHTTPS();
  } else {
    Serial.println("\n  Sin WiFi no se puede probar HTTPS. Revisa secrets.h.");
  }

  Serial.println("\n=== Fin de la prueba ===");
  Serial.println("La placa se queda en reposo, sin parpadear.");
}

void loop() {
  // Sin parpadeo: ya cumplió su función.
  //
  // El parpadeo existía para el paso 1, como prueba de que la placa
  // ejecutaba NUESTRO código y no algo que ya venía de fábrica. Eso ya
  // quedó demostrado, y a partir de aquí solo molesta: este aparato va a
  // vivir en un escritorio.
  //
  // Ojo con el detalle: el LED está en el GPIO2, que es uno de los pines
  // que el ESP32 lee al arrancar para decidir cómo bootear (strapping pin).
  // Por eso la tabla de conexiones de la pantalla no usa el GPIO2 —
  // ver 6-esquemas/conexiones-fisicas.md.
  digitalWrite(LED, LOW);
  delay(1000);
}
