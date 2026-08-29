# Bitácora de inventario

Todo movimiento de material se registra aquí, **en orden cronológico inverso**
(lo más nuevo arriba). El [INVENTARIO.md](INVENTARIO.md) dice *cómo están las
cosas hoy*; este log dice *cómo llegaron a estar así*.

## Por qué existe este archivo

Porque el inventario solo tiene el estado actual. Cuando dentro de tres meses te
preguntes *"¿por qué tengo dos ESP32 si compré uno?"* o *"¿este OLED lo quemé yo
o ya venía mal?"*, la respuesta está aquí.

**Regla clave:** un componente marcado `Dañado` en el inventario se ignora por
completo al planear un proyecto. La **única** forma de que vuelva a contar es que
tú escribas aquí una entrada de tipo `REPARACIÓN` o `REPOSICIÓN`. Yo no lo hago
por mi cuenta.

## Tipos de entrada

| Tipo | Cuándo se usa |
|---|---|
| `ALTA` | Entra material nuevo al taller |
| `COMPRA` | Se hizo un pedido (aún no llega) |
| `RECEPCIÓN` | Llegó algo que estaba en tránsito |
| `ASIGNACIÓN` | Un componente se monta en un proyecto (→ `En uso`) |
| `LIBERACIÓN` | Un componente se desmonta y vuelve al cajón (→ `Disponible`) |
| `DAÑO` | Algo se rompió, se quemó o dejó de funcionar |
| `REPARACIÓN` | **Declaras que vas a reparar** algo dañado (→ `A reparar`) |
| `REPOSICIÓN` | **Declaras que vas a reemplazar** algo dañado (→ `A reponer`) |
| `BAJA` | Se descarta definitivamente |
| `PRÉSTAMO` / `DEVOLUCIÓN` | Sale o vuelve prestado |
| `NOTA` | Observación que no cambia el estado |

## Formato de una entrada

```markdown
## AAAA-MM-DD — `TIPO` — Nombre del componente (ID)
- **Qué pasó:** una o dos líneas.
- **Estado:** `Anterior` → `Nuevo`
- **Proyecto:** nombre o `—`
- **Nota:** contexto útil para tu yo del futuro.
```

---

# Historial

## 2026-08-29 — `NOTA` — ESP32-S NodeMCU (C-001)

- **Qué pasó:** Primer intento de detectar la placa desde la Mac (macOS 26.5.2,
  Apple Silicon). **No fue detectada.** El árbol USB completo solo muestra el
  dock DisplayLink, sus hubs, la webcam y el instalador de driver del propio dock.
  No aparece ningún chip USB-Serial (CP2102 / CH340 / FTDI) ni ningún puerto
  `/dev/cu.usbserial*`.
- **Estado:** `Disponible` → `Disponible` *(sin cambio: no hay evidencia de daño,
  lo más probable es cable o conexión)*
- **Proyecto:** tamagotchi
- **Nota:** Ojo — sí apareció un dispositivo del fabricante `wch.cn` (el mismo que
  hace los chips CH340), pero al abrirlo resultó ser el instalador de drivers del
  **dock DisplayLink**, no la placa. Falso positivo descartado. Pendiente probar
  con otro cable USB (que sea de datos, no de solo carga) y conectando directo a
  la Mac en vez de al dock.

## 2026-08-29 — `ALTA` — ESP32-S NodeMCU (C-001)

- **Qué pasó:** Se registra la placa que ya tenías al arrancar el taller.
- **Estado:** — → `Disponible`
- **Proyecto:** — *(se asignará al Tamagotchi cuando la conectemos)*
- **Nota:** Falta confirmar el modelo exacto (cuántos pines, qué chip USB-Serial
  trae). Eso se documenta en
  `proyectos/tamagotchi/3-componentes/esp32-nodemcu.md` en cuanto la placa
  responda.
