# ADR-0009 — Incrustar el paquete de CAs de Mozilla, no un certificado suelto

- **Estado:** `Aceptado`
- **Fecha:** 2026-08-29
- **Relacionado con:** [ADR-0002](ADR-0002-como-obtener-datos-de-spotify.md), RNF-05, RNF-06

---

## Contexto

El ESP32 habla con Spotify por HTTPS. Para que eso sea seguro tiene que poder
**verificar que el servidor es realmente Spotify** y no alguien haciéndose pasar
por él. Si no lo verifica, cualquiera en la misma red podría interceptar la
conexión y quedarse con el `refresh_token` — que da acceso a la cuenta.

El problema de fondo, en una frase:

> **Tu Mac tiene un llavero con las autoridades en las que confía de fábrica.
> El ESP32 no tiene nada. No confía en nadie porque no conoce a nadie.**

El spike de memoria usó `setInsecure()`, que acepta a cualquiera. Sirvió para
medir, pero no puede quedarse en el firmware final.

## Qué sabíamos y qué no

**Lo que sabíamos con certeza:**

- Este mismo problema ya nos mordió en la Mac: el Python de python.org falló con
  `CERTIFICATE_VERIFY_FAILED` porque su paquete de certificados nunca se enlazó.
  Ver [`5-investigacion/README.md`](../5-investigacion/README.md).
- Los certificados raíz **caducan** y las empresas **cambian de autoridad**.

**Lo que se verificó el 2026-08-29:**

- ✅ El core de Arduino para ESP32 (2.0.17) incluye `setCACertBundle()` y todo el
  código para leer un paquete de CAs comprimido.
- ✅ **No incluye el paquete ya generado** ni el script que lo genera. Hay que
  fabricarlo.
- ✅ El formato está documentado en `esp_crt_bundle.c` y es simple.
- ⚠️ **Verificar certificados exige saber la fecha de hoy**, y el ESP32 arranca
  creyendo que estamos en 1970. Sin sincronizar la hora, *todos* los
  certificados parecen "aún no válidos".

## Opciones consideradas

### Opción A — `setInsecure()`

- **A favor:** una línea, funciona siempre.
- **En contra:** **no verifica nada.** Cualquiera en la red podría suplantar a
  Spotify y quedarse con el `refresh_token`. Inaceptable en un aparato que
  guarda credenciales con permiso de control.

### Opción B — Incrustar un solo certificado raíz (`setCACert`)

Pegar en el código el PEM de la CA que usa Spotify hoy.

- **A favor:** ocupa ~2 KB. Simple.
- **En contra:** es **frágil por diseño**. Si esa raíz caduca, o si Spotify se
  cambia de autoridad, el aparato deja de funcionar **solo, un día, sin que nadie
  lo haya tocado** — el peor tipo de fallo que hay, porque no hay ningún cambio
  al que culpar. Era el riesgo #1 anotado en los requerimientos.

### Opción C — Paquete completo de CAs de Mozilla ← *elegida*

Las ~150 autoridades en las que confían Firefox y prácticamente todo el mundo,
convertidas al formato compacto del ESP32.

- **A favor:**
  - Funciona con **cualquier** servidor, no solo con Spotify. Cuando agreguemos
    la fuente de letras ([ADR-0003](ADR-0003-fuente-de-las-letras.md)), ya está
    resuelto.
  - Sobrevive a que Spotify cambie de autoridad certificadora.
  - Es lo que hace ESP-IDF por defecto: camino probado.
  - Se actualiza regenerándolo, sin tocar el código.
- **En contra:** ocupa **66.8 KB de flash** (1.7% de los 4 MB) y hay que
  fabricarlo con un script propio.
- **Costo:** una tarde de script, cero mantenimiento después.

## Decisión

**Opción C: el paquete completo de Mozilla, generado desde `certifi`.**

El factor decisivo es cuál de las dos opciones seguras **falla mejor**. Con un
certificado suelto, el fallo llega sin aviso y sin causa aparente. Con el
paquete completo, haría falta que se cayera la infraestructura de confianza
entera — y si eso pasa, tendremos problemas más grandes que el Tamagotchi.

Los 66.8 KB son irrelevantes: el firmware quedó en 77% de la flash con todo
incluido.

### Cómo queda implementado

| Pieza | Dónde |
|---|---|
| Generador del paquete | [`tools/gen-cert-bundle.py`](../tools/gen-cert-bundle.py) |
| Paquete generado | `firmware/data/cert/x509_crt_bundle.bin` (66.8 KB, 150 CAs) |
| Se incrusta con | `board_build.embed_files` en `platformio.ini` |
| Se activa con | `cliente.setCACertBundle(rootca_crt_bundle_start)` |
| Fuente de las CAs | `certifi` — la lista de Mozilla |

**Y una pieza que no es opcional:** antes de cualquier conexión TLS, el firmware
sincroniza la hora por NTP (`configTime`) y **espera a que sea creíble**. Sin
eso, la validación fallaría siempre y el mensaje de error no diría por qué.

### Verificado funcionando

```
[esp_crt_bundle.c:142] esp_crt_verify_callback(): Certificate validated
```

En cada conexión, tanto a `accounts.spotify.com` como a `api.spotify.com`.

## Consecuencias

**Lo que esto nos habilita:**

- Conexiones HTTPS verificadas de verdad. El `refresh_token` viaja protegido.
- Cualquier servidor futuro (letras, hora, actualizaciones) funciona sin trabajo
  extra.
- El riesgo "deja de funcionar solo un día" pasa de **Alto** a **Muy bajo**.

**Lo que esto nos cuesta:**

- 66.8 KB de flash.
- Una dependencia del arranque: si NTP falla, no hay TLS. El firmware lo hace
  explícito en vez de fallar en silencio.

**Lo que hay que vigilar:**

- 🔄 **Regenerar el paquete de vez en cuando** (`python3 tools/gen-cert-bundle.py`
  tras un `pip install -U certifi`). Una vez al año sobra.
- ⏰ **Que NTP funcione.** Si la red bloquea el puerto 123, el arranque se queda
  esperando. Convendría añadirle un timeout.

## Cómo revertir

**Trivial.** Cambiar `setCACertBundle(...)` por `setInsecure()` en una función.
Pero eso sería renunciar a la verificación entera — solo tiene sentido como
diagnóstico temporal, nunca como estado final.
