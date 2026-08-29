# El reloj daba saltos, y cómo se hace una transición sin memoria extra

- **Fecha:** 2026-08-29

---

## 1. Por qué el tiempo retrocedía

**Síntoma:** el número de tiempo transcurrido daba un saltito cada segundo y
medio, aproximadamente.

**Dos causas, y las dos se arreglaron.**

### Causa A — la corrección de golpe

Entre consulta y consulta el progreso se **calcula** con el reloj del ESP32. Ese
cálculo nunca coincide exactamente con lo que Spotify reporta: hay unos cientos
de milisegundos de diferencia, por la red y porque cada uno mide a su manera.

Al llegar cada respuesta se sobrescribía el progreso **de golpe**. Si nuestra
cuenta iba 300 ms adelantada y el segundo acababa de cambiar, el número
**retrocedía**:

```
1:24  ->  1:23  ->  1:24
```

**Arreglo:** distinguir dos situaciones.

| Diferencia | Qué significa | Qué se hace |
|---|---|---|
| **> 1,5 s** | Alguien movió la canción o cambió de pista | Resincronizar de golpe |
| **≤ 1,5 s** | Solo deriva acumulada | Corregir un **20%** cada vez |

Corrigiendo poco a poco, el reloj se acerca al valor correcto sin dar nunca un
salto visible.

🔑 Es la misma idea que usan los relojes de red: **no se ajustan de un tirón, se
van arrastrando** hacia la hora correcta. Un salto atrás en un reloj se nota
muchísimo más que un error de 300 ms que nadie percibe.

### Causa B — el parpadeo del repintado

El bloque de tiempos se borraba y redibujaba **10 veces por segundo**, aunque el
texto fuera idéntico 9 de esas 10. Ese borrado constante se percibe como
parpadeo.

**Arreglo:** guardar lo último pintado y salir sin hacer nada si no ha cambiado.
Ahora se repinta **una vez por segundo**, cuando de verdad cambia el número.

## 2. La transición entre frases, sin gastar memoria

**Lo que se ve:** la frase nueva no aparece de golpe, sino que se aclara desde
gris oscuro hasta blanco en unos 170 ms.

**Lo interesante es que no cuesta nada.** El enfoque obvio sería usar un buffer
intermedio para mezclar, y para la zona de la letra (240 × 112 píxeles) eso serían
**53 KB** — una cuarta parte de la memoria libre.

No hace falta. Las letras de la fuente son de **un solo bit**: cada píxel está o
no está. Si se dibuja el mismo texto encima con un gris más claro, **solo se
aclaran esos mismos píxeles**, y no hay que borrar nada.

```
paso 1: gris 70   ┐
paso 2: gris 110  │  el mismo texto,
paso 3: gris 150  │  siempre encima,
paso 4: gris 190  │  cada vez más claro
paso 5: gris 225  │
paso 6: blanco    ┘
```

Sin borrado no hay parpadeo. Y como solo se van aclarando píxeles ya existentes,
el resultado es un fundido limpio con **cero memoria adicional**.

Lo único que se guarda es cómo quedó repartida la frase en renglones, para no
recalcularlo en cada paso.

🔑 **Un efecto visual no siempre necesita un buffer.** Aprovechando que el
contenido no cambia entre fotogramas —solo su color, y siempre hacia más claro—
se consigue lo mismo dibujando encima.

## 3. De regalo: tres candidatos de letra en vez de uno

Observado en la práctica: una entrada de LRCLIB puede existir y responder
`HTTP 200` **pero no tener letra sincronizada**, solo el texto plano.

```
[Letra] mejor coincidencia: id=23275899 (±0 s)
[Letra] por id 23275899 -> HTTP 200
[Letra] ❌ no encontrada          <- existía, pero sin sincronizar
```

Quedarse con el mejor por duración y rendirse desperdicia las demás versiones,
que muchas veces sí la tienen.

**No se puede saber de antemano cuál tiene letra sincronizada sin descargarla** —
ese campo es justo el que pesa. Así que ahora se guardan los **tres** mejores por
parecido de duración y se prueban por orden. Cada intento son ~8 KB.
