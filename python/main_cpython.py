import academy_cp as cp

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