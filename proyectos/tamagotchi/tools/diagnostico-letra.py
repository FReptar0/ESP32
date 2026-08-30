#!/usr/bin/env python3
"""Replica EXACTAMENTE la búsqueda de letra que hace el firmware.

Sirve para responder "la canción tiene letra en LRCLIB, ¿por qué la placa no la
encuentra?" sin tener que recompilar ni mirar el puerto serie.

Uso:
    python3 tools/diagnostico-letra.py                 # la canción que suena ahora
    python3 tools/diagnostico-letra.py "titulo" "artista" [album] [segundos]
"""
import re, io, sys, json, ssl, base64, urllib.parse, urllib.request, certifi, os

CTX = ssl.create_default_context(cafile=certifi.where())
UA  = {"User-Agent": "tamagotchi-esp32"}
AQUI = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def pedir(url, hdr=None, data=None):
    req = urllib.request.Request(url, data=data)
    for k, v in (hdr or {}).items(): req.add_header(k, v)
    try:
        r = urllib.request.urlopen(req, context=CTX)
        return r.status, json.load(r)
    except urllib.error.HTTPError as e:
        return e.code, None

def sonando_ahora():
    s = io.open(os.path.join(AQUI, "firmware/include/secrets.h"), encoding="utf-8").read()
    val = lambda k: re.search(r'#define\s+%s\s+"([^"]*)"' % k, s).group(1)
    cid, csec, rt = val("SPOTIFY_CLIENT_ID"), val("SPOTIFY_CLIENT_SECRET"), val("SPOTIFY_REFRESH_TOKEN")
    body = urllib.parse.urlencode({"grant_type": "refresh_token", "refresh_token": rt}).encode()
    _, tk = pedir("https://accounts.spotify.com/api/token",
                  {"Authorization": "Basic " + base64.b64encode(f"{cid}:{csec}".encode()).decode(),
                   "Content-Type": "application/x-www-form-urlencoded"}, body)
    cod, d = pedir("https://api.spotify.com/v1/me/player/currently-playing",
                   {"Authorization": "Bearer " + tk["access_token"]})
    if not d: sys.exit("No hay nada sonando")
    it = d["item"]
    return it["name"], it["artists"][0]["name"], it["album"]["name"], it["duration_ms"] // 1000

if len(sys.argv) >= 3:
    t, a = sys.argv[1], sys.argv[2]
    al = sys.argv[3] if len(sys.argv) > 3 else ""
    segs = int(sys.argv[4]) if len(sys.argv) > 4 else 0
else:
    t, a, al, segs = sonando_ahora()

print(f"🎵 {t} — {a}")
print(f"   álbum «{al}» · Spotify dice {segs} s\n")

# ---- PASO 1: la búsqueda exacta (lo que el firmware intenta primero) ----
url = ("https://lrclib.net/api/get?track_name=" + urllib.parse.quote(t) +
       "&artist_name=" + urllib.parse.quote(a) +
       "&album_name=" + urllib.parse.quote(al) + "&duration=" + str(segs))
cod, d = pedir(url, UA)
print(f"1) exacta          -> HTTP {cod}", "✅ con letra sincronizada"
      if cod == 200 and d and d.get("syncedLyrics") else "")
if cod == 200 and d and d.get("syncedLyrics"):
    print("\n   La placa DEBERÍA encontrarla. Si no lo hace, el problema no es LRCLIB.")
    sys.exit()

# ---- PASO 2: la búsqueda difusa ----
url = ("https://lrclib.net/api/search?track_name=" + urllib.parse.quote(t) +
       "&artist_name=" + urllib.parse.quote(a))
cod, res = pedir(url, UA)
print(f"2) búsqueda difusa -> HTTP {cod}, {len(res or [])} resultados\n")
if not res:
    print("   LRCLIB no conoce esta canción con ese título+artista.")
    sys.exit()

# ---- PASO 3: el filtro del firmware (±5 s) ----
print("   Lo que devuelve LRCLIB, y qué haría el firmware con cada uno:")
print("   " + "-" * 66)
dentro = 0
for r in res[:12]:
    dur  = int(r.get("duration") or 0)
    dif  = abs(dur - segs)
    sync = bool(r.get("syncedLyrics"))
    if dif <= 5: dentro += 1
    veredicto = ("✅ candidato" if dif <= 5 and sync else
                 "⚠️ sin letra sincronizada" if dif <= 5 else
                 f"❌ DESCARTADO por duración (±{dif} s > 5)")
    print(f"   {dur:>4} s  sync={'sí' if sync else 'no ':<3}  {veredicto}")
print("   " + "-" * 66)

conletra = [r for r in res if abs(int(r.get('duration') or 0) - segs) > 5 and r.get("syncedLyrics")]
print(f"\n   Dentro del margen ±5 s: {dentro}")
if dentro == 0 and conletra:
    print(f"\n   🔑 CAUSA: hay {len(conletra)} versión(es) CON letra sincronizada, pero todas")
    print(f"      fuera del margen de ±5 s. El firmware las descarta por duración.")
    print(f"      Spotify dice {segs} s; LRCLIB tiene "
          + ", ".join(str(int(r['duration'])) + ' s' for r in conletra[:5]) + ".")
