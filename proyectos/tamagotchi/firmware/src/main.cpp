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
#include "joystick.h"
#include <Preferences.h>
#include "fuentes.h"
#include "iconos.h"
#include "snoopy.h"
#include "letras.h"

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_x509_crt_bundle_bin_start");

TFT_eSPI tft = TFT_eSPI();

// Va DESPUÉS de declarar `tft`: el volcado de bloques de la portada
// escribe directamente en la pantalla y necesita tenerla a la vista.
#include "portada.h"

Letras letras;      // la letra que se está mostrando
Letras precarga;    // la de la SIGUIENTE canción, ya descargada
String clavePrecarga;

const char* URL_TOKEN   = "https://accounts.spotify.com/api/token";
const char* URL_CANCION = "https://api.spotify.com/v1/me/player/currently-playing";
const char* URL_COLA    = "https://api.spotify.com/v1/me/player/queue";
const char* URL_PLAYLIST = "https://api.spotify.com/v1/playlists/";
const char* URL_ARTISTA  = "https://api.spotify.com/v1/artists/";
// Control de reproducción (RF-08, RF-09). Usan el permiso
// `user-modify-playback-state`, pedido en el ADR-0010.
const char* URL_PAUSA     = "https://api.spotify.com/v1/me/player/pause";
const char* URL_REANUDAR  = "https://api.spotify.com/v1/me/player/play";
const char* URL_SIGUIENTE = "https://api.spotify.com/v1/me/player/next";
const char* URL_ANTERIOR  = "https://api.spotify.com/v1/me/player/previous";
const char* URL_MIS_LISTAS = "https://api.spotify.com/v1/me/playlists?limit=50";

/* ============================================================
 *  Las playlists del selector (RF-14)
 * ============================================================
 *  ⚠️ AQUÍ SE EDITAN. Se buscan por el PRINCIPIO del nombre, no por
 *     su identificador: los nombres son legibles y los identificadores
 *     son cadenas opacas de 22 caracteres que nadie puede revisar.
 *
 *  Buscar por prefijo tiene dos ventajas concretas:
 *
 *    · Aguanta los emojis. "Alo" encuentra "Alo 💜" sin tener que
 *      escribir el emoji correcto en el código fuente.
 *    · Una playlist que todavía NO EXISTE simplemente no aparece, y
 *      el día que se cree aparece sola. Sin tocar el firmware.
 *
 *  Se muestran en ESTE orden, no en el que las devuelva Spotify.
 * ============================================================ */
const char* LISTAS_ELEGIDAS[] = { "Alo", "for you", "A&F" };
const int   N_LISTAS_ELEGIDAS = 3;
const int   MAX_LISTAS = 6;

struct ListaElegida { String nombre, id, imagen; };
ListaElegida listas[MAX_LISTAS];
int nListas = 0;
int listaSel = 0;                 // cuál está señalada
bool repintarSelector = false;
void moverSeleccion();            // se define junto al dibujo del selector
bool reproducirLista(const String& id);   // vive con las demás peticiones
/* Cuándo se intentó traerlas por última vez.
 *
 * ⚠️ La primera versión guardaba un simple "ya se preguntó" y no volvía
 *    a intentarlo. Un tropiezo de red —uno solo— dejaba el selector
 *    diciendo "sin playlists" hasta el siguiente reinicio. Pasó el
 *    2026-08-30 y desde fuera parecía que la función estaba rota.
 *
 * 🔑 "No insistir en bucle" no es lo mismo que "rendirse para siempre".
 *    Lo primero es prudencia; lo segundo convierte un fallo pasajero en
 *    uno permanente — y en un aparato regalado, nadie va a reiniciarlo. */
unsigned long listasIntentadasEn = 0;
bool listasPedidas = false;

/* Dos ritmos de reintento, según lo que esté pasando:
 *
 *  · NINGUNA encontrada  -> cada 15 s. Algo va mal (red, token) y se
 *    espera que se arregle pronto.
 *
 *  · ALGUNAS encontradas -> cada 5 min. Las que faltan simplemente
 *    todavía no existen. Es el caso de `A&F`, que se creó un día
 *    después: buscarlas por nombre solo tiene sentido si el aparato
 *    vuelve a mirar de vez en cuando.
 *
 *  ⚠️ La primera versión pedía la lista UNA VEZ por arranque. Como ya
 *     había encontrado dos, no volvía a mirar nunca — y la playlist
 *     nueva no aparecía hasta reiniciar. Eso rompía justo la promesa
 *     de buscar por nombre.
 *
 *  🔑 Un dato que puede cambiar solo no se puede leer una sola vez.
 *     Y "poder cambiar" incluye "todavía no existe". */
const unsigned long MS_REINTENTAR_LISTAS   = 15000;    // no hay ninguna
const unsigned long MS_REVISAR_SI_APARECEN = 300000;   // faltan algunas

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
const int ALTO_GRANDE = 28, ALTO_CHICA = 20;

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
// Ámbar. Deliberadamente distinto del rojo de "Sin conexion": son dos
// problemas distintos y quien mire la pantalla debe poder distinguirlos.
// Rojo = no hay WiFi (revisa el router). Ámbar = hay WiFi, falla Spotify.
const uint16_t NARANJA_AVISO = 0xFD20;

// --- Distribución de la pantalla (240x240) ---
const int MARGEN = 8, ANCHO = 240 - 16;
const int Y_TITULO = 32, Y_ARTISTA = 52;
const int Y_LETRA = 78, ALTO_LETRA = 112;
const int Y_BARRA = 196, Y_TIEMPOS = 214;

String accessToken;
unsigned long tokenCaducaEn = 0;

// Hasta cuándo Spotify nos pidió no volver a preguntar (429 + Retry-After).
// Se compara por resta, como todo temporizador de este firmware.
unsigned long noConsultarAntesDe = 0;

// Cuántas renovaciones seguidas han fallado. A partir de FALLOS_PARA_AVISAR
// se deja de disimular y se avisa en pantalla: si el refresh_token se
// invalidó (cambio de contraseña, permiso revocado), esto no se arregla solo
// y quien tenga el aparato merece verlo, no una canción congelada.
int fallosToken = 0;
const int FALLOS_PARA_AVISAR = 3;   // ~90 s de reintentos antes de avisar

/* ------------------------------------------------------------
 *  ¿Caducó el token? — por RESTA, nunca por valor absoluto
 * ------------------------------------------------------------
 *  `millis()` cuenta milisegundos en 32 bits, así que da la vuelta
 *  a cero cada 2^32 ms = 49,7 días.
 *
 *  La analogía: es el cuentakilómetros de un coche viejo. Si marca
 *  999.980 y preguntas "¿he pasado ya de 1.000.020?", la respuesta
 *  es siempre no, porque ese número no existe en el marcador. Pero
 *  si preguntas "¿cuánto he andado desde entonces?", la resta sí
 *  funciona aunque el marcador haya dado la vuelta.
 *
 *  Escrito como resta con signo, esto sigue siendo correcto el día
 *  49,7 y todos los siguientes. Es la misma forma que ya usan el
 *  resto de temporizadores del firmware.
 * ------------------------------------------------------------ */
bool tokenCaducado() {
  return (long)(millis() - tokenCaducaEn) >= 0;
}
String claveActual;

// Para que la letra avance suave entre consultas: se guarda el progreso
// que reportó Spotify y CUÁNDO lo dijo. Entre consulta y consulta, el
// progreso se calcula sumando el tiempo transcurrido. Sin esto la letra
// saltaría de golpe cada 5 segundos y se vería fatal.
long  progresoBase = 0, duracionMs = 0;
unsigned long instanteBase = 0;
bool  sonando = false;
bool  colaPedida = false;
String contextoUri;        // de dónde sale la música: playlist, álbum, artista...
String nombreContexto;     // el nombre legible, para la barra de arriba
String descContexto;       // la descripción de la playlist

// Sprite para la barra de arriba. Un sprite es un trozo de pantalla que
// se dibuja EN MEMORIA y luego se vuelca de golpe. Es lo que permite
// desplazar el texto sin que parpadee: si se dibujara directo sobre la
// pantalla, se vería el borrado entre fotograma y fotograma.
TFT_eSprite barraSpr = TFT_eSprite(&tft);
const int BARRA_ANCHO = 240 - MARGEN * 2 - 20;   // dejando sitio al play/pausa
const int BARRA_ALTO  = 20;
// La barra tiene dos zonas: el nombre de la playlist se queda QUIETO a la
// izquierda, y solo la descripción se desplaza en el espacio que sobra.
// Moverlo todo hacía perder de vista de qué playlist se trataba.
int  xDescripcion   = 0;    // dónde empieza la zona que se mueve
int  anchoZonaDesc  = 0;    // cuánto espacio tiene para moverse
int  anchoDesc      = 0;    // cuánto mide la descripción
int  cicloDesc      = 0;    // ancho + hueco: cada cuánto se repite
int  desplazamiento = 0;
bool hayQueDesplazar = false;
unsigned long ultimoDesplaz = 0;

// Velocidad: un píxel cada 65 ms ≈ 15 px/s. Lento y constante, que es
// como se lee cómodo. Más rápido cansa; a tirones, molesta.
const unsigned long MS_POR_PIXEL = 65;
const int HUECO_BUCLE = 40;   // separación entre el final y el reinicio

int  lineaPintada = -2;
unsigned long ultimaConsulta = 0, ultimoRefresco = 0;

/* ============================================================
 *  Los dos núcleos del ESP32
 * ============================================================
 *  PROBLEMA: pedirle algo a internet BLOQUEA. Una consulta a
 *  Spotify tarda ~1,2 s (sobre todo el handshake TLS), y durante
 *  ese tiempo el programa no hace nada más — ni dibujar.
 *
 *  Como se consulta cada 2 segundos, el texto que se desplaza se
 *  congelaba ~18 píxeles cada vez. Esas eran las pausas: no venían
 *  del bucle del texto, sino de la red parando el dibujo.
 *
 *  SOLUCIÓN: el ESP32 tiene DOS núcleos. Se reparte el trabajo:
 *
 *      Núcleo 0  ->  hablar con internet (puede bloquearse)
 *      Núcleo 1  ->  dibujar, y solo dibujar (nunca se bloquea)
 *
 *  Así la red puede tardar lo que quiera: la animación no se entera.
 *
 *  ⚠️ Reparto deliberado: TODO el dibujo se queda en un solo núcleo.
 *     La librería de pantalla no está preparada para que dos núcleos
 *     dibujen a la vez — se corrompería la imagen. Con esta división
 *     nunca ocurre.
 *
 *  Lo único que comparten son los datos, y para eso está el mutex:
 *  un turno que garantiza que nadie lea a medias lo que el otro está
 *  escribiendo.
 * ============================================================ */
SemaphoreHandle_t mutexDatos;

/* ------------------------------------------------------------
 *  Un turno para la radio, y un número de serie por canción
 * ------------------------------------------------------------
 *  MUTEX DE RED: solo UNA conexión cifrada a la vez.
 *
 *  Un handshake TLS no es esperar a la red: es CALCULAR. Son uno o
 *  dos segundos de criptografía pura. Dos a la vez saturaban el
 *  núcleo y el vigilante del sistema reiniciaba la placa:
 *      "Task watchdog got triggered: IDLE0 did not reset"
 *
 *  Con el turno, el hilo de Spotify espera como mucho UNA petición
 *  de letra (~1,2 s) — nunca una cadena de ellas.
 *
 *  GENERACIÓN: un número que sube con cada cambio de canción.
 *
 *  Sin él pasaba esto: se empieza a descargar la letra de A, cambias
 *  a B, la descarga de A termina... y se publicaba como si fuera de
 *  B. Comprobar "¿sigue siendo la misma canción?" AL FINAL no basta,
 *  porque la variable que dice cuál es ya había cambiado.
 *
 *  🔑 Para saber si un resultado llega tarde, hay que marcarlo al
 *     EMPEZAR y comprobar la marca al terminar. Mirar solo el estado
 *     final no distingue "sigue igual" de "cambió y volvió a cambiar".
 * ------------------------------------------------------------ */
SemaphoreHandle_t mutexRed;
volatile uint32_t generacion = 0;

#define CON_RED(bloque) do { \
  if (xSemaphoreTake(mutexRed, pdMS_TO_TICKS(15000)) != pdTRUE) { \
    Serial.printf("[TURNO] red: 15s esperando · nucleo %d · linea %d\n", \
                  xPortGetCoreID(), __LINE__); \
    xSemaphoreTake(mutexRed, portMAX_DELAY); \
    Serial.printf("[TURNO] red: por fin · linea %d\n", __LINE__); \
  } \
  bloque; \
  xSemaphoreGive(mutexRed); } while (0)
String tituloAct, artistaAct;
volatile bool pedirRedibujado = false;   // el núcleo 0 avisa al 1

/* ------------------------------------------------------------
 *  Búsqueda de letra cancelable
 * ------------------------------------------------------------
 *  Antes la letra se buscaba dentro de consultar(), de una tirada:
 *  hasta 5 peticiones seguidas, ~9 s en el peor caso. Durante ese
 *  rato no se le preguntaba nada a Spotify, así que si cambiabas
 *  de canción el aparato no se enteraba y se quedaba atascado.
 *
 *  Ahora la búsqueda es una cola de PASOS, y el bucle de red hace
 *  UN paso por vuelta. Entre paso y paso vuelve a consultar a
 *  Spotify, y si la canción cambió se tira lo que quedaba.
 * ------------------------------------------------------------ */
/*  UNA sola máquina de pasos para TODAS las peticiones lentas, tanto la
 *  letra de la canción actual como la precarga de la siguiente.
 *
 *  La primera versión solo partió en pasos la búsqueda de la canción
 *  actual, dejando la precarga como un bloque entero. Fue un arreglo a
 *  medias: la precarga encadena hasta 6 peticiones (~9 s) y se lanza
 *  justo después de cargar la letra nueva — o sea, en el mismo momento
 *  en que uno tiende a saltar de canción.
 *
 *  Ahora TODO pasa por aquí, y cada vuelta del bucle hace exactamente
 *  UNA petición. Entre vuelta y vuelta se consulta a Spotify.
 */
enum Fase { FASE_NADA, FASE_COLA, FASE_EXACTA, FASE_CANDIDATOS, FASE_PROBAR };
Fase   fase = FASE_NADA;
bool   faseEsPrecarga = false;      // ¿para la canción actual o la siguiente?
int    candidatoActual = 0;
String claveObj, tObj, aObj, alObj; // la canción que se está buscando
long   dObj = 0;

/* ⏱ EL CHIVATO DEL TURNO — se queda. No es codigo de depuracion suelto.
 *
 *  Se puso el 2026-08-30 para cazar un cuelgue, y lo cazo al tercer
 *  intento. Se conserva porque su valor no era encontrar AQUEL fallo,
 *  sino que un cuelgue deje de ser invisible: antes el aparato se
 *  quedaba mudo y no habia forma de saber si era un bloqueo o memoria
 *  pisada. Ahora dice quien espera, en que nucleo y en que linea.
 *
 *  🔑 Es la regla del regalo aplicada al diagnostico: degradar, no
 *     romper. Cuesta cero cuando todo va bien y convierte un silencio
 *     en una respuesta cuando no.
 *
 *  El comportamiento es IDENTICO al de antes: si se agota el plazo se
 *  sigue esperando para siempre, igual que hacia `portMAX_DELAY`. Lo
 *  unico que cambia es que ANTES de quedarse mudo, avisa de quien
 *  espera, en que nucleo y en que linea.
 *
 *  Sin esto el abrazo mortal y la memoria pisada son indistinguibles:
 *  los dos se ven igual desde fuera, un silencio. Con esto se separan.
 *
 *  El plazo del turno de DATOS es corto (3 s) porque ese turno se
 *  suelta en microsegundos. El de RED es largo (15 s) porque ahi 6 s
 *  de espera son NORMALES —medido en controles-joystick-latencia.md—
 *  y un plazo corto solo daria falsas alarmas.
 *
 *  Ver `5-investigacion/cuelgue-por-turno-anidado.md`. */
#define CON_DATOS(bloque) do { \
  if (xSemaphoreTake(mutexDatos, pdMS_TO_TICKS(3000)) != pdTRUE) { \
    Serial.printf("[TURNO] datos: 3s esperando · nucleo %d · linea %d\n", \
                  xPortGetCoreID(), __LINE__); \
    xSemaphoreTake(mutexDatos, portMAX_DELAY); \
    Serial.printf("[TURNO] datos: por fin · linea %d\n", __LINE__); \
  } \
  bloque; \
  xSemaphoreGive(mutexDatos); } while (0)
String estadoPintado;

