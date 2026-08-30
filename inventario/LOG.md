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

## 2026-08-29 — `NOTA` — ✅ C-001 identificada y funcionando

- **Qué pasó:** La placa fue detectada, identificada e interrogada con `esptool`.
  **La causa del bloqueo eran cables USB-C de solo carga**, no la placa.
- **Estado:** `Disponible` → `En uso`
- **Proyecto:** tamagotchi
- **Nota:** Identidad confirmada: **ESP32-D0WD-V3 rev v3.1**, 2 núcleos @ 240 MHz,
  4 MB de flash, Wi-Fi + BT, chip USB-Serie **CP2102** (Silicon Labs), puerto
  `/dev/cu.usbserial-0001`. **MAC `84:1F:E8:2C:3E:C4`** — sirve para distinguirla
  de la C-002, que es idéntica por fuera.
  El `-S` de "ESP32-S" era marca comercial del vendedor: es el ESP32 clásico, no
  un S2 ni un S3. Auto-reset funcionando, así que no hace falta apretar BOOT al
  programar.

## 2026-08-29 — `NOTA` — ⚠️ Cables USB-C de solo carga (causa raíz del bloqueo)

- **Qué pasó:** Varios de los cables C-a-C disponibles **llevan corriente pero no
  datos**. Con ellos el LED de la placa enciende pero la Mac no ve nada.
- **Estado:** —
- **Proyecto:** tamagotchi
- **Nota:** Se registra aquí porque **el cable es material de taller**, y este es
  exactamente el tipo de cosa que uno vuelve a sufrir en un año.
  🔖 **Regla:** ante un "no me detecta la placa", conectar ese cable a un celular.
  Si la compu no ve el celular, el cable no pasa datos y no hay nada más que
  investigar. Cuesta 20 segundos.
  💡 Vale la pena marcar físicamente el cable que sí funciona.

## 2026-08-29 — `NOTA` — GMT130 (C-004): especificaciones confirmadas

- **Qué pasó:** Verificación física del módulo. Confirmado: **`Driver IC: ST7789`,
  3.3 V, 7 pines** (`GND · VCC · SCK · SDA · RES · DC · BLK`).
- **Estado:** `Disponible` → `Disponible`
- **Proyecto:** tamagotchi
- **Nota:** Los 3 supuestos del S-07 quedan resueltos de una vez. Consecuencias:
  (1) la librería `TFT_eSPI` es compatible con seguridad; (2) al ser 3.3 V se
  conecta directo al ESP32 sin conversor de nivel; (3) al no tener `CS`, la
  pantalla acapara el bus SPI — irrelevante hoy, importante si algún día se
  agrega otro dispositivo SPI. Esto desbloqueó la fase 6: el plano físico ya está
  escrito en `6-esquemas/conexiones-fisicas.md`.

## 2026-08-29 — `NOTA` — C-003 (DEVKIT v1) tampoco es detectada

- **Qué pasó:** Se conectó la C-003 (mini-USB) a través del hub UGreen. **No
  aparece.** El árbol USB solo muestra el hub VIA Labs, el DisplayLink y el
  Billboard. Cero puertos serie nuevos.
- **Estado:** sin cambios
- **Proyecto:** tamagotchi
- **Nota:** Descarta que el problema sea exclusivo del USB-C (la hipótesis de las
  resistencias CC), porque la C-003 es mini-USB y no puede tener ese problema.
  Con dos placas distintas y dos conectores distintos fallando igual, el
  sospechoso pasa a ser lo que tienen en común: **el cable o el puerto del hub**.
  Sigue descartado que sea un driver: un dispositivo sin driver igual aparecería
  en el árbol USB como no reconocido. Aquí no aparece *nada*, lo que significa
  que la Mac no ve conexión eléctrica alguna.

## 2026-08-29 — `NOTA` — Nueva hipótesis sobre por qué C-001 no se detecta

- **Qué pasó:** Al declarar el inventario salió un dato clave: **C-001 y C-002
  tienen puerto USB-C**. Eso abre una hipótesis nueva y muy probable que no
  estaba sobre la mesa en el primer diagnóstico.
