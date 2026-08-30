---
name: cerrar-sesion
description: "Cierra una sesión del taller de electrónica dejando el contexto completo: estado físico de la placa, bitácora, hallazgos medidos y verificaciones pendientes. Úsala antes de limpiar la conversación o de dejar el proyecto."
allowed-tools:
  - Read
  - Write
  - Edit
  - Bash
  - Glob
  - Grep
---

# Cerrar sesión del taller

Deja el proyecto en un estado del que se pueda retomar sin haber estado presente.

> **Por qué esta skill y no una genérica de software:** aquí **la mitad del estado
> no está en Git**. Está en una placa que sigue enchufada, con un firmware
> concreto dentro, unos cables puestos de una manera y unas cosas que solo se
> pueden comprobar mirando la pantalla. Un handoff que ignore eso deja al
> siguiente adivinando.

---

## Paso 1 — Fotografiar el estado físico

Esto es lo que ningún `git status` cuenta. Ejecuta y guarda los resultados:

```bash
cd "$(git rev-parse --show-toplevel)"

echo "=== ¿Está la placa conectada? ==="
ls /dev/cu.usbserial* 2>/dev/null || echo "  ❌ ninguna placa visible"

echo "=== ¿Hay algún proceso ocupando el puerto? ==="
lsof /dev/cu.usbserial-0001 2>/dev/null || echo "  ✅ puerto libre"

echo "=== ¿Qué firmware está compilado? ==="
ls -la proyectos/*/firmware/.pio/build/*/firmware.bin 2>/dev/null || echo "  (sin compilar)"

echo "=== Últimos commits ==="
git log --oneline -6
```

⚠️ **Si hay un proceso ocupando el puerto, mátalo antes de terminar.** Un lector
de serie olvidado impide la siguiente carga de firmware y da un `FAILED` que no
explica nada — ya pasó dos veces.

Anota en la bitácora:

- **Placa:** conectada o no · puerto · qué modelo (hay varias en el inventario)
- **Firmware cargado:** **pregúntaselo a la placa**, no lo deduzcas de Git. El
  firmware imprime su commit al arrancar:
  `=== Tamagotchi · firmware abc1234 ===`. Si dice `+cambios`, la placa lleva algo
  que no está commiteado — dilo explícitamente
- **Cableado:** qué hay conectado físicamente ahora mismo
- **¿Sigue funcionando?** Si la dejas encendida, dilo

## Paso 2 — Comprobar que no se escapa ningún secreto

```bash
# 1) Archivos que nunca deben estar rastreados
git ls-files | grep -iE '(^|/)(secrets|credentials)\.h$|(^|/)\.env$' \
  && echo "  🚨 REVISAR" || echo "  ✅ ninguno"

# 2) Valores reales dentro del firmware
#    Se acota a .h/.cpp/.ino y se excluyen las plantillas: buscar solo
#    "token" por el nombre da falsos positivos (el propio script de OAuth
#    se llama get-refresh-token.py y no contiene ningún secreto).
git grep -lE '#define[[:space:]]+(WIFI_PASSWORD|SPOTIFY_CLIENT_SECRET|SPOTIFY_REFRESH_TOKEN)[[:space:]]+"[^"{]{8,}"' \
  -- '*.h' '*.cpp' '*.ino' && echo "  🚨 REVISAR" || echo "  ✅ ninguno"

# 3) Nada sensible esperando commit
git diff --cached --name-only | grep -iE '(^|/)(secrets|credentials)\.h$|(^|/)\.env$' \
  && echo "  🚨 REVISAR" || echo "  ✅ limpio"
```

> 💡 La comprobación busca **valores**, no nombres de archivo. Un grep por
> "token" a secas marcaría `tools/get-refresh-token.py`, que es el script de
> OAuth y no guarda ningún secreto — y un aviso que salta siempre acaba
> ignorándose.

Este proyecto guarda contraseña de WiFi, `client_secret` y un `refresh_token` que
**da acceso a la cuenta de Spotify con permiso de control**. Nunca deben entrar a
Git. Si aparece algo, **para y avisa** antes de commitear.

## Paso 3 — Actualizar la bitácora del proyecto