/* ------------------------------------------------------------
 *  Copias de la barra que pertenecen SOLO al núcleo 1
 * ------------------------------------------------------------
 *  `nombreContexto` y `descContexto` los reasigna el núcleo 0 cada
 *  vez que cambias de playlist. Un `String` de Arduino es puntero +
 *  longitud: asignarlo libera el buffer viejo, reserva otro y copia.
 *  El núcleo 1 los estaba leyendo ~200 veces por segundo para
 *  dibujar, sin mutex — y además RECORTABA `nombreContexto` para que
 *  cupiera en la barra, o sea que ESCRIBÍA sobre ellos.
 *
 *  Dos núcleos escribiendo el mismo String es liberar dos veces el
 *  mismo bloque de memoria. Eso no se ve en pantalla: corrompe el
 *  montón y revienta minutos después en otro sitio cualquiera.
 *
 *  🔑 La solución no es poner un mutex en el bucle de dibujo (200
 *     veces por segundo), sino COPIAR una vez cuando algo cambia y
 *     dibujar siempre desde la copia. El dato que se dibuja deja de
 *     ser compartido.
 * ------------------------------------------------------------ */
String nombreBarra, descBarra;

Joystick joystick;

/* ============================================================
 *  Los modos de pantalla (RF-11)
 * ============================================================
 *  Arriba y abajo recorren las cuatro en bucle. El orden no es
 *  casual: va de menos a más información, y la de elegir playlist
 *  queda al final porque es la única que no muestra la canción.
 * ============================================================ */
enum Modo { MODO_SIN_LETRA, MODO_CON_LETRA, MODO_PORTADA, MODO_PLAYLISTS, N_MODOS };
int  modo = MODO_CON_LETRA;
bool repintarModo = false;      // el núcleo 1 lo levanta al cambiar

/* ============================================================
 *  La pantalla oculta y su combinación secreta (RF-15 / RF-16)
 * ============================================================
 *  `↑ ↑ ↓ ↓` — la mitad del Código Konami. Ver ADR-0012.
 *
 *  ⚠️ NO es un modo más. Vive fuera del bucle de `N_MODOS`:
 *
 *     1. Dentro del bucle dejaría de estar escondida: se llegaría
 *        empujando arriba cuatro veces.
 *     2. NO se guarda en NVS. Si se guardara y el aparato se
 *        reiniciara solo, quien lo recibió despertaría en una
 *        pantalla de diagnóstico sin música y sin saber salir.
 *
 *  🔑 Un estado excepcional no se persiste. Lo que sobrevive a un
 *     reinicio debe ser algo en lo que sea razonable despertar.
 * ============================================================ */
bool pantallaOculta = false;      // ¿se está mostrando ahora?
bool abrirOculta = false;         // el detector lo levanta; el dibujo lo cumple
bool cerrarOculta = false;
int  modoAntesDeOculta = MODO_CON_LETRA;
unsigned long ultimoRefrescoOculta = 0;

/* Solo puede usar gestos que NO pidan nada a la red (arriba y abajo):
 * así el detector vive entero en el núcleo del dibujo.
 *
 * ⚠️ Que la secuencia sea invisible depende de que `N_MODOS` sea 4:
 *    subir dos y bajar dos vuelve a la pantalla de partida. */
const Gesto COMBINACION_SECRETA[] = { GESTO_ARRIBA, GESTO_ARRIBA,
                                      GESTO_ABAJO,  GESTO_ABAJO };
const int PASOS_SECRETOS = sizeof(COMBINACION_SECRETA) / sizeof(COMBINACION_SECRETA[0]);

/* Cuánto se puede tardar de un gesto al siguiente sin perder la racha.
 *
 * ⚠️ La primera versión puso 1200 ms **suponiendo** que bastaba. Se
 *    probó en la placa el 2026-08-30 y NO bastó: la secuencia entera
 *    se hizo bien y no se abrió nada, porque entre el tercer gesto y
 *    el cuarto pasaron más de 3 s. Un plazo que nadie alcanza no
 *    protege de nada: solo hace la función inalcanzable.
 *
 * 🔑 Este número no se puede razonar, hay que medirlo con la mano de
 *    alguien. Por eso cada paso escribe cuánto tardó: el `[Secreto]`
 *    del registro es lo que permite ajustarlo con datos. */
const unsigned long MS_ENTRE_PASOS_SECRETOS = 2000;

int  pasoSecreto = 0;
unsigned long ultimoPasoSecreto = 0;

bool avanzarCombinacion(Gesto g) {
  unsigned long hueco = millis() - ultimoPasoSecreto;
  if (pasoSecreto > 0 && hueco > MS_ENTRE_PASOS_SECRETOS) {
    Serial.printf("[Secreto] racha rota: %lu ms desde el paso %d\n", hueco, pasoSecreto);
    pasoSecreto = 0;
  }
  ultimoPasoSecreto = millis();

  if (g == COMBINACION_SECRETA[pasoSecreto]) {
    pasoSecreto++;
    Serial.printf("[Secreto] paso %d de %d (+%lu ms)\n", pasoSecreto, PASOS_SECRETOS, hueco);
    if (pasoSecreto == PASOS_SECRETOS) { pasoSecreto = 0; return true; }
    return false;
  }

  /* Un gesto que rompe la racha puede ser el PRIMERO de un intento
   * nuevo. Sin esto, `↑↑↑↓↓` no abriría nada — y eso es justo lo que
   * hace quien se pasa de pantalla y corrige. */
  pasoSecreto = (g == COMBINACION_SECRETA[0]) ? 1 : 0;
  return false;
}

inline const char* nombreModo(int m) {
  switch (m) {
    case MODO_SIN_LETRA:  return "sin letra";
    case MODO_CON_LETRA:  return "con letra";
    case MODO_PORTADA:    return "portada";
    case MODO_PLAYLISTS:  return "playlists";
    default:              return "?";
  }
}

/* Guardar el modo para que sobreviva a un reinicio (RF-11)
 * --------------------------------------------------------
 *  Va a NVS, la memoria que no se borra al apagar.
 *
 *  ⚠️ Escribir en NVS desgasta la memoria (unas 100.000 veces por
 *     hueco). Un cambio de modo lo pide una persona, así que no hay
 *     riesgo real — pero si alguien juguetea moviendo la palanca
 *     arriba y abajo, serían decenas de escrituras en un minuto.
 *     Por eso no se guarda al cambiar, sino 3 s DESPUÉS del último
 *     cambio: la ráfaga entera cuesta una sola escritura.
 *
 *  🔑 Guardar el resultado de una ráfaga, no cada paso de la ráfaga. */
/* ------------------------------------------------------------
 *  La portada: descarga el núcleo 0, dibuja el núcleo 1
 * ------------------------------------------------------------
 *  ⚠️ El reparto NO es opcional. La pantalla va por SPI, y si los
 *     dos núcleos escriben en ese bus a la vez la imagen sale
 *     corrupta o se cuelga el bus. El núcleo 0 no dibuja NUNCA.
 *
 *  El relevo es el buffer: el núcleo 0 lo llena y levanta la
 *  bandera; el núcleo 1 lo pinta, lo libera y la baja. Solo uno de
 *  los dos lo toca en cada momento, y la bandera dice cuál.
 * ------------------------------------------------------------ */
// El ancho que se le pide a Spotify. Es fijo: la de 640 no cabe en
// memoria y la de 64 es demasiado pequeña para la pantalla.
const int ANCHO_QUE_PEDIMOS = 300;

String urlPortada;             // la de la canción actual (del JSON)
String portadaPintada;         // la que ya está en pantalla
String portadaEnBuffer;        // a cuál corresponde lo descargado
uint8_t* bufPortada = nullptr;
size_t   lenPortada = 0;
volatile bool portadaLista = false;   // el núcleo 1 tiene trabajo
bool portadaVisible = false;          // ¿la de ahora está pintada?
volatile bool portadaFallo = false;   // no se pudo traer: hay que decirlo
String portadaFallida;                // cuál falló, para no reintentar en bucle

Preferences prefs;
unsigned long modoCambiadoEn = 0;
int modoGuardado = -1;

void guardarModoSiToca() {
  if (modoCambiadoEn == 0 || modo == modoGuardado) return;
  if (millis() - modoCambiadoEn < 3000) return;
  prefs.putInt("modo", modo);
  modoGuardado = modo;
  modoCambiadoEn = 0;
  Serial.printf("[Modo] guardado: %s\n", nombreModo(modo));
}

/* ------------------------------------------------------------
 *  El buzón: cómo le pide el núcleo 1 algo al núcleo 0
 * ------------------------------------------------------------
 *  El joystick se lee en el núcleo 1 (dibujo), pero hablar con
 *  Spotify es trabajo del núcleo 0 — es el que tiene el turno de
 *  red y el que sabe esperar sin que se note.
 *
 *  ⚠️ El núcleo del dibujo NO debe hacer peticiones. Una petición
 *     tarda ~1,2 s, y durante ese rato la pantalla se congelaría:
 *     la letra dejaría de avanzar y la barra de la playlist de
 *     desplazarse. Justo lo que el reparto en dos núcleos vino a
 *     evitar (ver 5-investigacion/dos-nucleos.md).
 *
 *  Así que el núcleo 1 no ejecuta: deja una nota. El núcleo 0 la
 *  recoge en su siguiente vuelta y la cumple cuando le toca turno.
 *
 *  🔑 Cuando dos hilos tienen capacidades distintas, no se comparte
 *     el trabajo: se comparte un mensaje. El que sabe esperar espera.
 * ------------------------------------------------------------ */
enum Orden { ORDEN_NINGUNA, ORDEN_PLAYPAUSA, ORDEN_SIGUIENTE, ORDEN_ANTERIOR,
             ORDEN_PONER_LISTA };

/*  Por qué una COLA y no una sola ranura
 *  --------------------------------------
 *  La primera versión guardaba una sola orden. Si hacías dos gestos
 *  seguidos —saltar y luego pausar— el segundo pisaba al primero antes
 *  de que el núcleo 0 llegara a recogerlo, y el salto se perdía sin
 *  dejar rastro. Visto en el registro el 2026-08-29.
 *
 *  Cuatro huecos bastan: es más de lo que una mano hace mientras el
 *  otro núcleo da una vuelta. Si aun así se llenara, se avisa en vez
 *  de tirar la orden en silencio.
 *
 *  🔑 Un buzón de una sola nota no es un buzón: es una pizarra. Y en
 *     una pizarra, escribir es borrar lo anterior.                    */
const int MAX_ORDENES = 4;
Orden         colaOrdenes[MAX_ORDENES];
unsigned long colaCuando[MAX_ORDENES];   // ⏱ para medir el retraso real
int           nOrdenes = 0;
unsigned long ordenEncoladaEn = 0;       // ⏱ la que se está ejecutando

// Desde el núcleo 1 (dibujo).
void encolarOrden(Orden o) {
  CON_DATOS({
    if (nOrdenes < MAX_ORDENES) { colaCuando[nOrdenes] = millis(); colaOrdenes[nOrdenes++] = o; }
    else Serial.println("[Orden] ⚠️ cola llena, orden descartada");
  });
}

// Desde el núcleo 0 (red). Devuelve ORDEN_NINGUNA si no hay nada.
Orden sacarOrden() {
  Orden o = ORDEN_NINGUNA;
  CON_DATOS({
    if (nOrdenes > 0) {
      o = colaOrdenes[0]; ordenEncoladaEn = colaCuando[0];
      for (int i = 1; i < nOrdenes; i++) { colaOrdenes[i-1] = colaOrdenes[i]; colaCuando[i-1] = colaCuando[i]; }
      nOrdenes--;
    }
  });
  return o;
}

// La consulta el hilo de letras para saber si debe apartarse.
// Cuándo se tocó la palanca por última vez. La escribe el núcleo 1 y la
// lee el núcleo 0; en el ESP32 un `unsigned long` alineado se lee y se
// escribe de una sola vez, así que no hace falta mutex para esto.
volatile unsigned long ultimoGesto = 0;
// ¿Hay alguna orden del usuario sin completar? El núcleo 1 la enciende
// al leer el gesto y el núcleo 0 la apaga cuando ya no queda ninguna.
volatile bool ordenEnVuelo = false;
const unsigned long MS_NAVEGANDO = 2500;   // tras el último gesto

// ¿Está el usuario navegando ahora mismo?
bool navegando() {
  return ultimoGesto && (millis() - ultimoGesto < MS_NAVEGANDO);
}

bool hayOrdenesPendientes() {
  int n = 0;
  CON_DATOS({ n = nOrdenes; });
  return n > 0;
}

inline const char* nombreOrden(Orden o) {
  switch (o) {
    case ORDEN_PLAYPAUSA: return "play/pausa";
    case ORDEN_SIGUIENTE: return "siguiente";
    case ORDEN_ANTERIOR:  return "anterior";
    case ORDEN_PONER_LISTA: return "poner playlist";
    default:              return "-";
  }
}

// El núcleo 0 no toca `estadoPintado` (es del núcleo 1): levanta esta
// bandera y el núcleo 1 se da por enterado en su siguiente vuelta.
volatile bool forzarBarra = false;

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

/* Como `escribir`, pero pasando por el dibujante de iconos.
 *
 * `aLatin1` convierte los emojis conocidos en códigos de marca, y solo
 * `dibujarMixto` sabe pintarlos: `drawString` los tira. Por eso la
 * playlist «Alo 💜» perdía el corazón en el selector mientras que en
 * la barra de arriba sí se veía — la barra ya usaba este camino.
 *
 * 🔑 Dos sitios que muestran el mismo texto tienen que dibujarlo con
 *    el mismo pincel, o uno de los dos pierde cosas por el camino. */
void escribirMixtoCentrado(const String& s, int y, uint16_t color) {
  usarFuente(false);
  int w = anchoMixto(tft, s);
  dibujarMixto(tft, s, (240 - w) / 2, y, color);
}

// Símbolos de reproducción dibujados a mano: un triángulo para "play",
// dos barras para "pausa". Ninguna fuente los trae.
void dibujarPlayPausa(int x, int y, bool reproduciendo, uint16_t color) {
  if (reproduciendo) tft.fillTriangle(x, y - 6, x, y + 6, x + 10, y, color);
  else { tft.fillRect(x, y - 6, 4, 12, color); tft.fillRect(x + 6, y - 6, 4, 12, color); }
}

/* ------------------------------------------------------------
 *  El spinner: "te oí", sin esperar a la red
 * ------------------------------------------------------------
 *  Medido el 2026-08-29: una orden del joystick tarda 1 s en el
 *  caso bueno y hasta 6 s si choca con otra petición en curso.
 *  El segundo de red no se puede quitar —es el viaje a Spotify—,
 *  pero el problema real no era el tiempo: era no saber si el
 *  aparato te había hecho caso. Sin señal, uno vuelve a empujar.
 *
 *  Esto aparece en el siguiente fotograma (menos de 20 ms) porque
 *  no depende de la red para nada: solo de que el gesto se leyó.
 *
 *  🔑 La latencia que molesta no es la que tarda: es la que no
 *     avisa. Un segundo con respuesta se siente instantáneo; uno
 *     sin respuesta se siente roto.
 *
 *  Ocho puntos en círculo con una estela que se apaga. Va en el
 *  hueco del icono de reproducción, que está fuera del sprite de
 *  la barra — así no pelea con el desplazamiento del texto.
 * ------------------------------------------------------------ */
void dibujarSpinner(int cx, int cy, int radio = 6, int punto = 2) {
  // Ocho posiciones en círculo. Se calculan con los senos y cosenos de
  // 45 grados escalados, para que valga con cualquier radio.
  static const int8_t sx[8] = {   0,  71, 100,  71,   0, -71, -100, -71 };
  static const int8_t sy[8] = { -100, -71,  0,  71, 100,  71,    0, -71 };
  // Los colores salen de la paleta que ya usa el aparato: la estela
  // es el mismo gris con el que se dibuja todo lo secundario.
  static const uint16_t estela[4] = { VERDE_SPOTIFY, GRIS, GRIS_OSCURO, GRIS_LINEA };

  int paso = (millis() / 90) % 8;           // una vuelta cada ~0,7 s
  int caja = radio + punto + 1;
  tft.fillRect(cx - caja, cy - caja, caja * 2 + 1, caja * 2 + 1, TFT_BLACK);
  for (int i = 0; i < 8; i++) {
    int atras = (paso - i + 8) % 8;
    if (atras > 3) continue;                // los demás no se pintan
    tft.fillCircle(cx + sx[i] * radio / 100, cy + sy[i] * radio / 100,
                   punto, estela[atras]);
  }
}

// Pinta la barra: la descripción desplazada, y encima el nombre fijo.
//
// El orden importa. Primero se dibuja la descripción, que puede invadir
// la zona del nombre al desplazarse; después se tapa esa zona y se pinta
// el nombre encima. Así el nombre nunca se ve pisado, y no hace falta
// recortar nada a mano.
void pintarContenidoBarra(int off, uint16_t colorNombre) {
  barraSpr.fillSprite(TFT_BLACK);
  barraSpr.setFreeFont(&FuenteChica);

  if (descBarra.length()) {
    if (hayQueDesplazar) {
      // Bucle continuo: se dibuja la descripción DOS veces, separadas por
      // un hueco fijo. Cuando la primera copia se va por la izquierda, la
      // segunda ya está entrando por la derecha — nunca hay un momento en
      // blanco, así que no se percibe ningún salto ni pausa.
      int o = off % cicloDesc;
      dibujarMixto(barraSpr, descBarra, xDescripcion - o, 2, GRIS);
      dibujarMixto(barraSpr, descBarra, xDescripcion - o + cicloDesc, 2, GRIS);
    } else {
      dibujarMixto(barraSpr, descBarra, xDescripcion, 2, GRIS);
    }
  }

  // Tapar la zona fija y pintar encima el nombre + el separador.
  barraSpr.fillRect(0, 0, xDescripcion, BARRA_ALTO, TFT_BLACK);
  int x = dibujarMixto(barraSpr, nombreBarra, 0, 2, colorNombre);
  if (descBarra.length()) dibujarMixto(barraSpr, SEPARADOR, x, 2, GRIS);

  barraSpr.pushSprite(MARGEN, 4);
}

