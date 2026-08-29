/*
 * ============================================================
 *  Tamagotchi Spotify — Prueba de vida
 * ============================================================
 *  Esto NO es el proyecto todavía. Es la prueba de que toda la
 *  cadena funciona: escribir código → compilar → cargar a la
 *  placa → ver lo que imprime.
 *
 *  Hace tres cosas, y cada una verifica algo distinto:
 *    1. Parpadea el LED     → la placa ejecuta código
 *    2. Imprime info del chip → la comunicación va en ambos sentidos
 *    3. Escanea redes WiFi   → la radio funciona (y verifica el
 *                              supuesto S-06: que tu red sea 2.4 GHz)
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>

// El LED integrado de la mayoría de placas ESP32 está en el GPIO2.
// Si tu placa no parpadea, puede que el suyo esté en otro pin —
// no es un error del código.
const int LED = 2;

void escanearRedes() {
  Serial.println();
  Serial.println("=== Escaneando redes WiFi ===");
  Serial.println("(El ESP32 solo ve redes de 2.4 GHz. Si tu red de casa");
  Serial.println(" aparece aquí, tiene banda de 2.4 GHz y podremos usarla.)");
  Serial.println();

  // WiFi.STA = modo "estación": el ESP32 se conecta a una red,
  // en vez de crear una propia.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();

  if (n == 0) {
    Serial.println("  No se encontró ninguna red. Algo raro pasa con la antena.");
    return;
  }

  Serial.printf("  Se encontraron %d redes:\n\n", n);
  Serial.println("   #  Señal   Canal  Seguridad  Nombre de la red");
  Serial.println("  --- ------  -----  ---------  ----------------");

  for (int i = 0; i < n; i++) {
    // RSSI es la potencia de la señal en dBm. Es un número negativo:
    // cuanto MÁS CERCA DE CERO, mejor.
    //   -30 a -55  excelente
    //   -55 a -70  buena
    //   -70 a -80  débil  <- aquí empiezan las desconexiones
    //   peor de -80 casi inutilizable
    int rssi = WiFi.RSSI(i);
    const char* calidad = rssi > -55 ? "excelente"
                        : rssi > -70 ? "buena    "
                        : rssi > -80 ? "débil    "
                                     : "muy débil";

    Serial.printf("  %2d  %4d dBm  %2d     %-9s  %s   [%s]\n",
                  i + 1,
                  rssi,
                  WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "abierta" : "con clave",
                  WiFi.SSID(i).c_str(),
                  calidad);
  }
  Serial.println();
  WiFi.scanDelete();
}

void setup() {
  Serial.begin(115200);
  delay(1000);   // le da tiempo al monitor serie de engancharse

  pinMode(LED, OUTPUT);

  Serial.println();
  Serial.println("############################################");
  Serial.println("#  Tamagotchi Spotify — Prueba de vida     #");
  Serial.println("############################################");
  Serial.println();

  Serial.println("=== Quién soy ===");
  Serial.printf("  Modelo de chip : %s\n",   ESP.getChipModel());
  Serial.printf("  Revisión       : %d\n",   ESP.getChipRevision());
  Serial.printf("  Núcleos        : %d\n",   ESP.getChipCores());
  Serial.printf("  Velocidad CPU  : %lu MHz\n", (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("  Memoria flash  : %lu MB\n", (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.printf("  Dirección MAC  : %s\n",   WiFi.macAddress().c_str());
  Serial.println();

  // La memoria libre es EL número a vigilar en este proyecto.
  // Cuando lleguemos a HTTPS + JSON (supuesto S-04), este dato
  // decide si el ESP32 puede hablar con Spotify por sí solo o
  // si necesitamos un puente en la Mac (ADR-0002).
  Serial.println("=== Memoria ===");
  Serial.printf("  RAM libre ahora: %lu bytes (%.1f KB)\n",
                (unsigned long)ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0);
  Serial.println("  ^ Este número es el que hay que vigilar cuando");
  Serial.println("    empecemos con HTTPS. Ver supuesto S-04.");

  escanearRedes();

  Serial.println("=== Todo listo ===");
  Serial.println("Si ves esto y el LED parpadea, la cadena completa funciona:");
  Serial.println("  editar → compilar → cargar → ejecutar → leer.");
  Serial.println();
}

void loop() {
  // Un parpadeo lento: prendido 100 ms, apagado 900 ms.
  // Es a propósito: un parpadeo raro y reconocible confirma que
  // estás viendo TU código y no algo que ya venía en la placa.
  digitalWrite(LED, HIGH);
  delay(100);
  digitalWrite(LED, LOW);
  delay(900);
}
