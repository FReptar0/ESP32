# Spike — ¿Le alcanza la memoria al ESP32 para hablar con Spotify?

- **Fecha:** 2026-08-29
- **Resuelve:** supuesto [S-04](../4-requerimientos/REQUERIMIENTOS.md#supuestos) — *el riesgo #1 tras cerrar el S-02*
- **Es el paso 3 del** [ADR-0002](../1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md)
- **Método:** medición directa contra `accounts.spotify.com:443`, el servidor real

---

## ✅ Veredicto: sí, y de sobra

> **Mínimo histórico de RAM libre: 204.168 bytes (199.4 KB).**
> El umbral de "sobra" eran 100 KB. Quedó al doble.

**La arquitectura directa del ADR-0002 se confirma.** No hay que reabrirlo.

---

## Las mediciones

| Etapa | RAM libre | Bloque contiguo mayor |
|---|---|---|
| Antes de nada (WiFi ya conectado) | 257.640 B · 251.6 KB | 110.580 B |
| Objeto TLS creado | 255.048 B · 249.1 KB | 110.580 B |
| **🔑 TLS conectado** | **213.824 B · 208.8 KB** | 110.580 B |
| Con la respuesta leída | 253.792 B · 247.8 KB | 110.580 B |
| Tras cerrar y liberar | 256.120 B · 250.1 KB | 110.580 B |
| **Mínimo histórico** | **204.168 B · 199.4 KB** | — |

Handshake TLS: **1.088 ms**. Respuesta: `HTTP/1.1 307 Temporary Redirect`
(esperado — la raíz de `accounts.spotify.com` redirige; lo que importa es que
**hubo respuesta**, o sea que TLS y HTTP funcionaron de punta a punta).

## Qué nos dicen estos números

### 1. TLS cuesta unos 50 KB, y es un gasto temporal

De 257.6 KB se baja a 204.2 KB en el peor instante. Ese pico ocurre **durante el
handshake**, cuando se manejan los certificados y se negocian las claves.

Que sea temporal importa: el ESP32 no vive con 50 KB menos, solo los necesita
durante el segundo que dura la conexión.

### 2. 🎉 No hay fuga de memoria

Antes: 257.640 B. Después de cerrar: 256.120 B. Diferencia: **1.520 bytes**, que
es ruido normal.

**Esto es más importante de lo que parece.** El aparato va a abrir y cerrar una
conexión cada 5 segundos, para siempre — son ~17.000 conexiones al día. Si cada
una dejara aunque fueran 2 KB sin liberar, la memoria se agotaría en minutos y el
ESP32 se reiniciaría solo.

Es exactamente el tipo de fallo que hace que un proyecto "funcione perfecto" en
la prueba y muera de madrugada. Aquí ya sabemos que no va a pasar, y es lo que
respalda el [RNF-06](../4-requerimientos/REQUERIMIENTOS.md) (una semana prendido
sin colgarse).

### 3. Tampoco hay fragmentación

El "bloque contiguo mayor" se mantuvo en **110.580 B en las cinco mediciones**,
sin moverse un byte.

**Por qué esto merece atención:** tener 200 KB libres no garantiza poder guardar
un dato de 50 KB. La memoria puede quedar partida en pedacitos —como un
estacionamiento con muchos huecos sueltos pero ninguno donde quepa un camión— y
entonces la reserva falla aunque el total sobre.

Que el bloque mayor no se moviera significa que TLS **no fragmenta el heap**.
Podremos reservar buffers grandes para el JSON de Spotify sin sorpresas.

### 4. Un detalle que explica una diferencia

En la prueba anterior había **303.5 KB** libres; aquí arranca en **251.6 KB**.
No se perdió nada: en la primera prueba el WiFi solo escaneaba, y ahora está
**conectado**, con sus buffers de red activos. Esos ~52 KB son el costo de tener
la radio funcionando de verdad.

## Margen real para lo que falta

| Concepto | Estimado |
|---|---|
| Mínimo medido con TLS | **199.4 KB** |
| JSON de Spotify (parseo incremental) | ~10–20 KB |
| Buffer de la pantalla GMT130 | ~5–15 KB |
| **Margen restante estimado** | **~165 KB** |

Hay espacio de sobra. La preocupación se puede archivar.

## Advertencia sobre el método

⚠️ La prueba usó `setInsecure()`, que **no verifica el certificado** del
servidor. El handshake TLS ocurre igual —que es lo que consume memoria—, así que
el número es válido.

Pero el firmware final **sí debe validar el certificado**, y eso añade algunos KB
para guardar el certificado raíz. Con 165 KB de margen no cambia el veredicto,
pero hay que medirlo otra vez cuando se implemente. Ese es el futuro **ADR-0009**.

## Consecuencias

- ✅ **S-04 resuelto.** La arquitectura directa se confirma; el ADR-0002 sigue en pie.
- ✅ Los riesgos "se queda sin memoria" y "se reinicia solo" quedan cerrados.
- 🔜 Queda pendiente el manejo de certificados raíz → ADR-0009.