void barraEstado(bool reproduciendo, uint16_t color) {
  /* ------------------------------------------------------------
   *  Mirar la barra 5 veces por segundo, no 200
   * ------------------------------------------------------------
   *  Esta función se llama en cada vuelta del bucle de dibujo. La
   *  primera versión copiaba los dos textos y componía una clave en
   *  CADA una: unas 150 veces por segundo, con varias reservas de
   *  memoria cada vez.
   *
   *  Eso costaba dos cosas a la vez:
   *    · El bucle se volvía lento e irregular, y el desplazamiento
   *      del texto se veía a tirones.
   *    · Miles de reservas y liberaciones por segundo machacaban el
   *      montón — el mismo que estamos vigilando por fragmentación.
   *
   *  Y todo para comprobar algo que cambia cuando cambias de canción
   *  o de playlist: unas pocas veces por MINUTO.
   *
   *  🔑 La frecuencia de una comprobación debe salir de lo rápido que
   *     cambia el dato, no de lo rápido que corre el bucle. Comprobar
   *     de más no da más precisión: solo gasta.
   *
   *  200 ms es imperceptible para quien mira, y baja el gasto 40 veces.
   *  El desplazamiento del texto NO pasa por aquí: lo mueve
   *  `moverBarra`, que sigue yendo a la velocidad del bucle.
   * ------------------------------------------------------------ */
  static unsigned long ultimaRevision = 0;
  if (!forzarBarra && millis() - ultimaRevision < 200) return;
  ultimaRevision = millis();

  // Una sola copia, bajo mutex. A partir de aquí se trabaja con `nombre`
  // y `desc`, que son nuestros: el núcleo 0 puede reasignar los originales
  // mientras dibujamos y no pasa nada.
  String nombre, desc;
  CON_DATOS({ nombre = nombreContexto; desc = descContexto; });

  if (forzarBarra) { forzarBarra = false; estadoPintado = ""; }

  String clave = nombre + "|" + desc + (reproduciendo ? "|1" : "|0");
  if (estadoPintado != clave) {
    estadoPintado = clave;
    barraSpr.setFreeFont(&FuenteChica);

    // El nombre no puede comerse toda la barra: se le deja como mucho el
    // 55%, para que la descripción tenga sitio donde moverse. Se recorta
    // la COPIA — recortar el original era la escritura desde dos núcleos.
    int topeNombre = (BARRA_ANCHO * 55) / 100;
    while (nombre.length() > 1 &&
           anchoMixto(barraSpr, nombre) > topeNombre)
      nombre = nombre.substring(0, nombre.length() - 1);

    // Publicar para el dibujo. Desde aquí, `pintarContenidoBarra` y
    // `moverBarra` solo miran estas dos.
    nombreBarra = nombre; descBarra = desc;

    int anchoNombre = anchoMixto(barraSpr, nombreBarra);
    xDescripcion = anchoNombre + (descBarra.length() ? anchoMixto(barraSpr, SEPARADOR) : 0);
    anchoZonaDesc = BARRA_ANCHO - xDescripcion;
    anchoDesc = descBarra.length() ? anchoMixto(barraSpr, descBarra) : 0;

    hayQueDesplazar = (anchoDesc > anchoZonaDesc);
    cicloDesc = anchoDesc + HUECO_BUCLE;
    desplazamiento = 0;

    tft.fillRect(0, 0, 240, 27, TFT_BLACK);
    dibujarPlayPausa(240 - MARGEN - 11, 13, reproduciendo, color);
    tft.drawFastHLine(0, 27, 240, GRIS_LINEA);
    pintarContenidoBarra(0, color);
  }
}

// Avanza el desplazamiento. Se llama desde el bucle principal, no desde
// las consultas a Spotify: así se mueve a ritmo constante aunque no haya
// novedades.
void moverBarra(uint16_t color) {
  if (!hayQueDesplazar) return;
  unsigned long ahora = millis();
  if (ahora - ultimoDesplaz < MS_POR_PIXEL) return;
  ultimoDesplaz = ahora;
  desplazamiento = (desplazamiento + 1) % cicloDesc;
  pintarContenidoBarra(desplazamiento, color);
}

void cabecera(const char* titulo, const char* artista) {
  tft.fillRect(0, 30, 240, 40, TFT_BLACK);
  // aLatin1 conserva los acentos: la fuente generada llega al código 255.
  String l[6]; bool g;
  partirConReserva(aLatin1(String(titulo)), ANCHO, l, 1, g);
  escribir(l[0], Y_TITULO, false, TFT_WHITE);
  partirConReserva(aLatin1(String(artista)), ANCHO, l, 1, g);
  escribir(l[0], Y_ARTISTA, false, VERDE_SPOTIFY);
  tft.drawFastHLine(MARGEN, 70, ANCHO, GRIS_LINEA);
}

/* ------------------------------------------------------------
 *  MODO 1 — sin letra: la canción, en grande
 * ------------------------------------------------------------
 *  Sin la letra sobra media pantalla, así que el título deja de
 *  ser una línea recortada y pasa a ocupar hasta tres, centrado
 *  en el hueco entero.
 *
 *  Se reutiliza `partirConReserva`, que ya sabe encoger la fuente
 *  antes que recortar palabras: más vale una letra pequeña que un
 *  título a medias.
 * ------------------------------------------------------------ */
void cabeceraGrande(const String& titulo, const String& artista) {
  const int yTope = 34, yFondo = Y_BARRA - 10;
  tft.fillRect(0, yTope - 4, 240, yFondo - yTope + 4, TFT_BLACK);

  String lt[6]; bool gt;
  int nt = partirConReserva(aLatin1(titulo), ANCHO, lt, 3, gt);
  int altoT = nt * (gt ? ALTO_GRANDE : ALTO_CHICA);

  String la[4]; bool ga;
  int na = partirConReserva(aLatin1(artista), ANCHO, la, 2, ga);
  int altoA = na * ALTO_CHICA;

  const int HUECO = 14;
  int total = altoT + HUECO + altoA;
  int y = yTope + ((yFondo - yTope) - total) / 2;

  for (int i = 0; i < nt; i++) {
    escribir(lt[i], y, gt, TFT_WHITE);
    y += gt ? ALTO_GRANDE : ALTO_CHICA;
  }
  y += HUECO;
  for (int i = 0; i < na; i++) {
    escribir(la[i], y, false, VERDE_SPOTIFY);
    y += ALTO_CHICA;
  }
}

/* ------------------------------------------------------------
 *  El fundido entre modos
 * ------------------------------------------------------------
 *  Cambiar de modo repinta la pantalla entera, y el salto seco se
 *  siente brusco: aparece todo de golpe y encima se ve un instante
 *  la pantalla a medio dibujar.
 *
 *  La solución no necesita memoria ni una sola línea de gráficos:
 *  la pantalla YA tiene el brillo por PWM, puesto para el modo
 *  nocturno del RNF-02. Se baja a cero, se repinta a oscuras, y se
 *  sube. Nadie ve el repintado — solo un parpadeo suave.
 *
 *  🔑 La pieza que hacía falta ya estaba puesta para otra cosa. El
 *     brillo regulable se añadió para no molestar de noche, y sirve
 *     igual para esto sin cambiar nada.
 *
 *  Bajar es más rápido que subir a propósito: cortar rápido y
 *  aparecer despacio se siente intencionado; al revés, se siente
 *  como un fallo de contacto.
 * ------------------------------------------------------------ */
void fundirBrillo(int desde, int hasta, int ms) {
  const int PASOS = 14;
  for (int i = 1; i <= PASOS; i++) {
    ledcWrite(CANAL_PWM, desde + (hasta - desde) * i / PASOS);
    delay(ms / PASOS);
  }
  ledcWrite(CANAL_PWM, hasta);
}

// Los modos 3 y 4 todavía no tienen contenido. Se dice, en vez de
// dejar una pantalla negra que parecería una avería.
void pantallaEnObras(const char* que) {
  tft.fillRect(0, 30, 240, Y_BARRA - 34, TFT_BLACK);
  escribir(que, 100, true, GRIS);
  escribir("en construccion", 132, false, GRIS_OSCURO);
}

/* ------------------------------------------------------------
 *  MODO 3 — la portada, a pantalla completa
 * ------------------------------------------------------------
 *  Sin barra de arriba ni barra de progreso: la portada ocupa los
 *  240x240 enteros. Es lo que pedía el modo — ver la carátula, no
 *  ver la carátula rodeada de cosas.
 * ------------------------------------------------------------ */
/* ------------------------------------------------------------
 *  ¿Qué imagen DEBERÍA estar en pantalla ahora mismo?
 * ------------------------------------------------------------
 *  Un solo juego de variables sirve a dos pantallas —la portada del
 *  álbum y la imagen de la playlist elegida—. El núcleo 0 decide qué
 *  bajar mirando el modo, pero la descarga tarda ~1 s, y en ese rato
 *  puedes haber cambiado de pantalla o de playlist.
 *
 *  Sin comprobarlo, la imagen que llega se dibuja donde toque en ese
 *  momento: reportado el 2026-08-30, la carátula de la canción salió
 *  como imagen de la playlist «Alo».
 *
 *  🔑 Es el mismo fallo que la «letra de la canción anterior», y el
 *     mismo arreglo: antes de publicar un resultado que tardó en
 *     llegar, comprobar que todavía es el que se esperaba. Un dato
 *     lento sin marca de a quién pertenece acaba en el sitio
 *     equivocado. La lección estaba escrita en
 *     5-investigacion/concurrencia-y-carreras.md y no la apliqué aquí.
 * ------------------------------------------------------------ */
String imagenQueTocaAhora() {
  if (modo == MODO_PORTADA) { String u; CON_DATOS({ u = urlPortada; }); return u; }
  if (modo == MODO_PLAYLISTS && nListas > 0) return listas[listaSel].imagen;
  return "";
}

void olvidarImagen() {
  if (bufPortada) { free(bufPortada); bufPortada = nullptr; lenPortada = 0; }
  portadaLista = false; portadaEnBuffer = ""; portadaPintada = "";
  // ⚠️ También se levanta el veto. `portadaFallida` evita reintentar en
  //    bucle dentro de una misma pantalla, pero si no se limpiara nunca,
  //    una imagen que falló una vez quedaría vetada hasta el reinicio.
  portadaFallida = "";
}

// Vuelca la imagen que haya llegado, en el encuadre que esté puesto.
// La usan el modo portada (212x212) y el selector (150x150).
bool volcarImagenSiLlego(int lado, int y0) {
  if (!portadaLista || !bufPortada) return false;

  // ⚠️ ¿Sigue siendo la que esperábamos? Si cambiaste de pantalla o de
  //    playlist mientras bajaba, esta imagen es de otra cosa.
  if (portadaEnBuffer != imagenQueTocaAhora()) {
    Serial.println("[Imagen] ⛔ descartada: ya no es la que toca");
    olvidarImagen();
    return false;
  }

  unsigned long t0 = millis();
  if (!prepararEncuadre(bufPortada, lenPortada, lado, y0)) {
    free(bufPortada); bufPortada = nullptr; lenPortada = 0;
    portadaLista = false; portadaFallo = true;
    return false;
  }
  TJpgDec.drawJpg(0, 0, bufPortada, lenPortada);
  Serial.printf("[Imagen] dibujada en %lu ms · RAM %lu B\n",
                millis() - t0, (unsigned long)ESP.getFreeHeap());
  free(bufPortada); bufPortada = nullptr; lenPortada = 0;
  portadaLista = false;
  portadaPintada = portadaEnBuffer;
  portadaVisible = true;
  return true;
}

void pintarPortada() {
  if (portadaLista && bufPortada) {
    if (portadaEnBuffer != imagenQueTocaAhora()) {
      Serial.println("[Imagen] ⛔ descartada: ya no es la que toca");
      olvidarImagen(); return;
    }
    unsigned long t0 = millis();
    if (!prepararEncuadre(bufPortada, lenPortada,
                          240 - PORTADA_ALTO_BARRA, PORTADA_ALTO_BARRA)) {
      free(bufPortada); bufPortada = nullptr; lenPortada = 0;
      portadaLista = false; portadaFallo = true; return;
    }
    TJpgDec.drawJpg(0, 0, bufPortada, lenPortada);
    Serial.printf("[Portada] dibujada en %lu ms · RAM %lu B\n",
                  millis() - t0, (unsigned long)ESP.getFreeHeap());
    /* ⚠️ Se libera AQUÍ, nada más pintarla. Se probó a guardarla para
     *    no tener que rebajarla al reentrar al modo, y hubo que quitarlo:
     *    ver `5-investigacion/portada-cache-descartada.md`. En resumen,
     *    guardar 30-57 KB durante minutos partía el montón (el bloque
     *    contiguo mayor caía de 110 KB a 45 KB) y provocaba un vaivén de
     *    soltar-y-rebajar que congelaba la pantalla 400 ms cada vez.
     *
     *    🔑 Una optimización que ahorra un segundo pero introduce un
     *       tirón repetido no ahorra nada: cambia una espera que se
     *       entiende por una molestia que no. */
    free(bufPortada); bufPortada = nullptr; lenPortada = 0;
    portadaLista = false;
    portadaPintada = portadaEnBuffer;
    portadaVisible = true;
    return;
  }
  /* Todavía no está: el bucle dibujará el spinner grande en el centro.
   *
   * Y se olvida cuál se bajó, para que el núcleo 0 la vuelva a pedir.
   * Hace falta porque la imagen se libera nada más pintarla: al salir
   * del modo y volver, no queda nada que repintar. Volver a bajarla
   * cuesta ~1 s; guardarla costaría 50 KB permanentes, y esos 50 KB son
   * los que el hilo de letras necesita para su conexión. */
  olvidarImagen(); portadaFallo = false;
  /* ⚠️ Se borra el hueco con las medidas FIJAS del modo portada, no con
   *    `PORTADA_Y0`: esa variable la deja el último dibujo, y si venías
   *    del selector todavía vale 34 en vez de 28. */
  tft.fillRect(0, PORTADA_ALTO_BARRA, 240, 240 - PORTADA_ALTO_BARRA, TFT_BLACK);
  portadaVisible = false;
}

/* ------------------------------------------------------------
 *  MODO 4 — elegir playlist
 * ------------------------------------------------------------
 *  La imagen va más pequeña que en el modo portada (150 en vez de
 *  212) para dejar sitio abajo al nombre y al "2 de 3". Es el mismo
 *  decodificador: solo cambia el encuadre.
 * ------------------------------------------------------------ */
const int LISTA_LADO = 150;
const int LISTA_Y0   = 34;
const int LISTA_Y_NOMBRE = LISTA_Y0 + LISTA_LADO + 12;   // 196

/* Al moverse por la lista NO se baja la imagen enseguida.
 *
 * Cada imagen cuesta ~1 s de red y ~300 ms de dibujo. Si se bajara en
 * cada empujón, recorrer tres playlists tardaría cuatro segundos y el
 * aparato se sentiría atascado. Se espera a que la selección se quede
 * quieta; el nombre, en cambio, aparece al instante.
 *
 * 🔑 Lo barato va inmediato y lo caro espera a que se note que hace
 *    falta. Es la misma idea del spinner: separar "te oí" de "ya está". */
unsigned long listaMovidaEn = 0;
const unsigned long MS_ASENTARSE = 450;

void moverSeleccion() {
  listaMovidaEn = millis();
  repintarSelector = true;
}

void pintarSelectorListas() {
  tft.fillRect(0, PORTADA_ALTO_BARRA, 240, 240 - PORTADA_ALTO_BARRA, TFT_BLACK);
  portadaVisible = false;
  olvidarImagen();                // que se vuelva a pedir la que toque

  // Mientras se piden las playlists no se escribe "buscando": lo dice
  // el spinner, que además se mueve — un texto quieto no distingue
  // "esperando" de "colgado".
  if (nListas == 0) {
    if (listasPedidas) {
      escribir("sin playlists", 110, false, GRIS_OSCURO);
      escribir("revisa los nombres", 136, false, GRIS_OSCURO);
    }
    return;                       // si aún no se han pedido, manda el spinner
  }

  // El nombre y la posición se pintan YA, sin esperar a la imagen: al
  // moverte por la lista lo que importa es saber dónde estás.
  String l[3]; bool g;
  int n = partirConReserva(aLatin1(listas[listaSel].nombre), ANCHO, l, 2, g);
  int y = LISTA_Y_NOMBRE;
  for (int i = 0; i < n; i++) { escribirMixtoCentrado(l[i], y, TFT_WHITE); y += ALTO_CHICA; }

  if (nListas > 1) {
    char pos[16];
    snprintf(pos, sizeof(pos), "%d de %d", listaSel + 1, nListas);
    escribir(pos, 218, false, GRIS_OSCURO);
  }
}

