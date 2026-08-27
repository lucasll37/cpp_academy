# ============================================================
# src/asm_course/meson.build
#
# Subprojeto: Curso Completo de Assembly x86-64 com C++
#
# Módulos (cada um é um executável independente):
#
#   01_registers      — Registradores, tamanhos, aliases
#   02_memory         — Modos de endereçamento, LEA, ponteiros
#   03_arithmetic     — ADD, SUB, MUL, DIV, flags, overflow
#   04_bitwise        — AND, OR, XOR, NOT, shifts, rotações
#   05_control_flow   — CMP, JMP condicional, CMOV, SETcc
#   06_stack_frames   — PUSH/POP, CALL/RET, prólogo/epílogo
#   07_calling_conv   — System V AMD64 ABI completo, variádico
#   08_simd_sse       — XMM, operações float/double vetorizadas
#   09_simd_avx       — YMM, operações 256-bit
#   10_string_ops     — REP MOVS/STOS/SCAS/CMPS
#   11_syscalls       — SYSCALL direto (sem libc)
#   12_fp_x87         — FPU x87 (legado mas ainda presente)
#   13_optimization   — Branchless, pipeline, prefetch, unroll
#   14_disassembly    — Como ler o output de objdump/gdb
#   15_capstone       — Projeto final: memcpy/strlen/sort em ASM
#
# Flags:
#   -msse4.2  → habilita SSE4.2 (POPCNT, etc.)
#   -mavx2    → habilita AVX2  (YMM 256-bit)
#   -mfma     → habilita FMA   (fused multiply-add)
# ============================================================

asm_course_flags = [
    # Habilitam os conjuntos de instruções que os módulos 8, 9 e 4 usam.
    # ⚠ Sem eles, os intrínsecos de AVX2/FMA nem compilam — e o binário
    #   resultante NÃO roda em processador que não os tenha. É a mesma troca
    #   de `-march=native`, discutida em src/performance/demo_metodo.hpp.
    '-msse4.2',
    '-mavx2',
    '-mfma',

    # ⚠ `-O2` é DELIBERADO, e tem uma consequência que o módulo 13 precisa
    #   levar em conta.
    #
    #   Ele está aqui porque um curso de assembly deve mostrar o código que o
    #   compilador REALMENTE gera para produção. Ler o assembly de um `-O0` é
    #   ler um código que ninguém entrega: cada variável vai para a pilha,
    #   nada é mantido em registrador, e o módulo 14 (lendo o assembly do
    #   compilador) ficaria sem sentido.
    #
    #   O efeito colateral: no módulo 13, os demos de "branchless vs branchy"
    #   tendem a dar TEMPOS IGUAIS — porque com `-O2` o GCC já converte o
    #   `if` em `cmov` por conta própria. Isso NÃO é um defeito do demo: é o
    #   resultado, e é o mesmo que src/performance/demo_branch.hpp mediu e
    #   documentou (o experimento famoso do "vetor ordenado" não reproduz
    #   mais a partir de `-O1`).
    #
    #   Para VER a diferença, recompile o módulo 13 à mão sem otimização:
    #     g++ -std=c++20 -O0 -msse4.2 -mavx2 -mfma \
    #         src/asm_course/13_optimization.cpp -o /tmp/opt0 -lfmt
    '-O2',
]

# Helper para criar cada módulo do curso
foreach module : [
    ['01_registers',    ['01_registers.cpp']],
    ['02_memory',       ['02_memory.cpp']],
    ['03_arithmetic',   ['03_arithmetic.cpp']],
    ['04_bitwise',      ['04_bitwise.cpp']],
    ['05_control_flow', ['05_control_flow.cpp']],
    ['06_stack_frames', ['06_stack_frames.cpp', '06_stack_asm.s']],
    ['07_calling_conv', ['07_calling_conv.cpp', '07_calling_asm.s']],
    ['08_simd_sse',     ['08_simd_sse.cpp',     '08_simd_asm.s']],
    ['09_simd_avx',     ['09_simd_avx.cpp',     '09_avx_asm.s']],
    ['10_string_ops',   ['10_string_ops.cpp',   '10_string_asm.s']],
    ['11_syscalls',     ['11_syscalls.cpp',      '11_syscall_asm.s']],
    ['12_fp_x87',       ['12_fp_x87.cpp',        '12_x87_asm.s']],
    ['13_optimization', ['13_optimization.cpp', '13_opt_asm.s']],
    ['14_disassembly',  ['14_disassembly.cpp']],
    ['15_capstone',     ['15_capstone.cpp',      '15_capstone_asm.s']],
]
    executable(
        'asm_' + module[0],
        module[1],
        dependencies: [fmt_dep],
        cpp_args: asm_course_flags,
        install: true,
    )
endforeach
