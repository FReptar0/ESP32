# ADR-0010 — Pedir permisos de lectura **y control** desde el inicio

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-29
- **Reemplaza a:** — *(matiza el criterio de mínimo privilegio aplicado antes en `tools/get-refresh-token.py`)*
- **Relacionado con:** RF-08 a RF-10, RNF-05, [ADR-0002](ADR-0002-como-obtener-datos-de-spotify.md)

---

## Contexto

El script de OAuth se escribió pidiendo **un solo permiso**
(`user-read-currently-playing`), aplicando el principio de mínimo privilegio: el
token va a vivir dentro del ESP32, un aparato que se puede perder o prestar, y
los requerimientos decían explícitamente *"❌ Controlar Spotify desde el aparato.
Solo mostrar."*

Ese alcance cambió. Sí se quiere control (play, pausa, siguiente), y además se
registró el **Web Playback SDK** en la app de Spotify.

La pregunta ya no es *cuál es el permiso mínimo*, sino **cuándo pedir los
permisos**: ahora, en un solo baile de OAuth, o más adelante, repitiéndolo.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- Los permisos quedan **fijados en el token** en el momento de autorizar. Añadir
  uno después obliga a repetir el flujo completo de OAuth.
- El flujo es manual: abrir navegador, iniciar sesión, autorizar, capturar el
  código. No es difícil, pero es fricción.

**Lo que se verificó el 2026-08-29:**

- ✅ **El Web Playback SDK no puede correr en el ESP32.** Es una librería
  JavaScript de navegador. La documentación dice que *"no puede correr en
  dispositivos embebidos ni microcontroladores"*, y necesita soporte de medios
  cifrados (DRM).
- ✅ **Controlar la reproducción desde el ESP32 sí es posible**, por otra vía: los
  endpoints de player de la Web API (`/me/player/play`, `/pause`, `/next`…), que
  requieren `user-modify-playback-state` y cuenta Premium.
- ✅ Ambas cosas requieren Premium, que ya está confirmado.

**Lo que quedó pendiente:**

- Confirmar que el SDK sigue exigiendo `user-read-email` y `user-read-private`
  además de `streaming`. Se verifica cuando se monte la página.

## La distinción que hay que tener clara

| | Qué hace | ¿Sirve en el ESP32? |
|---|---|---|
| **Web Playback SDK** | Convierte una **pestaña del navegador** en un altavoz de Spotify | ❌ No — es JavaScript y necesita DRM |
| **Web API, endpoints de player** | Manda órdenes a un aparato **que ya está reproduciendo** | ✅ Sí |

Con la Web API el ESP32 se vuelve un **control remoto**: le dice al celular o a
la Mac "pausa", "siguiente", "súbele el volumen". No reproduce audio él mismo —
para eso necesitaría hardware de audio y descifrado de DRM, que no tiene.

El Web Playback SDK sería útil para algo distinto: que una página web sea *el
reproductor*, y así el Tamagotchi tenga algo que controlar aunque el celular esté
apagado. Es un proyecto aparte, pero los permisos se pueden dejar listos.

## Opciones consideradas

### Opción A — Mantener solo `user-read-currently-playing`

- **A favor:** máxima seguridad. Si el aparato se pierde, lo peor que se filtra es
  qué música escuchas.
- **En contra:** hay que repetir el baile de OAuth en cuanto se quiera controlar
  algo. Y otra vez para el SDK.

### Opción B — Pedir todo de una vez, en un solo token ← *elegida*

- **A favor:** un solo baile de OAuth. El proyecto puede crecer sin fricción.
- **En contra:** el token del ESP32 carga permisos que hoy no usa. Si el aparato
  se pierde, quien lo tenga podría controlar la reproducción.
- **Mitigación:** el `refresh_token` **se puede revocar en cualquier momento**
  desde el perfil de Spotify. El daño máximo es que alguien te ponga pausa —
  molesto, no grave. No da acceso a datos personales ni a medios de pago.

### Opción C — Dos tokens, uno por destino

Uno con perfil `esp32` (leer + controlar) para la placa, y otro `completo` (con
los del SDK) para el navegador.

- **A favor:** cada token lleva exactamente lo que su destino necesita. Si se
  pierde el aparato, se revoca solo ese.
- **En contra:** dos bailes de OAuth y dos tokens que gestionar.

## Decisión

**Se implementan las tres, y se elige por perfil.** El script
[`tools/get-refresh-token.py`](../tools/get-refresh-token.py) acepta `--perfil`:

| Perfil | Permisos | Para qué |
|---|---|---|
| `esp32` *(por defecto)* | `user-read-currently-playing`, `user-read-playback-state`, `user-modify-playback-state` | El token que va en la placa |
| `completo` | los 3 anteriores + `streaming`, `user-read-email`, `user-read-private` | Cuando se monte la página con Web Playback SDK |
| `solo-lectura` | `user-read-currently-playing` | El mínimo, si algún día se prefiere |

**El perfil por defecto es `esp32`**, que es la opción B aplicada al aparato:
leer y controlar, sin cargar los tres permisos del SDK que la placa nunca podrá
usar.

Y como el script permite volver a correrlo con otro perfil, **la opción C queda
disponible sin trabajo extra** el día que se monte el reproductor en navegador.

## Consecuencias

**Lo que esto nos habilita:**

- Un solo baile de OAuth para todo lo que la placa vaya a hacer.
- Los requerimientos de control (RF-08 a RF-10) dejan de estar bloqueados por
  permisos.
- El camino al Web Playback SDK queda preparado, sin comprometerse a él.

**Lo que esto nos cuesta:**

- El token del ESP32 puede pausar y saltar canciones. Es una superficie de
  ataque real, aunque de impacto bajo.

**Lo que hay que vigilar:**

- 🔐 **Que el `refresh_token` no acabe en Git.** El `.gitignore` bloquea
  `secrets.h` (RNF-05). Verificado.
- 🔐 **Si el aparato se pierde o se regala: revocar el token** desde el perfil de
  Spotify. Anotarlo como parte del "apagado" del proyecto.
- ⚠️ **No meter el perfil `completo` en el ESP32.** Los tres permisos del SDK son
  inútiles ahí y solo amplían lo que se filtraría.

## Cómo revertir

**Barato, pero con fricción.** Revocar el token desde el perfil de Spotify y
volver a correr el script con otro perfil. Cinco minutos. No toca el código del
firmware, solo el valor de `SPOTIFY_REFRESH_TOKEN` en `secrets.h`.