/* Repinta lo fijo del modo actual. Se llama al cambiar de modo y al
 * cambiar de canción.
 *
 * ⚠️ HAY QUE LLAMARLA CON EL TURNO DE DATOS COGIDO. Lee `tituloAct`,
 *    `artistaAct` y `listas[]`, que los escribe el núcleo 0.
 *
 * ⚠️ Y JUSTO POR ESO LA PORTADA NO SE DIBUJA AQUÍ.
 * ------------------------------------------------------------
 *  `pintarPortada()` pide el turno de datos por su cuenta, a través de
 *  `imagenQueTocaAhora()`. Llamarla desde aquí significaba pedir el
 *  turno **dos veces desde el mismo núcleo** — y el turno se crea con
 *  `xSemaphoreCreateMutex()`, que NO es reentrante: el núcleo se queda
 *  esperándose a sí mismo, para siempre, porque la espera es
 *  `portMAX_DELAY`.
 *
 *  El aparato entero se colgaba en silencio: sin reinicio, sin
 *  `Guru Meditation`, sin nada. Solo pasaba al entrar en el modo
 *  portada teniendo ya una imagen en memoria, por eso tardó en salir.
 *  Diagnosticado el 2026-08-30 poniéndole un plazo al turno; ver
 *  `5-investigacion/cuelgue-por-turno-anidado.md`.
 *
 *  🔑 Una función que pide un turno no se puede llamar desde dentro de
 *     ese turno. Y lo difícil es que aquí **no se veía**: la petición
 *     estaba dos llamadas más abajo.
 * ------------------------------------------------------------ */
void pintarContenidoModo() {
  switch (modo) {
    case MODO_SIN_LETRA: cabeceraGrande(tituloAct, artistaAct); break;
    case MODO_CON_LETRA: cabecera(tituloAct.c_str(), artistaAct.c_str()); break;
    case MODO_PORTADA:   break;          // se dibuja FUERA del turno
    default:             pintarSelectorListas(); break;
  }
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
    if (!esSilencio(letras.texto(i))) return i;
  return -1;
}