- **Estado:** sin cambios
- **Proyecto:** tamagotchi
- **Nota:** Muchísimas placas ESP32 baratas con USB-C **omiten dos resistencias
  de 5.1 kΩ** que el estándar USB-C exige en las líneas CC. Sin ellas, una
  computadora con puertos USB-C **no reconoce que hay algo conectado** y ni
  siquiera le entrega corriente por un cable C-a-C. La misma placa funciona
  perfecto con un cable A-a-C. El síntoma coincide exactamente con lo observado:
  cero enumeración, dispositivo invisible.
  **Prueba decisiva:** usar la C-003 (DEVKIT v1, mini-USB), que por su conector
  no puede tener este problema. Si esa sí aparece, la hipótesis queda confirmada
  y el bloqueante se resuelve con un cable, no con una placa nueva.

## 2026-08-29 — `ALTA` — TFT IPS 1.3" GMT130-v1.0 (C-004) ×4

- **Qué pasó:** Se registran 4 pantallas TFT que ya tenías.
- **Estado:** — → `Disponible`
- **Proyecto:** una se reservará para el tamagotchi
- **Nota:** 240×240 píxeles, IPS, a color. Es **muy superior** a la OLED de 0.96"
  que se había supuesto: en una OLED caben ~4 líneas de 20 caracteres; aquí caben
  del orden de 15 líneas de ~30. Eso cambia por completo la viabilidad de mostrar
  letras (RF-04) y hace pensable mostrar la portada del álbum más adelante.
  Tener 4 unidades quita presión: se puede experimentar sin miedo a quemar la única.

## 2026-08-29 — `ALTA` — ESP32 DEVKIT v1, mini-USB (C-003)

- **Qué pasó:** Se registra una tercera placa, más vieja, con puerto mini-USB.
- **Estado:** — → `Disponible`
- **Proyecto:** — (se usará para diagnóstico)
- **Nota:** 🎯 Esta es la pieza más valiosa del inventario **hoy mismo**. Al no
  ser USB-C, permite descartar de un solo intento la hipótesis del cable/CC.

## 2026-08-29 — `ALTA` — ESP32-S NodeMCU USB-C (C-002)

- **Qué pasó:** Se registra una segunda placa idéntica a C-001.
- **Estado:** — → `Disponible`
- **Proyecto:** —
- **Nota:** Sirve de refacción y de control experimental: si C-001 no aparece
  pero C-002 sí con el mismo cable, C-001 está dañada.

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

## 2026-08-29 — `ALTA` — Módulo joystick analógico de 2 ejes (C-005)

- **Qué pasó:** Se registra un joystick que ya tenías, para los controles de
  reproducción del Tamagotchi (RF-08, RF-09, RF-10) y la navegación entre modos.
- **Estado:** — → `Disponible`
- **Proyecto:** tamagotchi
- **Specs observadas:** 5 pines rotulados `GND`, `+5V`, `VRx`, `VRy`, `SW`. Es la
  forma estándar del módulo tipo KY-023: **dos potenciómetros de 10 kΩ** (uno por
  eje) y un **pulsador** que cierra a `GND` al empujar la palanca hacia abajo.
- **Nota:** ⚠️ El rótulo `+5V` es engañoso. **Se alimentará a 3,3 V, no a 5 V.**
  A 5 V las salidas `VRx`/`VRy` llegarían a 5 V y los pines del ESP32 no toleran
  más de 3,3 V. Al ser potenciómetros, funcionan igual con menos tensión: solo
  cambia el rango de lectura, no el comportamiento.
  Pendiente confirmar midiendo que en reposo cada eje queda cerca de la mitad
  del rango.

## 2026-08-29 — `ALTA` — ESP32-S NodeMCU (C-001)

- **Qué pasó:** Se registra la placa que ya tenías al arrancar el taller.
- **Estado:** — → `Disponible`
- **Proyecto:** — *(se asignará al Tamagotchi cuando la conectemos)*
- **Nota:** Falta confirmar el modelo exacto (cuántos pines, qué chip USB-Serial
  trae). Eso se documenta en
  `proyectos/tamagotchi/3-componentes/esp32-nodemcu.md` en cuanto la placa
  responda.
