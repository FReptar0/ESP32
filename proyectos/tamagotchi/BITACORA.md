# Bitácora — Tamagotchi Spotify

Diario de trabajo. Lo más nuevo arriba. Aquí va **el trayecto**: los intentos que
fallaron, las horas perdidas, las sospechas sin confirmar. Las *decisiones* van en
[`1-decisiones/`](1-decisiones/).

---

## 2026-08-29 — Sesión 1: arranque del taller y primer diagnóstico

### Qué se hizo

- Se armó la estructura completa del taller y la metodología de 6 fases.
- Se creó el inventario general con su bitácora.
- Se escribieron los primeros 3 ADRs y el borrador de requerimientos.
- Se mapeó la arquitectura end-to-end.
- Se intentó detectar el ESP32 desde la Mac.

### ❌ El ESP32 no fue detectado

Esta fue la sorpresa de la sesión. Dejo el diagnóstico completo porque, si el
problema resulta ser otro, este registro es lo que va a permitir descartar
hipótesis rápido.

**Qué se revisó, y qué salió:**

| Comprobación | Resultado |
|---|---|
| `ls /dev/cu.*` | Solo Bluetooth, `debug-console` y `wlan-debug`. **Ningún puerto USB-Serie** |
| Árbol USB completo (`ioreg -p IOUSB`) | Hub VIA Labs, USB Billboard, webcam C920, segundo hub, **dock DisplayLink** e "instalador de driver" del dock. **Nada más** |
| Drivers serie cargados (`kextstat`) | Ninguno relacionado con CP2102, CH340 ni FTDI |
| Puertos serie registrados en el kernel | Los 3 internos de macOS. Ninguno USB |

**Un falso positivo que vale la pena documentar:**

Apareció un dispositivo del fabricante `wch.cn` (VID `0x1A86`) — que es
justamente la empresa que fabrica los chips **CH340**, el conversor USB-Serie más
común en placas ESP32 baratas. Por un momento pareció que era la placa.

No lo era. Al montarlo resultó ser un disco USB falso de 197 KB llamado *"Driver
Install"*, cuyo contenido apunta a los drivers de **DisplayLink/Synaptics**:

```
/Volumes/NO NAME/macOS Driver.url
  → https://www.synaptics.com/products/displaylink-graphics/downloads/macos
```

Es decir: el dock DisplayLink usa un controlador WCH para su propio instalador de
drivers. Coincidencia pura de fabricante. **No tiene nada que ver con el ESP32.**

Lo anoto porque la próxima vez que aparezca `wch.cn` en la lista, ya sabemos que
no es la placa.

**Conclusión:** la placa no está enumerando. Descartado que sea un problema de
driver faltante (macOS 26 trae soporte nativo para CP2102 y CH340) — el sistema
operativo ni siquiera ve un dispositivo al que asignarle driver. El problema está
antes: en el cable, en la conexión o en la placa.

### Hipótesis, ordenadas por probabilidad

1. 🥇 **El cable es de solo carga.** Muchísimos cables micro-USB (sobre todo los
   que vienen con power banks y audífonos) solo llevan los cables de corriente,
   sin los de datos. La placa se enciende, el LED prende, y la computadora no la
   ve. Es la causa #1 y la más frustrante porque *parece* que todo está bien.
2. 🥈 **Está conectada al dock DisplayLink**, no directo a la Mac. Los docks a
   veces no pasan bien dispositivos serie.
3. 🥉 **No está conectada ahora mismo**, simplemente.
4. **La placa está dañada** (regulador quemado, chip USB muerto). Es lo menos
   probable, y no hay que asumirlo hasta descartar lo anterior.

### 🔜 En qué me quedé — siguiente sesión

**Lo primero:** conseguir que la placa aparezca. El plan de prueba está en el
[README del proyecto](README.md#-bloqueante-actual). Sin eso, todo lo demás está
bloqueado.

**Lo segundo, y se puede hacer en paralelo sin la placa:** verificar que la API
de Spotify todavía permite leer "reproduciendo ahora" con una app personal. Es el
supuesto S-02 y el riesgo más grande del proyecto — si esto no se cumple, no hay
código que lo salve.

### Decisiones pendientes que te toca resolver

- [ADR-0002](1-decisiones/ADR-0002-como-obtener-datos-de-spotify.md) — ¿el ESP32
  habla directo con Spotify, o pasa por un puente en la Mac?
- Qué pantalla usar. El inventario no tiene ninguna registrada.
