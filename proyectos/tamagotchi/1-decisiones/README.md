# Decisiones del proyecto Tamagotchi (ADRs)

Índice de todas las decisiones tomadas. **Ninguna se borra**: cuando una decisión
cambia, se escribe un ADR nuevo que reemplaza al viejo, y el viejo se queda ahí
con su razonamiento original intacto.

| ADR | Decisión | Estado | Fecha |
|---|---|---|---|
| [0001](ADR-0001-plataforma-de-desarrollo.md) | Usar PlatformIO con framework Arduino | ✅ `Aceptado` | 2026-08-29 |
| [0002](ADR-0002-como-obtener-datos-de-spotify.md) | El ESP32 habla **directo** con Spotify, sin puente | ✅ `Aceptado` | 2026-08-29 |
| [0003](ADR-0003-fuente-de-las-letras.md) | De dónde salen las letras | ⚠️ `Propuesto` | 2026-08-29 |
| [0004](ADR-0004-pantalla-gmt130.md) | Usar la TFT IPS 1.3" GMT130 (240×240) | ✅ `Aceptado` | 2026-08-29 |
| [0010](ADR-0010-permisos-de-spotify.md) | Pedir permisos de lectura **y control** desde el inicio | ✅ `Aceptado` | 2026-08-29 |

## Decisiones que faltan por tomar

Estas ya se sabe que van a hacer falta. Se escribirán cuando haya información
suficiente:

| Futuro ADR | Tema | Qué lo desbloquea |
|---|---|---|
| ADR-0005 | Alimentación: ¿USB o batería? | Definir si el aparato se mueve de lugar o vive enchufado |
| ADR-0006 | Cómo se guardan las credenciales de WiFi y Spotify | 🟢 Desbloqueado por el ADR-0002 |
| ADR-0009 | Cómo se manejan los certificados raíz TLS (y su caducidad) | 🎯 **Desbloqueado y ahora es el riesgo #1** |
| ADR-0007 | Cómo se interactúa con el Tamagotchi (botones, encoder, touch) | Definir qué hace además de mostrar la canción |
| ADR-0008 | Qué librería usar para la pantalla (`TFT_eSPI` vs `Adafruit_ST7789`) | Confirmar el controlador de la GMT130 |

---

## Cómo escribir uno nuevo

1. Copia [`metodologia/plantillas/ADR-plantilla.md`](../../../metodologia/plantillas/ADR-plantilla.md)
2. Nómbralo `ADR-NNNN-titulo-corto.md` con el siguiente número libre
3. Agrégalo a la tabla de arriba

O simplemente dime *"documenta esta decisión"* y yo lo escribo.

## Cuándo escribir uno

No todo merece un ADR. La prueba es: **¿te vas a acordar de por qué, dentro de
tres meses?** Si la respuesta es no, y la decisión afecta algo que sería molesto
cambiar después, escríbelo.

Merecen ADR:
- Elegir un componente sobre otro
- Elegir una librería, un protocolo o una API
- Decidir cómo se alimenta el circuito
- Decidir **no** hacer algo que parecía obvio

No merecen ADR: nombres de variables, colores, o cualquier cosa que se cambie en
30 segundos.
