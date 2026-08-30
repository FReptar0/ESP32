# Auditoría del firmware — 5 fallos antes de añadir nada nuevo

- **Fecha:** 2026-08-29
- **Por qué:** antes de abrir requerimientos nuevos, comprobar que lo que hay aguanta.
- **Método:** revisión completa de `main.cpp`, `letras.h`, `texto.h` y `platformio.ini`, contrastada contra los ADRs y esta carpeta. Cada hallazgo se verificó en el código antes de tocarlo.

---

## Por qué esta auditoría no propuso tonterías

Se le dio una instrucción explícita: **leer primero los ADRs y la investigación**,
y no reportar como defecto nada que ya estuviera decidido a propósito.

Funcionó. Separó correctamente los defectos reales de las decisiones conscientes,
y dejó fuera —bien— cosas que *parecen* fallos y están razonadas: el NTP sin
timeout, los ~9 s para rendirse ante una canción sin letra, y la ausencia de
keep-alive (probada y revertida ese mismo día).

🔑 **Una auditoría sin el histórico delante genera ruido.** El histórico es lo que
distingue "esto está mal" de "esto se decidió así, y aquí está el porqué".

---

## 1. 🔴 La renovación del token saltaba el mutex de red

**Dónde:** `main.cpp`, en `tareaSpotify`.

`renovarToken()` abre su propia conexión TLS, y era **el único sitio del firmware
que lo hacía sin pedir turno**. Todos los demás lo respetan.

Es exactamente la condición que causó los **11 reinicios en 150 s** documentados
en [`concurrencia-y-carreras.md` §1](concurrencia-y-carreras.md): dos handshakes
simultáneos saturan el núcleo 0, IDLE0 no llega a ejecutarse y salta el watchdog.

El token caduca ~26 veces al día. Cada una podía caer sobre una descarga de letra.

**Arreglo:** envolver esa llamada en `CON_RED`.

> ⚠️ **La trampa al arreglarlo:** `consultar()` ya llama a `renovarToken()` ante un
> 401, y eso ya corre dentro de `CON_RED`. El mutex **no es recursivo**: envolver
> también esa segunda llamada habría sido un interbloqueo instantáneo. Se envolvió
> solo la de `tareaSpotify`.

**Verificado:** ✅ que no hay interbloqueo (el token renueva dentro del mutex, en
placa). ❌ el beneficio no es observable en una sesión: hay que esperar a que una
renovación coincida con una descarga.

## 2. 🔴 No existía "no puedo hablar con Spotify"

Había pantalla para *sin WiFi* y para *en silencio*, pero ninguna para *el WiFi va
bien y Spotify no contesta*. Si `renovarToken()` fallaba de forma persistente, el
bucle reintentaba cada 30 s **para siempre** sin tocar nada de lo que se dibuja.

El núcleo 1 seguía pintando con los datos viejos: el título de hace horas, la
barra llena al 100 %, la última frase de la letra fija. Indefinidamente.

**No es hipotético:** el `refresh_token` se invalida si el dueño cambia la
contraseña o revoca la app. El aparato estaría en casa de otra persona, sin
reinicio que lo arregle y sin forma de distinguirlo de un aparato roto.

**Arreglo:** estado `__sinspotify__` tras 3 fallos seguidos (~90 s), en **ámbar**.
El color no es decorativo: rojo ya significa *no hay WiFi, revisa el router*.
Ámbar dice *el router va bien, el problema está más allá*.

**Verificado:** ✅ **mirando la pantalla.** Se compiló una versión con el
`refresh_token` roto a propósito —sin tocar `secrets.h`—, se cargó, y a los ~41 s
apareció "Sin Spotify / reintentando..." en ámbar. Cero reinicios durante la
prueba. Después se restauró el firmware bueno desde una copia de seguridad.

## 3. 🟠 A los 49,7 días: una hora renovando el token sin parar

```cpp
tokenCaducaEn = millis() + (dur - 300) * 1000UL;   // se guarda
if (millis() > tokenCaducaEn)                      // ← se compara MAL
```

Era el **único** cálculo con `millis()` del firmware que no usaba resta. Al girar
el contador (2³² ms ≈ 49,7 días) la comparación se vuelve permanentemente cierta
hasta 55 minutos: renovación en cada vuelta del bucle, Spotify empieza a
rechazarlas, y con el hallazgo 1 sin arreglar eso era una tormenta de reinicios.

Es una **fecha fija**, y un regalo pensado para semanas sin tocarlo llega ahí.

**Arreglo:** `tokenCaducado()`, que compara por resta con signo.

