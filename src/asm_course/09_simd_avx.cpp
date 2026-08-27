// ============================================================
// src/asm_course/09_simd_avx.cpp
//
// MÓDULO 9 — AVX/AVX2: Registradores YMM (256 bits)
//
// ┌─────────────────────────────────────────────────────────┐
// │  AVX: Advanced Vector Extensions (Intel Sandy Bridge+)  │
// │  AVX2: adiciona inteiros de 256 bits (Haswell+)         │
// │                                                         │
// │  Registradores YMM: 256 bits (YMM0 … YMM15)            │
// │  YMM é a extensão de XMM: YMM0[127:0] == XMM0          │
// │                                                         │
// │  Um YMM pode conter:                                    │
// │    8 × float   (32 bits)  → o dobro do XMM!            │
// │    4 × double  (64 bits)                                │
// │    32 × int8   (AVX2)                                   │
// │    16 × int16  (AVX2)                                   │
// │    8 × int32   (AVX2)                                   │
// │    4 × int64   (AVX2)                                   │
// │                                                         │
// │  IMPORTANTE: misturar SSE e AVX sem VZEROUPPER          │
// │  causa stalls severos (até 70 ciclos no Sandy Bridge)!  │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <immintrin.h>
#include <fmt/core.h>

extern "C" {
    void asm_avx_add8f(const float* a, const float* b, float* out);
    void asm_avx_scale8f(float* arr, float scalar, int n);
    void asm_avx_fma(const float* a, const float* b, const float* c, float* out, int n);
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

void demo_ymm_basics() {
    section("9.1 Registradores YMM — 8 floats em paralelo");

    alignas(32) float a[8] = {1,2,3,4,5,6,7,8};
    alignas(32) float b[8] = {10,20,30,40,50,60,70,80};
    alignas(32) float out[8];

    asm_avx_add8f(a, b, out);

    fmt::print("  VADDPS (8 floats):\n  [");
    for (int i = 0; i < 8; ++i)
        fmt::print("{:.0f}{}", out[i], i<7?",":"");
    fmt::print("]\n");

    // VMOVAPS com YMM
    alignas(32) float loaded[8];
    asm volatile (
        "vmovaps (%1), %%ymm0\n\t"   // carrega 8 floats alinhados
        "vmovaps %%ymm0, (%0)\n\t"
        "vzeroupper"                  // limpa bits altos YMM → evita stall SSE
        :
        : "r"(loaded), "r"(a)
        : "%ymm0", "memory"
    );
    fmt::print("  VMOVAPS carregou: [");
    for (int i = 0; i < 8; ++i)
        fmt::print("{:.0f}{}", loaded[i], i<7?",":"");
    fmt::print("]\n");
}

void demo_fma() {
    section("9.2 FMA — Fused Multiply-Add (a*b + c em uma instrução)");
    //
    // FMA3: a = a*b + c   sem arredondamento intermediário
    // Útil em: dot products, filtros, redes neurais, matrizes
    //
    // VFMADD231PS ymm0, ymm1, ymm2  →  ymm0 = ymm1*ymm2 + ymm0

    alignas(32) float a[8] = {1,2,3,4,5,6,7,8};
    alignas(32) float b[8] = {2,2,2,2,2,2,2,2};   // multiplicador
    alignas(32) float c[8] = {10,10,10,10,10,10,10,10};  // bias
    alignas(32) float out[8];

    asm_avx_fma(a, b, c, out, 8);

    fmt::print("  FMA: a[i]*2 + 10:\n  [");
    for (int i = 0; i < 8; ++i)
        fmt::print("{:.0f}{}", out[i], i<7?",":"");
    fmt::print("]\n");
    fmt::print("  Esperado: [12,14,16,18,20,22,24,26]\n");
    fmt::print("  FMA é mais preciso que MUL+ADD separados (1 arredondamento vs 2)\n");
}

void demo_avx_integers() {
    section("9.3 AVX2 — inteiros de 256 bits");

    alignas(32) int32_t a[8] = {1,2,3,4,5,6,7,8};
    alignas(32) int32_t b[8] = {10,20,30,40,50,60,70,80};
    alignas(32) int32_t out[8];

    // VPADDD: 8 somas de int32 em paralelo
    asm volatile (
        "vmovdqa (%1), %%ymm0\n\t"
        "vmovdqa (%2), %%ymm1\n\t"
        "vpaddd  %%ymm1, %%ymm0, %%ymm0\n\t"
        "vmovdqa %%ymm0, (%0)\n\t"
        "vzeroupper"
        :
        : "r"(out), "r"(a), "r"(b)
        : "%ymm0", "%ymm1", "memory"
    );
    fmt::print("  VPADDD (8×int32):\n  [");
    for (int i = 0; i < 8; ++i)
        fmt::print("{}{}", out[i], i<7?",":"");
    fmt::print("]\n");
}

void demo_vzeroupper() {
    section("9.4 VZEROUPPER — por que é obrigatório");
    //
    // Problema: CPUs que suportam AVX têm duas implementações de XMM:
    //   - Legacy SSE: usa apenas os 128 bits baixos, bits altos UNDEFINED
    //   - VEX-encoded (AVX): limpa os bits altos automaticamente
    //
    // Se você usa instrução AVX (VMOVAPS etc.) e depois chama código
    // que usa SSE legada (MOVAPS sem V), a CPU detecta a transição e
    // gasta 70-100 ciclos salvando/restaurando o estado dos YMM.
    //
    // Solução: VZEROUPPER antes de transitar para SSE/C++
    //   → zera bits 255:128 de todos os YMM
    //   → sinaliza à CPU que a transição é segura

    asm volatile ("vzeroupper" ::: "memory");
    fmt::print("  VZEROUPPER executado\n");
    fmt::print("  Regra: SEMPRE emita VZEROUPPER antes de:\n");
    fmt::print("    - Retornar de função AVX para caller SSE\n");
    fmt::print("    - Chamar função da libc (que usa SSE)\n");
    fmt::print("    - Qualquer boundary AVX→SSE\n");
    fmt::print("  O compilador faz isso com -mavx -O2 automaticamente\n");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 09 · AVX/AVX2 (YMM 256-bit)   ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_ymm_basics();
    demo_fma();
    demo_avx_integers();
    demo_vzeroupper();

    fmt::print("\n\033[1;32m✓ Módulo 09 completo\033[0m\n\n");
}
