#!/usr/bin/env bash
# Crea un proyecto nuevo con la estructura de 6 fases del taller.
# Uso: ./herramientas/nuevo-proyecto.sh nombre-del-proyecto

set -euo pipefail

TALLER="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NOMBRE="${1:-}"
HOY="$(date +%Y-%m-%d)"

if [[ -z "$NOMBRE" ]]; then
  echo "Uso: $0 <nombre-del-proyecto>"
  echo "Ejemplo: $0 estacion-clima"
  exit 1
fi

DEST="$TALLER/proyectos/$NOMBRE"

if [[ -e "$DEST" ]]; then
  echo "❌ Ya existe: $DEST"
  exit 1
fi

echo "📁 Creando estructura de 6 fases..."
mkdir -p "$DEST"/{1-decisiones,2-entorno,3-componentes,4-requerimientos,5-investigacion,6-esquemas,firmware}

echo "📋 Copiando plantillas..."
cp "$TALLER/metodologia/plantillas/requerimientos-plantilla.md" \
   "$DEST/4-requerimientos/REQUERIMIENTOS.md"
cp "$TALLER/metodologia/plantillas/ADR-plantilla.md" \
   "$DEST/1-decisiones/ADR-0001-CAMBIAME.md"

cat > "$DEST/1-decisiones/README.md" <<EOF
# Decisiones del proyecto $NOMBRE (ADRs)

Ninguna decisión se borra. Cuando una cambia, se escribe un ADR nuevo que
reemplaza al viejo, y el viejo se conserva íntegro con su razonamiento original.

| ADR | Decisión | Estado | Fecha |
|---|---|---|---|
| 0001 | *(pendiente)* | \`Propuesto\` | $HOY |

Plantilla: [\`metodologia/plantillas/ADR-plantilla.md\`](../../../metodologia/plantillas/ADR-plantilla.md)
EOF

cat > "$DEST/BITACORA.md" <<EOF
# Bitácora — $NOMBRE

Diario de trabajo. Lo más nuevo arriba. Aquí va **el trayecto**: los intentos que
fallaron, las sospechas sin confirmar, el cable que resultó estar malo. Las
*decisiones* van en [\`1-decisiones/\`](1-decisiones/).

---

## $HOY — Sesión 1: arranque

### Qué se hizo
- Se creó la estructura del proyecto.

### 🔜 En qué me quedé
-
EOF

cat > "$DEST/README.md" <<EOF
# $NOMBRE

*(Una frase: qué es este proyecto.)*

- **Estado:** 🟡 Recién creado
- **Inicio:** $HOY

## Organización

Las carpetas siguen las [6 fases del taller](../../metodologia/README.md):

| Carpeta | Qué contiene | Estado |
|---|---|---|
| [\`1-decisiones/\`](1-decisiones/) | ADRs: por qué hicimos lo que hicimos | 🔴 |
| [\`2-entorno/\`](2-entorno/) | El entorno aislado y cómo levantarlo | 🔴 |
| [\`3-componentes/\`](3-componentes/) | Ficha de cada pieza | 🔴 |
| [\`4-requerimientos/\`](4-requerimientos/) | Qué tiene que hacer | 🔴 |
| [\`5-investigacion/\`](5-investigacion/) | Lo averiguado, con fuentes | 🔴 |
| [\`6-esquemas/\`](6-esquemas/) | El mapa: lógico y físico | 🔴 |
| \`firmware/\` | El código | 🔴 |

## Material

El inventario es **general y compartido**:
[\`inventario/INVENTARIO.md\`](../../inventario/INVENTARIO.md).
Los componentes que use este proyecto se marcan ahí con \`En uso\`.
EOF

echo "🐍 Creando entorno virtual aislado..."
python3 -m venv "$DEST/2-entorno/.venv"
"$DEST/2-entorno/.venv/bin/pip" install --quiet --upgrade pip
"$DEST/2-entorno/.venv/bin/pip" install --quiet platformio esptool

echo
echo "✅ Proyecto creado: proyectos/$NOMBRE"
echo
echo "Siguientes pasos:"
echo "  1. cd proyectos/$NOMBRE"
echo "  2. source 2-entorno/.venv/bin/activate"
echo "  3. Escribe los requerimientos en 4-requerimientos/REQUERIMIENTOS.md"
echo "  4. Renombra 1-decisiones/ADR-0001-CAMBIAME.md con tu primera decisión"
echo "  5. Registra en inventario/LOG.md el material que vayas a usar"
