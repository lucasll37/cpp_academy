# ============================================================
# src/inline_asm/meson.build
#
# Subprojeto: Assembly inline em C++
#
# Conceitos demonstrados (em ordem crescente de complexidade):
#   1. asm básico          — instrução única, sem operandos
#   2. asm com output      — escreve resultado em variável C++
#   3. asm com input       — lê variável C++ como operando
#   4. asm input + output  — aritmética; constraints "+r" e "0"
#   5. clobbers            — registradores usados fora dos operandos; "cc"
#   6. volatile            — o que o GCC pode remover, e o que é barreira
#   7. labels e saltos     — um laço inteiro em assembly, com %=
#   8. CPUID               — instrução real de hardware, 4 saídas fixas
#   9. RDTSC               — lê o contador de ciclos da CPU
#  10. constraint "m"      — operando na memória
#  11. flags + SETcc       — converter uma flag em valor C++ (8 bits!)
#  12. operandos nomeados  — %[nome] em vez de %0, %1
#
# Assembly em arquivos .s SEPARADOS é o subprojeto vizinho: src/outline_asm/.
#
# Não requer dependências externas além de fmt.
# Alvo: x86-64 Linux, GCC/Clang, C++17.
# ============================================================

executable('inline_asm',
    ['main.cpp'],
    dependencies: [fmt_dep],
    include_directories: include_directories('.'),
    install: true,
)
