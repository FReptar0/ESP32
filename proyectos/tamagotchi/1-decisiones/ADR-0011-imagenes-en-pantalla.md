# ADR-0011 — Decodificar JPEG por bloques, usar solo la versión de 300 px, y no guardarla

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-30
- **Relacionado con:** RF-13, RF-14 · [ADR-0008](ADR-0008-libreria-de-pantalla.md)

---

## Contexto

Los modos 3 y 4 muestran imágenes: la carátula del álbum y la de la playlist
elegida. Spotify las sirve como JPEG por HTTPS desde `i.scdn.co`, en varios
tamaños.

Las restricciones son de memoria, y son duras:

- La pantalla es de **240×240**. Descomprimida a 16 bits son **115 KB** — más de
  la mitad de la RAM libre del aparato.
- Una conexión cifrada cuesta **~50 KB** mientras está abierta.
- El hilo de letras necesita ese margen para su propia conexión. Sin él, la placa
  se reinicia — es el fallo que más costó arreglar en este proyecto.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- La RAM libre en régimen ronda los 150–185 KB.
- `TFT_eSPI` ([ADR-0008](ADR-0008-libreria-de-pantalla.md)) puede recibir la
  imagen por trozos, sin necesitar el fotograma entero.

**Lo que se midió antes de decidir** (carátulas reales, 2026-08-29/30):

| Tamaño | Peso | Pico con TLS |
|---|---|---|
| 640×640 | **161 KB** | **211 KB** ❌ no cabe |
| 300×300 | 26–57 KB | ~91 KB ✅ |
| 64×64 | 3 KB | trivial, pero ilegible en pantalla |

**Lo que asumimos sin verificar (y resultó falso):**

- ~~Que guardar la imagen en memoria sería barato.~~ Ver abajo.
- ~~Que todas las imágenes miden 300 px.~~ Las de playlist pueden ser 640, o un
  mosaico. Por eso ahora se le **pregunta a la imagen** cuánto mide.

---

## Decisión

**Tres cosas, y las tres salen de la memoria disponible:**

### 1. Decodificar por bloques con `TJpg_Decoder@1.1.0`

Saca la imagen en bloques de 16×16 y cada bloque se manda a la pantalla y se
olvida. **La imagen descomprimida nunca existe entera**, que es lo único que hace
esto posible con 115 KB de necesidad y 150 de disponibilidad.

### 2. Pedir siempre la versión de **300 px**

No es una preferencia estética: **la de 640 no cabe**. Con los 50 KB de la
conexión abierta, sus 161 KB dan un pico de 211 KB sobre ~185 disponibles.

⚠️ El ancho que se pide (`ANCHO_QUE_PEDIMOS = 300`) es **una constante aparte**
del tamaño que se está decodificando. Mezclarlos ya causó un fallo real: al
volverse variable, `consultar()` se puso a buscar portadas de 150 px —que no
existen— y la carátula dejaba de cargar.

### 3. **No guardar la imagen**: se libera nada más pintarla

Se probó lo contrario y **hubo que revertirlo**. Medido:

| | Sin caché | Con caché |
|---|---|---|
| Bloque contiguo mayor | **77–110 KB** | **45 KB, clavado** |
| Congelones de 400 ms | 1 por canción | repetidos en bucle |

🔑 **Guardar algo grande durante mucho tiempo en un montón pequeño no cuesta
memoria: cuesta continuidad.** Había 150 KB libres y aun así no cabía nada de 50,
porque estaban partidos en trozos. Y un ESP32 no tiene quien le recomponga los
huecos.

Detalle completo en
[`portada-cache-descartada.md`](../5-investigacion/portada-cache-descartada.md).

## Consecuencias

**A favor:**

- El montón vuelve siempre a su bloque contiguo sano de 110.580 B.
- Dibujar cuesta 170–440 ms, una sola vez por canción.
- La misma máquina sirve para las dos pantallas: solo cambia el encuadre.

**En contra:**

- **Al volver al modo, la imagen se rebaja**: ~1 s de spinner. Es el precio
  aceptado a cambio de no fragmentar.
- **Congelón de 300–440 ms** mientras se decodifica. Durante ese rato el joystick
  no se lee, así que un gesto justo en ese instante puede perderse. Anotado, no
  observado como molesto.
- La descarga y el dibujo viven en **núcleos distintos** —red en el 0, pantalla en
  el 1— y el relevo necesita una marca de a quién pertenece la imagen. Sin ella,
  la carátula de la canción salía como imagen de la playlist.

## Alternativas descartadas

| Alternativa | Por qué no |
|---|---|
| **Usar la de 640 px** | 211 KB de pico. Reinicia la placa |
| **Guardar la imagen descomprimida** | 115 KB permanentes. Ni de lejos |
| **Guardar el JPEG comprimido** | Probado y medido: fragmenta el montón. Ver arriba |
| **Recortar 300→240 en vez de reducir** | Perdería un 20 % de la carátula por lado. Reducir por proporción no pierde nada |