int anteriorConTexto(int desde) {
  for (int i = desde; i >= 0; i--)
    if (!esSilencio(letras.texto(i))) return i;
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

/* ============================================================
 *  Transición entre líneas
 * ============================================================
 *  La frase nueva no aparece de golpe: se dibuja varias veces,
 *  cada una un poco más clara, hasta llegar al blanco.
 *
 *  El truco está en que NO hace falta borrar entre paso y paso.
 *  Las letras son de un solo bit (cada píxel está o no está), y
 *  cada pasada solo ACLARA los mismos píxeles. Sin borrado no hay
 *  parpadeo, y no cuesta nada de memoria extra.
 * ============================================================ */
const int PASOS_FUNDIDO = 6;
const int MS_POR_PASO   = 28;
const uint8_t NIVELES[PASOS_FUNDIDO] = { 70, 110, 150, 190, 225, 255 };

int   pasoFundido = PASOS_FUNDIDO;      // terminado
unsigned long ultimoPaso = 0;

// Guardamos cómo quedó repartida la frase para poder repintarla sin
// recalcular nada en cada paso del fundido.
String lineasFrase[6];
int    numLineasFrase = 0, yFrase = 0, altoFrase = 0;
bool   fraseGrande = true;

inline uint16_t grisNivel(uint8_t v) {
  return ((v >> 3) << 11) | ((v >> 2) << 5) | (v >> 3);
}

void pintarFrase(uint16_t color) {
  for (int i = 0; i < numLineasFrase; i++)
    escribir(lineasFrase[i], yFrase + i * altoFrase, fraseGrande, color);
}

// Da un paso del fundido. Se llama desde el bucle de dibujo.
void avanzarFundido() {
  if (pasoFundido >= PASOS_FUNDIDO || numLineasFrase == 0) return;
  if (millis() - ultimoPaso < MS_POR_PASO) return;
  ultimoPaso = millis();
  pintarFrase(grisNivel(NIVELES[pasoFundido]));
  pasoFundido++;
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

  bool enSilencio = (idx < 0) || esSilencio(letras.texto(idx));

  // ------------------------------------------------------------------
  //  SILENCIO: intro, solo instrumental o pausa entre estrofas.
  //  Se dibujan las notas EN BLANCO y CENTRADAS (ocupan el sitio de la
  //  frase que se cantaría), y debajo en gris la frase que viene. Así se
  //  puede seguir la canción antes de que empiece a cantarse, y queda
  //  claro que el aparato está vivo.
  // ------------------------------------------------------------------
  if (enSilencio) {
    numLineasFrase = 0; pasoFundido = PASOS_FUNDIDO;
    int sig = siguienteConTexto(idx < 0 ? 0 : idx + 1);

    String s2[6]; bool g2 = false; int ns = 0;
    if (sig >= 0) ns = partirConReserva(String(letras.texto(sig)), ANCHO, s2, 2, g2);

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
  /* La zona de la letra mide 112 px. Con la fuente grande (28 px) caben
   * 4 renglones justos; con la chica (20 px), 5.
   *
   * Antes se permitían 3 y 4, y frases largas como "And I know you're
   * really mad at me, but you're on your way to some kind of life" no
   * cabían ni encogiendo, así que se recortaban con "..." — perdiendo
   * el final de la frase, que muchas veces es lo que le da sentido. */
  numLineasFrase = partirConReserva(String(letras.texto(idx)), ANCHO,
                                    lineasFrase, 4, fraseGrande);

  // Aviso solo si una frase no cupo ni encogiendo: sirve para saber si
  // algún día hay que subir los límites, sin llenar el log el resto del
  // tiempo.
  if (numLineasFrase > 0 && lineasFrase[numLineasFrase-1].endsWith("...")) {
    Serial.printf("[Frase] ⚠️ recortada (%d car): \"%s\"\n",
                  strlen(letras.texto(idx)), letras.texto(idx));
  }

  int n = numLineasFrase;
  bool grande = fraseGrande;
  String* c = lineasFrase;

  int alto = grande ? ALTO_GRANDE : ALTO_CHICA;
  altoFrase = alto;
  int bloque = n * alto;
  int y = Y_LETRA + (ALTO_LETRA - bloque) / 2;
  yFrase = y;

  // El contexto solo se dibuja si sobra sitio: cuando la frase actual es
  // larga, ella manda.
  bool hayHueco = (bloque + 38) <= ALTO_LETRA;

  if (hayHueco) {
    int ant = anteriorConTexto(idx - 1);
    if (ant >= 0) {
      String a[6]; bool ga;
      partirConReserva(String(letras.texto(ant)), ANCHO, a, 1, ga);
      escribir(a[0], Y_LETRA, false, GRIS_OSCURO);
    }
  }

  // Arranca el fundido: se pinta en gris oscuro y el bucle de dibujo la
  // irá aclarando hasta el blanco.
  pasoFundido = 0;
  ultimoPaso = 0;
  pintarFrase(grisNivel(NIVELES[0]));

  if (hayHueco) {
    int sig = siguienteConTexto(idx + 1);
    if (sig >= 0) {
      String sg[6]; bool gs;
      partirConReserva(String(letras.texto(sig)), ANCHO, sg, 1, gs);
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

  // El texto solo se repinta cuando CAMBIA de segundo. Antes se borraba
  // y se volvía a dibujar 10 veces por segundo, y ese borrado constante
  // se percibía como un parpadeo.
  static String tiempoPintado;
  char a[12], b[12];
  snprintf(a, sizeof(a), "%ld:%02ld", p / 60000, (p / 1000) % 60);
  snprintf(b, sizeof(b), "%ld:%02ld", duracionMs / 60000, (duracionMs / 1000) % 60);
  String ahoraTxt = String(a) + "/" + String(b);
  if (ahoraTxt == tiempoPintado) return;
  tiempoPintado = ahoraTxt;

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

/* ============================================================
 *  La pantalla de carga: Snoopy bailando
 * ============================================================
 *  Sustituye al "Tamagotchi / conectando al WiFi..." de antes.
 *
 *  Fondo BLANCO y tinta negra, al revés que el resto del aparato.
 *  Es deliberado: Snoopy está dibujado como línea negra sobre papel,
 *  y invertirlo le vuelve la oreja blanca — se ve como un negativo.
 *
 *  Los cuadros viven en `snoopy.h`, a 1 bit por píxel. En color
 *  serían 56 KB cada uno y no cabrían siete; así son 3,4 KB. No se
 *  pierde nada porque el dibujo YA es de dos tonos.
 *  Ver `tools/gen-animacion.py`.
 *
 *  🔑 Y de paso deja de ser un adorno: si la animación se para, se ve
 *     al instante que el arranque se atascó. Un texto fijo no dice
 *     nada — no se distingue "esperando" de "colgado".
 * ============================================================ */
const int SNOOPY_X = (240 - SNOOPY_ANCHO) / 2;
const int SNOOPY_Y = 8;
const int Y_TEXTO_CARGA = 206;

int cuadroSnoopy = 0;
unsigned long ultimoCuadroSnoopy = 0;

/* ¿Seguimos en la pantalla de carga?
 *
 * ⚠️ No basta con animar durante `setup()`. Cuando `setup()` termina,
 *    el núcleo 0 todavía está haciendo su PRIMERA consulta a Spotify,
 *    y eso es casi un segundo más con Snoopy congelado en pantalla.
 *    Un fotograma quieto se lee como "se colgó" — que es justo lo que
 *    esta animación existe para desmentir.
 *
 * 🔑 La animación no acompaña al arranque: acompaña a la ESPERA. Y la
 *    espera no termina donde termina `setup()`, sino cuando hay algo
 *    que enseñar. */
bool enCarga = true;

/* ⚠️ No se usa `tft.drawBitmap()`. El de la librería llama a
 *    `drawPixel` una vez por punto, y aquí son 27.968 puntos: cada
 *    uno abre su propia ventana de direcciones en el bus. Tardaría
 *    más que los 110 ms que dura el cuadro, y la animación iría a
 *    tirones justo cuando debe demostrar que el aparato está vivo.
 *
 *    Volcando fila a fila se manda una tirada de 152 píxeles por
 *    transacción: ~11 ms el cuadro entero.
 *
 * 🔑 Dibujar punto por punto no cuesta por los puntos: cuesta por
 *    abrir el bus una vez por cada uno. */
void dibujarCuadroSnoopy(int n) {
  const int bytes_fila = (SNOOPY_ANCHO + 7) / 8;
  const uint8_t* cuadro = SNOOPY[n];
  uint16_t linea[SNOOPY_ANCHO];

  tft.startWrite();
  tft.setAddrWindow(SNOOPY_X, SNOOPY_Y, SNOOPY_ANCHO, SNOOPY_ALTO);
  for (int y = 0; y < SNOOPY_ALTO; y++) {
    const uint8_t* f = cuadro + y * bytes_fila;
    for (int x = 0; x < SNOOPY_ANCHO; x++)
      linea[x] = (f[x >> 3] & (0x80 >> (x & 7))) ? TFT_BLACK : TFT_WHITE;
    tft.pushColors(linea, SNOOPY_ANCHO, true);
  }
  tft.endWrite();
}

/* Cambia SOLO el renglón de abajo. Los tres pasos del arranque
 * comparten pantalla; repintarla entera en cada uno haría parpadear a
 * Snoopy tres veces sin ninguna razón. */
void textoCarga(const char* texto) {
  tft.fillRect(0, Y_TEXTO_CARGA, 240, ALTO_CHICA + 6, TFT_WHITE);
  usarFuente(false);
  tft.setTextColor(GRIS_OSCURO, TFT_WHITE);   // ⚠️ fondo blanco, no negro
  tft.setTextDatum(TC_DATUM);
  tft.drawString(texto, 120, Y_TEXTO_CARGA);
}

/* El token inicial se pide desde el NÚCLEO 0, no desde el del dibujo.
 *
 * ⚠️ La primera versión lo pedía dentro de `setup()`, que corre en el
 *    núcleo 1. `renovarToken()` abre una conexión cifrada y bloquea
 *    1-2 s, así que Snoopy se quedaba congelado justo antes de abrir
 *    la pantalla — y se veía como si el aparato se hubiera colgado.
 *
 *    Y era una excepción sin motivo a la regla que ordena todo este
 *    firmware: el núcleo 0 habla con la red, el 1 dibuja, y nunca al
 *    revés. El arranque no tenía por qué saltársela.
 *
 * 🔑 Una animación que existe para decir "sigo vivo" no puede pararse
 *    por culpa de la espera que venía a disimular.
 *
 * La tarea se borra sola al acabar: solo hace falta una vez. */
volatile bool tokenInicialListo = false;
bool renovarToken();            // vive más abajo, con las demás peticiones

void tareaTokenInicial(void*) {
  renovarToken();
  tokenInicialListo = true;
  vTaskDelete(NULL);
}

void pantallaCarga(const char* texto) {
  tft.fillScreen(TFT_WHITE);
  estadoPintado = ""; lineaPintada = -2;
  cuadroSnoopy = 0; ultimoCuadroSnoopy = millis();
  dibujarCuadroSnoopy(0);
  textoCarga(texto);
}

/* Se llama DENTRO de las esperas del arranque, no en vez de ellas.
 * Conectarse al WiFi y pedir la hora bloquean; lo que se hace es
 * esperar en trozos cortos y aprovechar cada uno para avanzar. */
void animarCarga() {
  if (millis() - ultimoCuadroSnoopy < (unsigned long)SNOOPY_MS) return;
  ultimoCuadroSnoopy = millis();
  cuadroSnoopy = (cuadroSnoopy + 1) % SNOOPY_CUADROS;
  dibujarCuadroSnoopy(cuadroSnoopy);
}

/* ============================================================
 *  La pantalla oculta (RF-15) y su sorpresa (RF-16)
 * ============================================================
 *  El encargo del RF-16 no es "que se vea bonito": es que
 *  encontrarla sea **un regalo y no un susto**. Quien reciba el
 *  aparato no sabe programar. Si de pronto se le llena la pantalla
 *  de números sin explicación, la conclusión razonable es "lo rompí".
 *
 *  Por eso el orden: primero la sorpresa (esto ha pasado a
 *  propósito), luego el texto que tranquiliza, y los números al
 *  final. Los datos técnicos son lo MENOS importante de esta
 *  pantalla aunque sean su contenido.
 * ============================================================ */
const uint16_t ROSA_CORAZON = 0xFB56;

// El corazón de `iconos.h` es de tamaño fijo (11 px) porque va dentro
// de una línea de texto; aquí hace falta que crezca.
void corazonGrande(int cx, int cy, int s, uint16_t c) {
  tft.fillCircle(cx - s / 2, cy - s / 2, s / 2 + 1, c);
  tft.fillCircle(cx + s / 2, cy - s / 2, s / 2 + 1, c);
  tft.fillTriangle(cx - s, cy - s / 3, cx + s, cy - s / 3, cx, cy + s, c);
}

/* Tres ondas desde el centro y un corazón que crece.
 *
 * ⚠️ Con círculos, NO con una imagen. Una animación en mapa de bits
 *    necesitaría decenas de KB, y este aparato ya demostró que no le
 *    sobran (`portada-cache-descartada.md`). Un círculo cuesta cero.
 *
 * El anillo se borra a su paso en vez de limpiar la pantalla entera
 * en cada paso: 240x240 son 57.600 píxeles por fotograma y se vería
 * parpadear; un anillo son unos cientos. */
void animacionSorpresa() {
  tft.fillScreen(TFT_BLACK);
  for (int onda = 0; onda < 3; onda++) {
    for (int r = 6; r < 168; r += 9) {
      tft.drawCircle(120, 120, r, VERDE_SPOTIFY);
      if (r > 15) tft.drawCircle(120, 120, r - 9, TFT_BLACK);
      delay(4);
    }
    tft.drawCircle(120, 120, 159, TFT_BLACK);
    tft.drawCircle(120, 120, 168, TFT_BLACK);
  }
  for (int s = 3; s <= 26; s += 2) { corazonGrande(120, 116, s, ROSA_CORAZON); delay(16); }
  delay(240);
}

// `123456` -> `1h 23m`. Sin segundos: cambian todo el rato y distraen.
String tiempoEncendido() {
  unsigned long s = millis() / 1000;
  unsigned long h = s / 3600, m = (s % 3600) / 60;
  if (h > 0) return String(h) + "h " + String(m) + "m";
  if (m > 0) return String(m) + "m";
  return String(s) + "s";
}

/* ⚠️ El valor se borra con un rectángulo antes de escribirlo. El fondo
 *    del texto NO basta cuando el valor ENCOGE: al pasar de `178 KB` a
 *    `99 KB`, alineado a la derecha, el sobrante del número viejo se
 *    queda por la izquierda y se lee `8199 KB`. */
const int X_VALOR = 118;

void filaValor(const String& valor, int y) {
  tft.fillRect(X_VALOR, y, 240 - MARGEN - X_VALOR, ALTO_CHICA, TFT_BLACK);
  usarFuente(false);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(valor, 240 - MARGEN, y);
  tft.setTextDatum(TC_DATUM);
}

void filaEstado(const char* etiqueta, const String& valor, int y) {
  usarFuente(false);
  tft.setTextColor(GRIS, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(etiqueta, MARGEN, y);
  tft.setTextDatum(TC_DATUM);
  filaValor(valor, y);
}

/* ⚠️ La última fila acaba en 236 de 240. No caben más: si se añade un
 *    dato hay que quitar otro, no apretar el espaciado. */
const int Y_TITULO_OCULTA = 10;
const int Y_CALMA_1 = 42, Y_CALMA_2 = 62;
const int Y_LINEA_ARRIBA = 88;
const int Y_WIFI = 96, Y_SENAL = 120, Y_LIBRE = 144,
          Y_BLOQUE = 168, Y_VERSION = 192, Y_ENCENDIDO = 216;

void pintarPantallaOculta() {
  tft.fillScreen(TFT_BLACK);
  estadoPintado = ""; lineaPintada = -2;

  /* ⚠️ Todo el texto fijo de aquí es ASCII a propósito. `escribir()` va
   *    directo a `drawString`, que NO pasa por `aLatin1()`. La fuente
   *    está indexada por Latin-1, así que una tilde, una ñ o un `·`
   *    llegan como DOS bytes y salen dos glifos basura.
   *
   * 🔑 Dos textos en la misma pantalla pueden necesitar pinceles
   *    distintos: el que viene de fuera y el escrito en el código no
   *    llegan igual. */
  escribir("Lo encontraste", Y_TITULO_OCULTA, true, VERDE_SPOTIFY);
  escribir("no rompiste nada", Y_CALMA_1, false, TFT_WHITE);
  escribir("un clic y vuelves", Y_CALMA_2, false, GRIS);
  tft.drawFastHLine(MARGEN, Y_LINEA_ARRIBA, 240 - 2 * MARGEN, GRIS_LINEA);

  /* La RAM va en dos números porque son dos cosas distintas:
   *   - libre  = cuánta queda en total
   *   - bloque = la mayor pieza SEGUIDA que se puede pedir
   * Se puede tener 150 KB libres y no poder guardar nada de 50 si
   * están troceados. El bloque es el que avisa de verdad. */
  filaEstado("WiFi",      WiFi.isConnected() ? WiFi.SSID() : String("sin conexion"), Y_WIFI);
  filaEstado("Senal",     String(WiFi.RSSI()) + " dBm",                             Y_SENAL);
  filaEstado("Libre",     String(ESP.getFreeHeap() / 1024) + " KB",                 Y_LIBRE);
  filaEstado("Bloque",    String(ESP.getMaxAllocHeap() / 1024) + " KB",            Y_BLOQUE);
  filaEstado("Version",   String(FW_VERSION),                                     Y_VERSION);
  filaEstado("Encendido", tiempoEncendido(),                                    Y_ENCENDIDO);

  // Medir en vez de suponer que el texto cabe: `drawString` centrado no
  // avisa, recorta por los dos lados.
  static bool medido = false;
  if (!medido) {
    medido = true;
    usarFuente(true); int wt = tft.textWidth("Lo encontraste"); usarFuente(false);
    Serial.printf("[Oculta] ancho titulo %d px, calma %d/%d px (limite 240)\n",
                  wt, tft.textWidth("no rompiste nada"), tft.textWidth("un clic y vuelves"));
  }
}

/* ⚠️ Volver a llamar a `pintarPantallaOculta()` cada segundo sería lo
 *    cómodo y estaría mal: limpia y redibuja la pantalla entera, así
 *    que se vería parpadear. Es el mismo fallo que ya costó una sesión
 *    con la portada, donde el parpadeo se achacó a un cable flojo.
 *
 * 🔑 Repintar de más no es solo lento: se VE. */
void refrescarValoresOcultos() {
  filaValor(String(WiFi.RSSI()) + " dBm",                 Y_SENAL);
  filaValor(String(ESP.getFreeHeap() / 1024) + " KB",     Y_LIBRE);
  filaValor(String(ESP.getMaxAllocHeap() / 1024) + " KB", Y_BLOQUE);
  filaValor(tiempoEncendido(),                        Y_ENCENDIDO);
}

/* El adiós: un corazón y `Te quiero!!` (RF-16). Deja la pantalla en
 * negro y la luz APAGADA — la vuelve a encender quien llama. */
void despedida() {
  fundirBrillo(BRILLO, 0, 110);
  tft.fillScreen(TFT_BLACK);
  corazonGrande(120, 96, 26, ROSA_CORAZON);
  escribir("Te quiero!!", 140, true, TFT_WHITE);
  fundirBrillo(0, BRILLO, 200);
  delay(1600);
  fundirBrillo(BRILLO, 0, 110);
}

/* Volver de la pantalla oculta: repintar TODO y encender la luz.
 *
 * ⚠️ Esto NO puede delegarse en `repintarModo`, que era lo obvio y fue
 *    el fallo del 2026-08-30. Ese bloque vive dentro de
 *    `if (claveActual.length() && claveActual[0] != '_')`: si no hay
 *    canción sonando —en pausa, sin Spotify, en silencio— NO CORRE. Y
 *    como la despedida deja la luz apagada, la pantalla se quedaba
 *    negra para siempre y el aparato parecía muerto.
 *
 * 🔑 Apagar en un sitio y encender en otro solo vale si el segundo
 *    corre SIEMPRE. Si tiene condiciones, no vale.
 *
 * Por eso aquí se cubren también los estados especiales: es la única
 * forma de que la luz vuelva pase lo que pase. */
void volverDeLaPantallaOculta() {
  bool hayCancion = false;
  CON_DATOS({
    if (claveActual == "__sinred__")
      pantallaMensaje("Sin conexion", "reintentando...", TFT_RED);
    else if (claveActual == "__sinspotify__")
      pantallaMensaje("Sin Spotify", "reintentando...", NARANJA_AVISO);
    else if (claveActual == "__nada__")
      pantallaMensaje("En silencio", "no hay nada sonando", GRIS);
    else {
      tft.fillScreen(TFT_BLACK);
      estadoPintado = ""; lineaPintada = -2;
      pintarContenidoModo();
      if (modo == MODO_CON_LETRA) {
        lineaPintada = (letras.estado == LETRA_OK) ? letras.lineaEn(progresoAhora()) : -1;
        zonaLetra(lineaPintada);
      }
      hayCancion = true;
    }
  });

  // FUERA del turno: `pintarPortada()` lo pide por su cuenta.
  if (hayCancion && modo == MODO_PORTADA) pintarPortada();
  if (hayCancion) {
    barraEstado(sonando, sonando ? VERDE_SPOTIFY : GRIS);
    if (modo == MODO_SIN_LETRA || modo == MODO_CON_LETRA) barraProgreso();
  }
  fundirBrillo(0, BRILLO, 200);
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
  // ⚠️ Se guarda el instante de caducidad, pero NUNCA se compara con `>`
  // directamente: ver `tokenCaducado()`. `millis()` da la vuelta a los
  // 49,7 días y una comparación de valor absoluto se rompe ese día.
  tokenCaducaEn = millis() + (dur - 300) * 1000UL;
  Serial.printf("[Token] ✅ renovado (%ld s)\n", dur);
  return true;
}

// ============================================================
//  De dónde sale la música (playlist, álbum, artista)
// ============================================================
//  El endpoint de "reproduciendo ahora" trae un campo `context` con
//  el ORIGEN de la reproducción, pero solo su identificador:
//
//      "context": { "type": "playlist",
//                   "uri":  "spotify:playlist:37i9dQZF1DX..." }
//
//  El NOMBRE no viene. Hay que pedirlo aparte, una sola vez por
//  contexto — y se guarda, porque mientras sigas en la misma playlist
//  no cambia.
// Pide un nombre a un endpoint de Spotify quedándose SOLO con el campo
// "name". El filtro evita cargar la respuesta entera, que en el caso de
// un artista trae imágenes, géneros y estadísticas que no necesitamos.
String pedirNombre(const String& url) {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, url)) return "";
  https.addHeader("Authorization", "Bearer " + accessToken);
  int cod = https.GET();
  String nombre;
  if (cod == 200) {
    JsonDocument filtro; filtro["name"] = true; filtro["description"] = true;
    JsonDocument doc;
    if (!deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filtro))) {
      nombre = aLatin1(String((const char*)(doc["name"] | "")));
      descContexto = limpiarHTML(String((const char*)(doc["description"] | "")));
    }
  }
  https.end();
  Serial.printf("[Contexto] HTTP %d -> \"%s\"\n", cod, nombre.c_str());
  return nombre;
}

void resolverContexto(const char* uri, const char* tipo,
                      const char* album, const char* artista) {
  String u = String(uri ? uri : "");

  if (u.length() == 0) {
    // Sin contexto: la canción se puso suelta. Mostrar el álbum es mucho
    // más útil que un "Reproduciendo" que no dice nada.
    String alt = aLatin1(String(album ? album : ""));
    if (nombreContexto != alt || contextoUri != "") {
      contextoUri = ""; descContexto = ""; nombreContexto = alt; forzarBarra = true;
    }
    return;
  }
  if (contextoUri == u) return;           // ya resuelto, no repetir la petición

  contextoUri = u;
  nombreContexto = "";
  descContexto = "";
  forzarBarra = true;                     // forzar repintado de la barra

  int corte = u.lastIndexOf(':');
  String id = (corte >= 0) ? u.substring(corte + 1) : "";
  String t = String(tipo ? tipo : "");

  if (t == "playlist" && id.length()) {
    // Solo las playlists tienen descripción, y solo su endpoint acepta
    // ?fields=. Si es privada hace falta el permiso playlist-read-private.
    nombreContexto = pedirNombre(String(URL_PLAYLIST) + id + "?fields=name,description");
    if (!nombreContexto.length()) nombreContexto = "Playlist";

  } else if (t == "album") {
    // El nombre del álbum YA viene en la respuesta de "reproduciendo
    // ahora". Pedirlo otra vez sería una petición desperdiciada.
    nombreContexto = aLatin1(String(album ? album : "Album"));

  } else if (t == "artist" && id.length()) {
    // Aquí sí hace falta preguntarlo: el contexto puede ser un artista
    // distinto del primero de la canción (colaboraciones, recopilatorios).
    nombreContexto = pedirNombre(String(URL_ARTISTA) + id);
    descContexto = "";                    // los artistas no tienen descripción
    if (!nombreContexto.length()) nombreContexto = aLatin1(String(artista ? artista : "Artista"));

  } else if (t == "collection") {
    nombreContexto = "Tus me gusta";

  } else if (t == "show" || t == "episode") {
    nombreContexto = "Podcast";

  } else {
    // Tipo desconocido o nuevo: el álbum siempre es una respuesta útil.
    nombreContexto = aLatin1(String(album ? album : ""));
  }
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
// Pide la cola de reproducción y ENCOLA la búsqueda de la letra de la
// siguiente canción. Una sola petición: el resto lo hace la máquina de
// pasos, igual que para la canción actual.
/* ------------------------------------------------------------
 *  Cumplir una orden del joystick (RF-08, RF-09)
 * ------------------------------------------------------------
 *  ⚠️ Se llama SIEMPRE desde dentro de `CON_RED`. Abre una conexión
 *     TLS como cualquier otra petición, y dos handshakes a la vez
 *     reinician la placa.
 *
 *  Spotify responde 204 (hecho, sin contenido) y no 200. Un 404 aquí
 *  significa "no hay ningún aparato reproduciendo": no es un fallo
 *  del aparato, es que no hay nada que controlar.
 * ------------------------------------------------------------ */
bool ejecutarOrden(Orden o) {
  const char* url;
  const char* metodo;
  bool sonandoAhora = false;
  CON_DATOS({ sonandoAhora = sonando; });
  switch (o) {
    case ORDEN_PLAYPAUSA: url = sonandoAhora ? URL_PAUSA : URL_REANUDAR; metodo = "PUT"; break;
    case ORDEN_SIGUIENTE: url = URL_SIGUIENTE; metodo = "POST"; break;
    case ORDEN_ANTERIOR:  url = URL_ANTERIOR;  metodo = "POST"; break;
    case ORDEN_PONER_LISTA:
      // Esta no encaja en el patrón de las otras: lleva cuerpo.
      return (nListas > 0) ? reproducirLista(listas[listaSel].id) : false;
    default: return false;
  }

  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, url)) return false;
  https.addHeader("Authorization", "Bearer " + accessToken);
  // Sin `Content-Length: 0` explícito, Spotify rechaza el PUT sin cuerpo.
  https.addHeader("Content-Length", "0");
  int cod = (strcmp(metodo, "PUT") == 0) ? https.PUT("") : https.POST("");
  https.end();

  bool ok = (cod == 200 || cod == 202 || cod == 204);
  // Se registra SIEMPRE, no solo el fallo. Sin la línea del acierto no
  // se puede distinguir "se envió y funcionó" de "nunca se envió",
  // y eso deja una investigación sin salida.
  Serial.printf("[Orden] %s %s -> HTTP %d %s\n",
                metodo, nombreOrden(o), cod, ok ? "✅" : "❌");
  if (cod == 404) Serial.println("[Orden] ⚠️ no hay ningun aparato reproduciendo");
  return ok;
}

/* ------------------------------------------------------------
 *  Bajar la portada a memoria (NO la dibuja: eso es del núcleo 1)
 * ------------------------------------------------------------
 *  ⚠️ Se llama desde dentro de `CON_RED`, como toda petición.
 *
 *  Se reserva el buffer justo antes y se libera en cuanto la
 *  pantalla lo ha usado. Son ~41 KB: tenerlos ocupados de más
 *  aprieta el margen del hilo de letras, que necesita ~50 KB para
 *  su propia conexión.
 * ------------------------------------------------------------ */
bool bajarPortada(const String& url) {
  if (bufPortada) { free(bufPortada); bufPortada = nullptr; lenPortada = 0; }

  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, url)) return false;
  int cod = https.GET();
  if (cod != 200) { https.end(); Serial.printf("[Portada] HTTP %d\n", cod); return false; }

  int total = https.getSize();
  // 60 KB de tope: las de 300 px rondan los 41 KB. Si llega algo mucho
  // mayor es que no es lo que esperábamos, y más vale no reservarlo.
  if (total <= 0 || total > 60000) {
    https.end(); Serial.printf("[Portada] tamano raro: %d B\n", total); return false;
  }

  /* ⏱ El borde de la memoria: cuanto se pide y cuanto hay.
   * Interesa el BLOQUE, no el libre: se puede tener 125 KB libres y no
   * poder reservar 55 KB si estan troceados. */
  Serial.printf("[Portada] pidiendo %d B · libre %lu B · bloque %lu B\n",
                total, (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMaxAllocHeap());
  uint8_t* buf = (uint8_t*)malloc(total);
  if (!buf) { https.end(); Serial.println("[Portada] ❌ sin memoria"); return false; }

  WiFiClient* flujo = https.getStreamPtr();
  int leidos = 0;
  unsigned long limite = millis() + 8000;
  while (leidos < total && (long)(millis() - limite) < 0) {
    int hay = flujo->available();
    if (hay > 0) leidos += flujo->readBytes(buf + leidos, min(hay, total - leidos));
    else delay(5);
  }
  https.end();

  if (leidos != total) {
    free(buf);
    Serial.printf("[Portada] incompleta: %d de %d B\n", leidos, total);
    return false;
  }

  /* ⚠️ El puntero y la URL a la que pertenece se publican EN EL MISMO
   *    bloque protegido, no uno detrás del otro.
   *
   *    La primera versión asignaba `bufPortada` aquí y `portadaEnBuffer`
   *    en quien llamaba, unas líneas después. El núcleo 1 se coló justo
   *    en medio: vio una imagen cuya URL todavía era la anterior, dedujo
   *    "esta es de otra canción" y la liberó al instante. Capturado en el
   *    registro el 2026-08-30, con los dos sucesos en el mismo segundo.
   *
   *    🔑 Si dos datos solo tienen sentido juntos, publicarlos por
   *       separado es dejar una rendija por la que se cuela el otro
   *       núcleo. Y esa rendija dura microsegundos: falla una vez de
   *       cada muchas, que es la peor clase de fallo. */
  CON_DATOS({ bufPortada = buf; lenPortada = total; portadaEnBuffer = url; });
  Serial.printf("[Portada] bajada %d B · RAM %lu B\n",
                total, (unsigned long)ESP.getFreeHeap());
  return true;
}

/* ------------------------------------------------------------
 *  Traer las playlists elegidas (RF-14)
 * ------------------------------------------------------------
 *  ⚠️ Desde dentro de `CON_RED`, como toda petición.
 *
 *  Se pide una sola vez por arranque: tus playlists no cambian de
 *  nombre mientras el aparato está encendido, y la respuesta trae 50
 *  entradas con todos sus datos.
 * ------------------------------------------------------------ */
