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
    '-msse4.2',
    '-mavx2',
    '-mfma',
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
