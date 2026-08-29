# Componente — TFT IPS 1.3" GMT130-v1.0 (240×240)

- **ID en inventario:** C-004 (tienes **4 unidades**)
- **Categoría:** Pantalla
- **Fecha de la ficha:** 2026-08-29
- **Estado de la ficha:** ⚠️ **Por confirmar contra la plaquita física** — todo lo
  marcado con ⚠️ hay que verificarlo mirando el módulo antes de conectar
- **Decisión asociada:** [ADR-0004](../1-decisiones/ADR-0004-pantalla-gmt130.md)

---

## Qué hace, en una frase

Es una pantallita a color de 1.3 pulgadas y bastante buena resolución (240×240),
que se controla con unos pocos cables y sirve para dibujarle lo que quieras.

Piénsala como un monitor diminuto: el ESP32 le manda "pinta este píxel de este
color" muy rápido, y de ahí sale el texto y los gráficos.

## Qué significa cada parte del nombre

| Término | Qué quiere decir |
|---|---|
| **TFT** | El tipo de pantalla LCD. Cada píxel tiene su propio transistor → imagen nítida y refresco rápido |
| **IPS** | Una mejora del TFT: **se ve bien desde ángulos laterales**. En una TFT normal, mirarla de lado lava los colores. Importa mucho para algo que vive en un escritorio |
| **1.3"** | La diagonal de la pantalla: unos 3.3 cm. Pequeñita |
| **240×240** | Píxeles de ancho por alto. Cuadrada |
| **GMT130** | El nombre del módulo (la placa verde donde va montada) |
| **ST7789** ⚠️ | El chip controlador. Es el que "entiende" las órdenes del ESP32. **Por confirmar** |

## Alimentación

| Concepto | Valor | Nota |
|---|---|---|
| Alimentación (`VCC`) | ⚠️ **3.3 V** | Por confirmar en la plaquita |
| **Voltaje lógico** | ⚠️ **3.3 V** | Por confirmar |
| Consumo típico | ~20–40 mA | Mayormente el backlight (la luz de atrás) |

> ### 🎉 Buena noticia: no hace falta conversor de nivel
>
> Si se confirma que es 3.3 V (lo normal en este módulo), **la pantalla y el
> ESP32 hablan el mismo idioma eléctrico**. Se conectan directo, sin piezas
> intermedias y sin riesgo de quemar nada por voltaje.
>
> ⚠️ **Pero confírmalo antes.** Si en la plaquita dice 5 V, avísame y replaneamos:
> conectar una salida de 5 V a un pin del ESP32 lo daña.

**¿Necesita conversor de nivel lógico con el ESP32?** ⚠️ Por confirmar — se
espera que **no**.

## Cómo se comunica

- **Protocolo:** **SPI**
- **Velocidad:** puede ir a decenas de MHz. Con `TFT_eSPI` en ESP32 se suelen usar
  40 MHz sin problema.

<!-- SPI en corto: un cable de reloj (marca el ritmo), un cable de datos
     (los bits), y un cable que dice "te estoy hablando a ti". Es rápido —
     que es justo lo que hace falta para refrescar una pantalla entera. -->

**Por qué SPI y no I2C:** I2C usa solo 2 cables pero es lento. Para llenar una
pantalla de 240×240 hay que mover 57.600 píxeles; con I2C se vería el refresco
"barrer" la pantalla. SPI mueve eso en milisegundos. El precio son unos pines más,
y el ESP32 tiene de sobra.

## Conexiones

⚠️ **Pinout esperado — CONFIRMAR contra las etiquetas impresas en tu módulo
antes de conectar.** La GMT130 típica trae 7 pines:

| Pin del módulo | Qué es | Va a (ESP32, propuesto) | Nota |
|---|---|---|---|
| `GND` | Tierra | `GND` | Siempre va primero. Es la conexión que más se olvida |
| `VCC` | Alimentación | `3V3` | ⚠️ **Nunca** al pin de 5 V / `VIN` |
| `SCL` / `SCK` | Reloj SPI | `GPIO18` | Marca el ritmo de la comunicación |
| `SDA` / `MOSI` | Datos SPI | `GPIO23` | Por aquí viajan los píxeles |
| `RES` / `RST` | Reset | `GPIO4` | Reinicia la pantalla al arrancar |
| `DC` / `A0` | Data/Command | `GPIO2` | Le dice a la pantalla si lo que llega es una orden o un dato |
| `BLK` / `BL` | Backlight | `GPIO32` | La luz de atrás. En un pin PWM → **brillo regulable** (RNF-02) |

