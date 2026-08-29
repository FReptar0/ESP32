# Medición — ¿Hay fuga de memoria en el ciclo de consultas?

- **Fecha:** 2026-08-29
- **Relacionado con:** RNF-06 (7 días encendido sin colgarse), [ADR-0002](../1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md)
- **Método:** 31 consultas consecutivas a Spotify, leyendo `ESP.getFreeHeap()` en cada una

---

## ✅ Veredicto: no hay fuga

La memoria baja durante las primeras ~16 consultas y después **se estabiliza**.

## Los datos

```
201448 201256 201032 200776 200572 200356 200148 199896   ← bajando
199688 199444 199220 199000 198788 198548 198320 198120   ← bajando
────────────────────────────────────────────────────────
198116 198088 198128 198108 198136 198120 198132 198092   ← plano
198124 198100 198128 198128 198120 198004 198012          ← plano
```

| | |
|---|---|
| Caída inicial | −3.3 KB en 16 consultas |
| Estado estable | ~198.1 KB |
| Variación en régimen | ±130 B, **sin tendencia** |

## Por qué esto no es una fuga

Al ver las primeras cinco muestras (−230 B cada una) la conclusión inmediata era
"fuga de memoria". A ese ritmo la placa se habría quedado sin RAM en poco más de
una hora, y el RNF-06 sería inalcanzable.

Pero al mirar más tiempo, **la caída se detiene**. Los últimos 15 valores oscilan
alrededor de 198.1 KB sin bajar más.

Eso es el **calentamiento del heap**, no una fuga: los buffers internos de TLS,
la pila de red y los grupos de reserva del asignador crecen hasta su tamaño de
trabajo y ahí se quedan. Una fuga real no se aplana nunca.

> 🔑 **La lección:** un número que baja no es automáticamente una fuga. Hay que
> observar hasta que se aplane — o hasta convencerse de que no lo hará. Cinco
> muestras no alcanzan; treinta sí.
>
> La forma de distinguirlas: una fuga es **lineal e infinita**; el calentamiento
> es **asintótico**. Si tras N ciclos el valor deja de moverse, no era fuga.

## Qué respalda esto

- ✅ **RNF-06** (más de 7 días encendido sin colgarse) es alcanzable. Queda por
  confirmar con una prueba larga de verdad, pero el mecanismo que lo impediría
  no existe.
- ✅ Refuerza el spike de TLS: la memoria se recupera al cerrar cada conexión.
- ✅ Quedan ~198 KB libres en régimen, margen de sobra para la pantalla.

## Pendiente

Una prueba de resistencia real: dejar la placa corriendo **24 h o más** y volver
a medir. El plateau de 200 segundos es evidencia fuerte, no definitiva.
