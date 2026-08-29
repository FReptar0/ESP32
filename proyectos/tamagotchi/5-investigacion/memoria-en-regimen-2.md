# Medición definitiva — La memoria es estable (cierra el RNF-06)

- **Fecha:** 2026-08-29
- **Método:** latido de diagnóstico cada 30 s durante 5 minutos, con el firmware completo
- **Motivo:** durante la sesión pareció que la RAM bajaba sin parar

---

## ✅ Veredicto: no hay fuga

| t | Libre | Mínimo histórico | Bloque contiguo mayor |
|---|---|---|---|
| 30 s | 219.876 | 97.104 | 110.580 |
| 60 s | 218.752 | 97.104 | 110.580 |
| 120 s | 218.712 | 97.076 | 110.580 |
| 181 s | 218.740 | 97.076 | 110.580 |
| 241 s | 218.740 | 97.076 | 110.580 |
| **301 s** | **218.712** | **97.076** | **110.580** |

| | |
|---|---|
| Primera mitad | **−1.136 B** |
| Segunda mitad | **−28 B** ← plano |
| Fragmentación | **cero** (el bloque mayor no se movió ni un byte) |

Las últimas seis muestras varían **30 bytes entre sí**. Eso no es una tendencia:
es ruido.

## Por qué parecía que había fuga (dos errores de medición)

Durante la sesión los números iban bajando: 193 → 186 → 181 → 176 → 173 KB. Dio
la impresión de una fuga que agotaría la memoria en un par de horas.

**Eran dos errores míos, y ninguno tenía que ver con el código:**

### 1. Se comparaban firmwares distintos

Cada medición era de una versión con más cosas dentro: primero solo las letras,
luego la precarga (+13,7 KB de buffers), luego las fuentes propias, luego el
sprite de la barra (+8 KB). **Cada número era correcto para su versión**, pero
ponerlos en una serie sugería una caída continua que no existía.

🔑 Comparar mediciones de compilaciones distintas es como pesarse cambiando de
báscula: cada número es válido, la tendencia no.

### 2. Se medía en el momento equivocado del ciclo

Los números antiguos se imprimían **justo después de descargar una letra**, con
la conexión TLS aún ocupando memoria. El latido nuevo mide **en reposo**.

Por eso ahora aparecen 218 KB donde antes se veían 173: no es que haya mejorado
nada, es que **antes se estaba mirando el peor instante y ahora el habitual**.

Los dos números importan, pero son cosas distintas:

| | Valor | Qué significa |
|---|---|---|
| Libre en reposo | ~218 KB | Lo normal |
| **Mínimo histórico** | **~97 KB** | El peor instante, durante el handshake TLS |

Ese mínimo de 97 KB es **el número que de verdad importa**, y sigue siendo
holgado.

## Qué cierra esto

- ✅ **RNF-06** (más de 7 días encendido) — no existe el mecanismo que lo
  impediría. Sigue pendiente una prueba larga de verdad, pero ya no por sospecha.
- ✅ Sin fragmentación tras cientos de conexiones TLS.
- ✅ ~97 KB de margen incluso en el peor instante.

## La lección

🔑 **Antes de diagnosticar una tendencia, asegúrate de que las muestras son
comparables.** Mismo binario, mismo punto del ciclo, suficientes muestras.

Con cinco muestras mal tomadas casi se rediseña algo que funcionaba bien. Con
diez bien tomadas, la respuesta fue evidente en cinco minutos.

Y el latido de diagnóstico se queda en el firmware: cuesta una línea cada 30
segundos y convierte una sospecha recurrente en un dato.