`proyectos/<nombre>/BITACORA.md` — **es obligatorio**, y va arriba del todo.

La bitácora captura **el trayecto**, no las decisiones (eso son los ADRs). O sea:
los intentos fallidos, las hipótesis descartadas, el cable que resultó estar mal.

Escribe:

```markdown
### <emoji> <Título de lo que se hizo>

Qué se hizo y qué se descubrió.

🔑 **La lección**, si la hubo — en una frase que sirva dentro de seis meses.

### 🔜 En qué me quedé — siguiente sesión

**Verificaciones pendientes de mirar:**   ← las que solo puede hacer el usuario
- ...

**Lo siguiente:**
- ...
```

### Distingue lo medido de lo supuesto

Es la regla más importante de este taller, y ya evitó rediseñar cosas que no
estaban rotas.

- **Medido** → pon el número y cómo se obtuvo (`RAM mínima 92 KB, 11 muestras
  en 5,5 min`)
- **Supuesto** → dilo con esas palabras, y anota qué lo confirmaría

Si esta sesión convirtió un supuesto en un dato, **dilo explícitamente**: es lo
más valioso que se puede dejar escrito.

### Si te equivocaste, déjalo escrito

Los diagnósticos fallidos valen tanto como los aciertos: evitan que la siguiente
sesión repita el mismo camino. *"Sospeché de X, lo medí, no era"* es información.

## Paso 4 — Repartir lo aprendido por su fase

Cada cosa va donde le toca. **No lo metas todo en la bitácora.**

| Qué apareció | Dónde va |
|---|---|
| Una **decisión** (componente, librería, protocolo, arquitectura) | `1-decisiones/ADR-NNNN-*.md` **nuevo** |
| Cambios en el entorno o herramientas | `2-entorno/README.md` |
| Datos de una pieza (voltaje, pines, rarezas) | `3-componentes/<pieza>.md` |
| Un requerimiento o supuesto que se movió | `4-requerimientos/REQUERIMIENTOS.md` + fila en el historial |
| Algo **averiguado o medido** | `5-investigacion/<tema>.md` + entrada en su README |
| Cableado, memoria, flujo de datos | `6-esquemas/*.md` |
| Material que entró, salió, se rompió o se usó | `inventario/LOG.md` **primero**, luego `INVENTARIO.md` |

### ⚠️ Reglas duras que no se negocian

**Los ADRs no se editan ni se borran.** Si una decisión cambió, escribe uno
**nuevo**, marca el viejo como `Reemplazado por ADR-XXXX`, y deja el viejo
intacto con su razonamiento original. Eso es lo que permite el backtrack.

**Un componente `Dañado` no vuelve solo.** Solo pasa a `A reparar` o `A reponer`
si el usuario lo declara en el `LOG.md`. Nunca cambies ese estado por iniciativa
propia.

**El inventario es uno y compartido.** Ningún proyecto tiene el suyo.

**Un hallazgo superado no se borra.** Márcalo como superado y explica qué cambió.
El razonamiento viejo casi siempre sigue siendo válido; lo que cambió es el
contexto.

## Paso 5 — Anotar lo que solo puede comprobar el usuario

Sección aparte, porque es fácil que se pierda: **hay cosas que no se pueden
verificar desde la terminal.** Si algo quedó pendiente de que alguien lo mire,
tiene que estar escrito.

Típicamente:

- ¿Se ve bien en la pantalla? ¿Se lee a distancia de escritorio?
- ¿La sincronía de la letra va cuadrada?
- ¿Molesta el brillo a oscuras?
- ¿Se comporta bien al pausar, saltar o quedarse sin red?

Ponlas como preguntas concretas y **con qué respuesta se espera**, no como
"revisar la pantalla".

## Paso 6 — Commitear

Un commit por bloque de trabajo con sentido propio. **No un WIP genérico**: los
mensajes de este repo explican *por qué*, y varias veces han servido de
documentación.

Estilo del repo:

```
Titulo corto en una linea

CAUSA / QUE PASABA: el sintoma observado y por que ocurria.

SOLUCION: que se cambio y por que asi y no de otra forma.

-> La leccion, si la hay.

Numeros medidos, si los hay.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
```