bool pedirListas() {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_MIS_LISTAS)) return false;
  https.addHeader("Authorization", "Bearer " + accessToken);
  int cod = https.GET();
  if (cod != 200) { https.end(); Serial.printf("[Listas] HTTP %d\n", cod); return false; }

  // 50 playlists con todo su detalle son cientos de KB. El filtro deja
  // pasar cuatro campos por entrada: unos pocos cientos de bytes.
  JsonDocument filtro;
  filtro["items"][0]["name"] = true;
  filtro["items"][0]["id"] = true;
  filtro["items"][0]["images"][0]["url"] = true;
  filtro["items"][0]["images"][0]["width"] = true;

  JsonDocument doc;
  DeserializationError err;
  if (https.getSize() < 0) {
    LectorChunked lector(&cli);
    err = deserializeJson(doc, lector, DeserializationOption::Filter(filtro));
  } else {
    err = deserializeJson(doc, https.getStream(), DeserializationOption::Filter(filtro));
  }
  https.end();
  if (err) { Serial.printf("[Listas] JSON: %s\n", err.c_str()); return false; }

  /* Se recorre la lista de ELEGIDAS por fuera, no la de Spotify: así el
   * orden en pantalla es el que se escribió en el código y no el que
   * decida la API. */
  int n = 0;
  for (int i = 0; i < N_LISTAS_ELEGIDAS && n < MAX_LISTAS; i++) {
    for (JsonObject p : doc["items"].as<JsonArray>()) {
      String nom = p["name"] | "";
      if (!nom.startsWith(LISTAS_ELEGIDAS[i])) continue;
      listas[n].nombre = nom;
      listas[n].id     = String((const char*)(p["id"] | ""));
      /* La de 300 px si existe; si no, la única que haya.
       *
       * Medido: `for you` ofrece 640 (82 KB), 300 (26 KB) y 60 (2 KB),
       * mientras que `Alo 💜` trae un mosaico suelto de 30 KB sin
       * dimensiones. Coger a ciegas la primera —la mayor— traía 82 KB
       * y se rechazaba por tamaño. */
      String img;
      for (JsonObject im : p["images"].as<JsonArray>())
        if ((int)(im["width"] | 0) == ANCHO_QUE_PEDIMOS) { img = im["url"] | ""; break; }
      if (!img.length()) img = String((const char*)(p["images"][0]["url"] | ""));
      listas[n].imagen = img;
      Serial.printf("[Listas] %d: %s\n", n + 1, nom.c_str());
      n++;
      break;                       // la primera que case con ese nombre
    }
  }
  nListas = n;
  if (listaSel >= nListas) listaSel = 0;   // por si ahora hay menos que antes
  Serial.printf("[Listas] %d de %d encontradas · RAM %lu B\n",
                n, N_LISTAS_ELEGIDAS, (unsigned long)ESP.getFreeHeap());
  return true;
}

/* Poner a sonar una playlist entera. A diferencia de pausar o saltar,
 * esta SÍ lleva cuerpo: hay que decirle cuál. */
bool reproducirLista(const String& id) {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_REANUDAR)) return false;
  https.addHeader("Authorization", "Bearer " + accessToken);
  https.addHeader("Content-Type", "application/json");
  String cuerpo = "{\"context_uri\":\"spotify:playlist:" + id + "\"}";
  int cod = https.PUT(cuerpo);
  https.end();
  bool ok = (cod == 200 || cod == 202 || cod == 204);
  Serial.printf("[Listas] reproducir %s -> HTTP %d %s\n", id.c_str(), cod, ok ? "✅" : "❌");
  if (cod == 404) Serial.println("[Listas] ⚠️ no hay ningun aparato reproduciendo");
  return ok;
}

bool pedirCola() {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_COLA)) return false;
  https.addHeader("Authorization", "Bearer " + accessToken);
  int cod = https.GET();
  if (cod != 200) { https.end(); Serial.printf("[Cola] HTTP %d\n", cod); return false; }

  // La cola trae hasta 20 canciones con todos sus datos: decenas de KB.
  // El filtro se queda solo con la PRIMERA y sus cuatro campos.
  JsonDocument filtro;
  filtro["queue"][0]["name"] = true;
  filtro["queue"][0]["duration_ms"] = true;
  filtro["queue"][0]["album"]["name"] = true;
  filtro["queue"][0]["artists"][0]["name"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, https.getStream(),
                                             DeserializationOption::Filter(filtro));
  https.end();
  if (err) { Serial.printf("[Cola] JSON: %s\n", err.c_str()); return false; }

  JsonObject sig = doc["queue"][0];
  if (sig.isNull()) return false;
  const char* t = sig["name"] | "";
  if (!*t) return false;

  claveObj = String(t) + "|" + String((const char*)(sig["artists"][0]["name"] | ""));
  if (claveObj == clavePrecarga) return false;      // ya la teníamos
  tObj = t;
  aObj = (const char*)(sig["artists"][0]["name"] | "");
  alObj = (const char*)(sig["album"]["name"] | "");
  dObj = sig["duration_ms"] | 0;
  Serial.printf("[Precarga] siguiente: %s — %s\n", tObj.c_str(), aObj.c_str());
  return true;
}

void consultar() {
  WiFiClientSecure cli; cli.setCACertBundle(rootca_crt_bundle_start);
  HTTPClient https;
  if (!https.begin(cli, URL_CANCION)) return;
  https.addHeader("Authorization", "Bearer " + accessToken);

  /* ------------------------------------------------------------
   *  Hay que PEDIR las cabeceras que se quieran leer
   * ------------------------------------------------------------
   *  `HTTPClient` tira todas las cabeceras de la respuesta según
   *  las lee, salvo las que se le hayan apuntado antes con
   *  `collectHeaders()`. Sin esta línea, `header("Retry-After")`
   *  devuelve cadena vacía SIEMPRE — no un error, una cadena vacía,
   *  que `.toInt()` convierte en 0 tan campante.
   *
   *  Resultado antes de arreglarlo: ante un 429 se esperaba 1 s
   *  fijo dijera lo que dijera Spotify, incumpliendo el ADR-0002.
   *  Y el log imprimía "esperando 0 s", que habría desmentido el
   *  diagnóstico a quien lo investigara.
   *
   *  🔑 Una API que devuelve vacío en vez de fallar convierte un
   *     error de programación en un dato falso. Y un dato falso
   *     viaja hasta el registro, donde ya parece verdad.
   * ------------------------------------------------------------ */
  const char* cabecerasQueImportan[] = { "Retry-After" };
  https.collectHeaders(cabecerasQueImportan, 1);

  // Se mide cuánto tarda el viaje de ida y vuelta. El progress_ms que
  // devuelve Spotify corresponde a un instante ANTERIOR a que llegue la
  // respuesta: aproximadamente la mitad del viaje. Sin compensarlo, la
  // letra va sistemáticamente atrasada.
  unsigned long t0 = millis();
  int cod = https.GET();
  unsigned long viaje = millis() - t0;

  if (cod == 204) {                       // nada sonando: NO es un error (RF-06)
    if (claveActual != "__nada__") {
      CON_DATOS({ claveActual = "__nada__"; letras.limpiar(); sonando = false;
                  pedirRedibujado = true; });
    }
    https.end(); return;
  }
  if (cod == 401) { https.end(); renovarToken(); return; }
  if (cod == 429) {
    int e = https.header("Retry-After").toInt();
    // Si aun así no viene, 5 s es más prudente que 1: ante la duda,
    // molestar menos. Y se acota por arriba para que una cabecera
    // absurda no deje el aparato mudo media hora.
    if (e <= 0) e = 5;
    if (e > 60) e = 60;

    // Spotify no publica su límite exacto, así que si se queja subimos
    // el intervalo y ya no volvemos a molestarle tanto. Se autorregula.
    cadaCuantoMs = min(cadaCuantoMs + 1000UL, 8000UL);
    Serial.printf("[API] ⚠️ límite alcanzado: esperando %d s, intervalo -> %lu ms\n",
                  e, cadaCuantoMs);
    /* ⚠️ NO se duerme aquí. `consultar()` corre DENTRO de `CON_RED`, así
     * que esperar 60 s en este punto dejaría al hilo de letras bloqueado
     * todo ese rato — y LRCLIB no tiene ninguna culpa de que Spotify nos
     * haya limitado. Se apunta hasta cuándo no preguntar y se suelta el
     * turno de inmediato. */
    noConsultarAntesDe = millis() + (unsigned long)e * 1000UL;
    https.end();
    return;
  }
  if (cod != 200) { Serial.printf("[API] ❌ %d\n", cod); https.end(); return; }

  JsonDocument filtro;
  filtro["is_playing"] = true; filtro["progress_ms"] = true;
  filtro["item"]["name"] = true; filtro["item"]["duration_ms"] = true;
  filtro["item"]["album"]["name"] = true; filtro["item"]["artists"][0]["name"] = true;
  // La portada. Se piden ancho y URL de todas y luego se elige la de 300.
  filtro["item"]["album"]["images"][0]["url"] = true;
  filtro["item"]["album"]["images"][0]["width"] = true;
  filtro["context"]["uri"] = true; filtro["context"]["type"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, https.getStream(),
                                             DeserializationOption::Filter(filtro));
  https.end();
  if (err) { Serial.printf("[API] ❌ JSON: %s\n", err.c_str()); return; }

  resolverContexto(doc["context"]["uri"] | "", doc["context"]["type"] | "",
                   doc["item"]["album"]["name"] | "",
                   doc["item"]["artists"][0]["name"] | "");

  const char* titulo  = doc["item"]["name"] | "(sin título)";
  const char* album   = doc["item"]["album"]["name"] | "";
  const char* artista = doc["item"]["artists"][0]["name"] | "";

  /* De las tres que manda Spotify (640/300/64) solo cabe la de 300.
   * Se busca por ancho y no por posición: si algún día cambian el
   * orden, esto sigue eligiendo bien.
   *
   * ⚠️ Usa `ANCHO_QUE_PEDIMOS`, NO `PORTADA_ORIGEN`.
   *
   *    Aquí hubo un fallo real (2026-08-30). `PORTADA_ORIGEN` empezó
   *    siendo la constante 300 y esta línea la usaba tan tranquila.
   *    Después se convirtió en VARIABLE, porque el decodificador
   *    necesitaba saber cuánto mide cada imagen. Al dibujar una imagen
   *    de playlist quedaba valiendo 150 — y entonces esta línea se
   *    ponía a buscar una portada de álbum de 150 px, que no existe.
   *    `urlPortada` quedaba vacía y la carátula no volvía a cargarse
   *    hasta reiniciar. Solo fallaba VINIENDO del selector, que es lo
   *    que lo hacía difícil de ver.
   *
   *    🔑 Convertir una constante en variable es un cambio silencioso:
   *       el compilador no avisa de quién dependía de que no cambiara.
   *       Si un valor tiene dos papeles —"lo que pido" y "lo que estoy
   *       procesando"— hay que darle dos nombres antes de que uno de
   *       los dos empiece a moverse. */
  {
    String u;
    for (JsonObject im : doc["item"]["album"]["images"].as<JsonArray>())
      if ((int)(im["width"] | 0) == ANCHO_QUE_PEDIMOS) { u = im["url"] | ""; break; }
    CON_DATOS({ urlPortada = u; });
  }
  long progresoReportado = doc["progress_ms"] | 0;
  duracionMs   = doc["item"]["duration_ms"] | 0;
  sonando      = doc["is_playing"] | false;

  /* --------------------------------------------------------------
   *  Por qué el reloj daba saltitos
   * --------------------------------------------------------------
   *  Entre consulta y consulta, el progreso se CALCULA con el reloj
   *  del ESP32. Ese cálculo nunca coincide exactamente con lo que
   *  Spotify reporta: hay unos cientos de milisegundos de diferencia
   *  por la red y por cómo mide cada uno.
   *
   *  Al llegar cada respuesta se sobrescribía el progreso de golpe.
   *  Si nuestra cuenta iba 300 ms adelantada y el segundo acababa de
   *  cambiar, el número RETROCEDÍA: 1:24 -> 1:23 -> 1:24. Eso era el
   *  salto, y ocurría a cada consulta.
   *
   *  Ahora se distingue entre dos casos:
   *
   *    · Diferencia GRANDE (>1,5 s)  -> alguien movió la canción o
   *      cambió de pista: hay que resincronizar de golpe.
   *    · Diferencia PEQUEÑA          -> es solo deriva. Se corrige
   *      un 20% cada vez, así el reloj se acerca poco a poco sin
   *      dar nunca un salto visible.
   *
   *  Es la misma idea que usan los relojes de red: no se ajustan de
   *  un tirón, se van "arrastrando" hacia la hora correcta.
   * -------------------------------------------------------------- */
  long estimado = progresoAhora();
  long diferencia = progresoReportado - estimado;

  if (instanteBase == 0 || labs(diferencia) > 1500) {
    progresoBase = progresoReportado;
    instanteBase = millis() - viaje / 2;     // compensar el viaje de red
  } else {
    progresoBase = estimado + diferencia / 5;
    instanteBase = millis();
  }

  String clave = String(titulo) + "|" + String(artista);
  if (clave != claveActual) {
    Serial.printf("\n🎵 %s — %s\n", titulo, artista);
    bool desdePrecarga = (clave == clavePrecarga && precarga.estado != LETRA_NO_BUSCADA);

    // Primero se avisa de la canción nueva para que el otro núcleo pinte
    // ya la cabecera y el "buscando letra...". Luego se descarga.
    CON_DATOS({
      generacion++;                    // marca de agua: todo lo anterior caduca
      claveActual = clave;
      tituloAct = titulo; artistaAct = artista;
      colaPedida = false;
      if (desdePrecarga) { letras = precarga; clavePrecarga = ""; precarga.limpiar(); }
      else letras.limpiar();
      pedirRedibujado = true;
    });

    // Cualquier trabajo en curso deja de valer: la canción es otra.
    fase = FASE_NADA;

    if (desdePrecarga) {
      Serial.printf("[Letra] ⚡ desde precarga (%d líneas)\n", letras.n);
    } else {
      // No se busca aquí. Se ENCOLA, y el bucle de red dará un paso por
      // vuelta, consultando a Spotify entre medias.
      claveObj = clave;
      tObj = titulo; aObj = artista; alObj = album; dObj = duracionMs;
      fase = FASE_EXACTA; faseEsPrecarga = false; candidatoActual = 0;
      clavePrecarga = "";       // precarga se usará como borrador
      precarga.limpiar();
      Serial.printf("[Letra] buscando: %s — %s (%lds)\n", titulo, artista, duracionMs / 1000);
    }
  }
  if (!nombreContexto.length()) nombreContexto = sonando ? "Reproduciendo" : "En pausa";
}

// ============================================================

void tareaSpotify(void*);   // se definen más abajo
void tareaLetras(void*);

