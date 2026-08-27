#!/usr/bin/env python3
# =============================================================================
#  main_cpython.py — exercita o modulo academy_cp, escrito com a CPython API
#  crua (academy_cpython.cpp).
#
#  Como rodar (a partir da raiz do repositorio, depois de `make build`):
#
#      PYTHONPATH=build/src/python python3 src/python/main_cpython.py
# =============================================================================
import sys

try:
    import academy_cp as cp
except ImportError as e:
    sys.exit(
        f"{e}\n\n"
        "O modulo compilado nao esta no sys.path. Rode a partir da raiz:\n"
        "  make build\n"
        "  PYTHONPATH=build/src/python python3 src/python/main_cpython.py"
    )

print("=== CPython API ===")
print(cp.somar(3, 4))
print(cp.multiplicar(3, 4))
print(cp.fatorial(5))

try:
    cp.fatorial(-1)
except ValueError as e:
    print(f"erro capturado: {e}")

print(cp.info_threads())
print(cp.multiplicar_vetor([1.0, 2.0, 3.0], 10.0))
print(cp.soma_vetor([1.0, 2.0, 3.0, 4.0, 5.0]))

# De onde o Python carregou o .so, e com que nome de arquivo. O nome carrega a
# versao (cp312) e a plataforma: um .so de 3.12 NAO importa num Python 3.11.
print(f"\ncarregado de: {cp.__file__}")
