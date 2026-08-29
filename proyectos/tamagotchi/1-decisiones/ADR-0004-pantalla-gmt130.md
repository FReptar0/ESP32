# ADR-0004 — Usar la TFT IPS 1.3" GMT130 (240×240) como pantalla del Tamagotchi

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-29
- **Reemplaza a:** —
- **Relacionado con:** RF-01 a RF-04, RNF-01, RNF-02, [ADR-0003](ADR-0003-fuente-de-las-letras.md)

---

## Contexto

Hay que elegir en qué se muestra la información. Es la decisión que más
condiciona el proyecto: define cuánta información cabe, qué se puede mostrar y
qué no, y cuántos pines del ESP32 quedan ocupados.

Al levantar el inventario apareció un dato que cambió el panorama: **ya tienes 4
pantallas TFT IPS de 1.3", modelo GMT130-v1.0, de 240×240 píxeles a color.**

El plan original asumía una OLED de 0.96" (128×64, monocromática), que es lo que
suele venir en los kits para principiantes. La diferencia entre las dos es mucho
más grande de lo que sugieren las pulgadas.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- Tienes **4 unidades**. Eso quita la presión de "no la quemes, es la única".
- 240×240 píxeles a color, panel IPS (que significa buen ángulo de visión: se ve
  bien aunque no la mires de frente).
- El ESP32 tiene SPI de sobra para manejarla.

**Lo que asumimos sin verificar:**

- ⚠️ Que el controlador es un **ST7789**. Es el que llevan prácticamente todas
  las placas etiquetadas "GMT130 / 1.3 IPS 240×240", pero **hay que confirmarlo**
  antes de elegir librería.
- ⚠️ Que trabaja a **3.3 V** tanto en alimentación como en lógica. Es lo normal
  en este módulo, pero hay que leer lo que diga impreso en la plaquita.
- ⚠️ Que el conector tiene **7 pines y no incluye CS**. Es lo típico de la
  GMT130 — ver "Lo que hay que vigilar".

**Lo que quedó pendiente de averiguar:**

- Cuántos caracteres caben *cómodamente leídos a 60 cm* (RNF-01), que no es lo
  mismo que cuántos caben técnicamente.
- Si el brillo del backlight es controlable por PWM (importa para el RNF-02:
  que no moleste de noche).

## Opciones consideradas

### Opción A — OLED SSD1306 0.96" (128×64, monocromática)

Era la suposición del plan original.

- **A favor:** consume poquísimo, es I2C (solo 2 cables de datos), negro real
  (los píxeles apagados no emiten luz), muy fácil de empezar.
- **En contra:** **caben ~4 líneas de 20 caracteres.** Un verso de una canción
  rara vez cabe en 20 caracteres, así que las letras (RF-04) quedarían picadas en
  pedacitos ilegibles. Sin color. Y las OLED sufren *burn-in*: dejar un elemento
  fijo muchas horas lo marca de forma permanente — justo lo que haría un aparato
  que vive prendido en el escritorio.
- **Costo:** hay que comprarla. **No tienes ninguna.**

### Opción B — GMT130, TFT IPS 1.3" 240×240 ← *elegida*

- **A favor:**
  - **Ya la tienes, y tienes cuatro.** Cero costo, cero espera.
  - Del orden de **15 líneas de ~30 caracteres** con una fuente pequeña. Eso hace
    el RF-04 (letras) genuinamente viable, que con la OLED no lo era.
  - **Color**, lo que permite jerarquía visual: título grande en blanco, artista
    en gris, estado en un color de acento. Con una pantalla monocromática todo
    compite por la atención.
  - IPS: se lee bien desde un ángulo, que es como se mira algo en un escritorio.
  - Sin *burn-in*. Puede estar prendida indefinidamente (RNF-06).
  - Deja la puerta abierta a mostrar la portada del álbum más adelante.
  - **3.3 V**, igual que el ESP32 → conexión directa, sin conversor de nivel.
- **En contra:**
  - SPI usa más pines que I2C (unos 5-6 contra 2).
  - Consume más que una OLED, sobre todo por el backlight. Irrelevante ahora que
    va enchufada por USB; importaría si algún día va a batería.
  - Ilumina más de noche → hay que atender el RNF-02 con control de brillo.
  - Un poquito más de complejidad inicial: hay que configurar la librería
    diciéndole qué pines se usaron.
- **Costo:** cero.

### Opción C — Comprar algo más grande

- **En contra:** no tiene sentido gastar antes de haber probado lo que ya
  tienes. Y hasta no ver cómo se lee una letra real en la GMT130, no hay
  información para decidir si hace falta algo mejor.

## Decisión

**Usamos la GMT130 (opción B).**

El factor decisivo: **ya la tienes, y encima es mejor para este proyecto que la
alternativa que íbamos a comprar.** No hay ningún argumento para gastar dinero
aquí.

Y hay un efecto secundario importante: el
[ADR-0003](ADR-0003-fuente-de-las-letras.md) señalaba que el cuello de botella
de las letras iba a ser el tamaño de la pantalla, no la API. **Con 240×240 ese
cuello de botella se afloja bastante.** El RF-04 pasa de "probablemente
impracticable" a "hay que probarlo".

Tener 4 unidades también cambia cómo se puede trabajar: se puede experimentar sin
el miedo paralizante de romper la única que hay. Cuando estás aprendiendo, eso
importa más de lo que parece.

## Consecuencias

**Lo que esto nos habilita:**

- Coste cero y arranque inmediato.
- El RF-04 (letras) vuelve a ser realista.
- Jerarquía visual por color y por tamaño de fuente.
- Se puede considerar la portada del álbum en el futuro (hoy está fuera de
  alcance, pero la pantalla ya no es el impedimento).

**Lo que esto nos cierra o nos cuesta:**

- Ocupa ~6 pines del ESP32. Con 30+ pines disponibles, no es problema.
- Hay que resolver el brillo nocturno (RNF-02) por software, con PWM en el pin
  del backlight.
- Descarta ir a batería sin repensarlo. Ya estaba fuera de alcance.

**Lo que hay que vigilar:**

- ⚠️ **Confirmar el controlador y el voltaje** leyendo lo impreso en la plaquita
  antes de conectar nada.
- ⚠️ **Contar los pines del conector.** La GMT130 típica trae **7 pines y no
  tiene CS** (chip select). Eso significa que la pantalla acapara el bus SPI: si
  algún día quieres poner otro dispositivo SPI encima, va a haber conflicto. Para
  este proyecto no estorba, pero hay que saberlo.
- Que el texto se lea de verdad a 60 cm (RNF-01). 240×240 es bastante resolución
  en 1.3 pulgadas: los píxeles son diminutos, y una fuente demasiado chica se ve
  nítida pero **no se lee**. Es un error fácil de cometer.

## Cómo revertir

**Barato**, si el código separa "qué información mostrar" de "cómo dibujarla".
Cambiar de pantalla implicaría reescribir la capa de dibujo y volver a cablear,
pero nada de la lógica de red ni de datos.

Es la misma separación que piden el [ADR-0002](ADR-0002-como-obtener-datos-de-spotify.md)
y el [ADR-0003](ADR-0003-fuente-de-las-letras.md). Van apareciendo tres razones
distintas para la misma decisión de estructura — señal de que es la correcta.
