/*
 * ============================================================
 *  Tamagotchi Spotify — Paso 7a: prueba de la pantalla
 * ============================================================
 *  Prueba POR ETAPAS, a propósito.
 *
 *  Si conectas 7 cables y no funciona, tienes 7 sospechosos.
 *  Esta prueba enciende las cosas de una en una y te dice en
 *  la terminal qué deberías estar viendo en cada momento.
 *  Cuando algo no cuadre, sabrás exactamente qué cable revisar.
 * ============================================================
 */

#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI pantalla = TFT_eSPI();

void esperar(const char* que, int ms) {
  Serial.printf("   👀 Deberías ver: %s\n", que);
  delay(ms);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n############################################");
  Serial.println("#  Paso 7a — Prueba de pantalla por etapas #");
  Serial.println("############################################\n");

  // ---------- ETAPA 1: la luz de atrás ----------
  // Prueba los cables VCC, GND y BLK. Nada más.
  // Si esto no funciona, no tiene sentido seguir: es alimentación.
  Serial.println("ETAPA 1 · Backlight (prueba VCC, GND y BLK)");
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW);
  esperar("la pantalla APAGADA, negra", 1500);
  digitalWrite(TFT_BL, HIGH);
  esperar("la pantalla ENCENDIDA: gris o blancuzca, aunque sin dibujo", 2500);
  Serial.println("   Si sigue apagada -> revisa VCC(3V3), GND y BLK(P32)\n");

  // ---------- ETAPA 2: arrancar el controlador ----------
  // Aquí entran en juego SCK, SDA, DC y RES.
  Serial.println("ETAPA 2 · Iniciando el controlador ST7789...");
  pantalla.init();
  pantalla.setRotation(0);
  Serial.println("   init() terminó (esto siempre pasa, aunque los cables estén mal)\n");

  // ---------- ETAPA 3: colores ----------
  // Prueba real de SCK, SDA, DC y RES: si no llegan bien, la
  // pantalla se queda blanca o muestra ruido.
  Serial.println("ETAPA 3 · Colores puros (prueba SCK, SDA, DC y RES)");
  struct { uint16_t c; const char* nombre; } colores[] = {
    {TFT_RED,   "ROJO"}, {TFT_GREEN, "VERDE"}, {TFT_BLUE, "AZUL"},
  };
  for (auto& x : colores) {
    pantalla.fillScreen(x.c);
    Serial.printf("   👀 Deberías ver: pantalla toda %s\n", x.nombre);
    delay(1800);
  }
  Serial.println();
  Serial.println("   ⚠️ IMPORTANTE — mira bien qué colores viste:");
  Serial.println("      · ROJO, VERDE, AZUL   -> ✅ perfecto");
  Serial.println("      · CIAN, MAGENTA, AMARILLO -> los colores están invertidos");
  Serial.println("        (se arregla con una línea, dímelo y ya)");
  Serial.println("      · blanco o ruido      -> revisa SCK(P18), SDA(P23),");
  Serial.println("        DC(P16) y RES(P4)\n");

  // ---------- ETAPA 4: geometría ----------
  // Comprueba que la pantalla está bien alineada. Si el panel
  // tuviera un desplazamiento mal configurado, se vería un borde
  // cortado o una franja de basura en un lado.
  Serial.println("ETAPA 4 · Geometría y bordes");
  pantalla.fillScreen(TFT_BLACK);
  pantalla.drawRect(0, 0, 240, 240, TFT_WHITE);
  pantalla.drawRect(1, 1, 238, 238, TFT_WHITE);
  pantalla.drawLine(0, 0, 239, 239, TFT_RED);
  pantalla.drawLine(239, 0, 0, 239, TFT_RED);
  pantalla.fillCircle(120, 120, 30, TFT_GREEN);
  esperar("un marco blanco COMPLETO en los 4 bordes, una X roja\n"
          "                    y un círculo verde centrado", 3500);
  Serial.println("   Si falta un borde o hay una franja rara -> el desplazamiento");
  Serial.println("   del panel (CGRAM_OFFSET) necesita ajuste. Dímelo.\n");

  // ---------- ETAPA 5: texto ----------
  Serial.println("ETAPA 5 · Texto y tamaños de fuente");
  pantalla.fillScreen(TFT_BLACK);
  pantalla.setTextColor(TFT_GREEN, TFT_BLACK);
  pantalla.setTextDatum(TC_DATUM);           // centrado arriba
  pantalla.drawString("FUNCIONA", 120, 10, 4);
  pantalla.setTextColor(TFT_WHITE, TFT_BLACK);
  pantalla.drawString("Tamagotchi Spotify", 120, 50, 2);

  // Tres tamaños, para decidir cuál se lee cómodo a 60 cm (RNF-01).
  pantalla.setTextColor(TFT_CYAN, TFT_BLACK);
  pantalla.drawString("Fuente 2 - chica", 120, 90, 2);
  pantalla.setTextColor(TFT_YELLOW, TFT_BLACK);
  pantalla.drawString("Fuente 4 - media", 120, 120, 4);
  pantalla.setTextColor(TFT_ORANGE, TFT_BLACK);
  pantalla.drawString("Fuente 6", 120, 160, 6);

  Serial.println("   👀 Texto en 3 tamaños distintos.");
  Serial.println("   📏 ALÉJATE ~60 cm y dime CUÁL SE LEE CÓMODO.");
  Serial.println("      Eso decide el diseño de la pantalla (RNF-01).\n");

  Serial.println("=== Prueba terminada ===");
  Serial.printf("RAM libre: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
}

void loop() {
  delay(1000);
}
