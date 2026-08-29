# Cómo funciona el inventario

Dos archivos, dos trabajos distintos:

| Archivo | Responde a la pregunta | Se lee como |
|---|---|---|
| [`INVENTARIO.md`](INVENTARIO.md) | *¿Qué tengo **hoy** y en qué estado?* | Una tabla, siempre actualizada |
| [`LOG.md`](LOG.md) | *¿**Cómo** llegó a estar así?* | Un diario, se agrega abajo nunca se borra |

Es la misma idea que un saldo bancario y el estado de cuenta: el saldo lo miras
de un vistazo, los movimientos te explican de dónde salió.

---

## Es de uso general, a propósito

No hay un inventario por proyecto. Hay **uno solo**, aquí, y todos los proyectos
lo consultan.

¿Por qué? Porque el ESP32 es físicamente uno. Si el Tamagotchi lo está usando,
no está disponible para otro proyecto, y un inventario por proyecto te haría
creer que sí. Un solo inventario no puede mentirte sobre eso.

Cuando un proyecto toma un componente, se anota en la columna `Proyecto` y el
estado pasa a `En uso`. Cuando lo desmontas, vuelve a `Disponible`.

---

## La regla de los componentes dañados

Esta es la regla que más importa, y funciona así:

```
Se te quema un OLED
   ↓
Entrada `DAÑO` en el LOG  →  Estado `Dañado`
   ↓
A partir de aquí, para mí ese componente NO EXISTE.
No lo propongo, no lo cuento, no lo incluyo en ningún diseño.
   ↓
   ├── Tú escribes `REPARACIÓN` en el LOG  →  Estado `A reparar`  →  vuelvo a contarlo
   └── Tú escribes `REPOSICIÓN` en el LOG  →  Estado `A reponer`  →  vuelvo a contarlo
```

El punto es que **la decisión de recuperar algo es tuya, no mía**. Yo no voy a
asumir que vas a arreglar una pantalla quemada. Pero en cuanto lo digas, lo
retomo en la planeación.

`Dañado` no se borra nunca. Sirve para que dentro de un año veas *"ya quemé tres
de estos, algo estoy haciendo mal"* — que es información valiosísima cuando estás
aprendiendo.

---

## Los estados, y por qué son estos

Podrían ser solo tres (`tengo` / `lo estoy usando` / `se rompió`), pero se
quedan cortos en la práctica:

**Los que significan "lo tengo físicamente":**

- `Disponible` — en el cajón, libre. El estado normal.
- `Reservado` — sigue en el cajón pero ya lo apartaste para algo. Evita que lo
  uses en otro lado y luego te falte.
- `En uso` — montado en un proyecto. No lo tienes libre.
- `Dañado` — lo tienes, pero no sirve. Se ignora.

**Los que significan "todavía no lo tengo, pero contará":**

- `Por comprar` — sabes que lo necesitas, no lo has pedido.
- `En tránsito` — ya lo pediste, va en camino.
- `A reparar` — está roto pero declaraste que lo arreglarás.
- `A reponer` — está roto pero declaraste que comprarás otro.

**Los que significan "ya no está":**

- `Prestado` — lo tiene alguien más.
- `Baja` — se descartó, se perdió o se regaló. Queda solo el registro.

La distinción entre `Por comprar` y `En tránsito` importa más de lo que parece:
la primera te dice *"tengo que ir a comprarlo"*, la segunda *"solo hay que
esperar"*. Cuando estés armando algo un sábado, esa diferencia decide si puedes
avanzar o no.

---

## Cómo actualizarlo

**Pídemelo en lenguaje normal.** Por ejemplo:

> *"llegó el OLED que pedí"*
> *"se me quemó un ESP32, voy a comprar otro"*
> *"presté el multímetro"*

Yo me encargo de escribir la entrada en el `LOG.md` con la fecha, ajustar el
estado en el `INVENTARIO.md` y mantener los dos consistentes.

Si prefieres hacerlo a mano, respeta el formato de entrada que está documentado
en el `LOG.md` — así los dos archivos se siguen entendiendo entre sí.
