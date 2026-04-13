# ============================================================
# src/outline_asm/meson.build
#
# Subprojeto: Assembly em arquivo .s separado, linkado ao C++
#
# Conceitos demonstrados:
#   - Arquivo .s compilado pelo assembler GAS (GNU Assembler)
#   - Convenção de chamada System V AMD64 ABI:
#       argumentos: rdi, rsi, rdx, rcx, r8, r9
#       retorno:    rax (inteiro) / xmm0 (float)
#       callee-saved: rbx, rbp, r12-r15
#   - extern "C" no C++ → desabilita name mangling
#   - .globl → torna o símbolo visível para o linker
#   - Múltiplas funções no mesmo .s
#   - Mistura de tipos: int, int64, float, ponteiro
#
# Build: Meson compila asm_funcs.s com o assembler nativo,
#        gera asm_funcs.s.o e linka junto com main.cpp.o
# ============================================================

executable('outline_asm',
    [
        'main.cpp',
        'asm_funcs.s',     # GAS monta este arquivo → asm_funcs.s.o
    ],
    dependencies: [fmt_dep],
    include_directories: include_directories('.'),
    install: true,
)
