# ============================================================
# src/inline_asm/meson.build
#
# Subprojeto: Assembly inline em C++
#
# Conceitos demonstrados (em ordem crescente de complexidade):
#   1. asm básico         — instrução única, sem I/O
#   2. asm com output     — escreve resultado em variável C++
#   3. asm com input      — lê variável C++ como operando
#   4. asm input + output — operação aritmética completa
#   5. clobbers           — avisa o compilador sobre registradores usados
#   6. volatile           — impede o compilador de otimizar/remover
#   7. múltiplas instruções — sequência com labels e saltos
#   8. CPUID              — instrução real de hardware
#   9. RDTSC              — lê o contador de ciclos da CPU
#  10. função inteira em asm separado (.s) linkada ao C++
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
