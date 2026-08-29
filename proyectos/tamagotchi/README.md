# 🐣 Tamagotchi Spotify

Un aparatito de escritorio con ESP32 que muestra qué estás escuchando en Spotify
—canción, álbum, artista— y más adelante, la letra sincronizada.

> **Por qué se llama Tamagotchi:** por dónde queremos llegar (una mascota virtual
> que reacciona a tu música), no por lo que hace hoy. La v1 es "una pantalla que
> muestra la canción", que ya trae bastante trabajo.

- **Estado:** 🟢 **Funcionando** · muestra la canción en pantalla
- **Tipo:** Proyecto de descubrimiento (se compran piezas sobre la marcha)
- **Objetivo real:** aprender electrónica e IoT

---

## 📍 Dónde vamos

```mermaid
graph LR
    A["1 · Blink"] --> B["2 · WiFi"]
    B --> C["3 · HTTPS"]
    C --> D["4 · OAuth"]
    D --> E["5 · Token"]
    E --> F["6 · Leer la<br/>canción"]
    F --> G["7 · Pantalla"]
    G --> H["8 · Letras"]

    style A fill:#2d4a2d,color:#fff
    style B fill:#2d4a2d,color:#fff
    style C fill:#2d4a2d,color:#fff
    style D fill:#2d4a2d,color:#fff
    style E fill:#2d4a2d,color:#fff
    style F fill:#2d4a2d,color:#fff
    style G fill:#2d4a2d,color:#fff
    style H fill:#5a4a2d,color:#fff
```

## ✅ Funcionando

El requerimiento original del proyecto —*"mostrar lo que estoy escuchando en
Spotify, la canción y el disco al que pertenece"*— **está cumplido**.

```
┌────────────────────────────┐
│ SONANDO                    │
│ ──────────────────────────  │
│                            │
│      Chilango Blues        │  ← fuente 4, blanco
│                            │
│       Mon Laferte          │  ← fuente 4, verde Spotify
│      Chilango Blues        │  ← fuente 2, gris
│                            │
│  ████████░░░░░░░░░░░░░░░░  │
│  1:24                3:57  │
└────────────────────────────┘
```

**RF-01, RF-02, RF-03 y RF-05 cerrados.** El ESP32 lo hace todo solo: se conecta,
sincroniza la hora, valida el certificado de Spotify, renueva su propio token
cada hora y dibuja. Sin la Mac de por medio.

### 🔜 Siguiente: las letras (RF-04)

**La prueba más barata primero, y no necesita programar nada:** entra a
[lrclib.net](https://lrclib.net) y busca **5 canciones que escuches de verdad**.
Si tres no aparecen, hay que replantear el requerimiento antes de invertirle
tiempo (supuesto S-05).

### Pendientes menores

- Verificar RF-06 (pausar Spotify → "EN PAUSA") y RF-07 (apagar el WiFi → rojo)
- Ver la pantalla a oscuras y ajustar el brillo (RNF-02)
- Prueba de 24 h para cerrar el RNF-06
- Timeout en la sincronización NTP
- Títulos largos: hoy se cortan con `...`; con *sprites* se podrían desplazar

## Cómo está organizado

Las carpetas siguen las 6 fases de la
[metodología del taller](../../metodologia/README.md):

| Carpeta | Qué contiene | Estado |
|---|---|---|
| [`1-decisiones/`](1-decisiones/) | ADRs: por qué hicimos lo que hicimos | 🟢 7 escritos · solo el 0003 sigue abierto |
| [`2-entorno/`](2-entorno/) | El entorno aislado y cómo levantarlo | 🟢 Funcionando · recompila en ~28 s |
| [`3-componentes/`](3-componentes/) | Ficha de cada pieza | 🟢 ESP32 y GMT130 confirmados |
| [`4-requerimientos/`](4-requerimientos/) | Qué tiene que hacer el aparato | 🟢 Borrador listo |
| [`5-investigacion/`](5-investigacion/) | Lo que hubo que averiguar, con fuentes | 🟢 9 hallazgos documentados |
| [`6-esquemas/`](6-esquemas/) | El mapa: lógico y físico | 🟢 Los dos listos |
| `firmware/` | El código | 🟢 Completo hasta el paso 7 |
| [`BITACORA.md`](BITACORA.md) | Qué pasó en cada sesión | 🟢 Al día |

---

## Lo que hay que saber antes de empezar

Tres cosas que van a salir tarde o temprano, mejor saberlas desde ahora:

**El ESP32 solo se conecta a WiFi de 2.4 GHz.** No entiende 5 GHz. Y como muchos
routers usan el mismo nombre de red para ambas bandas, es un clásico de "no me
conecta y no sé por qué".

**Los pines del ESP32 son de 3.3 V y no toleran 5 V.** Conectar algo de 5 V a un
pin de datos es la forma más común de matar una placa. Cuando aplique, te aviso.

**El ESP32 no confía en nadie de fábrica.** Tu Mac tiene un llavero con las
autoridades certificadoras en las que confía; el ESP32 no tiene nada. Por eso el
firmware lleva incrustado el paquete de 150 CAs de Mozilla
([ADR-0009](1-decisiones/ADR-0009-certificados-raiz.md)). Y como verificar un
certificado exige saber qué día es hoy, sincroniza la hora por NTP antes de
cualquier conexión — si no, todos los certificados le parecerían "aún no
válidos".

**Spotify no da letras por su API pública.** Las letras que ves en la app vienen
de otro proveedor y no están disponibles desde fuera. Van a tener que salir de
otra fuente. Está documentado en el
[ADR-0003](1-decisiones/ADR-0003-fuente-de-las-letras.md).

## 📦 Material disponible

| Componente | Cant. | Para qué |
|---|---|---|
| ESP32-S NodeMCU (USB-C) | 2 | El cerebro. Una de repuesto |
| ESP32 DEVKIT v1 (mini-USB) | 1 | 🎯 La placa de diagnóstico |
| TFT IPS 1.3" GMT130, 240×240 color | 4 | La pantalla ([ADR-0004](1-decisiones/ADR-0004-pantalla-gmt130.md)) |

Detalle en [`inventario/INVENTARIO.md`](../../inventario/INVENTARIO.md).

**La pantalla resultó mejor de lo esperado.** El plan asumía una OLED de 0.96"
(4 líneas de 20 caracteres). La GMT130 da del orden de 15 líneas de 30, a color.
Eso mueve las letras (RF-04) de "probablemente inviable" a "hay que probarlo".

---

## Decisiones abiertas

| Qué | Dónde | Por qué depende de ti |
|---|---|---|
| ~~¿Directo o vía puente?~~ | [ADR-0002](1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md) | ✅ Resuelto: **directo**, porque el objetivo es aprender |
| ~~¿Qué pantalla?~~ | [ADR-0004](1-decisiones/ADR-0004-pantalla-gmt130.md) | ✅ Resuelto: la GMT130 que ya tienes |
| **¿`TFT_eSPI` o `Adafruit_ST7789`?** | Futuro ADR-0008 | Velocidad contra facilidad de aprendizaje. Es la siguiente |
| ¿De dónde salen las letras? | [ADR-0003](1-decisiones/ADR-0003-fuente-de-las-letras.md) | Depende de probar la cobertura de LRCLIB con tu música (S-05) |