> **La analogía:** es el cuentakilómetros de un coche viejo. Si marca 999.980 y
> preguntas *"¿he pasado ya de 1.000.020?"*, la respuesta es siempre no, porque
> ese número no cabe en el marcador. Pero *"¿cuánto he andado desde entonces?"*
> sigue funcionando aunque el marcador haya dado la vuelta.

**Verificado:** ❌ no es observable. Se confirmó que el resto de temporizadores del
firmware ya usaban la forma correcta.

## 4. 🟠 La barra de estado se escribía desde los dos núcleos

`barraEstado()` se llamaba fuera de todo mutex, y dentro **recortaba**
`nombreContexto` para que cupiera en la barra — o sea, el núcleo 1 escribía sobre
un `String` que el núcleo 0 reasigna al cambiar de playlist.

Un `String` de Arduino es puntero + longitud: asignarlo libera el buffer viejo,
reserva otro y copia. Dos núcleos escribiendo el mismo `String` es **liberar dos
veces el mismo bloque**. Eso no se ve en pantalla: corrompe el montón y revienta
minutos después en cualquier otro sitio, con una pila que no señala a nadie.

Solo se disparaba con nombres de playlist largos — de ahí que unas playlists lo
activaran y otras no.

**Arreglo:** el núcleo 1 dibuja desde **copias propias** (`nombreBarra`,
`descBarra`), refrescadas una vez bajo `CON_DATOS` cuando cambia el contexto, y
recorta la copia. El núcleo 0 ya no toca `estadoPintado`: levanta una bandera.

🔑 **La solución no era poner un mutex en el bucle de dibujo** (que corre ~200
veces por segundo), sino **copiar una vez cuando algo cambia**. El dato que se
dibuja deja de ser compartido, y entonces no hay nada que proteger.

**Verificado:** ✅ que no rompió el dibujo (contexto resuelto y pintado en placa).
❌ la carrera en sí no se puede provocar a voluntad.

## 5. 🟠 El `Retry-After` de los 429 no se leía nunca

El `ADR-0002` decidió *"respetar siempre el `Retry-After`"*. El código creía que
lo hacía:

```cpp
int e = https.header("Retry-After").toInt();
```

Pero `HTTPClient` **descarta todas las cabeceras de la respuesta** salvo las que
se le hayan apuntado antes con `collectHeaders()`, y eso no se llamaba en ningún
sitio. Así que `header()` devolvía cadena vacía —no un error— y `.toInt()` la
convertía en 0 tan campante: esperaba 1 s fijo dijera lo que dijera Spotify.

Y el log imprimía **"esperando 0 s"**, que habría desmentido el diagnóstico a
quien lo investigara.

🔑 **Una API que devuelve vacío en vez de fallar convierte un error de
programación en un dato falso.** Y un dato falso viaja hasta el registro, donde
ya parece verdad.

**Arreglo:** `collectHeaders()` con `Retry-After`, suelo de 5 s y tope de 60 s.

### Un fallo dentro del arreglo, que la auditoría no vio

La espera del 429 ocurría **dentro de `CON_RED`**. Subir el tiempo de espera a 60 s
habría dejado al hilo de letras bloqueado todo ese rato — y LRCLIB no tiene
ninguna culpa de que Spotify nos haya limitado.

Se cambió a apuntar `noConsultarAntesDe` y soltar el turno de inmediato.

🔑 **Antes de alargar una espera, mira qué cerraduras tienes en la mano.**

---

## Lo que la auditoría confirmó que está bien

- **El orden de los mutex es correcto y no hay interbloqueo posible.** El único
  anidamiento es `CON_RED` → `CON_DATOS`, siempre en esa dirección.
- **Seguridad de memoria en el parseo de letras: sin agujeros.** `parsearLRC`
  comprueba límites antes de cada escritura.
- **La marca de agua `generacion` es sólida** en los dos sentidos.
- **La recuperación de WiFi funciona.**

## Pendiente, anotado sin arreglar

- **Podcasts:** `/currently-playing` devuelve `item: null` para episodios si no se
  pide `additional_types=episode`. Se vería "(sin título)" sin artista y sin barra
  de progreso. 🟡 y **sin verificar**.
- **Código muerto:** `Letras::buscar()` ya no se llama desde ningún sitio, y su
  comentario dice que lo usa la precarga — que es falso desde la máquina de pasos.
- **Comentario con cifra caducada:** el de `RAM_MINIMA` cita "mínimo histórico
  ~59 KB", que es la medición *anterior* al mutex de red.
