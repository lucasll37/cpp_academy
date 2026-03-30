import academy

print(academy.somar(3, 4))        # 7
print(academy.multiplicar(3, 4))  # 12
print(academy.fatorial(5))        # 120

# testando exceção
try:
    academy.fatorial(-1)
    
except ValueError as e:
    print(f"Erro: {e}")