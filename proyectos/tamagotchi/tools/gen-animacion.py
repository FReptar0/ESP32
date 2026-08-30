#!/usr/bin/env python3
"""Convierte un GIF en fotogramas de 1 bit para el ESP32.

Por qué 1 bit y no color:
  Un fotograma a 240x240 en color son 115.200 B. Siete no caben ni de
  broma. En 1 bit son 7.200 B — dieciséis veces menos. Y no se pierde
  nada, porque Snoopy YA es línea negra sobre blanco.

Por qué no se reproduce el GIF directamente:
  Habría que descomprimirlo en memoria en marcha, y este aparato tiene
  el montón muy troceado (ver 5-investigacion/portada-cache-descartada.md).
  Convertirlo una vez, aquí en el ordenador, sale gratis en la placa.

Uso:  gen-animacion.py <entrada.gif> <nombre> [ancho] [alto]
"""
import sys
from collections import deque
from PIL import Image, ImageSequence

# --- Umbrales, sacados de medir este GIF concreto (2026-08-30) ---
#
# ⚠️ La tinta tiene que ser oscura Y NEUTRA. El primer intento solo
#    miraba lo oscuro, y la franja naranja de la pared del GIF entraba
#    como si fuera tinta: salía una raya negra cruzando la pantalla.
#    El negro de Snoopy tiene saturación 11 de mediana; la franja, 69-135.
BRILLO_TINTA = 95
# ⚠️ Este umbral tiene DOS bordes y hay que respetar los dos:
#    - Muy alto (45+): entra la sombra marrón de la pared, RGB(70,49,26),
#      saturación 44. Cruza toda la imagen y revienta el encuadre.
#    - Muy bajo (30): se come el contorno de Snoopy. Medido: pasa de
#      21.535 a 18.830 píxeles de tinta, y las líneas salen ROTAS.
#    Se queda en 45 —líneas sólidas— y el decorado se quita por su
#    FORMA, no por su color: ver ANCHO_DECORADO.
SATUR_MAX    = 45

MOTA_MIN     = 12    # trozos sueltos más pequeños que esto son ruido
# Un personaje nunca cruza la imagen entera; una línea de decorado sí.
# Es lo que separa la franja de la pared de la oreja de Snoopy sin
# tocar el color, y por tanto sin adelgazarle el contorno.
ANCHO_DECORADO = 0.5   # fracción del ancho total
RAYA_ALTO    = 5     # "plano y ancho" = trozo de decorado, no de Snoopy
RAYA_ANCHO   = 14
FINA_GROSOR  = 2     # línea de 1-2 px de alto...
FINA_LARGO   = 16    # ...y larga: es del fondo, no suya

# Desde qué altura se mide el TORSO, en fracción del alto.
#
# ⚠️ Centrar por la caja que envuelve al personaje NO lo centra a la
#    vista. La oreja de Snoopy sobresale mucho a la derecha, así que la
#    caja queda centrada mientras su CUERPO se ve corrido a la
#    izquierda. Reportado mirando la pantalla, y confirmado midiendo:
#    caja centrada en 63,5 de 64, pero torso en 54.
#
# 🔑 Un personaje no es un rectángulo. Lo que el ojo centra es su eje,
#    no sus extremos.
TORSO_DESDE  = 0.55

def es_tinta(p):
    r, g, b = p
    return (r + g + b) / 3 < BRILLO_TINTA and max(p) - min(p) < SATUR_MAX

def sacar_mascara(f, W, H):
    pix = f.load()
    return [[1 if es_tinta(pix[x, y]) else 0 for x in range(W)] for y in range(H)]

