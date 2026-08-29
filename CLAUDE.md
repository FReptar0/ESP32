# Instrucciones para Claude Code en este taller

## Contexto sobre el usuario

Está **aprendiendo** electrónica e IoT. No asumas conocimiento previo de
hardware, protocolos ni conceptos de embebidos.

Esto cambia cómo hay que trabajar aquí:

- **Explica el porqué, no solo el qué.** "Conecta SDA al GPIO21" no sirve solo;
  hay que decir qué es SDA y por qué ese pin.
- **Usa analogías y ejemplos concretos.** Los conceptos abstractos de electrónica
  (pull-up, nivel lógico, corriente de pico) se entienden mucho mejor con una
  comparación que con una definición.
- **Advierte antes de que rompa algo.** Cada vez que haya riesgo de dañar un
  componente —voltaje equivocado, pin incorrecto, polaridad invertida— dilo con
  un ⚠️ **antes** de la instrucción, no después.
- **Traduce la jerga la primera vez** que aparezca en cada documento.
- **Un paso a la vez cuando sea hardware.** Conectar cinco cosas y probar deja
  cinco sospechosos si falla. De una en una, siempre se sabe quién falló.
- **Todo en español**, incluidos los comentarios del código.

## Antes de instalar algo

El usuario dio autorización para instalar por CLI, con una condición: **decirle
qué se instaló y por qué.** Siempre.

---

## Metodología obligatoria

Todo proyecto sigue las 6 fases de [`metodologia/README.md`](metodologia/README.md),
y las carpetas numeradas de cada proyecto las reflejan.

### ADRs

- Van en `<proyecto>/1-decisiones/` con el formato
  [`ADR-plantilla.md`](metodologia/plantillas/ADR-plantilla.md).
- **Nunca se borran ni se reescriben.** Si una decisión cambia: se escribe un ADR
  nuevo, se marca el viejo como `Reemplazado por ADR-XXXX`, y el viejo se queda
  íntegro con su razonamiento original. Eso es lo que permite el backtrack.
- Se escribe un ADR cuando la decisión sería molesta de revertir: elegir un
  componente, una librería, un protocolo, una API, o el esquema de alimentación.
  También cuando se decide **no** hacer algo que parecía obvio.
- Llenar siempre la sección **"Qué sabíamos y qué no"**. Es lo que después
  permite distinguir "me equivoqué al decidir" de "decidí bien y el mundo cambió".
- Actualizar el índice en `1-decisiones/README.md`.

### Inventario

- Es **uno solo y general**: [`inventario/INVENTARIO.md`](inventario/INVENTARIO.md).
  Ningún proyecto tiene el suyo.
- Todo cambio de estado se registra **primero** en
  [`inventario/LOG.md`](inventario/LOG.md) con fecha absoluta, y después se
  actualiza la tabla del inventario. Los dos deben quedar consistentes.
- **Regla dura sobre lo dañado:** un componente en estado `Dañado` **no existe**
  para efectos de planeación. No proponerlo, no contarlo, no incluirlo en ningún
  diseño.
  - Solo pasa a `A reparar` o `A reponer` si **el usuario lo declara** en el LOG.
  - **Nunca cambiar ese estado por iniciativa propia.**
  - `Dañado` no se borra nunca: el histórico de "ya quemé tres de estos" es
    información valiosa para alguien que está aprendiendo.

### Bitácora

Cada proyecto tiene `BITACORA.md`. Al terminar una sesión de trabajo, agregar
entrada con fecha: qué se hizo, qué falló, en qué se quedó.

Diferencia con los ADRs: el ADR captura **una decisión**; la bitácora captura **el
trayecto** — los intentos fallidos, las sospechas sin confirmar, el cable que
resultó estar malo.

---

## Reglas técnicas del taller

- **Diagramas en Mermaid**, nunca imágenes. Al ser texto viven junto al código,
  se versionan, y se puede ver exactamente qué cambió.
- **Fechas absolutas** siempre (`2026-08-29`), nunca relativas.
- **Versiones clavadas** en `platformio.ini` (`@6.21.3`, no "la última"). Es el
  requerimiento RNF-04: recompilar en un año y que funcione igual.
- **Cero secretos en el repositorio.** Credenciales de WiFi y tokens van en
  archivos ignorados por Git. Revisar antes de cada commit.
- **Un entorno virtual por proyecto** en `<proyecto>/2-entorno/.venv/`.

## Avisos de seguridad eléctrica que hay que repetir cuando apliquen

No asumir que ya se dijeron. Repetirlos en contexto:

- ⚠️ **Los pines del ESP32 son 3.3 V y no toleran 5 V.** Es lo que más placas mata.
- ⚠️ **El ESP32 solo se conecta a WiFi de 2.4 GHz**, no a 5 GHz. Clásico de "no
  conecta y no sé por qué", porque muchos routers usan el mismo nombre para ambas.
- ⚠️ **El pico de consumo del ESP32 llega a ~500 mA** al transmitir. Si la fuente
  no lo aguanta, la placa se reinicia sola y parece un bug de software.
- ⚠️ **Desconectar el USB antes de recablear.**

---

## Estado actual

**Bloqueante:** la Mac no detecta el ESP32 (verificado 2026-08-29 por dos vías
independientes: árbol USB del sistema y `pio device list`). Diagnóstico completo
en [`proyectos/tamagotchi/BITACORA.md`](proyectos/tamagotchi/BITACORA.md).

Antes de sugerir cualquier paso de firmware, verificar si esto ya se resolvió:

```bash
ls /dev/cu.* | grep -iE 'usbserial|wchusbserial|usbmodem'
```
