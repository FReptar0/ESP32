#!/usr/bin/env python3
"""
============================================================
 Tamagotchi Spotify — obtener el refresh_token (paso 4)
============================================================
 Esto se corre UNA SOLA VEZ, desde la Mac.

 Qué hace, en tres actos:
   1. Abre tu navegador en la página de Spotify para que TÚ
      inicies sesión y autorices la app.
   2. Levanta un servidor diminuto en 127.0.0.1:8888 que
      atrapa el "código de autorización" que Spotify devuelve.
   3. Cambia ese código por un refresh_token permanente.

 El refresh_token NO caduca. Se guarda en el ESP32 y con él
 la placa se renueva sola para siempre.

 ⚠️ Tu contraseña de Spotify nunca pasa por aquí: la escribes
    en la página de Spotify, en tu navegador. Este script solo
    ve el código que Spotify devuelve después.

 Uso:
     python3 tools/get-refresh-token.py
============================================================
"""

import base64
import http.server
import json
import re
import secrets as rnd
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from getpass import getpass
from pathlib import Path

# --- Configuración ---------------------------------------------------------

HOST, PORT = "127.0.0.1", 8888
REDIRECT = f"http://{HOST}:{PORT}/callback"

# ⚠️ SOLO el permiso mínimo necesario.
#
# /me/player/currently-playing pide únicamente este scope. NO pedimos
# "user-modify-playback-state" a propósito: ese permitiría controlar la
# reproducción (play, pausa, saltar), está fuera del alcance del proyecto,
# y este token va a vivir dentro del ESP32. Si el aparato se pierde, lo
# máximo que se filtra es qué música escuchas — no el control de tu cuenta.
SCOPE = "user-read-currently-playing"

SECRETS_H = Path(__file__).resolve().parent.parent / "firmware" / "include" / "secrets.h"

# --- Credenciales ----------------------------------------------------------

print(__doc__)
print(f"Redirect URI que debe estar registrado en tu app:\n    {REDIRECT}\n")
print("(Si en el dashboard pusiste 'localhost' en vez de '127.0.0.1',")
print(" cámbialo ahora: Spotify ya no acepta 'localhost'.)\n")

CLIENT_ID = input("Client ID: ").strip()
CLIENT_SECRET = getpass("Client Secret (no se muestra al teclear): ").strip()

if not CLIENT_ID or not CLIENT_SECRET:
    raise SystemExit("❌ Faltan credenciales. Salen del dashboard de Spotify.")

# --- Acto 1: mandar al usuario a autorizar ---------------------------------

# El "state" es un número al azar que va y vuelve sin cambiar. Sirve para
# comprobar que la respuesta que recibimos corresponde a la petición que
# hicimos nosotros, y no a una inyectada por otro (protección CSRF).
estado = rnd.token_urlsafe(16)

url_auth = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode({
    "client_id": CLIENT_ID,
    "response_type": "code",
    "redirect_uri": REDIRECT,
    "scope": SCOPE,
    "state": estado,
})

# --- Acto 2: atrapar el código que devuelve Spotify ------------------------

capturado = {}


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        partes = urllib.parse.urlparse(self.path)

        # Los navegadores piden /favicon.ico por su cuenta. Si no lo
        # filtráramos, esa petición se comería el turno del servidor y
        # perderíamos el callback de verdad.
        if partes.path != "/callback":
            self.send_response(404)
            self.end_headers()
            return

        capturado.update({k: v[0] for k, v in urllib.parse.parse_qs(partes.query).items()})
        ok = "code" in capturado and capturado.get("state") == estado

        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        titulo = "✅ Listo" if ok else "❌ Algo salió mal"
        self.wfile.write(
            f"<html><body style='font-family:system-ui;text-align:center;padding-top:3em'>"
            f"<h2>{titulo}</h2><p>Ya puedes cerrar esta pestaña y volver a la terminal.</p>"
            f"</body></html>".encode("utf-8")
        )

    def log_message(self, *a):
        pass  # silencio: no queremos los logs del servidor en la terminal


print("\nAbriendo el navegador. Inicia sesión y dale a 'Agree'...")
print("(Si no se abre solo, entra a esta dirección a mano:)")
print(f"\n{url_auth}\n")

webbrowser.open(url_auth)

