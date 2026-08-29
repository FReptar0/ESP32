# Investigación — Tamagotchi

Lo que hubo que averiguar antes de poder construir, **con fecha y fuente**.

La fecha no es adorno: el hardware y las APIs cambian, y la información en
internet se pudre. Un tutorial de 2019 puede describir una API que ya no existe.
Anotar cuándo lo leíste es lo que después te permite distinguir *"lo implementé
mal"* de *"esto funcionaba y cambió"*.

---

## 🔴 Pendientes, en orden de urgencia

### 1. ¿La API de Spotify sigue permitiendo leer "reproduciendo ahora"?

**Este es el riesgo número uno del proyecto.** Si la respuesta es no, no hay
código que lo arregle: sería otro proyecto.

Corresponde al supuesto
[S-02](../4-requerimientos/REQUERIMIENTOS.md#supuestos).

Qué hay que responder:

- ¿El endpoint `/me/player/currently-playing` sigue disponible en 2026?
- ¿Una app en **modo desarrollo** (uso personal, sin revisión de Spotify) puede
  usarlo? ¿Cuántos usuarios permite?
- ¿Qué permisos (*scopes*) hace falta pedir?
- ¿Cuál es el límite de peticiones? Esto decide cada cuánto puede preguntar el
  ESP32.
- ¿Devuelve `progress_ms`? Ese dato es el que después permitiría sincronizar las
  letras.

> **Se puede hacer sin la placa.** No depende del hardware, así que es la tarea
> ideal mientras el ESP32 no responde.

### 2. ¿Cuánta RAM libre queda en el ESP32 con TLS activo?

Supuesto [S-04](../4-requerimientos/REQUERIMIENTOS.md#supuestos). Decide entre las
dos arquitecturas del
[ADR-0002](../1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md).

Requiere la placa funcionando. Es un **spike**: un programa desechable que se
conecta por HTTPS a cualquier sitio e imprime la memoria libre. La respuesta se
guarda; el código se tira.

### 3. ¿Qué cobertura de letras hay para tu música?

Supuesto [S-05](../4-requerimientos/REQUERIMIENTOS.md#supuestos), relacionado con
el [ADR-0003](../1-decisiones/ADR-0003-fuente-de-las-letras.md).

Prueba barata que puedes hacer tú mismo, sin programar nada: buscar en LRCLIB, a
mano desde el navegador, **5 canciones que de verdad escuches**. Si tres no
aparecen, hay que replantear el RF-04 antes de invertir tiempo en él.

---

## ✅ Hallazgos

### 2026-08-29 — La placa es un ESP32-D0WD-V3 con USB-Serie CP2102

**Qué se averiguó:** `ESP32-D0WD-V3` rev v3.1, 2 núcleos @ 240 MHz, 4 MB de
flash, Wi-Fi + BT. Chip USB-Serie **CP2102** (Silicon Labs), reconocido por macOS
sin instalar drivers. MAC `84:1F:E8:2C:3E:C4`.
**Fuente:** prueba propia — `esptool chip-id` y `flash-id`.
**Por qué importa:** fija el `board = esp32dev` del `platformio.ini`. Confirma
también que el "-S" del nombre comercial no era un ESP32-S2 ni S3.
**Confianza:** máxima — se lo preguntamos al chip directamente.
**Consecuencia:** desbloqueó la fase 2 (entorno) y la fase 6 (plano físico).

### 2026-08-29 — Los cables USB-C de solo carga son indistinguibles a simple vista

**Qué se averiguó:** varios cables C-a-C llevan corriente pero **no datos**. El
síntoma es LED encendido + cero dispositivos en el árbol USB.
**Fuente:** prueba propia — 4 rondas de diagnóstico.
**Por qué importa:** costó todo el arranque del proyecto. Y se va a repetir.
**Confianza:** máxima.
**Consecuencia:** 🔖 **Regla del taller:** ante un "no me detecta la placa",
probar ese cable con un celular antes que nada. 20 segundos contra horas.

### 2026-08-29 — La API pública de Spotify no da letras

**Qué se averiguó:** las letras que aparecen en la app de Spotify vienen de un
proveedor externo (Musixmatch) por un endpoint interno que no forma parte de la
API pública.

**Por qué importa:** aunque el ESP32 logre hablar con Spotify perfectamente, de
ahí **no van a salir las letras**. Hace falta una segunda fuente y cruzar la
canción con ella. Esto cambia el plan del RF-04 por completo.

**Confianza:** alta, pero conviene reconfirmarlo al llegar a esa fase.

**Consecuencia:** se escribió el
[ADR-0003](../1-decisiones/ADR-0003-fuente-de-las-letras.md).

### 2026-08-29 — El ESP32 no fue detectado por la Mac

Diagnóstico completo en la [bitácora](../BITACORA.md). Verificado por dos vías
independientes (árbol USB del sistema y `pio device list`).

---

## Cómo se anota un hallazgo

```markdown
### AAAA-MM-DD — Título de lo que se averiguó

**Qué se averiguó:** el hecho, en una o dos frases.
**Fuente:** URL, o "prueba propia".
**Por qué importa:** qué decisión o requerimiento afecta.
**Confianza:** alta / media / baja — y por qué.
**Consecuencia:** qué ADR o requerimiento cambia a raíz de esto.
```

## Sobre los spikes

Un **spike** es una prueba rápida y **desechable** para responder una pregunta
concreta. *"¿El ESP32 aguanta una conexión HTTPS sin quedarse sin memoria?"* →
escribes 30 líneas feas, mides, anotas el número aquí, **borras el código**.

La clave está en lo de desechable. Un spike no se hace bien, se hace rápido, y su
único producto es una respuesta escrita en este archivo. Si el código del spike
sobrevive y acaba en el proyecto final, dejó de ser un spike y se convirtió en
deuda técnica.
