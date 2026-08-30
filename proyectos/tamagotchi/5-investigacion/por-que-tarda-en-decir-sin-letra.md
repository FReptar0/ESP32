# Por qué tarda en decir "sin letra" — y por qué el arreglo obvio es falso

- **Fecha:** 2026-08-29
- **Reportado:** *"con una canción sin letra tarda mucho en determinar que no tiene letra, alrededor de 12 s"*
- **Estado:** ⚠️ **Sin resolver.** Se intentó un arreglo, empeoró las cosas y se revirtió.
- **Confianza:** alta en las mediciones (tomadas de la placa); el arreglo está descartado por prueba directa.

> ⚠️ **Ojo, se mezclaron dos fallos distintos.** El mismo día se reportó también
> *"una canción pasó completa sin letra"*. Eso **no** era lentitud: era una
> carrera entre la precarga y el cambio de canción, ya arreglada
> ([`concurrencia-y-carreras.md` §4](concurrencia-y-carreras.md)). Lo de este
> documento —tardar ~9 s en **rendirse**— sigue sin resolver.
>
> 🔑 Dos síntomas parecidos el mismo día no son el mismo fallo. Separarlos
> antes de investigar habría ahorrado un revert.

---

## 1. La medición: a dónde se van los segundos

Instrumentando cada petición con `millis()` en la placa real:

| Petición | Resultado | Tiempo |
|---|---|---|
| `exacta` (`/api/get`) | HTTP **404** | **4151 ms** |
| `búsqueda` (`/api/search`) | 200, **0 candidatos** | **4095 ms** |
| **TOTAL "sin letra"** | | **9253 ms** |

Y para comparar, una canción **con** letra: una sola petición, **3569–4382 ms**.

También se midió la espera del turno de red (el mutex): **0 ms y 294 ms.**

## 2. 🔑 El hallazgo: los 4 s no son descargar, son *marcar el teléfono*

La petición que devolvió **404 no trae contenido ninguno** — cero bytes de
cuerpo. Y aun así tardó 4151 ms.

Si no se descargó nada, esos 4 segundos no pueden ser transferencia. Son
**abrir la conexión**: resolver `lrclib.net`, el saludo TCP y sobre todo el
**apretón de manos TLS** — el intercambio de certificados que cifra el canal,
que en un ESP32 es criptografía lenta.

> **La analogía:** cuesta marcar, no hablar. Cada pregunta a LRCLIB es colgar y
> volver a marcar.

Por eso una canción sin letra tarda el doble que una con letra: **paga dos
apretones de manos en vez de uno.** Y si la búsqueda hubiera devuelto 3
candidatos serían **5 conexiones ≈ 21 s**.

## 3. ❌ El arreglo que parecía obvio, y por qué está mal

**La idea:** las 5 peticiones van al mismo servidor. Reutilizar la conexión
(`HTTPClient::setReuse(true)`) hace que el apretón de manos se pague una sola
vez — la 2ª petición baja de ~4000 ms a ~300 ms. Verificado que la librería lo
soporta: `HTTPClient::connect()` reutiliza el socket si sigue conectado.

**Se implementó, se cargó en la placa, y el resultado fue peor.**

Para reutilizar la conexión hay que encadenar las peticiones sin soltar el turno
de red. Y el hilo de red es **el único sentido del aparato**: mientras está
ocupado, nadie le pregunta a Spotify qué suena. Los cambios de canción pasaron a
notarse tardísimo.

Es exactamente el fallo que ya estaba documentado en
[`cancelacion-de-busquedas.md`](cancelacion-de-busquedas.md), que además avisaba
de la variante en la que se volvió a caer — **la precarga no se cancela nunca**,
así que precargar una canción sin letra bloqueaba a Spotify de principio a fin.

🔑 **Una medición correcta no basta para justificar un cambio.** Los 4 s eran
reales. La conclusión era falsa, porque el coste que importaba no era el tiempo
de la búsqueda sino **el tiempo que el aparato pasa ciego**.

🔑 **Optimizar consiste en elegir qué recurso gastar.** Aquí hay tres en
conflicto —tiempo de búsqueda, reactividad y memoria— y solo se pueden tener
dos. El diseño actual sacrifica el primero **a propósito**.

## 4. ¿Puede Spotify decirnos de antemano si hay letra? — **No**

Verificado el 2026-08-29 contra documentación oficial:

| Comprobación | Resultado |
|---|---|
| Campo en el objeto `Track` | ❌ Ninguno de sus 20 campos. `explicit` es "contenido explícito", **no** "tiene letra" |
| Endpoint de letras en la Web API | ❌ No existe |
| Changelogs de feb / mar / may 2026 | ❌ No añaden nada de letras |

Las letras de la app de Spotify vienen de Musixmatch por un endpoint **interno**
(`spclient…/color-lyrics/v2`) que no forma parte de la Web API, exige un token
del reproductor web y va contra los términos de uso.

⚠️ **Descartado también por diseño:** el aparato es un regalo y debe seguir
funcionando solo. Una dependencia no documentada se rompe sin avisar y sin que
nadie pueda repararla.

## 5. Lo que sí queda por explorar

Ordenado por lo que parece más prometedor:

1. **Recordar los fallos.** Una lista corta de "esta canción no tiene letra" en
   memoria evita repetir la búsqueda al volver a sonar. No arregla la primera
   vez, pero es barato y no toca la red.
2. **Abaratar el apretón de manos**, que es el coste de fondo de *todo* el
   proyecto. Reanudación de sesión TLS no existe en el core actual
   (arduino-esp32 **2.x**, comprobado: no hay API de sesión). Subir de core
   chocaría con el RNF-04 (versiones clavadas) y habría que medirlo.
3. **Aceptarlo.** La precarga ya tapa la mayoría de los casos: cuando la
   siguiente canción no tiene letra, al llegar se muestra `sin letra` al
   instante. El problema solo se ve al **saltar** a una canción sin letra.

---

## Fuentes

- [Get Track — objeto Track completo](https://developer.spotify.com/documentation/web-api/reference/get-track)
- [Web API Changelog — mayo 2026](https://developer.spotify.com/documentation/web-api/references/changes/may-2026)
- [Web API Changelog — febrero 2026](https://developer.spotify.com/documentation/web-api/references/changes/february-2026)
- Mediciones propias sobre la placa, 2026-08-29