Los GPIO propuestos son tentativos: se fijan de verdad cuando confirmemos el
modelo exacto del ESP32 y qué pines evitar.

> ### ⚠️ Cuenta los pines de tu módulo
>
> Si tiene **7 pines**, no tiene `CS` (*chip select*) — está fijo internamente.
> Consecuencia: la pantalla **acapara el bus SPI** y no puedes poner otro
> dispositivo SPI en paralelo. Para este proyecto no estorba, pero hay que saberlo.
>
> Si tiene **8 pines**, uno será `CS` y hay que conectarlo también.

## Precauciones

- ⚠️ **Confirma el voltaje antes de conectar.** Si dice 5 V, no la conectes
  directo: avísame primero.
- ⚠️ **Conecta `GND` antes que nada** y desconéctalo al final. Sin tierra común,
  las señales de datos no tienen referencia y pasan cosas raras (o se daña algo).
- ⚠️ **`VCC` nunca a `VIN` ni a 5 V.**
- ⚠️ **Desconecta el USB del ESP32 antes de mover cualquier cable.**
- ⚠️ **El flex de conexión es frágil.** Es la cinta que une el vidrio con la
  placa verde. Doblarlo o jalar la pantalla desde el vidrio la arruina de forma
  irreversible. Manipúlala siempre por la placa.

## Cómo probarla aislada

Solo cuando el ESP32 ya funcione por su cuenta (ver
[ficha del ESP32](esp32s-nodemcu.md)):

1. Con el USB **desconectado**, cablear los 7 pines según la tabla de arriba.
2. Revisar dos veces `VCC` y `GND`. Es el par que quema cosas.
3. Conectar el USB. La pantalla debería al menos **encender el backlight** (se ve
   grisácea o blanquecina, aunque no haya nada dibujado).
4. Cargar el ejemplo de prueba de la librería.

**Cómo se ve el éxito:** aparecen colores y texto. Si el backlight enciende pero
la pantalla se queda blanca, el problema está en las líneas de datos (`SDA`,
`SCL`, `DC`, `RES`), no en la alimentación — que ya es un diagnóstico muy útil.

## Librería a usar

- **Propuesta:** `TFT_eSPI` (de Bodmer). Es la estándar para ST7789 en ESP32 y va
  muy rápida porque usa el hardware SPI del chip.
- **Alternativa:** `Adafruit_ST7789` + `Adafruit_GFX`. Más lenta pero mejor
  documentada para principiantes.
- ⚠️ **Peculiaridad de `TFT_eSPI` que hay que saber:** los pines **no** se
  configuran en el código, sino en un archivo de configuración de la propia
  librería (`User_Setup.h`). Es poco intuitivo y es la causa #1 de "no me
  funciona la pantalla" con esta librería. Cuando lleguemos ahí te lo explico
  con calma.
- Se decidirá formalmente en un ADR cuando confirmemos el controlador.

## Cuánto texto cabe realmente

Estimación con una fuente de 8×16 píxeles:

- **~30 caracteres por línea** × **~15 líneas**

Compárala con la OLED de 0.96" que se iba a comprar: ~20 caracteres × 4 líneas.

Ahí está la razón de fondo del [ADR-0004](../1-decisiones/ADR-0004-pantalla-gmt130.md):
esta diferencia es lo que hace que mostrar letras (RF-04) pase de "prácticamente
inviable" a "hay que probarlo".

⚠️ **Pero ojo con el RNF-01** (que se lea a 60 cm): 240 píxeles en 1.3 pulgadas
significa píxeles diminutos. Una fuente de 8×16 se va a ver **nítida pero
probablemente demasiado chica para leerla cómodo**. Muy probablemente haya que
usar fuentes más grandes y aceptar menos caracteres. Eso se decide midiendo, con
la pantalla enfrente — no en el papel.

## Notas y sorpresas

*(Se va llenando conforme la usemos.)*
