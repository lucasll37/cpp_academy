#!/usr/bin/env python3
# =============================================================================
#  main_pybind.py — exercita o modulo academy_pb (pybind11) e compara com o
#  academy_cp (CPython API) no mesmo trabalho.
#
#  Como rodar (a partir da raiz do repositorio, depois de `make build`):
#
#      PYTHONPATH=build/src/python python3 src/python/main_pybind.py
#
#  Nao ha sys.path.insert() aqui de proposito: quem escolhe onde o .so mora e
#  o PYTHONPATH, nao o script. Um sys.path.insert com caminho fixo quebra
#  assim que o diretorio de build muda — foi exatamente o que aconteceu com a
#  versao anterior deste arquivo.
# =============================================================================
import sys
import time

try:
    import academy_pb as pb
    import academy_cp as cp
except ImportError as e:
    sys.exit(
        f"{e}\n\n"
        "Os modulos compilados nao estao no sys.path. Rode a partir da raiz:\n"
        "  make build\n"
        "  PYTHONPATH=build/src/python python3 src/python/main_pybind.py"
    )

print("=== pybind11 ===")
print(pb.somar(3, 4))
print(pb.multiplicar(3, 4))
print(pb.fatorial(5))

try:
    pb.fatorial(-1)
except ValueError as e:
    print(f"erro capturado: {e}")

print(pb.info_threads())
print(pb.multiplicar_vetor([1.0, 2.0, 3.0], 10.0))
print(pb.soma_vetor([1.0, 2.0, 3.0, 4.0, 5.0]))

# -----------------------------------------------------------------------------
# Benchmark. Tres licoes de uma vez:
#   1. multiplicar_vetor devolve uma lista de 10 milhoes de floats Python. Criar
#      esses 10M objetos custa mais do que a multiplicacao paralela economiza.
#   2. soma_vetor devolve UM float: nao paga a volta, e por isso e ~7x mais
#      rapida no mesmo vetor.
#   3. a lista Python pura (list comprehension) empata com a extensao. O gargalo
#      nao e a aritmetica, e a travessia Python <-> C++.
# min-de-N, nao media: o minimo e a amostra menos contaminada por ruido.
# -----------------------------------------------------------------------------
print("\n=== benchmark (10.000.000 doubles, min de 5) ===")
grande = [1.0] * 10_000_000


def bench(fn, repeticoes=5):
    """Roda fn N vezes e devolve o MENOR tempo em milissegundos."""
    tempos = []
    for _ in range(repeticoes):
        inicio = time.perf_counter()
        fn()
        tempos.append((time.perf_counter() - inicio) * 1000)
    return min(tempos)


print(f"cpython  multiplicar_vetor: {bench(lambda: cp.multiplicar_vetor(grande, 2.0)):7.1f} ms")
print(f"pybind11 multiplicar_vetor: {bench(lambda: pb.multiplicar_vetor(grande, 2.0)):7.1f} ms")
print(f"python   [x*2.0 for x in v]:{bench(lambda: [x * 2.0 for x in grande], 3):7.1f} ms")
print(f"cpython  soma_vetor:        {bench(lambda: cp.soma_vetor(grande)):7.1f} ms")
print(f"pybind11 soma_vetor:        {bench(lambda: pb.soma_vetor(grande)):7.1f} ms")
print(f"python   sum(v):            {bench(lambda: sum(grande), 3):7.1f} ms")

print("\n=== custo de UMA travessia (somar(2,3), 500k chamadas) ===")
for nome, mod in (("cpython ", cp), ("pybind11", pb)):
    t = time.perf_counter()
    for _ in range(500_000):
        mod.somar(2, 3)
    print(f"{nome}: {(time.perf_counter() - t) * 1e9 / 500_000:6.0f} ns/chamada")


def somar_puro(a, b):
    return a + b


t = time.perf_counter()
for _ in range(500_000):
    somar_puro(2, 3)
print(f"python  : {(time.perf_counter() - t) * 1e9 / 500_000:6.0f} ns/chamada")
