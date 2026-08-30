/*
 * ============================================================
 *  Joystick analógico de 2 ejes (C-005)
 * ============================================================
 *  Convierte una palanca ruidosa y torcida en GESTOS limpios.
 *
 *  Todo lo de aquí sale de medir esta unidad concreta el
 *  2026-08-29 (ver 3-componentes/joystick-analogico-2ejes.md):
 *  reposo en 1737/1760 —no en 2048—, ruido de ±4 cuentas, y un
 *  recorrido asimétrico de 2358 cuentas hacia un lado contra
 *  1737 hacia el otro.
 * ============================================================
 */
#pragma once
#include <Arduino.h>

// ⚠️ ADC1 obligatorio. El driver de WiFi se apropia del ADC2, y este
//    aparato tiene la radio encendida siempre. Con un pin del ADC2 esto
//    funcionaría en una prueba suelta y fallaría al añadir la red.
//
/* ⚠️ OJO CON LOS ROTULOS DEL MODULO — medido el 2026-08-29
 * ------------------------------------------------------------
 *  El módulo trae los pines rotulados `VRx` y `VRy`, y es tentador
 *  leerlos como "horizontal" y "vertical". **No lo son.** Se refieren
 *  a los ejes DEL MÓDULO, que dependen de cómo quede orientado al
 *  montarlo.
 *
 *  En este montaje, comprobado gesto a gesto:
 *
 *      empujar ARRIBA   -> se mueve el pin `VRx` (GPIO34), en positivo
 *      empujar DERECHA  -> se mueve el pin `VRy` (GPIO35), en positivo
 *
 *  O sea que están CRUZADOS respecto a lo que parecía. Por eso aquí
 *  los pines se nombran por su FUNCIÓN y no por el rótulo: así el
 *  código dice la verdad aunque el rótulo no.
 *
 *  Los signos sí salieron naturales: arriba y derecha son positivos.
 *
 *  🔑 El rótulo de un componente describe el componente, no tu montaje.
 *     En cuanto lo giras, deja de coincidir — y el código que se fía
 *     del rótulo hereda una mentira.
 *
 *  Si algún día se remonta girado, se vuelve a derivar en un minuto:
 *  empujar en una dirección y ver qué eje se mueve y con qué signo.
 * ------------------------------------------------------------ */
const int PIN_EJE_VERTICAL   = 34;   // el módulo lo rotula `VRx`
const int PIN_EJE_HORIZONTAL = 35;   // el módulo lo rotula `VRy`
const int PIN_SW             = 33;   // pulsador — necesita pull-up

enum Gesto {
  GESTO_NINGUNO,
  GESTO_IZQUIERDA, GESTO_DERECHA,
  GESTO_ARRIBA,    GESTO_ABAJO,
  GESTO_CLIC,      GESTO_CLIC_LARGO
};

inline const char* nombreGesto(Gesto g) {
  switch (g) {
    case GESTO_IZQUIERDA:  return "IZQUIERDA";
    case GESTO_DERECHA:    return "DERECHA";
    case GESTO_ARRIBA:     return "ARRIBA";
    case GESTO_ABAJO:      return "ABAJO";
    case GESTO_CLIC:       return "CLIC";
    case GESTO_CLIC_LARGO: return "CLIC LARGO";
    default:               return "-";
  }
}

class Joystick {
public:
  void  iniciar();
  Gesto leer();                       // llamar a menudo; casi siempre NINGUNO
  float x() const { return ejeX; }    // -1.0 … +1.0, ya compensado
  float y() const { return ejeY; }
  int   reposoX() const { return cerorX; }
  int   reposoY() const { return cerorY; }

private:
  int  cerorX = 2048, cerorY = 2048;
  float ejeX = 0, ejeY = 0;

  bool armado = true;                 // ¿listo para un gesto nuevo?
  bool pulsado = false, largoLanzado = false;
  unsigned long tPulsacion = 0, ultimaLectura = 0, tCambioBoton = 0;
  bool lecturaBotonPrevia = true;

  int  medir(int pin) const;
  float normalizar(int v, int reposo) const;
};

/* ------------------------------------------------------------
 *  Constantes de comportamiento
 * ------------------------------------------------------------ */
// Cada cuánto se mira la palanca. 20 ms son 50 veces por segundo:
// de sobra para la mano, y despreciable para el núcleo de dibujo.
const unsigned long MS_ENTRE_LECTURAS = 20;

/*  Umbral y rearme: por qué son DOS números y no uno
 *  --------------------------------------------------
 *  Con un solo umbral, una palanca sostenida justo en el borde
 *  entraría y saldría sola con el ruido, disparando una ráfaga de
 *  gestos. Es el mismo motivo por el que un termostato no enciende
 *  y apaga la calefacción en el grado exacto: espera a bajar un
 *  poco más antes de volver a arrancar.
 *
 *  Así que hay dos líneas: una para DISPARAR (0,55) y otra, más
 *  abajo, para volver a ARMARSE (0,25). Entre ambas no pasa nada.
 *  Se llama histéresis.                                            */
const float UMBRAL_DISPARO = 0.55f;   // hay que empujar de verdad
const float UMBRAL_REARME  = 0.25f;   // y volver casi al centro

const unsigned long MS_ANTIRREBOTE = 30;    // el contacto del botón "tiembla"
const unsigned long MS_CLIC_LARGO  = 800;

