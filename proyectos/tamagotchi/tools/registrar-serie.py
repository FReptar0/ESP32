#!/usr/bin/env python3
"""Graba lo que dice la placa en un archivo, con hora de la Mac.

Existe porque los fallos interesantes ocurren cuando nadie está mirando el
puerto serie. Sin esto, para investigar algo hay que reproducirlo justo
mientras se captura — y hay fallos que tardan una canción entera en aparecer.

Uso:  python3 tools/registrar-serie.py [minutos]      (por defecto 60)
Sale en:  logs/serie-AAAA-MM-DD-HHMM.log
"""
import serial, time, sys, os, datetime

AQUI = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
mins = float(sys.argv[1]) if len(sys.argv) > 1 else 60.0
nombre = datetime.datetime.now().strftime("serie-%Y-%m-%d-%H%M.log")
ruta = os.path.join(AQUI, "logs", nombre)

p = serial.Serial('/dev/cu.usbserial-0001', 115200, timeout=0.3)
p.reset_input_buffer()
fin = time.time() + mins * 60
resto = ""
with open(ruta, "w", encoding="utf-8", buffering=1) as f:
    f.write(f"# Registro iniciado {datetime.datetime.now():%Y-%m-%d %H:%M:%S}\n")
    print(f"Grabando en logs/{nombre} durante {mins:g} min…")
    while time.time() < fin:
        d = p.read(4096)
        if not d:
            continue
        resto += d.decode('utf-8', errors='replace')
        *lineas, resto = resto.split("\n")
        for l in lineas:
            l = l.strip()
            if l:
                f.write(f"{datetime.datetime.now():%H:%M:%S}  {l}\n")
p.close()
print(f"Terminado: logs/{nombre}")
