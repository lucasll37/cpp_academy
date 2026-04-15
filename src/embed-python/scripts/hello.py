"""
hello.py — script externo executado pelo host C++

Demonstra que o C++ pode rodar qualquer arquivo .py do disco,
sem recompilação. Isso é a base de sistemas de plugin em Python.
"""

import sys
import platform
import datetime

print(f"  rodando como script externo!")
print(f"  Python    : {sys.version.split()[0]}")
print(f"  plataforma: {platform.system()} {platform.machine()}")
print(f"  hora      : {datetime.datetime.now().strftime('%H:%M:%S')}")

# o namespace __main__ compartilhado permite ler variáveis
# que o C++ injetou antes de chamar PyRun_SimpleFile
if 'sensor_data' in dir():
    print(f"  sensor_data disponível: {sensor_data[:3]}...")
