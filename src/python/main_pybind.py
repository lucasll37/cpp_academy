import sys
sys.path.insert(0, 'build/python/pybind')
import academy_pb as pb

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

# benchmark comparativo
import time
grande = [1.0] * 10_000_000

sys.path.insert(0, 'build/python/cpython')
import academy_cp as cp

for nome, mod in [("cpython ", cp), ("pybind11", pb)]:
    inicio = time.perf_counter()
    mod.multiplicar_vetor(grande, 2.0)
    fim = time.perf_counter()
    print(f"{nome}: {(fim - inicio) * 1000:.2f} ms")