⚠️ **Usa un archivo para el mensaje** (`git commit -F archivo.txt`). Los mensajes
largos con acentos, comillas o backticks se rompen si van en la línea de
comandos — ya pasó una vez y hubo que rehacer el commit.

```bash
git add -A
git -c user.name="freptar0" -c user.email="yahir.dev13@gmail.com" \
    commit -F /ruta/al/mensaje.txt
```

## Paso 7 — Resumen final al usuario

Cierra con esto, corto:

1. **Qué quedó funcionando** — verificado, con la evidencia
2. **Qué quedó a medias** y por qué
3. **El estado físico** — placa, firmware cargado, cableado
4. **Lo que necesita que mire** (paso 5)
5. **Por dónde seguir** — la siguiente acción concreta, no una lista de ideas

---

## Trampas de este repo, para no repetirlas

Cosas que ya costaron tiempo. Si la siguiente sesión las conoce, no las repite:

| Trampa | Señal | Qué hacer |
|---|---|---|
| Proceso ocupando el puerto serie | La carga falla en 2 s sin explicación | `lsof /dev/cu.usbserial-0001` y matarlo |
| Macros de Arduino (`HEX`, `cli`, `min`, `max`…) | Error de compilación absurdo con un nombre corto | Renombrar la variable |
| `Transfer-Encoding: chunked` | HTTP 200 pero el JSON no se puede leer | Usar `LectorChunked` |
| Objetos grandes en la pila | `Stack canary watchpoint triggered` | Hacerlos globales. La pila son 8–16 KB, no los 200 KB del montón |
| Dos handshakes TLS a la vez | `Task watchdog got triggered: IDLE0` | Serializar con el mutex de red |
| Certificados de Python en macOS | `CERTIFICATE_VERIFY_FAILED` | El script usa `certifi`; si falla, `Install Certificates.command` |
| Comparar RAM entre compilaciones distintas | Parece que hay una fuga | Medir con el mismo binario y en el mismo punto del ciclo |
| Buscar el fallo en el último eslabón | El síntoma sale al final de una cadena | Instrumentar la cadena **entera** antes de tocar nada |
| **Optimizar el hilo de red encadenando peticiones** | Los cambios de canción tardan en notarse | El hilo de red es **el único sentido** del aparato. Nunca encadenar peticiones sin soltar el turno: [`cancelacion-de-busquedas.md`](../../../proyectos/tamagotchi/5-investigacion/cancelacion-de-busquedas.md) |
| **Cambiar algo sin leer si ya se decidió** | Se "arregla" y reaparece un fallo viejo | `grep -ril "<tema>" proyectos/*/5-investigacion/ proyectos/*/1-decisiones/` **antes** de tocar |
| **Apagar una máquina de estados compartida** | Una canción suena entera sin letra, y repetirla no la recupera | Comprobar que `fase` sigue siendo tuya (`generacion == miGen`) antes de ponerla a NADA |
| **"Repetirlo no lo arregla"** | El usuario reintenta y da igual | No es una queja, es un dato: el fallo está en que **no se reintenta**, no en la operación |
| **Mutex no recursivo** | Cuelgue total al "mejorar" la protección de una función | `CON_RED` NO es recursivo. Antes de envolver una llamada, comprobar si ya corre dentro de otro `CON_RED` |
| **Alargar una espera sin mirar qué tienes bloqueado** | Un hilo ajeno se queda parado sin culpa | Ver qué mutex se tienen en la mano ANTES de subir un `delay`. Mejor apuntar "hasta cuándo no" y soltar |
| **`HTTPClient::header()` devuelve vacío** | Una cabecera de respuesta siempre sale 0 o "" | Hay que pedirla antes con `collectHeaders()`. No falla: miente |
| **Abrir el puerto serie sin tocar DTR/RTS** | La captura sale como basura ilegible, megas de símbolos | Abrir el puerto en macOS activa DTR/RTS y deja la placa en modo arranque. Hacer siempre la secuencia de reinicio: `setDTR(False); setRTS(True); sleep; setRTS(False)` |
| **Pines del ADC2 con WiFi encendido** | El sensor analógico funciona suelto y falla al añadir la red | El driver de WiFi se apropia del ADC2. Solo ADC1: GPIO 32–36 y 39 |
