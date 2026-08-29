#!/usr/bin/env python3
"""
============================================================
 Genera una fuente CON ACENTOS para el ESP32
============================================================
 ¿Por qué hace falta?

 Las fuentes que trae TFT_eSPI solo tienen ASCII 32-127.
 Está en su propio código:

     if ((font>1) && (font<9) &&
         ((uniCode < 32) || (uniCode > 127))) return 0;

 O sea: la ñ, la á y la ó NO EXISTEN para esas fuentes y se
 descartan en silencio.

 PERO TFT_eSPI sí admite fuentes personalizadas en formato
 Adafruit GFX, y esas pueden cubrir hasta el código 255 —
 justo donde viven los caracteres del español (Latin-1).

 Este script coge una fuente TrueType del sistema, dibuja
 cada carácter del 32 al 255, y los empaqueta en el formato
 que el ESP32 entiende.

 Formato Adafruit GFX:
   · un array de bits con todos los dibujos, uno tras otro
   · una tabla que dice, por cada carácter, dónde empieza su
     dibujo, cuánto mide y cómo colocarlo
   · una cabecera con el rango de códigos cubierto

 Uso:
     python3 tools/gen-fuente.py
============================================================
"""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

# Avenir Next Demi Bold: geométrica-humanista, es lo más parecido que hay
# en macOS a la "Circular" que usa Spotify (que es propietaria y no se
# puede distribuir). El índice 2 dentro del .ttc es la variante Demi Bold.
TTF        = "/System/Library/Fonts/Avenir Next.ttc"
INDICE_TTF = 2

PRIMERO, ULTIMO = 0x20, 0xFF          # espacio .. ÿ  (cubre todo el español)
SALIDA = Path(__file__).resolve().parent.parent / "firmware" / "src" / "fuentes.h"


def generar(nombre, ruta_ttf, tam, indice=0):
    fuente = ImageFont.truetype(ruta_ttf, tam, index=indice)
    ascent, descent = fuente.getmetrics()

    bitmaps, glifos = [], []
    lienzo = tam * 3

    for cp in range(PRIMERO, ULTIMO + 1):
        ch = chr(cp)
        img = Image.new("1", (lienzo, lienzo), 0)
        d = ImageDraw.Draw(img)
        try:
            # 'ls' = anclar en la izquierda, sobre la línea base
            d.text((tam, tam * 2), ch, font=fuente, fill=1, anchor="ls")
        except Exception:
            pass

        caja = img.getbbox()
        avance = round(fuente.getlength(ch))

        if caja is None:                       # espacio o carácter sin dibujo
            glifos.append((len(bitmaps), 0, 0, avance, 0, 0))
            continue

        x0, y0, x1, y1 = caja
        an, al = x1 - x0, y1 - y0
        desplaz_x = x0 - tam
        desplaz_y = y0 - tam * 2               # negativo: por encima de la base

        inicio = len(bitmaps)
        # Los píxeles se empaquetan a 1 bit, SIN alinear cada fila:
        # el último bit de una fila y el primero de la siguiente comparten byte.
        bits = []
        px = img.load()
        for y in range(y0, y1):
            for x in range(x0, x1):
                bits.append(1 if px[x, y] else 0)
        for i in range(0, len(bits), 8):
            b = 0
            for j in range(8):
                if i + j < len(bits) and bits[i + j]:
                    b |= 0x80 >> j
            bitmaps.append(b)

        glifos.append((inicio, an, al, avance, desplaz_x, desplaz_y))

    alto_linea = ascent + descent

    out = [f"// {nombre}: {fuente.getname()[0]} {fuente.getname()[1]} a {tam}px, códigos "
           f"0x{PRIMERO:02X}-0x{ULTIMO:02X} (incluye ñ, tildes y ¿¡)",
           f"const uint8_t {nombre}Bitmaps[] PROGMEM = {{"]
    for i in range(0, len(bitmaps), 16):
        out.append("  " + ", ".join(f"0x{b:02X}" for b in bitmaps[i:i+16]) + ",")
    out.append("};\n")

    out.append(f"const GFXglyph {nombre}Glyphs[] PROGMEM = {{")
    for cp, g in zip(range(PRIMERO, ULTIMO + 1), glifos):
        vis = chr(cp) if 32 < cp < 127 else f"0x{cp:02X}"
        out.append(f"  {{ {g[0]:5d}, {g[1]:3d}, {g[2]:3d}, {g[3]:3d}, {g[4]:4d}, {g[5]:4d} }},  // '{vis}'")
    out.append("};\n")

    out.append(f"const GFXfont {nombre} PROGMEM = {{")
    out.append(f"  (uint8_t*){nombre}Bitmaps, (GFXglyph*){nombre}Glyphs,")
    out.append(f"  0x{PRIMERO:02X}, 0x{ULTIMO:02X}, {alto_linea} }};\n")

    return "\n".join(out), len(bitmaps), alto_linea


print(__doc__)
partes = ["#pragma once", "// GENERADO por tools/gen-fuente.py — no editar a mano.", ""]
total = 0
for nombre, tam in [("FuenteGrande", 20), ("FuenteChica", 14)]:
    txt, n, alto = generar(nombre, TTF, tam, INDICE_TTF)
    partes.append(txt)
    total += n
    print(f"  {nombre:<14} {tam:>2}px · alto de línea {alto:>2}px · {n:>6,} bytes de dibujos")

SALIDA.write_text("\n".join(partes))
print(f"\n✅ Escrito: {SALIDA}")
print(f"   Total en flash: ~{(total + 224*2*7)/1024:.1f} KB")
