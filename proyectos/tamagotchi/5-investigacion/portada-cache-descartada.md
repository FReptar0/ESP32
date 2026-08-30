# Guardar la portada en memoria: probado, medido y descartado

- **Fecha:** 2026-08-30
- **Pregunta que lo originó:** *"¿por qué cuando volvemos a la pantalla de la portada tiene que recargar?"*
- **Veredicto:** ❌ **Descartado.** Ahorra 1 s y cuesta el montón partido y tirones repetidos.
- **Confianza:** alta — medido antes y después, con el ritmo del bucle instrumentado.

---

## La idea, que era razonable

La portada se libera de memoria nada más pintarla. Eso obliga a volver a
descargarla cada vez que se entra al modo: ~1 s de spinner para acabar mostrando
exactamente lo mismo que hace un momento.

Guardarla parecía barato. Medido sobre carátulas reales de Spotify a 300×300:
entre **6 y 57 KB**, mediana ~30 KB. Con ~150 KB libres, parecía caber de sobra.

Se implementó con una válvula de seguridad: soltarla al cambiar de canción o
cuando la RAM bajara de un umbral.

## Lo que la medición enseñó

### 1. El montón se partió, y no se recompuso

| | Sin caché | Con caché |
|---|---|---|
| RAM libre | 137–187 KB | 79–154 KB |
| **Bloque contiguo mayor** | **77.812 / 110.580** | **45.044, clavado** |

La memoria libre seguía siendo mucha. Lo que se hundió fue el **hueco seguido
más grande**, un 60 %.

> **Por qué:** un bloque de 30–57 KB que se queda quieto mientras a su alrededor
> se piden y liberan conexiones cifradas parte el montón en dos. Los huecos
> quedan a los lados, y no se pueden juntar: un ESP32 no tiene quien le mueva
> los muebles.

Y no vuelve solo: sin la caché el bloque regresa a 110.580 en cuanto se cierra
una conexión; con ella se quedó en 45.044 indefinidamente.

### 2. La válvula provocó un vaivén

Con el umbral en 100 KB, la portada se soltaba y se rebajaba cada pocos
segundos, porque la RAM ronda esa cifra cuando hay una conexión abierta.

Se bajó a 70 KB y **volvió a pasar**, solo que menos: una portada de 57 KB más
una conexión bastan para cruzar ese umbral.

```
13:24:03  [Portada] soltada (memoria justa) · RAM 68880 B
13:24:07  [Portada] bajada 57969 B
13:24:08  [Portada] dibujada en 437 ms
```

🔑 **Una caché que se rinde demasiado pronto es peor que no tenerla:** paga el
coste de guardar y encima el de volver a traer.

### 3. Y cada rebajada congelaba la pantalla

Aquí está lo que el usuario notó como *"la pantalla brinca"*. Se instrumentó el
ritmo del bucle de dibujo y la correlación fue exacta:

```
[Portada] dibujada en 437 ms   ↔   peor fotograma = 444 ms
[Portada] dibujada en 378 ms   ↔   peor fotograma = 461 ms
```

Decodificar el JPEG y volcarlo por SPI bloquea el núcleo de dibujo ~400 ms. En
ese rato **se congela todo**: el desplazamiento del texto, el avance de la letra
y la lectura del joystick.

Una vez por canción es imperceptible. **En bucle, por culpa del vaivén, es un
aparato que tiembla.**

## El balance

| | Sin caché | Con caché |
|---|---|---|
| Bloque contiguo | 77–110 KB | 45 KB |
| Congelones de 400 ms | 1 por canción | repetidos |
| Beneficio | — | ahorra 1 s al reentrar |

## Las lecciones

🔑 **Guardar algo grande durante mucho tiempo en un montón pequeño no cuesta
memoria: cuesta CONTINUIDAD.** Los dos números del latido —libre y bloque
mayor— miden cosas distintas, y el segundo es el que decide si un aparato
aguanta semanas encendido.

🔑 **El síntoma que se ve y la causa pueden estar a tres saltos de distancia.**
El usuario reportó un parpadeo; parecía un cable flojo; era una caché
fragmentando el montón, que disparaba una válvula, que forzaba una descarga,
que bloqueaba el dibujo 400 ms. Ninguno de esos pasos se adivina: se miden.

🔑 **Instrumentar el ritmo del bucle valió más que tres hipótesis.** El
`fotograma medio 5 ms · peor 444 ms` dijo en una línea lo que ninguna
suposición había acertado: no era lentitud crónica, eran parones puntuales.

## Lo que se conserva de este intento

- **La publicación atómica del buffer.** Al implementar la caché salió una
  carrera real: el núcleo 0 reservaba la imagen y anotaba a qué canción
  pertenecía en dos pasos, y el núcleo 1 se colaba en medio y la liberaba por
  "ser de otra canción". Ahora se publican dentro del mismo bloque protegido.
  Ese arreglo vale igual sin caché.
- **La medición del ritmo del bucle** en cada latido. Costó tres líneas y es lo
  que resolvió el caso.

## Lo que queda abierto

El congelón de ~300–400 ms **al dibujar una portada sigue ahí**, una vez por
canción. Es inherente: decodificar un JPEG en este chip cuesta eso. Durante ese
rato el joystick no se lee, así que un gesto justo en ese instante puede
perderse. No se ha visto molestar, pero está anotado.