void setup() {
  Serial.begin(115200); delay(400);
  // Lo primero que dice al arrancar: qué versión es. Así se puede saber
  // qué hay cargado sin tener que adivinarlo desde Git.
  Serial.printf("\n=== Tamagotchi · firmware %s ===\n", FW_VERSION);
  ledcSetup(CANAL_PWM, 5000, 8); ledcAttachPin(TFT_BL, CANAL_PWM);
  ledcWrite(CANAL_PWM, BRILLO);
  mutexDatos = xSemaphoreCreateMutex();
  mutexRed   = xSemaphoreCreateMutex();
  tft.init(); tft.setRotation(0);
  // El texto ya viene convertido a Latin-1 (un byte por carácter) desde
  // aLatin1(), así que hay que apagar el decodificador UTF-8 de la
  // librería: si no, intentaría interpretar el byte 0xF1 (ñ) como el
  // principio de una secuencia de varios bytes y se perdería.
  tft.setAttribute(UTF8_SWITCH, false);
  // El sprite se crea UNA vez: 200x20 píxeles a 2 bytes = 8 KB.
  barraSpr.createSprite(BARRA_ANCHO, BARRA_ALTO);
  barraSpr.setAttribute(UTF8_SWITCH, false);

  pantallaCarga("conectando al WiFi...");
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { animarCarga(); delay(10); }
  Serial.printf("WiFi ✅ %s\n", WiFi.localIP().toString().c_str());

  /* El joystick se calibra AQUÍ, con la radio ya encendida, no antes.
   * Los ejes van a pines del ADC1 justamente porque el ADC2 deja de
   * funcionar con WiFi — y medir el reposo en las mismas condiciones
   * en que luego se lee es lo que hace que la referencia valga. */
  iniciarPortada();
  joystick.iniciar();

  // El modo elegido sobrevive al reinicio (RF-11).
  prefs.begin("tamagotchi", false);
  modo = prefs.getInt("modo", MODO_CON_LETRA);
  if (modo < 0 || modo >= N_MODOS) modo = MODO_CON_LETRA;   // por si acaso
  modoGuardado = modo;
  Serial.printf("[Modo] al arrancar: %s\n", nombreModo(modo));

  // Sin hora correcta no se validan certificados: el ESP32 arranca en 1970.
  textoCarga("sincronizando la hora...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t t = 0; while (t < 1700000000) { animarCarga(); delay(10); time(&t); }

  // El token lo pide el núcleo 0 mientras este sigue bailando.
  textoCarga("hablando con Spotify...");
  xTaskCreatePinnedToCore(tareaTokenInicial, "token0", 12288, NULL, 2, NULL, 0);
  while (!tokenInicialListo) { animarCarga(); delay(10); }
  claveActual = "";

  /* ------------------------------------------------------------
   *  Tres hilos, cada uno con un trabajo que no depende de los otros
   * ------------------------------------------------------------
   *  Antes había dos: red y dibujo. Pero dentro del hilo de red
   *  convivían dos cosas de naturaleza muy distinta:
   *
   *    · Detectar cambios de canción — rápido, y DEBE ser puntual
   *    · Descargar letras            — lento, y puede esperar
   *
   *  Al compartir hilo, lo lento retrasaba a lo urgente: si estabas
   *  descargando una letra, un cambio de canción no se detectaba
   *  hasta que terminara. Partir la descarga en pasos lo mejoró,
   *  pero no lo resolvió: seguía habiendo que esperar al paso en
   *  curso.
   *
   *  🔑 Si dos cosas tienen urgencias distintas, no comparten hilo.
   *     Trocear lo lento solo reparte la espera; separarlo la elimina.
   *
   *  La tarea de Spotify tiene prioridad más alta: cuando ambas
   *  quieren la radio, manda la que el usuario está esperando.
   * ------------------------------------------------------------ */
  //  Prioridad 2 para Spotify y 1 para las letras: cuando ambas quieren
  //  el turno de red, gana la que el usuario está esperando. Nada por
  //  encima de 2, para no dejar sin CPU a las tareas del sistema.
  xTaskCreatePinnedToCore(tareaSpotify, "spotify", 12288, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(tareaLetras,  "letras",  16384, NULL, 1, NULL, 0);
}

// Latido de diagnóstico: cada 30 s imprime la memoria libre y el mínimo
// histórico. Es la única forma de saber si hay una fuga real o solo el
// calentamiento del heap — que baja y se aplana. Una fuga es lineal e
// infinita; el calentamiento es asintótico. Con pocas muestras no se
// distinguen.
unsigned long ultimoLatido = 0;

// ============================================================
//  NÚCLEO 0 — solo internet. Puede tardar lo que haga falta.
// ============================================================
// ============================================================
//  HILO 1 — Spotify. Rápido y puntual. UNA petición corta cada 2 s.
//  Nunca espera a la descarga de una letra.
// ============================================================
void tareaSpotify(void*) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      CON_DATOS({ claveActual = "__sinred__"; pedirRedibujado = true; });
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }
    /* ------------------------------------------------------------
     *  Renovar el token: DENTRO del turno de red
     * ------------------------------------------------------------
     *  `renovarToken()` abre su propia conexión TLS. Era el único
     *  sitio del firmware que lo hacía sin pedir turno, y eso es
     *  exactamente lo que el mutex existe para impedir: dos
     *  handshakes a la vez saturan el núcleo 0, IDLE0 no llega a
     *  ejecutarse y salta el watchdog. Costó 11 reinicios en 150 s
     *  descubrirlo (ver 5-investigacion/concurrencia-y-carreras.md).
     *
     *  El token caduca ~26 veces al día; cada una de esas veces
     *  podía coincidir con una descarga de letra en curso.
     *
     *  ⚠️ Solo se envuelve ESTA llamada. La otra (`consultar()` ante
     *     un 401) ya corre dentro de `CON_RED`, y el mutex NO es
     *     recursivo: envolverla también sería un interbloqueo.
     * ------------------------------------------------------------ */
    if (accessToken == "" || tokenCaducado()) {
      bool renovado = false;
      CON_RED({ renovado = renovarToken(); });
      if (!renovado) {
        // Van varios intentos fallidos seguidos: no es un tropiezo, es
        // que no podemos hablar con Spotify. Hay que DECIRLO en pantalla.
        if (++fallosToken >= FALLOS_PARA_AVISAR) {
          CON_DATOS({ claveActual = "__sinspotify__"; pedirRedibujado = true; });
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
        continue;
      }
      fallosToken = 0;
    }

    long falta = duracionMs - progresoAhora();
    unsigned long intervalo = (sonando && duracionMs > 0 && falta < (long)UMBRAL_FIN_MS)
                              ? CERCA_DEL_FIN_MS : cadaCuantoMs;
    /* ¿Hay una orden del joystick esperando? Se atiende ANTES que el
     * sondeo: quien acaba de pulsar está mirando la pantalla. */
    /* Se vacía la cola ENTERA en esta vuelta, no una orden por vuelta.
     * Si empujaste dos veces seguidas querías saltar dos canciones, y
     * repartirlas en vueltas distintas las separaba varios segundos. */
    Orden o;
    while ((o = sacarOrden()) != ORDEN_NINGUNA) {
      bool ok = false;
      // ⏱ MEDICIÓN: ¿dónde se va el tiempo? cola, turno de red, o la red
      unsigned long tSacada = millis(), tDentro = 0;
      CON_RED({ tDentro = millis(); ok = ejecutarOrden(o); });
      Serial.printf("[⏱] %s: cola %lu ms + turno %lu ms + red %lu ms = %lu ms\n",
                    nombreOrden(o), tSacada - ordenEncoladaEn,
                    tDentro - tSacada, millis() - tDentro,
                    millis() - ordenEncoladaEn);

      if (ok && o == ORDEN_PLAYPAUSA) {
        /* Se da por hecho el resultado en vez de esperar a preguntarlo.
         *
         * El motivo es de percepción, no de eficiencia: pedir la pausa
         * cuesta ~1,2 s y volver a preguntar otros ~1,2 s. Dos segundos
         * y medio mirando un icono que no cambia se sienten como que el
         * aparato no te hizo caso — y acabas pulsando otra vez.
         *
         * Como SABEMOS qué pedimos, se refleja ya y el sondeo siguiente
         * lo confirma. Si la orden hubiera fallado, `ok` sería falso y
         * no se toca nada.
         *
         * 🔑 En una interfaz, ir por delante del servidor es legítimo
         *    cuando sabes lo que pediste y puedes corregirte después. */
        CON_DATOS({ sonando = !sonando; instanteBase = millis();
                    progresoBase = progresoAhora(); pedirRedibujado = true; });
      }
      // Preguntar enseguida: si fue un salto, la canción ya es otra.
      ultimaConsulta = 0;
    }
    // Se apaga solo si no quedó ninguna esperando: si el usuario siguió
    // empujando mientras ejecutábamos, el spinner debe seguir girando.
    if (ordenEnVuelo && !hayOrdenesPendientes()) ordenEnVuelo = false;

    /* ¿Hace falta bajar una portada? Solo en su modo, solo si es otra,
     * y solo si el núcleo 1 ya se llevó la anterior. Fuera del modo 3
     * sería gastar red y 41 KB para nada. */
    /* Las playlists se piden una sola vez, al entrar por primera vez
     * al selector: sus nombres no cambian mientras el aparato está
     * encendido, y la respuesta es grande. */
    unsigned long esperaListas = (nListas == 0) ? MS_REINTENTAR_LISTAS
                                                : MS_REVISAR_SI_APARECEN;
    if (modo == MODO_PLAYLISTS && nListas < N_LISTAS_ELEGIDAS
        && (listasIntentadasEn == 0
            || millis() - listasIntentadasEn > esperaListas)) {
      listasIntentadasEn = millis();
      bool ok = false;
      CON_RED({ ok = pedirListas(); });
      listasPedidas = true;          // ya hubo al menos un intento
      if (ok) repintarSelector = true;
    }

    if (!portadaLista) {
      /* Qué imagen toca depende de lo que se esté mirando. En el
       * selector se espera a que la selección se asiente: ver
       * `moverSeleccion()`. */
      String u = imagenQueTocaAhora();
      // En el selector se espera a que la selección se asiente: bajar
      // una imagen por cada empujón haría el recorrido insoportable.
      if (modo == MODO_PLAYLISTS && millis() - listaMovidaEn <= MS_ASENTARSE) u = "";

      if (u.length() && u != portadaEnBuffer && u != portadaFallida) {
        bool ok = false;
        CON_RED({ ok = bajarPortada(u); });   // publica buffer + URL a la vez
        if (ok) portadaLista = true;
        else { portadaFallida = u; portadaFallo = true; }
      }
    }

    bool nosPidieronEsperar = (long)(millis() - noConsultarAntesDe) < 0;
    if (millis() - ultimaConsulta >= intervalo && !nosPidieronEsperar) {
      ultimaConsulta = millis();
      CON_RED({ consultar(); });       // espera como mucho UNA petición ajena
    }

    /* ⏱ Latido del nucleo 0. Se queda: sin el no se puede saber si en un
     * cuelgue murieron los dos nucleos o solo uno, y esa diferencia
     * separa el abrazo mortal de la memoria pisada. El `[Latido]` de
     * siempre lo escribe el nucleo 1, y con uno solo no se distinguen. */
    static unsigned long latido0 = 0;
    if (millis() - latido0 > 15000) {
      latido0 = millis();
      Serial.printf("[Latido-0] t=%lus  nucleo 0 vivo  libre %lu B  bloque %lu B\n",
                    millis() / 1000, (unsigned long)ESP.getFreeHeap(),
                    (unsigned long)ESP.getMaxAllocHeap());
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ============================================================
//  HILO 2 — Letras. Lento, y puede tardar lo que quiera.
//  Si la canción cambia mientras trabaja, tira lo que llevaba.
// ============================================================
// Por debajo de este margen, el hilo de letras se aparta. Medido: el
// mínimo histórico en funcionamiento normal es ~59 KB, así que 45 KB
// solo se alcanzaría en una situación anómala.
const uint32_t RAM_MINIMA = 45000;

void tareaLetras(void*) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED || accessToken == "") {
      vTaskDelay(pdMS_TO_TICKS(1000)); continue;
    }

    /* --------------------------------------------------------------
     *  Red de seguridad: la letra es lo prescindible
     * --------------------------------------------------------------
     *  Los dos hilos pueden tener una conexión cifrada abierta a la
     *  vez, y cada una cuesta ~50 KB. Si por lo que sea la memoria
     *  bajara demasiado, el que debe apartarse es ESTE: sin letra el
     *  aparato sigue mostrando la canción; sin memoria, se reinicia.
     *
     *  🔑 Cuando dos cosas compiten por un recurso escaso, hay que
     *     decidir de antemano cuál cede. Si no se decide, cede la que
     *     pida primero — que es aleatorio.
     * -------------------------------------------------------------- */
    /* ------------------------------------------------------------
     *  Quien acaba de tocar la palanca va primero
     * ------------------------------------------------------------
     *  Medido el 2026-08-29: una orden del joystick tardaba ~5 s en
     *  ejecutarse. No era la red —la petición son 1,2 s—, era que el
     *  hilo de letras tenía tomado el turno con descargas de 4 s y la
     *  orden esperaba detrás de varias.
     *
     *  Una letra puede llegar dos segundos más tarde y nadie lo nota.
     *  Un salto de canción que tarda cinco segundos se siente como que
     *  el aparato no te hizo caso — y vuelves a empujar.
     *
     *  🔑 Cuando dos tareas compiten por un recurso, la que cede no es
     *     la menos importante: es la que nadie está mirando.
     * ------------------------------------------------------------ */
    if (hayOrdenesPendientes()) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

    /* ------------------------------------------------------------
     *  Mientras navegas, las letras se callan del todo
     * ------------------------------------------------------------
     *  El intento anterior solo silenciaba la precarga, y las órdenes
     *  seguían tardando hasta 5 s. La medición lo desmintió: lo que
     *  ocupaba la red no era la precarga, era la búsqueda de letra de
     *  cada canción nueva — y saltando canciones hay una canción nueva
     *  todo el rato.
     *
     *  Comprobar "¿hay órdenes?" antes de cada petición no basta: entre
     *  esa comprobación y el empujón caben 4 segundos de descarga ya
     *  empezada, y abortarla a medias es justo lo que este proyecto
     *  decidió no hacer por complejidad.
     *
     *  Así que se ataca al revés: mientras haya actividad reciente en
     *  la palanca, el hilo de letras ni lo intenta. Vuelve solo a los
     *  2,5 s del último gesto. Nadie lee la letra mientras salta
     *  canciones, y en cuanto te quedas en una, aparece.
     *
     *  🔑 Cuando no puedes acortar una tarea larga, evita empezarla en
     *     el momento equivocado. Sale mucho más barato que abortarla.
     * ------------------------------------------------------------ */
    if (navegando()) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

    if (ESP.getFreeHeap() < RAM_MINIMA) {
      Serial.printf("[Letras] ⏸ memoria baja (%lu B), esperando\n",
                    (unsigned long)ESP.getFreeHeap());
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    if (fase != FASE_NADA) {
      // Se copia el objetivo AHORA, junto con la generación. A partir de
      // aquí se trabaja con la copia: aunque la canción cambie a mitad de
      // la descarga, sabremos que lo que traemos ya no vale.
      uint32_t miGen; bool esPre; Fase f; int cand;
      String cl, t, a, al; long d;
      CON_DATOS({ miGen = generacion; esPre = faseEsPrecarga; f = fase;
                  cand = candidatoActual; cl = claveObj;
                  t = tObj; a = aObj; al = alObj; d = dObj; });

      if (!esPre && cl != claveActual) {
        Serial.println("[Letra] ⛔ cancelada: cambió la canción");
        CON_DATOS({ if (generacion == miGen) fase = FASE_NADA; });
        precarga.limpiar();
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }

      const uint8_t* ca = rootca_crt_bundle_start;
      bool ok = false, terminado = false;
      Fase siguiente = f;

      // Una sola petición, y con el turno de red tomado.
      CON_RED({
        switch (f) {
          case FASE_COLA:
            if (pedirCola()) { siguiente = FASE_EXACTA; cand = 0; precarga.limpiar(); }
            else { siguiente = FASE_NADA; }
            break;
          case FASE_EXACTA:
            ok = precarga.pedirExacta(ca, t, a, al, d / 1000);
            if (ok) terminado = true; else siguiente = FASE_CANDIDATOS;
            break;
          case FASE_CANDIDATOS:
            if (precarga.buscarCandidatos(ca, t, a, d / 1000)) { siguiente = FASE_PROBAR; cand = 0; }
            else terminado = true;
            break;
          case FASE_PROBAR:
            ok = precarga.pedirPorId(ca, precarga.candidatos[cand]);
            cand++;
            if (ok || cand >= precarga.numCandidatos) terminado = true;
            break;
          default: break;
        }
      });

      CON_DATOS({
        if (generacion != miGen && !esPre) {
          // Llegó tarde: mientras descargábamos, la canción cambió. Este
          // resultado es la letra de OTRA canción. Se tira, y no se toca
          // `fase` porque ya está apuntando al trabajo nuevo.
          Serial.println("[Letra] ⛔ descartada: llegó tarde");
          precarga.limpiar();
        } else if (terminado) {
          /* ------------------------------------------------------------
           *  ⚠️ Solo se apaga la máquina si `fase` sigue siendo NUESTRA
           * ------------------------------------------------------------
           *  El caso que se escapaba (visto en el registro, 2026-08-29):
           *
           *    [Precarga] siguiente: SAME SH!T      ← empieza a precargar
           *    🎵 Chest Pain — Malcolm Todd         ← cambias de canción
           *    [Letra] buscando: Chest Pain         ← se encola su búsqueda
           *    [Precarga] lista (53 líneas)         ← y este FASE_NADA
           *                                            borraba la de arriba
           *
           *  La precarga terminaba DESPUÉS del cambio de canción y
           *  apagaba la búsqueda de la canción nueva. Esa canción sonaba
           *  entera sin letra.
           *
           *  Y no se recuperaba sola: la búsqueda solo se encola cuando
           *  cambia el par «título|artista», así que repetir la canción
           *  tampoco servía de nada.
           *
           *  El `if` de arriba ya contemplaba esto, pero lleva `&& !esPre`
           *  — protegía la búsqueda normal y dejaba fuera la precarga.
           *
           *  🔑 Al apagar una máquina de estados compartida hay que
           *     comprobar que sigue apuntando al trabajo que uno hizo.
           *     Si otro la reprogramó mientras tanto, apagarla es borrarle
           *     el encargo.
           * ------------------------------------------------------------ */
          if (generacion == miGen) fase = FASE_NADA;
          if (!ok) precarga.estado = LETRA_NO_HAY;
          if (esPre) {
            clavePrecarga = cl;
            Serial.printf("[Precarga] lista (%d líneas) · RAM %lu B\n",
                          precarga.n, (unsigned long)ESP.getFreeHeap());
          } else {
            letras = precarga; precarga.limpiar();
            lineaPintada = -2; pedirRedibujado = true;
            Serial.printf("[Letra] %s (%d líneas) · RAM %lu B\n",
                          ok ? "✅" : "❌ no encontrada", letras.n,
                          (unsigned long)ESP.getFreeHeap());
          }
        } else {
          fase = siguiente; candidatoActual = cand;
        }
      });
    }

    /* ------------------------------------------------------------
     *  Mientras navegas, no se precarga
     * ------------------------------------------------------------
     *  La precarga adelanta la letra de la canción SIGUIENTE. Es una
     *  optimización valiosa cuando escuchas seguido — y completamente
     *  inútil mientras saltas canciones, porque "la siguiente" cambia
     *  antes de que termine de bajarla.
     *
     *  Y mientras tanto ocupa el turno de red 4 segundos, que es
     *  exactamente lo que retrasaba tus saltos (medido: hasta 4,5 s).
     *
     *  🔑 Una optimización que predice el futuro deja de servir en
     *     cuanto alguien cambia el futuro a propósito.
     * ------------------------------------------------------------ */
    if (fase == FASE_NADA && !colaPedida && sonando
        && letras.estado != LETRA_NO_BUSCADA
        && progresoAhora() > 8000 && claveActual.length() && claveActual[0] != '_') {
      colaPedida = true;
      fase = FASE_COLA; faseEsPrecarga = true;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ============================================================
//  NÚCLEO 1 — solo dibujar. Nunca se bloquea.
// ============================================================
void loop() {
  /* ⏱ Ritmo del bucle de dibujo. Un fotograma que tarda de más se ve
   *    como un tirón, y sin medirlo solo se puede opinar. Se guarda el
   *    peor de cada periodo, que es el que se nota. */
  static unsigned long tFotograma = 0, peorFotograma = 0, nFotogramas = 0, sumaFotogramas = 0;
  if (tFotograma) {
    unsigned long d = millis() - tFotograma;
    if (d > peorFotograma) peorFotograma = d;
    sumaFotogramas += d; nFotogramas++;
  }
  tFotograma = millis();

  if (millis() - ultimoLatido > 30000) {
    ultimoLatido = millis();
    Serial.printf("[Latido] t=%lus  RAM %lu B  minimo %lu B  bloque %lu B  fotograma medio %lu ms peor %lu ms\n",
                  millis() / 1000,
                  (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)ESP.getMinFreeHeap(),
                  (unsigned long)ESP.getMaxAllocHeap(),
                  nFotogramas ? sumaFotogramas / nFotogramas : 0, peorFotograma);
    peorFotograma = 0; sumaFotogramas = 0; nFotogramas = 0;
  }

  /* Mientras no haya nada que enseñar, Snoopy sigue. En cuanto el
   * núcleo 0 avisa de que ya tiene canción, se cede el paso al dibujo
   * de siempre, que limpia la pantalla blanca y pinta lo suyo. */
  if (enCarga) {
    if (!pedirRedibujado) { animarCarga(); delay(5); return; }
    enCarga = false;
  }

  /* ------------------------------------------------------------
   *  El joystick se lee desde el núcleo 1 (el del dibujo)
   * ------------------------------------------------------------
   *  Es el núcleo que NUNCA se bloquea: el de red puede pasarse
   *  4 segundos dentro de un handshake TLS, y una palanca que solo
   *  responde entre descargas se sentiría rota.
   *
   *  Leerla cuesta ~400 microsegundos cada 20 ms. Nada.
   *
   *  De momento SOLO informa. Que actúe es el paso siguiente:
   *  primero hay que ver que los gestos salen limpios.
   * ------------------------------------------------------------ */
  Gesto g = joystick.leer();
  if (g != GESTO_NINGUNO) {
    Serial.printf("[Joystick] %-10s  (x=%+.2f y=%+.2f)\n",
                  nombreGesto(g), joystick.x(), joystick.y());

    /* ⚠️ Solo cuentan como "navegando" los gestos que PIDEN ALGO A LA RED.
     *
     * La primera versión marcaba cualquier gesto, cambios de modo
     * incluidos. Y cambiar de modo no toca la red: es puro dibujo. El
     * efecto medido el 2026-08-30: al recorrer las pantallas se
     * reiniciaba una y otra vez el contador que aparta al hilo de
     * letras, y una letra tardaba 9 s en llegar. Justo cuando el
     * usuario estaba yendo a verla.
     *
     * 🔑 Al dar prioridad a alguien, hay que mirar si de verdad compite
     *    por el mismo recurso. Apartar al hilo de red por un gesto que
     *    no usa la red es pura pérdida. */
    if (g == GESTO_CLIC || g == GESTO_DERECHA || g == GESTO_IZQUIERDA) {
      ultimoGesto = millis();   // el hilo de letras lo mira para apartarse
      ordenEnVuelo = true;      // el spinner aparece ya, sin esperar a la red
    }

    // Solo se deja la nota. La petición la hará el núcleo 0.
    /* En el selector, izquierda y derecha recorren la lista en vez de
     * saltar canciones, y el clic pone a sonar la elegida. El mismo
     * gesto significa cosas distintas según lo que estés mirando —que
     * es como funciona cualquier mando. */
    bool enSelector = (modo == MODO_PLAYLISTS && nListas > 0);

    /* ⚠️ Con la pantalla oculta delante, el clic significa SALIR y nada
     *    más. Va antes del `switch`: si cayera dentro, el clic de salir
     *    pausaría la música de paso, y quien la encontró sin querer se
     *    quedaría además con la canción parada sin saber por qué.
     *
     * 🔑 Cuando una pantalla se pone delante, se queda con TODAS las
     *    entradas. */
    if (pantallaOculta) {
      if (g == GESTO_CLIC || g == GESTO_CLIC_LARGO) cerrarOculta = true;
      return;
    }

    switch (g) {
      case GESTO_CLIC:
        encolarOrden(enSelector ? ORDEN_PONER_LISTA : ORDEN_PLAYPAUSA);
        break;
      case GESTO_DERECHA:
        if (enSelector) { listaSel = (listaSel + 1) % nListas; moverSeleccion(); }
        else encolarOrden(ORDEN_SIGUIENTE);
        break;
      case GESTO_IZQUIERDA:
        if (enSelector) { listaSel = (listaSel + nListas - 1) % nListas; moverSeleccion(); }
        else encolarOrden(ORDEN_ANTERIOR);
        break;
      // Arriba y abajo NO van a la red: el cambio de modo es puro dibujo
      // y tiene que sentirse instantáneo.
      case GESTO_ARRIBA:
      case GESTO_ABAJO:
        modo = (modo + (g == GESTO_ARRIBA ? 1 : N_MODOS - 1)) % N_MODOS;
        modoCambiadoEn = millis();
        repintarModo = true;
        Serial.printf("[Modo] -> %s\n", nombreModo(modo));
        /* El cambio de pantalla se hace IGUAL aunque el gesto forme
         * parte de la combinación. Es lo que la deja invisible: sube
         * dos, baja dos, y acaba donde empezó. Tragarse los gestos
         * mientras acecha la secuencia haría que la palanca se
         * sintiera trabada al navegar rápido. */
        if (avanzarCombinacion(g)) abrirOculta = true;
        break;
      default: break;
    }
  }

  /* ------------------------------------------------------------
   *  La pantalla oculta manda mientras está delante
   * ------------------------------------------------------------
   *  Abrir y cerrar se hacen aquí, no dentro del `switch` de los
   *  gestos, porque las dos cosas dibujan y tardan (la animación
   *  ronda el segundo). El `switch` solo debe dejar notas.
   *
   *  Mientras esté visible, este bloque CORTA el resto del bucle con
   *  un `return`. Si no, todo lo que viene después —cabecera, letra,
   *  barra, portada— pintaría encima y duraría un fotograma.
   * ------------------------------------------------------------ */
  if (abrirOculta) {
    abrirOculta = false;
    modoAntesDeOculta = modo;
    pantallaOculta = true;
    Serial.println("[Oculta] abierta con la combinacion secreta");
    fundirBrillo(BRILLO, 0, 110);
    animacionSorpresa();
    pintarPantallaOculta();
    fundirBrillo(0, BRILLO, 200);
    ultimoRefrescoOculta = millis();
  }

  if (cerrarOculta) {
    cerrarOculta = false;
    pantallaOculta = false;
    Serial.println("[Oculta] cerrada");
    despedida();                     // deja la pantalla negra y la luz apagada
    /* Volver EXACTAMENTE a donde estaba: devolver a quien la abrió sin
     * querer a otra pantalla sería confirmarle que algo se descolocó. */
    modo = modoAntesDeOculta;
    volverDeLaPantallaOculta();      // repinta y vuelve a encender
  }

  if (pantallaOculta) {
    /* La RAM y el tiempo encendido se mueven solos: es la única de las
     * cinco pantallas que cambia sin que nadie toque nada — y mirar si
     * la memoria baja es precisamente para lo que sirve. */
    if (millis() - ultimoRefrescoOculta >= 1000) {
      ultimoRefrescoOculta = millis();
      refrescarValoresOcultos();
    }
    delay(5);
    return;
  }

  /* ¿El núcleo 0 avisó de que cambió algo? Se repinta la cabecera.
   *
   * ⚠️ Salvo en el modo portada, donde hay que hilar más fino: ahí la
   *    pausa NO se dibuja, así que repintar por un play/pausa significa
   *    tirar la carátula y volver a bajarla — un segundo de spinner cada
   *    vez que pulsas, para acabar mostrando exactamente lo mismo.
   *
   *    Solo se repinta si cambió la CANCIÓN, y eso se sabe mirando si
   *    cambió la URL de la portada.
   *
   * 🔑 "Algo cambió" no es razón suficiente para repintar. La pregunta
   *    es si cambió algo que este modo esté mostrando. */
  /* En el selector, la canción que suena NO se muestra: lo que hay es
   * la playlist elegida y su imagen. Repintar porque cambió la canción
   * significa tirar esa imagen y volver a bajarla —un segundo de
   * spinner— para acabar enseñando exactamente lo mismo.
   *
   * Los estados especiales (sin WiFi, sin Spotify) sí tienen que pintar:
   * esos sustituyen la pantalla entera. */
  if (pedirRedibujado && modo == MODO_PLAYLISTS) {
    bool especial = false;
    CON_DATOS({ especial = (claveActual.length() && claveActual[0] == '_'); });
    if (!especial) pedirRedibujado = false;
  }

  if (pedirRedibujado && modo == MODO_PORTADA && portadaVisible) {
    bool mismaCancion = false;
    CON_DATOS({ mismaCancion = (urlPortada == portadaPintada && urlPortada.length()); });
    if (mismaCancion) pedirRedibujado = false;
  }

  if (pedirRedibujado) {
    pedirRedibujado = false;
    bool eraPantallaNormal = false;   // ¿se pintó canción, o un estado especial?
    CON_DATOS({
      if (claveActual == "__sinred__") {
        pantallaMensaje("Sin conexion", "reintentando...", TFT_RED);
      } else if (claveActual == "__sinspotify__") {
        // El WiFi va bien pero Spotify no contesta. Sin este estado, el
        // núcleo 1 seguiría pintando la canción de hace horas con la barra
        // llena y la última frase fija — indistinguible de un aparato roto.
        pantallaMensaje("Sin Spotify", "reintentando...", NARANJA_AVISO);
      } else if (claveActual == "__nada__") {
        pantallaMensaje("En silencio", "no hay nada sonando", GRIS);
      } else {
        tft.fillScreen(TFT_BLACK);
        estadoPintado = ""; lineaPintada = -2;
        pintarContenidoModo();
        eraPantallaNormal = true;
      }
    });

    // La portada, FUERA del turno: lo pide ella sola. Ver el aviso en
    // `pintarContenidoModo()`.
    if (eraPantallaNormal && modo == MODO_PORTADA) pintarPortada();
  }

  if (claveActual.length() && claveActual[0] != '_') {
    /* ------------------------------------------------------------
     *  La barra convive con la portada, pero SIN tocarla
     * ------------------------------------------------------------
     *  La primera versión ponía la carátula a 240x240 y la barra se
     *  dibujaba encima. Se veía —fotos del 2026-08-30— el nombre de
     *  la playlist pisando la imagen.
     *
     *  Y era peor de lo que parecía: la portada se libera de memoria
     *  nada más pintarla, así que cualquier cosa dibujada encima
     *  BORRABA ese trozo para siempre. Al pausar, el hueco del icono
     *  se quedaba negro hasta la siguiente canción.
     *
     *  🔑 Si no puedes repintar algo, no dibujes encima. Un lienzo que
     *     no se puede recuperar hay que tratarlo como definitivo.
     *
     *  La solución no fue quitar la barra —hacía falta para ver la
     *  playlist y el estado— sino DARLE SU SITIO: la barra se queda
     *  con los 28 píxeles de arriba y la portada ocupa el cuadrado
     *  más grande que cabe debajo. Ya no compiten por el mismo pixel.
     * ------------------------------------------------------------ */
    bool modoPortada = (modo == MODO_PORTADA);

    barraEstado(sonando, sonando ? VERDE_SPOTIFY : GRIS);

    /* El spinner ocupa el hueco del icono de reproducción mientras hay
     * una orden en camino. Al terminar hay que volver a pintar el icono:
     * `barraEstado` solo lo redibuja cuando cambia el texto de la barra,
     * y aquí el texto puede ser el mismo de antes. */
    static bool spinnerVisible = false;
    if (ordenEnVuelo) {
      dibujarSpinner(240 - MARGEN - 6, 13);
      spinnerVisible = true;
    } else if (spinnerVisible) {
      spinnerVisible = false;
      tft.fillRect(240 - MARGEN - 14, 5, 17, 17, TFT_BLACK);
      dibujarPlayPausa(240 - MARGEN - 11, 13, sonando,
                       sonando ? VERDE_SPOTIFY : GRIS);
    }

    /* ¿Cambió el modo? Se apaga, se repinta a oscuras y se enciende.
     *
     * ⚠️ Hay que dibujar TODO antes de volver a subir el brillo —barra
     *    de arriba y de progreso incluidas—. Si se dejara alguna para
     *    la vuelta siguiente del bucle, aparecería después del fundido
     *    y se vería entrar sola. */
    if (repintarModo) {
      repintarModo = false;
      fundirBrillo(BRILLO, 0, 110);

      tft.fillScreen(TFT_BLACK);
      estadoPintado = ""; lineaPintada = -2;
      CON_DATOS({
        pintarContenidoModo();
        if (modo == MODO_CON_LETRA) {
          lineaPintada = (letras.estado == LETRA_OK) ? letras.lineaEn(progresoAhora()) : -1;
          zonaLetra(lineaPintada);
        }
      });
      // La portada, FUERA del turno. Ver el aviso en `pintarContenidoModo()`.
      if (modo == MODO_PORTADA) pintarPortada();
      barraEstado(sonando, sonando ? VERDE_SPOTIFY : GRIS);
      if (modo == MODO_SIN_LETRA || modo == MODO_CON_LETRA) barraProgreso();

      fundirBrillo(0, BRILLO, 200);
    }

    if (modo == MODO_PLAYLISTS) {
      if (repintarSelector) { repintarSelector = false; pintarSelectorListas(); }
      if (portadaLista) volcarImagenSiLlego(LISTA_LADO, LISTA_Y0);
      else if (!portadaVisible && (nListas > 0 || !listasPedidas))
        dibujarSpinner(120, LISTA_Y0 + LISTA_LADO / 2, 20, 4);
    }

    if (modoPortada) {
      if (portadaLista) pintarPortada();
      // Mientras no esté, un spinner grande en el centro: la descarga y
      // el decodificado tardan casi un segundo, y una pantalla negra en
      // ese rato parece que el aparato se colgó.
      else if (portadaFallo && !portadaVisible) {
        // Degradar, no romper: si no hay carátula se dice y se muestra
        // la canción, en vez de dejar un spinner girando para siempre.
        portadaVisible = true;              // no repintarlo en cada vuelta
        tft.fillRect(0, PORTADA_Y0, 240, 240 - PORTADA_Y0, TFT_BLACK);
        escribir("sin portada", PORTADA_Y0 + 10, false, GRIS_OSCURO);
        CON_DATOS({ cabeceraGrande(tituloAct, artistaAct); });
      }
      else if (!portadaVisible) dibujarSpinner(120, PORTADA_Y0 + PORTADA_LADO / 2, 26, 5);
    }

    if (millis() - ultimoRefresco >= REFRESCO_MS) {
      ultimoRefresco = millis();
      // La letra solo se dibuja en su modo. En los demás, ese trabajo
      // —y su parpadeo— sobra.
      if (modo == MODO_CON_LETRA) {
        CON_DATOS({
          int idx = (letras.estado == LETRA_OK) ? letras.lineaEn(progresoAhora()) : -1;
          if (idx != lineaPintada) { lineaPintada = idx; zonaLetra(idx); }
        });
      }
      // La barra de progreso se conserva en los modos que muestran canción.
      if (modo == MODO_SIN_LETRA || modo == MODO_CON_LETRA) barraProgreso();
    }
    if (modo == MODO_CON_LETRA) avanzarFundido();
    // ⚠️ `moverBarra` repinta la barra de arriba continuamente para el
    //    desplazamiento del texto. En el modo portada eso era justo lo
    //    que pisaba la carátula.
    moverBarra(sonando ? VERDE_SPOTIFY : GRIS);
    guardarModoSiToca();
  }
  delay(5);
}
