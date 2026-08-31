# Batería LiPo 1000 mAh (C-006)

- **Estado:** 🟡 **Registrada, sin conectar todavía** (2026-08-30)
- **Inventario:** [C-006](../../../inventario/INVENTARIO.md)
- **Alimentación:** [`6-esquemas/alimentacion-por-bateria.md`](../6-esquemas/alimentacion-por-bateria.md)

---

## ⚠️ Antes de nada: esto no es una pila normal

Una pila AA se puede tirar, doblar o cortocircuitar y como mucho se calienta. Una
**LiPo de bolsa** guarda mucha energía en muy poco sitio y **no tiene carcasa
rígida**. Si se maltrata, no falla: arde.

Las tres reglas que no admiten excepción:

- ⚠️ **Nunca cortocircuites el rojo con el negro.** Ni un instante, ni "para
  probar". Una celda como esta puede soltar varios amperios sin inmutarse.
- ⚠️ **Nunca la perfores, dobles ni aprietes.** La bolsa plateada es todo lo que
  separa el interior del aire.
- ⚠️ **Si se hincha, deja de usarla.** Una LiPo abombada ya está estropeada por
  dentro y no se arregla.

Y la que más cuesta interiorizar:

> **Una batería no se puede apagar.** El interruptor apaga *lo que alimenta*, no
> la batería. Sus terminales tienen tensión mientras exista. Cuando sueldes,
> asume que están vivos — porque lo están.

## Lo que dice la propia celda

Está serigrafiado en la bolsa:

| | |
|---|---|
| Tensión nominal | **3,7 V** |
| Capacidad | **1000 mAh** |
| Energía | **3,7 Wh** |
| Lote | `202407` |
| Celdas | **Una** (por eso 3,7 V y no 7,4) |

El `+` está marcado junto al cable rojo.

### Qué significa "3,7 V" — y por qué es la clave de todo

**3,7 V es un promedio, no lo que mide.** Una celda LiPo recorre este camino:

```
4,2 V  recién cargada        <- el máximo, nunca se pasa de aquí
3,7 V  la mayor parte de su vida
3,0 V  vacía                 <- por debajo se estropea
```

🔑 **Y ese rango es el problema central del proyecto:** el ESP32 quiere 3,3 V
estables y la batería nunca los da. Empieza **por encima** y acaba **por debajo**.
No se puede conectar directamente en ningún caso. Ver el esquema.

## Cuánto va a durar

Estimación, **no medición**:

| | |
|---|---|
| ESP32 con WiFi | 80–160 mA (de su propia ficha) |
| Pantalla + retroiluminación | ~20–40 mA |
| **Total en marcha** | **~150–200 mA a 3,3 V** |
| Con las pérdidas de la conversión | ~200–250 mA de la batería |
| **Autonomía estimada** | **4 a 6 horas** |

⚠️ **Supuesto, no medido.** Falta comprobarlo con el aparato montado. Y hay un
detalle que puede cambiarlo bastante: el firmware consulta a Spotify **cada 2
segundos**, y transmitir por WiFi es lo que más consume.

## Cómo está ahora

Los dos cables **ya vienen soldados a `B+` y `B-`** del cargador
[C-007](tp4056-cargador.md). Eso está bien hecho: la batería nunca debe conectarse
directamente al aparato, siempre a través del cargador con protección.
