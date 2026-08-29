# ¿Qué habría que pagar para que fuera "tiempo real"?

- **Fecha:** 2026-08-29
- **Pregunta:** *"¿cuál es el tradeoff para que parezca tiempo real?"*

---

## Lo primero: la mitad ya es tiempo real

Conviene separar dos cosas que se comportan muy distinto:

| | Cómo se actualiza | ¿Tiempo real? |
|---|---|---|
| **Barra de progreso** | El ESP32 la calcula con su propio reloj | ✅ Sí, 10 veces por segundo |
| **Sincronía de la letra** | Igual: reloj interno | ✅ Sí |
| **Pausa / cambio de canción** | Hay que **preguntarle a Spotify** | ❌ Depende del sondeo |

La barra avanza suave porque **no se pregunta**: se sabe el progreso y la
duración, y el resto es aritmética. Esa parte no tiene coste y ya es instantánea.

Lo que cuesta son los **eventos**: darle a pausa, saltar de canción, cambiar de
playlist. Eso solo lo sabe Spotify.

## Por qué hay que preguntar

**La Web API de Spotify es solo de consulta.** No existe forma de que Spotify
avise al aparato cuando algo cambia — no hay webhooks ni notificaciones push para
el estado de reproducción.

Un aparato que "se entera solo" necesitaría que el servidor mantuviera una
conexión abierta y empujara los cambios. Spotify no ofrece eso. Así que la única
vía es **preguntar cada cierto tiempo**, y ahí está todo el compromiso.

## El compromiso, con números

El límite de Spotify se mide sobre una **ventana móvil de 30 segundos**:

| Intervalo | Peticiones / 30 s | Retraso máximo | Riesgo |
|---|---|---|---|
| 5 s | 6 | 5 s | Ninguno |
| **2 s** | **15** | **2 s** | Bajo ← **actual** |
| 1 s | 30 | 1 s | Medio |
| 0,5 s | 60 | 0,5 s | Alto |

⚠️ **Spotify no publica el número exacto para el modo desarrollo.** Por eso el
firmware lleva un freno automático: si alguna vez responde `429`, sube el
intervalo solo.

## El coste que no es obvio: el handshake

El límite de peticiones no es lo único que se paga. **Cada consulta abre una
conexión TLS nueva**, y eso cuesta:

| | |
|---|---|
| Tiempo del handshake | ~1.000 ms |
| Pico de memoria | ~50 KB |

Con el intervalo en 2 segundos, **la mitad del tiempo el ESP32 está negociando
cifrado**. Bajar a 1 segundo significaría que casi no hace otra cosa.

🔑 **Ese es el verdadero techo, no el límite de Spotify.**

## Las tres formas de mejorarlo

### 1. Reutilizar la conexión (*keep-alive*) — la buena

En vez de abrir y cerrar una conexión por consulta, mantener una abierta y
reutilizarla. **Elimina el handshake**, que es el 90% del coste.

- **A favor:** con esto, sondear cada segundo saldría casi gratis.
- **En contra:** hay que gestionar conexiones que se caen o caducan, y mantiene
  memoria ocupada de forma permanente. Más código y más formas de fallar.

### 2. Ráfagas al detectar actividad — la lista

La idea: **cuando estás tocando Spotify, quieres respuesta inmediata; cuando no,
da igual.**

```
Estado normal          → preguntar cada 3 s
Se detecta un cambio   → preguntar cada 0,5 s durante ~15 s
Se acaba la ráfaga     → volver a 3 s
```

Si le das a pausa, la primera consulta lo detecta y a partir de ahí el aparato
entra en "modo atento": si saltas tres canciones seguidas, las tres se ven al
instante.

- **A favor:** se siente inmediato **justo cuando importa**, y en reposo consume
  menos que ahora.
- **En contra:** la *primera* detección sigue tardando lo mismo. Mejora la
  sensación, no el peor caso.

### 3. Predecir el final de la canción — ya está hecho

El ESP32 sabe cuánto dura y por dónde va, así que en los últimos 15 segundos ya
pregunta cada segundo. **Los cambios naturales de canción ya se detectan casi al
instante**; lo que tarda son los saltos manuales.

## Recomendación

**Combinar la 1 y la 2.** El *keep-alive* quita el coste real, y las ráfagas
ponen la agilidad donde se nota.

Con las dos, un intervalo base de 3 segundos en reposo y medio segundo en ráfaga
daría sensación de instantáneo, gastando **menos** peticiones que ahora.

🔑 **La lección general:** "tiempo real" casi nunca significa *preguntar más
rápido*. Significa **preguntar mejor** — calcular lo que se pueda en local,
predecir lo predecible, y reservar las preguntas para cuando de verdad puede
haber cambiado algo.
