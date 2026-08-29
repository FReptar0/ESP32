#!/usr/bin/env python3
"""
============================================================
 Genera el paquete de certificados raíz para el ESP32
============================================================
 ¿Por qué hace falta esto?

 Cuando tu Mac se conecta a Spotify, verifica que el servidor
 sea de verdad Spotify usando el llavero del sistema: una lista
 de "autoridades certificadoras" (CA) en las que el sistema
 confía de fábrica.

 El ESP32 no tiene llavero. No confía en nadie porque no
 conoce a nadie. Hay que darle esa lista nosotros.

 Este script toma la lista de CAs de Mozilla (la misma que usan
 Firefox y casi todo el mundo, distribuida en Python como el
 paquete 'certifi') y la convierte al formato binario compacto
 que entiende el ESP32.

 De cada certificado solo se guardan dos cosas:
   · el nombre del emisor (para poder buscarlo)
   · su clave pública      (para verificar firmas)
 El resto del certificado no hace falta, y así el archivo
 ocupa una fracción de lo que ocuparían los PEM completos.

 Formato (definido en esp_crt_bundle.c del core de Espressif):

   uint16  número de certificados          (big endian)
   por cada certificado, ordenados por nombre:
     uint16  longitud del nombre           (big endian)
     uint16  longitud de la clave          (big endian)
     bytes   nombre  (DER)
     bytes   clave   (DER, SubjectPublicKeyInfo)

 Uso:
     python3 tools/gen-cert-bundle.py
============================================================
"""

import struct
import sys
from pathlib import Path

try:
    import certifi
    from cryptography import x509
    from cryptography.hazmat.primitives import serialization
except ImportError as e:
    raise SystemExit(f"❌ Falta una dependencia: {e}\n"
                     f"   Instálala con: python3 -m pip install --user certifi cryptography")

SALIDA = Path(__file__).resolve().parent.parent / "firmware" / "data" / "cert" / "x509_crt_bundle.bin"

print(__doc__)

origen = Path(certifi.where())
print(f"Leyendo certificados de:\n  {origen}\n")

# El archivo de certifi son muchos certificados PEM pegados uno tras otro.
pem = origen.read_bytes()
certs = x509.load_pem_x509_certificates(pem)
print(f"Certificados encontrados: {len(certs)}")

entradas = []
descartados = 0
for c in certs:
    try:
        nombre = c.subject.public_bytes()
        clave = c.public_key().public_bytes(
            serialization.Encoding.DER,
            serialization.PublicFormat.SubjectPublicKeyInfo,
        )
    except Exception:
        # Algún certificado con un tipo de clave que el ESP32 no soportaría.
        descartados += 1
        continue
    entradas.append((nombre, clave))

# ⚠️ El ESP32 busca por nombre con búsqueda binaria, así que la lista
#    TIENE que ir ordenada. Si no, no encontraría certificados que sí están.
entradas.sort(key=lambda e: e[0])

datos = struct.pack(">H", len(entradas))
for nombre, clave in entradas:
    datos += struct.pack(">HH", len(nombre), len(clave)) + nombre + clave

SALIDA.parent.mkdir(parents=True, exist_ok=True)
SALIDA.write_bytes(datos)

print(f"Descartados (clave no soportada): {descartados}")
print(f"Incluidos: {len(entradas)}")
print()
print(f"✅ Escrito: {SALIDA}")
print(f"   Tamaño : {len(datos):,} bytes ({len(datos)/1024:.1f} KB de flash)")
print()
print("Para usarlo, el platformio.ini debe tener:")
print("    board_build.embed_files = data/cert/x509_crt_bundle.bin")
