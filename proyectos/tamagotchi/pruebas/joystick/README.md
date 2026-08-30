# Prueba del joystick — antes de tocar el firmware

Un programa que **solo** lee la palanca. Ni WiFi ni pantalla, a propósito: si
algo falla aquí, el sospechoso es uno solo.

## Cómo correrlo

⚠️ Esto **reemplaza temporalmente** el firmware del Tamagotchi en la placa.
Después hay que volver a cargar el bueno; no se pierde nada.

```bash
cd proyectos/tamagotchi
cp pruebas/joystick/joystick.ino firmware/src/main.cpp.PRUEBA
# (o compilarlo aparte — ver más abajo)
```

## Qué tiene que salir

| Situación | `X` / `Y` | `SW` |
|---|---|---|
| Sin tocar | Los dos cerca del reposo medido | `1` |
| A tope en cada dirección | Cerca de `0` en un extremo, de `4095` en el otro | `1` |
| Pulsada hacia abajo | Sin cambio | `0` |

## Qué significa cada cosa si sale mal

| Síntoma | Causa más probable |
|---|---|
| Un eje siempre `0` | Ese cable de señal está suelto, o va a `GND` |
| Un eje siempre `4095` | Ese cable de señal va a `3V3` en vez de al pin |
| Los dos ejes siempre `0` | Falta la alimentación del joystick, o falta `GND` |
| `SW` siempre `0` | El cable del pulsador está tocando `GND` |
| `SW` siempre `1` aun pulsando | El cable de `SW` está suelto o en otro pin |
| Valores que bailan solos mucho | Falta `GND` común, o hay mal contacto |

## Los dos números que salen de aquí

1. **El reposo de TU unidad.** Ninguno de estos módulos descansa en 2048 exacto.
   El firmware definitivo tendrá que medirlo al arrancar, igual que hace esta
   prueba, en vez de suponerlo.

2. **Cuánto ruido tiene en reposo.** De ahí sale la **zona muerta**: el margen
   alrededor del centro donde no se hace nada. Sin ella, el ruido del propio
   conversor haría saltar canciones sin que nadie toque el aparato.

   Regla práctica: zona muerta ≈ **4 veces** el vaivén que veas en reposo.