/* ============================================================
 *  Implementación
 * ============================================================ */

// Promedio de 4 lecturas: el ruido del conversor se cancela solo y
// cuesta unos 400 microsegundos.
inline int Joystick::medir(int pin) const {
  int s = 0;
  for (int i = 0; i < 4; i++) s += analogRead(pin);
  return s / 4;
}

/* ------------------------------------------------------------
 *  Normalizar: donde se corrige que la palanca esté torcida
 * ------------------------------------------------------------
 *  Esta unidad reposa en 1737, no en 2048. Eso deja 2358 cuentas
 *  de recorrido hacia arriba y solo 1737 hacia abajo.
 *
 *  Con un umbral en cuentas crudas, un lado pediría empujar el 25%
 *  del recorrido y el otro el 35%. Se siente como si la palanca
 *  "fuera dura" hacia un lado, y no se ve por qué.
 *
 *  Aquí cada dirección se divide entre SU PROPIO recorrido, así que
 *  el resultado va de -1,0 a +1,0 y "medio empujado" significa lo
 *  mismo en las cuatro direcciones.
 *
 *  🔑 Cuando el hardware es asimétrico, la simetría se pone en el
 *     software. La mano percibe algo parejo aunque el componente
 *     no lo sea.
 * ------------------------------------------------------------ */
inline float Joystick::normalizar(int v, int reposo) const {
  if (v >= reposo) {
    int recorrido = 4095 - reposo;
    return recorrido > 0 ? (float)(v - reposo) / recorrido : 0.0f;
  }
  return reposo > 0 ? (float)(v - reposo) / reposo : 0.0f;   // sale negativo
}

inline void Joystick::iniciar() {
  // Con pull-up interno: en reposo lee 1, pulsado lee 0. Va al revés
  // de lo que dice la intuición, y es lo normal en un botón a tierra.
  pinMode(PIN_SW, INPUT_PULLUP);

  /* Medir el reposo, NUNCA suponerlo.
   *
   * Esta unidad descansa 300 cuentas por debajo del centro teórico.
   * Dar por hecho 2048 haría que el aparato creyera que la palanca
   * está permanentemente empujada, y saltaría canciones solo.
   *
   * Y tampoco vale clavar el 1737 medido: otra unidad daría otro
   * número, y ésta puede derivar con la temperatura. Se mide cada
   * vez que arranca, que cuesta un tercio de segundo. */
  long sx = 0, sy = 0;
  for (int i = 0; i < 64; i++) { sx += analogRead(PIN_EJE_HORIZONTAL); sy += analogRead(PIN_EJE_VERTICAL); delay(5); }
  cerorX = sx / 64; cerorY = sy / 64;

  Serial.printf("[Joystick] reposo X=%d Y=%d (desviacion %+d / %+d del centro teorico)\n",
                cerorX, cerorY, cerorX - 2048, cerorY - 2048);
}

inline Gesto Joystick::leer() {
  unsigned long ahora = millis();
  if (ahora - ultimaLectura < MS_ENTRE_LECTURAS) return GESTO_NINGUNO;
  ultimaLectura = ahora;

  ejeX = normalizar(medir(PIN_EJE_HORIZONTAL), cerorX);
  ejeY = normalizar(medir(PIN_EJE_VERTICAL), cerorY);

  // ---- El pulsador ----
  // El contacto metálico rebota unos milisegundos al cerrarse, y sin
  // filtrarlo una sola pulsación se leería como varias. Se exige que
  // el nivel se mantenga estable MS_ANTIRREBOTE antes de creérselo.
  bool nivel = (digitalRead(PIN_SW) == LOW);
  if (nivel != lecturaBotonPrevia) { tCambioBoton = ahora; lecturaBotonPrevia = nivel; }

  if (ahora - tCambioBoton > MS_ANTIRREBOTE) {
    if (nivel && !pulsado) {                       // acaba de pulsarse
      pulsado = true; largoLanzado = false; tPulsacion = ahora;
    } else if (!nivel && pulsado) {                // acaba de soltarse
      pulsado = false;
      if (!largoLanzado) return GESTO_CLIC;        // corto: se avisa al SOLTAR
    } else if (nivel && !largoLanzado && ahora - tPulsacion >= MS_CLIC_LARGO) {
      // Largo: se avisa SIN soltar, para que se note que ya pasó algo.
      largoLanzado = true; return GESTO_CLIC_LARGO;
    }
  }

  // ---- Las direcciones ----
  // Un gesto por empujón: hay que volver cerca del centro antes de
  // que cuente otro. Si no, mantener la palanca sería una ráfaga.
  if (!armado) {
    if (fabsf(ejeX) < UMBRAL_REARME && fabsf(ejeY) < UMBRAL_REARME) armado = true;
    return GESTO_NINGUNO;
  }

  // Solo gana un eje. Sin esto, un empujón en diagonal dispararía
  // dos gestos a la vez y el aparato haría dos cosas de golpe.
  if (fabsf(ejeX) >= UMBRAL_DISPARO && fabsf(ejeX) >= fabsf(ejeY)) {
    armado = false;
    return ejeX > 0 ? GESTO_DERECHA : GESTO_IZQUIERDA;
  }
  if (fabsf(ejeY) >= UMBRAL_DISPARO) {
    armado = false;
    return ejeY > 0 ? GESTO_ARRIBA : GESTO_ABAJO;
  }
  return GESTO_NINGUNO;
}
