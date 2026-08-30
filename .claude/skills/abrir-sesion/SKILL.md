---
name: abrir-sesion
description: "Retoma el taller de electrónica: pregunta a la placa en qué estado está, lo contrasta con lo que dicen los documentos, y resume dónde se quedó todo y cuál es el siguiente paso. Úsala al empezar una sesión."
allowed-tools:
  - Read
  - Bash
  - Glob
  - Grep
---

# Retomar el taller

Pareja de [`/cerrar-sesion`](../cerrar-sesion/SKILL.md). Sirve para volver a un
proyecto sin haber estado presente cuando se dejó.

> **La regla de esta skill: pregunta primero, lee después.**
>
> Es tentador empezar leyendo la bitácora y dar por buenas sus conclusiones.
> Pero los documentos se escribieron **antes** de apagar, y desde entonces el
> mundo pudo cambiar: alguien desenchufó la placa, cambió la contraseña del
> WiFi, o Spotify actualizó su API.
>
> Ya pasó dos veces: un `CLAUDE.md` que llevaba una sesión entera diciendo que
> la placa no se detectaba, y una playlist devolviendo 404 que nadie había
> notado. **Ambas se descubrieron comprobando, no leyendo.**

---

## Paso 1 — Preguntarle a la placa

Antes de abrir un solo documento:

```bash
cd "$(git rev-parse --show-toplevel)"

echo "=== ¿Hay placa? ==="
ls /dev/cu.usbserial* 2>/dev/null || echo "  ❌ ninguna conectada"

echo "=== ¿Puerto libre? ==="
lsof /dev/cu.usbserial-0001 2>/dev/null || echo "  ✅ libre"
```

Y si hay placa, **pregúntale qué versión trae**. El firmware lo dice al arrancar:

```bash
cd proyectos/tamagotchi
./2-entorno/.venv/bin/python - <<'PY'
import serial, time
p = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=0.5)
p.setDTR(False); p.setRTS(True); time.sleep(0.15); p.setRTS(False)
time.sleep(0.1); p.reset_input_buffer()
fin = time.time() + 20; buf = b""
while time.time() < fin:
    d = p.read(4096)
    if d: buf += d
p.close()
print(buf.decode('utf-8', errors='replace')[-1200:])
PY
```

De ahí sale lo que ningún documento puede contar:

- **Qué firmware hay cargado** (`=== Tamagotchi · firmware abc1234 ===`)
- **Si arranca bien** o se queda en algún paso
- **Si sigue funcionando**: canción, letra, memoria libre
- **Si se está reiniciando**: busca `rst:0x` o `Guru Meditation`

## Paso 2 — Contrastar con Git

```bash
git log --oneline -5
git status --short
```

Compara lo que **dijo la placa** con lo que dice Git:

| Lo que ves | Qué significa |
|---|---|
| El commit coincide con `HEAD` | ✅ Todo alineado |
| El commit es **anterior** a `HEAD` | ⚠️ Hay código commiteado que **no está cargado**. Recargar antes de tocar nada |
| Dice **`+cambios`** | ⚠️ La placa tiene algo que **no está en Git**. Averiguar qué antes de compilar encima |
| `desconocido` | Firmware anterior a que se marcara la versión |

🔑 **Este paso es la razón de ser de la skill.** Compilar sobre una placa que
tiene otra cosa dentro produce fallos imposibles de entender.

## Paso 3 — Leer dónde se quedó

Ahora sí, y **en este orden**, sin leer más de lo necesario:

1. **`proyectos/<nombre>/BITACORA.md`** → el bloque **`🔚 Cierre de sesión`** del
   principio. Está puesto arriba a propósito: es el resumen del estado.
2. La sección **`🔜 En qué me quedé`** → verificaciones pendientes y la siguiente
   acción concreta.
3. **`CLAUDE.md`** → estado general y reglas del taller.

**No leas los ADRs ni la investigación completa todavía.** Son referencia, no
contexto de arranque. Se consultan cuando hagan falta.

## Paso 4 — Buscar desfases

Compara lo que dicen los documentos con lo que acabas de comprobar. Preguntas
concretas:

- ¿La bitácora dice que algo estaba bloqueado y **ya no lo está**? (o al revés)
- ¿Hay verificaciones marcadas como pendientes que **ya se resolvieron**?
- ¿El `Estado actual` de `CLAUDE.md` sigue siendo cierto?
- ¿Hay commits posteriores al último cierre sin documentar?

**Si encuentras un desfase, corrígelo antes de seguir y dilo.** Un documento que
miente es peor que no tener documento: se actúa sobre él con confianza.

## Paso 5 — Situar el proyecto

Un vistazo rápido, sin abrir los archivos enteros:

```bash
cd "$(git rev-parse --show-toplevel)"

echo "=== Supuestos sin resolver ==="
S=$(grep -hE '^\| S-[0-9]+ \|' proyectos/*/4-requerimientos/REQUERIMIENTOS.md | grep -v '✅')
[ -n "$S" ] && echo "$S" | cut -c1-80 || echo "  ✅ todos resueltos"

echo "=== Requerimientos pendientes ==="
grep -hE '^\| (RF|RNF)-[0-9]+ \|' proyectos/*/4-requerimientos/REQUERIMIENTOS.md \
  | grep -E '🔲|🟡' | cut -c1-80

echo "=== ADRs sin decidir ==="
grep -l 'Estado:.*Propuesto' proyectos/*/1-decisiones/ADR-*.md 2>/dev/null \
  || echo "  ✅ ninguno abierto"

echo "=== Material pendiente de comprar o reparar ==="
# Solo las filas de componentes (empiezan por C-NNN): filtrar la tabla
# entera pillaría también la leyenda que explica los estados.
M=$(grep -hE '^\| C-[0-9]+ \|' inventario/INVENTARIO.md | grep -E 'Por comprar|A reponer|A reparar')
[ -n "$M" ] && echo "$M" | cut -c1-80 || echo "  ✅ nada pendiente"
```

## Paso 6 — Resumir y proponer

Preséntalo **corto**. El objetivo es que en 30 segundos se sepa dónde se está, no
releer la historia del proyecto.

```markdown
## Dónde estamos

<una o dos frases: qué funciona hoy>

**Estado físico:** placa <conectada/no> · firmware `<commit>` · <arranca bien / se reinicia>

## Lo que comprobé

<solo si hay algo que NO coincide con lo escrito, o si todo cuadra, una línea>

## Siguiente paso

<la acción concreta que dejó escrita el cierre anterior>

¿Seguimos con eso, o hay otra prioridad?
```

**Termina preguntando.** El cierre anterior propuso un siguiente paso, pero quien
vuelve puede traer otra idea — y forzarle el plan viejo es peor que perder una
pregunta.

---

## Recordatorios al arrancar

**Las reglas duras del taller** (están en `CLAUDE.md`, pero se olvidan):

- Los ADR **no se editan ni se borran**: si algo cambió, se escribe uno nuevo
- Un componente `Dañado` **no vuelve solo** al inventario
- El inventario es **uno y compartido**
- **Cero secretos** en Git

**Las trampas conocidas** están en la tabla de
[`/cerrar-sesion`](../cerrar-sesion/SKILL.md#trampas-de-este-repo-para-no-repetirlas).
Vale la pena mirarla si aparece un error raro: casi todos los de este repo ya
pasaron una vez.

**El tono:** el usuario está aprendiendo electrónica. Explicar el porqué, avisar
⚠️ **antes** de lo que pueda romper algo, y un componente a la vez al cablear.