def quitar_motas(m, W, H):
    """Trozos sueltos: polvo del suelo y la sombra que aparece y
    desaparece entre cuadros (y por tanto parpadearía)."""
    visto = [[0]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if not m[y][x] or visto[y][x]:
                continue
            q = deque([(x, y)]); visto[y][x] = 1; comp = []
            while q:
                cx, cy = q.popleft(); comp.append((cx, cy))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = cx+dx, cy+dy
                        if 0 <= nx < W and 0 <= ny < H and m[ny][nx] and not visto[ny][nx]:
                            visto[ny][nx] = 1; q.append((nx, ny))
            xs = [c[0] for c in comp]; ys = [c[1] for c in comp]
            ancho, alto = max(xs)-min(xs)+1, max(ys)-min(ys)+1
            if (len(comp) < MOTA_MIN
                    or (alto <= RAYA_ALTO and ancho >= RAYA_ANCHO)
                    or ancho > W * ANCHO_DECORADO):
                for cx, cy in comp:
                    m[cy][cx] = 0

def quitar_rayas_finas(m, W, H):
    """Restos del decorado PEGADOS a Snoopy, que el filtro de trozos
    sueltos no ve porque forman un solo trozo con él.

    ⚠️ Hay que medir el grosor pixel a pixel, no en el centro de la
       racha: la raya sale de su pata, y el centro caía sobre la pata
       —que es gruesa— así que la raya se salvaba.

    Su oreja también es negra y ancha, pero es MACIZA: 20 filas de alto.
    Una raya de decorado tiene 1 o 2. Ahí está la diferencia."""
    def grosor(x, y):
        a = y
        while a > 0 and m[a-1][x]: a -= 1
        b = y
        while b < H-1 and m[b+1][x]: b += 1
        return b - a + 1
    def largo(x, y):
        a = x
        while a > 0 and m[y][a-1]: a -= 1
        b = x
        while b < W-1 and m[y][b+1]: b += 1
        return b - a + 1
    fuera = [(x, y) for y in range(H) for x in range(W)
             if m[y][x] and grosor(x, y) <= FINA_GROSOR and largo(x, y) >= FINA_LARGO]
    for x, y in fuera:
        m[y][x] = 0

def quitar_lejanos(m, W, H, holgura=25):
    """⚠️ NO SE USA. Se deja escrita porque la idea es tentadora y está
       MAL, y sin esta nota alguien la volvería a intentar.

       Borra lo que está lejos del "trozo más grande". Falla porque
       `quitar_rayas_finas` ya ha partido el contorno del personaje en
       pedazos: el trozo más grande deja de ser su silueta y pasa a ser
       su OREJA, que es lo único macizo. Resultado: se borra casi todo
       Snoopy y queda la oreja flotando.

       🔑 Un filtro que depende de "el trozo más grande" hay que
          aplicarlo ANTES de cualquier cosa que rompa trozos. Aquí no
          hacía falta: el encuadre por percentiles resuelve lo mismo
          sin borrar nada.

    Quita lo que sobrevivió a todo lo anterior pero está LEJOS del
    personaje.

    ⚠️ Hacía falta un tercer filtro porque quedaban motas de 12 a 30 px
       de la sombra de la pared: demasiado grandes para el filtro de
       motas y demasiado estrechas para el de decorado. Subir el umbral
       de motas las habría quitado, pero también los detalles de su
       cara, que son trozos sueltos de ese mismo tamaño.

    🔑 No se distinguen por tamaño ni por forma: se distinguen por
       DÓNDE están. Un ojo está pegado a la cabeza; una mota de la
       pared, a 150 px."""
    comps = []
    visto = [[0]*W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if not m[y][x] or visto[y][x]:
                continue
            q = deque([(x, y)]); visto[y][x] = 1; comp = []
            while q:
                cx, cy = q.popleft(); comp.append((cx, cy))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = cx+dx, cy+dy
                        if 0 <= nx < W and 0 <= ny < H and m[ny][nx] and not visto[ny][nx]:
                            visto[ny][nx] = 1; q.append((nx, ny))
            comps.append(comp)
    if not comps:
        return
    # El personaje es, con diferencia, el trozo más grande: su contorno
    # va todo unido (cabeza, cuello, cuerpo, patas).
    principal = max(comps, key=len)
    px = [c[0] for c in principal]; py = [c[1] for c in principal]
    a, b, c_, d = min(px)-holgura, min(py)-holgura, max(px)+holgura, max(py)+holgura
    for comp in comps:
        if comp is principal:
            continue
        cx = [q[0] for q in comp]; cy = [q[1] for q in comp]
        if max(cx) < a or min(cx) > c_ or max(cy) < b or min(cy) > d:
            for qx, qy in comp:
                m[qy][qx] = 0


def main():
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    ruta, nombre = sys.argv[1], sys.argv[2]
    ANCHO = int(sys.argv[3]) if len(sys.argv) > 3 else 128
    ALTO  = int(sys.argv[4]) if len(sys.argv) > 4 else 184

    im = Image.open(ruta)
    cuadros = [f.convert('RGB') for f in ImageSequence.Iterator(im)]
    W, H = cuadros[0].size
    duracion = im.info.get('duration', 100)
    print(f"  {ruta}: {W}x{H}, {len(cuadros)} cuadros, {duracion} ms cada uno")

    mascaras = []
    for f in cuadros:
        m = sacar_mascara(f, W, H)
        quitar_motas(m, W, H)
        quitar_rayas_finas(m, W, H)
        quitar_motas(m, W, H)      # otra vez: quitar rayas deja trozos nuevos sueltos
        mascaras.append(m)

    # Un encuadre COMÚN a todos los cuadros. Uno por cuadro haría que
    # el personaje diera saltos: cada cuadro se centraría distinto.
    #
    # ⚠️ El encuadre NO se saca del mínimo y el máximo. Basta UNA mota
    #    superviviente a 150 px del personaje para que la caja abarque
    #    media imagen y Snoopy salga diminuto. Pasó: el encuadre salía
    #    (173, 17, 478, 242) por 12 píxeles de sombra de pared.
    #
    #    Se recorta un 0,2 % por cada lado. Sobre ~20.000 píxeles de
    #    tinta eso son unas decenas —justo las motas— y no toca las
    #    puntas del personaje, que van respaldadas por miles.
    #
    # 🔑 Contra un dato con motas, la mediana o un percentil; nunca el
    #    máximo. El máximo ES la mota.
    xs, ys = [], []
    for m in mascaras:
        for y in range(H):
            for x in range(W):
                if m[y][x]:
                    xs.append(x); ys.append(y)
    xs.sort(); ys.sort()
    # 0,5 %: medido sobre este GIF, es donde el borde deja de moverse
    # (0,2 % -> x1=396 por las motas; 0,5 % -> 323, su borde real; 1 % ->
    # 320, ya empieza a comerle). El escalón entre 396 y 323 es la mota.
    recorte = max(1, int(len(xs) * 0.005))
    x0, x1 = xs[recorte], xs[-recorte]
    y0, y1 = ys[recorte], ys[-recorte]
    caja = (max(0, x0-5), max(0, y0-5), min(W, x1+6), min(H, y1+6))
    print(f"  encuadre comun: {caja}  ->  {ANCHO}x{ALTO}")

    escalados = []
    for m in mascaras:
        g = Image.new('L', (W, H), 0); p = g.load()
        for y in range(H):
            for x in range(W):
                if m[y][x]: p[x, y] = 255
        # ⚠️ Escalar a la fuerza a ANCHO x ALTO lo DEFORMA. El encuadre
        #    salía casi cuadrado y el destino es alargado, así que Snoopy
        #    aparecía aplastado de lado — flaco y estirado.
        #
        # 🔑 Un recorte y un destino con proporciones distintas no se
        #    ajustan estirando: se ajustan dejando aire.
        g = g.crop(caja)
        escala = min(ANCHO / g.width, ALTO / g.height)
        nw, nh = max(1, int(g.width * escala)), max(1, int(g.height * escala))
        escalados.append(g.resize((nw, nh), Image.LANCZOS))

    # --- Centrado por el TORSO, común a todos los cuadros ---
    #
    # El desplazamiento es UNO SOLO para los siete. Calcularlo por
    # cuadro clavaría a Snoopy en el centro y le quitaría el balanceo:
    # dejaría de bailar y parecería una figura rígida.
    nw, nh = escalados[0].size
    y_torso = int(nh * TORSO_DESDE)
    tx = []
    for g in escalados:
        gp = g.load()
        tx += [x for y in range(y_torso, nh) for x in range(nw) if gp[x, y] > 105]
    centro_torso = (min(tx) + max(tx)) / 2

    # Todo lo que hay que pintar, para comprobar que nada se sale
    todos = []
    for g in escalados:
        gp = g.load()
        todos += [x for y in range(nh) for x in range(nw) if gp[x, y] > 105]
    ink0, ink1 = min(todos), max(todos)

    off_x = int(round(ANCHO / 2 - centro_torso))
    off_y = (ALTO - nh) // 2
    print(f"  torso en x={centro_torso:.1f} de {nw} -> se corre {off_x:+d} px")
    if ink0 + off_x < 0 or ink1 + off_x > ANCHO - 1:
        ancho_min = 2 * max(ink1 - centro_torso, centro_torso - ink0) + 2
        print(f"  ⚠️  NO CABE: con ese desplazamiento se recorta. "
              f"Hacen falta al menos {int(ancho_min)+1} px de ancho.")
        sys.exit(1)

    bytes_por_fila = (ANCHO + 7) // 8
    salida = []
    for g in escalados:
        lienzo = Image.new('L', (ANCHO, ALTO), 0)
        lienzo.paste(g, (off_x, off_y))
        gp = lienzo.load()
        datos = bytearray()
        for y in range(ALTO):
            for bx in range(bytes_por_fila):
                b = 0
                for bit in range(8):
                    x = bx*8 + bit
                    if x < ANCHO and gp[x, y] > 105:
                        b |= 0x80 >> bit      # MSB primero, como espera drawBitmap
                datos.append(b)
        salida.append(datos)

    tam = len(salida[0])
    print(f"  {tam} B por cuadro x {len(salida)} = {tam*len(salida)} B")

    with open(f'../firmware/src/{nombre}.h', 'w') as fh:
        fh.write(f"""// Generado por tools/gen-animacion.py — NO editar a mano
//
// Origen: {ruta.split('/')[-1]}
// {len(salida)} cuadros de {ANCHO}x{ALTO}, 1 bit por pixel, MSB primero.
// {tam} B por cuadro, {tam*len(salida)} B en total.
//
// 1 = tinta (negro), 0 = papel (blanco). Se dibuja con
// `tft.drawBitmap(x, y, cuadro, ANCHO, ALTO, negro, blanco)`.
#pragma once
#include <Arduino.h>

const int {nombre.upper()}_ANCHO   = {ANCHO};
const int {nombre.upper()}_ALTO    = {ALTO};
const int {nombre.upper()}_CUADROS = {len(salida)};
const int {nombre.upper()}_MS      = {duracion};

""")
        for i, d in enumerate(salida):
            fh.write(f"const uint8_t {nombre}_{i}[{tam}] PROGMEM = {{\n")
            for j in range(0, len(d), 16):
                fh.write("  " + ",".join(f"0x{b:02X}" for b in d[j:j+16]) + ",\n")
            fh.write("};\n\n")
        fh.write(f"const uint8_t* const {nombre.upper()}[{len(salida)}] = {{\n")
        fh.write("  " + ", ".join(f"{nombre}_{i}" for i in range(len(salida))) + "\n};\n")
    print(f"  ✅ escrito firmware/src/{nombre}.h")

if __name__ == '__main__':
    main()
