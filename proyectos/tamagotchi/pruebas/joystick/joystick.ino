/*
 * ============================================================
 *  Prueba mínima del joystick (C-005) — NADA más
 * ============================================================
 *  No enciende el WiFi ni la pantalla a propósito. Si algo falla
 *  aquí, el sospechoso es uno solo: la palanca y sus cables.
 *
 *  Responde tres preguntas:
 *    1. ¿Están los tres cables de señal donde creemos?
 *    2. ¿Cuánto se desvía TU joystick del centro en reposo?
 *    3. ¿Cuánto ruido tiene, para saber qué zona muerta hace falta?
 * ============================================================
 */
#include <Arduino.h>   // implícito en un .ino, obligatorio al compilarlo como .cpp

// ⚠️ ADC1 obligatorio: con el WiFi encendido el ADC2 no se puede leer.
//    Aquí no hay WiFi, pero se usan los mismos pines que el firmware
//    final para que esta prueba valga de verdad.
const int PIN_VRX = 34;   // eje horizontal (ADC1, solo entrada)
const int PIN_VRY = 35;   // eje vertical   (ADC1, solo entrada)
const int PIN_SW  = 33;   // pulsador (necesita pull-up: 34-39 no lo tienen)

int reposoX = 0, reposoY = 0;
int minX = 4095, maxX = 0, minY = 4095, maxY = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Prueba de joystick (C-005) ===");

  // Con pull-up, el pulsador en reposo lee 1 y pulsado lee 0.
  pinMode(PIN_SW, INPUT_PULLUP);

  /* ------------------------------------------------------------
   *  Medir el reposo, no suponerlo
   * ------------------------------------------------------------
   *  Estos módulos son baratos y ninguno descansa en el centro
   *  exacto. Dar por hecho 2048 haría que el aparato creyera que
   *  mueves la palanca cuando no la estás tocando.
   *
   *  Se promedian 64 lecturas para que el ruido se cancele solo.
   * ------------------------------------------------------------ */
  Serial.println("Calibrando... NO toques la palanca.");
  delay(1500);
  long sx = 0, sy = 0;
  for (int i = 0; i < 64; i++) { sx += analogRead(PIN_VRX); sy += analogRead(PIN_VRY); delay(5); }
  reposoX = sx / 64; reposoY = sy / 64;

  Serial.printf("Reposo medido:  X=%d  Y=%d   (el centro teorico seria 2048)\n",
                reposoX, reposoY);
  Serial.printf("Desviacion:     X=%+d  Y=%+d\n", reposoX - 2048, reposoY - 2048);
  Serial.println("\nAhora muevela a los 4 extremos y pulsala.\n");
}

void loop() {
  int x = analogRead(PIN_VRX);
  int y = analogRead(PIN_VRY);
  bool pulsado = (digitalRead(PIN_SW) == LOW);   // LOW = pulsado, por el pull-up

  // Guardar los extremos vistos: así se sabe el recorrido real.
  if (x < minX) minX = x;  if (x > maxX) maxX = x;
  if (y < minY) minY = y;  if (y > maxY) maxY = y;

  // Cuánto se ha apartado del reposo, en cada eje.
  int dx = x - reposoX, dy = y - reposoY;

  const char* dir = "centro";
  if (abs(dx) > abs(dy)) { if (dx > 400) dir = "DERECHA"; else if (dx < -400) dir = "IZQUIERDA"; }
  else                   { if (dy > 400) dir = "ARRIBA";  else if (dy < -400) dir = "ABAJO"; }

  Serial.printf("X=%4d (%+5d)  Y=%4d (%+5d)  SW=%d  -> %-9s | recorrido X[%d..%d] Y[%d..%d]\n",
                x, dx, y, dy, pulsado ? 0 : 1, dir, minX, maxX, minY, maxY);
  delay(250);
}