servidor = http.server.HTTPServer((HOST, PORT), Handler)
servidor.timeout = 180  # 3 minutos de paciencia

print(f"Esperando la respuesta de Spotify en {REDIRECT} ...")
while "code" not in capturado and "error" not in capturado:
    servidor.handle_request()
servidor.server_close()

if "error" in capturado:
    raise SystemExit(f"\n❌ Spotify devolvió un error: {capturado['error']}\n"
                     "   Si dice 'access_denied', le diste a 'Cancel' en vez de 'Agree'.")

# Comprobación del state: si no coincide, la respuesta no es la nuestra.
if capturado.get("state") != estado:
    raise SystemExit("\n❌ El 'state' no coincide. Respuesta descartada por seguridad.\n"
                     "   Vuelve a correr el script.")

print("✅ Código de autorización recibido.\n")

# --- Acto 3: cambiar el código por los tokens ------------------------------

# Spotify espera las credenciales en la cabecera Authorization, como
# "Basic <client_id:client_secret en base64>". No es cifrado, solo
# codificación — por eso todo esto va obligatoriamente por HTTPS.
basic = base64.b64encode(f"{CLIENT_ID}:{CLIENT_SECRET}".encode()).decode()

peticion = urllib.request.Request(
    "https://accounts.spotify.com/api/token",
    data=urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": capturado["code"],
        "redirect_uri": REDIRECT,
    }).encode(),
    headers={
        "Authorization": f"Basic {basic}",
        "Content-Type": "application/x-www-form-urlencoded",
    },
)

try:
    with urllib.request.urlopen(peticion, timeout=30) as r:
        tokens = json.load(r)
except urllib.error.HTTPError as e:
    detalle = e.read().decode("utf-8", errors="replace")
    print(f"\n❌ Spotify rechazó el intercambio (HTTP {e.code}):\n   {detalle}\n")
    print("   Causas típicas:")
    print("     • 'Invalid redirect URI'  -> el Redirect URI del dashboard no es")
    print(f"       exactamente {REDIRECT} (revisa http/https, la IP y el puerto)")
    print("     • 'Invalid client'        -> Client ID o Secret mal copiados")
    raise SystemExit(1)

refresh = tokens.get("refresh_token")
if not refresh:
    raise SystemExit(f"❌ La respuesta no trae refresh_token:\n{tokens}")

print("=" * 62)
print("🎉 ¡Listo! Tokens obtenidos.")
print("=" * 62)
print(f"  Permisos concedidos : {tokens.get('scope')}")
print(f"  access_token caduca : en {tokens.get('expires_in')} s (~1 hora)")
print("  refresh_token       : obtenido ✅  (este NO caduca)")
print()

# --- Guardar en secrets.h --------------------------------------------------

print("⚠️  El refresh_token es tan valioso como una contraseña: da acceso a tu")
print("    cuenta de Spotify hasta que lo revoques desde tu perfil.\n")

if SECRETS_H.exists():
    resp = input(f"¿Lo escribo directamente en {SECRETS_H.name}? [S/n]: ").strip().lower()
    if resp in ("", "s", "si", "sí", "y"):
        texto = SECRETS_H.read_text()
        for clave, valor in (("SPOTIFY_CLIENT_ID", CLIENT_ID),
                             ("SPOTIFY_CLIENT_SECRET", CLIENT_SECRET),
                             ("SPOTIFY_REFRESH_TOKEN", refresh)):
            patron = rf'(#define\s+{clave}\s+)"[^"]*"'
            nuevo = rf'\g<1>"{valor}"'
            texto, n = re.subn(patron, nuevo, texto)
            if n == 0:
                texto += f'\n#define {clave} "{valor}"\n'
        SECRETS_H.write_text(texto)
        print(f"\n✅ Guardado en {SECRETS_H}")
        print("   Ese archivo está en .gitignore: nunca se sube al repositorio.")
    else:
        print(f"\nPégalo tú mismo en {SECRETS_H}:\n")
        print(f'  #define SPOTIFY_REFRESH_TOKEN "{refresh}"')
else:
    print(f"No encontré {SECRETS_H}. Pega esto donde corresponda:\n")
    print(f'  #define SPOTIFY_REFRESH_TOKEN "{refresh}"')

print("\nSiguiente paso: que el ESP32 use ese refresh_token para renovarse solo.")
