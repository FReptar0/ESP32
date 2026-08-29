# ¿Se pueden mostrar emojis? Sí — pero no como imágenes

- **Fecha:** 2026-08-29
- **Pregunta:** *"si hay emojis ¿es difícil de manejar o consume mucha memoria?"*

---

## La respuesta corta

**Guardarlos como imágenes sería carísimo. Dibujarlos cuesta prácticamente nada.**

## Por qué son caros como imágenes

Un emoji de verdad es un dibujo a color:

| | Cálculo | Coste |
|---|---|---|
| Un emoji a 20×20 px, color | 20 × 20 × 2 bytes | **800 bytes** |
| Los 20 más comunes | × 20 | 16 KB |
| Todos los que existen (~3.700) | × 3.700 | **~3 MB** |

3 MB no caben: el ESP32 tiene 4 MB de flash **en total**, y el firmware ya ocupa
1,1 MB. Y ni siquiera cubriría los emojis nuevos que salen cada año.

## Por qué son baratos dibujados

Pero mira lo que es realmente un corazón:

```
dos círculos arriba  +  un triángulo abajo
```

Tres llamadas a funciones de dibujo. **Eso es código, no datos**: unos cientos de
bytes de programa **para todos los corazones juntos**, porque el color es un
parámetro. 💜 💙 💚 💛 🖤 🤍 son el mismo dibujo con distinto color.

| | Coste |
|---|---|
| Dibujar el corazón | ~120 bytes de programa |
| Memoria por cada emoji mostrado | **0 bytes** |

🔑 **La diferencia entre guardar la respuesta y guardar la receta.** Una imagen es
la respuesta ya hecha, y hay que guardar una por cada caso. El dibujo es la
receta: sirve para todos.

## Qué se dibuja

| Emoji | Se dibuja como |
|---|---|
| 💜 💙 💚 💛 🧡 🖤 🤍 ❤️ 💕 💖… | Un corazón, en el color que toque |
| ⭐ ✨ 🌟 | Un destello de cuatro puntas |
| 🔥 | Una llama (círculo con punta) |
| 🎵 🎶 ♪ ♫ | Una nota musical |
| Cualquier otro | Un punto discreto |

Ese "cualquier otro" importa: **un emoji desconocido deja un punto, no un hueco**.
Un espacio vacío parece un fallo; un punto se lee como "aquí había algo".

## Un detalle que casi se escapa: los caracteres invisibles

`❤️` no es un carácter, son **dos**: `U+2764` (el corazón) más `U+FE0F` (un
"selector de variante" invisible que le dice al sistema que lo pinte a color).

También existe el "unificador de ancho cero" (`U+200D`), que pega emojis para
formar otros: 👨 + ZWJ + 👩 + ZWJ + 👧 = 👨‍👩‍👧.

Si esos invisibles se convirtieran en espacios, **cada emoji dejaría un hueco
fantasma al lado**. Se descartan explícitamente.

## Cómo llegan desde Spotify

Los emojis modernos ocupan **4 bytes** en UTF-8:

```
💜  =  F0 9F 92 9C  ->  U+1F49C  ->  corazón morado
🔥  =  F0 9F 94 A5  ->  U+1F525  ->  llama
✨  =  E2 9C A8      ->  U+2728   ->  destello   (este son 3)
❤️  =  E2 9D A4 EF B8 8F  ->  U+2764 + U+FE0F   (6 bytes, dos caracteres)
```

Verificado con una playlist real: `Alo 💜` se resolvió correctamente y el corazón
se dibuja en morado.

---

## De regalo: la descripción de la playlist

Se pide junto al nombre (`?fields=name,description`) y se muestra en gris, tras
un guion.

⚠️ **Viene con HTML.** Spotify devuelve la descripción tal cual la escribió el
usuario, con entidades (`&amp;` por `&`, `&#x27;` por `'`) y a veces etiquetas
`<a href="...">`. Sin limpiarla se leería literalmente `&#x27;` en pantalla.

Si el texto no cabe, **se desplaza**. Y para que no parpadee se usa un *sprite*:
un trozo de pantalla que se dibuja **en memoria** y se vuelca de golpe. Dibujando
directo se vería el borrado entre fotograma y fotograma.

Cuesta 8 KB (200 × 20 píxeles × 2 bytes) y es una de las razones por las que se
eligió `TFT_eSPI` en el [ADR-0008](../1-decisiones/ADR-0008-libreria-de-pantalla.md).

El texto se repite tras un hueco, para que el bucle se vea continuo en vez de
dar un salto al reiniciarse.


---

## Ajuste de color — el morado se veía azul

El primer violeta elegido fue `0x915C`. Descompuesto:

```
0x915C -> RGB(148, 40, 230)
             rojo      azul
```

**230 de azul contra 148 de rojo.** Técnicamente es morado, pero con esa
proporción el ojo lo lee como azulado — sobre todo en una figura pequeña, donde
hay pocos píxeles para juzgar el tono.

Corregido a `0xC1FD` → **RGB(197, 61, 239)**. Al subir el rojo de 148 a 197, el
azul deja de dominar y el violeta se reconoce de inmediato.

De paso se aclaró el azul del 💙 (`0x1C9F` → `0x2D5F`), para que no se confunda
con el violeta nuevo.

🔑 **Con figuras pequeñas hay que exagerar el tono.** Un color "correcto" sobre el
papel puede leerse mal cuando solo ocupa 12 × 12 píxeles: el ojo tiene menos
información para decidir, y se va al canal dominante.

## El separador: un guion no se veía

Entre el nombre de la playlist y su descripción había un `-`. A 14 píxeles de
alto, un guion es **una raya de tres píxeles entre dos textos grises**: se pierde.

Sustituido por un **punto dibujado**, reutilizando el mismo icono que se usa para
los emojis desconocidos. Dos ventajas:

- Es redondo y se ve, aunque sea pequeño.
- El icono ya mide 15 px de ancho con el punto centrado, así que **el aire a los
  lados sale gratis** — que era el otro problema: la descripción quedaba pegada.
