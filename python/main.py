import academy

# ============================================================
# FUNÇÕES BÁSICAS
# sem OpenMP — só demonstram o fluxo Python → C → Python
# ============================================================

print("=== funções básicas ===")

print(academy.somar(3, 4))           # 7
print(academy.multiplicar(3, 4))     # 12
print(academy.fatorial(5))           # 120

# testando exceção vinda do C via PyErr_SetString
try:
    academy.fatorial(-1)
except ValueError as e:
    print(f"erro capturado: {e}")    # fatorial não definido para negativos


# ============================================================
# INFO THREADS
# inspeciona o ambiente OpenMP antes de usar
# retorna um dict construído no C com PyDict_New
# ============================================================

print("\n=== ambiente openmp ===")

info = academy.info_threads()
print(f"processadores lógicos : {info['num_procs']}")
print(f"threads disponíveis   : {info['max_threads']}")


# ============================================================
# MULTIPLICAR VETOR
# demonstra o padrão: copiar para C → liberar GIL → paralelizar → devolver
# ============================================================

print("\n=== multiplicar_vetor ===")

vetor = [1.0, 2.0, 3.0, 4.0, 5.0]
resultado = academy.multiplicar_vetor(vetor, 10.0)
print(f"entrada  : {vetor}")
print(f"escalar  : 10.0")
print(f"saída    : {resultado}")     # [10.0, 20.0, 30.0, 40.0, 50.0]


# ============================================================
# SOMA VETOR
# demonstra reduction paralela — múltiplas threads somam
# parcialmente e o OpenMP combina os resultados no final
# ============================================================

print("\n=== soma_vetor ===")

vetor = [1.0, 2.0, 3.0, 4.0, 5.0]
total = academy.soma_vetor(vetor)
print(f"entrada  : {vetor}")
print(f"soma     : {total}")         # 15.0


# ============================================================
# TESTE DE ESCALA
# vetores grandes onde o paralelismo realmente faz diferença
# em vetores pequenos o overhead de criar threads pode superar o ganho
# ============================================================

print("\n=== teste de escala ===")

import time

grande = [1.0] * 10_000_000  # 10 milhões de elementos

inicio = time.perf_counter()
resultado = academy.multiplicar_vetor(grande, 2.0)
fim = time.perf_counter()

print(f"vetor de {len(grande):,} elementos")
print(f"tempo         : {(fim - inicio) * 1000:.2f} ms")
print(f"primeiro elem : {resultado[0]}")   # 2.0
print(f"último elem   : {resultado[-1]}")  # 2.0