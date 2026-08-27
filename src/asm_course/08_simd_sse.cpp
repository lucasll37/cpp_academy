// ============================================================
// src/asm_course/08_simd_sse.cpp
//
// MÓDULO 8 — SIMD com SSE/SSE2/SSE4: Registradores XMM
//
// ┌─────────────────────────────────────────────────────────┐
// │  SIMD = Single Instruction, Multiple Data               │
// │                                                         │
// │  Registradores XMM: 128 bits cada (XMM0 … XMM15)       │
// │                                                         │
// │  Um XMM pode conter:                                    │
// │    4 × float   (32 bits cada)  → "packed single" (ps)  │
// │    2 × double  (64 bits cada)  → "packed double" (pd)  │
// │    16 × int8_t                 → "packed byte"  (b)    │
// │    8 × int16_t                 → "packed word"  (w)    │
// │    4 × int32_t                 → "packed dword" (d)    │
// │    2 × int64_t                 → "packed qword" (q)    │
// │    1 × float   (baixo)         → "scalar single" (ss)  │
// │    1 × double  (baixo)         → "scalar double" (sd)  │
// │                                                         │
// │  Sufixos: ps=packed-single  pd=packed-double            │
// │           ss=scalar-single  sd=scalar-double            │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <cmath>
#include <immintrin.h>   // intrínsecas SSE/AVX
#include <fmt/core.h>

extern "C" {
    void asm_add_4floats(const float* a, const float* b, float* out);
    void asm_dot4(const float* a, const float* b, float* out);
    void asm_scale_array(float* arr, float scalar, int n);
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// ── Demo 1: Carregamento em XMM ───────────────────────────────
void demo_load_xmm() {
    section("8.1 Carregar dados em XMM — MOVAPS, MOVUPS, MOVSS");

    alignas(16) float arr[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float out[4];

    // MOVAPS: move aligned packed single (exige alinhamento 16B)
    asm volatile (
        "movaps (%1), %%xmm0\n\t"    // carrega 4 floats de arr
        "movaps %%xmm0, (%0)"        // salva no out
        :
        : "r"(out), "r"(arr)
        : "%xmm0", "memory"
    );
    fmt::print("  MOVAPS carregou: [{:.0f}, {:.0f}, {:.0f}, {:.0f}]\n",
               out[0], out[1], out[2], out[3]);

    // MOVSS: move scalar single (só o float baixo)
    float single;
    asm volatile (
        "movss (%1), %%xmm0\n\t"     // carrega apenas 1 float
        "movss %%xmm0, %0"
        : "=m"(single)
        : "r"(arr)
        : "%xmm0"
    );
    fmt::print("  MOVSS carregou:  {:.0f}  (apenas elemento 0)\n", single);

    // SHUFPS: embaralha elementos (shuffle)
    asm volatile (
        "movaps (%1), %%xmm0\n\t"
        "shufps $0b00011011, %%xmm0, %%xmm0\n\t"  // inverte ordem: 3,2,1,0
        "movaps %%xmm0, (%0)"
        :
        : "r"(out), "r"(arr)
        : "%xmm0", "memory"
    );
    fmt::print("  SHUFPS revertido:[{:.0f}, {:.0f}, {:.0f}, {:.0f}]\n",
               out[0], out[1], out[2], out[3]);
}

// ── Demo 2: Aritmética packed ─────────────────────────────────
void demo_packed_arithmetic() {
    section("8.2 Aritmética Packed — 4 operações em 1 instrução");

    alignas(16) float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    alignas(16) float b[4] = {10.f, 20.f, 30.f, 40.f};
    alignas(16) float out[4];

    asm_add_4floats(a, b, out);
    fmt::print("  ADDPS [1,2,3,4] + [10,20,30,40] = [{:.0f},{:.0f},{:.0f},{:.0f}]\n",
               out[0], out[1], out[2], out[3]);

    // Multiplicação packed
    asm volatile (
        "movaps (%1), %%xmm0\n\t"
        "movaps (%2), %%xmm1\n\t"
        "mulps  %%xmm1, %%xmm0\n\t"   // 4 multiplicações em paralelo!
        "movaps %%xmm0, (%0)"
        :
        : "r"(out), "r"(a), "r"(b)
        : "%xmm0", "%xmm1", "memory"
    );
    fmt::print("  MULPS [1,2,3,4] × [10,20,30,40] = [{:.0f},{:.0f},{:.0f},{:.0f}]\n",
               out[0], out[1], out[2], out[3]);

    // Divisão e raiz quadrada packed
    alignas(16) float c[4] = {4.f, 9.f, 16.f, 25.f};
    asm volatile (
        "movaps (%1), %%xmm0\n\t"
        "sqrtps %%xmm0, %%xmm0\n\t"   // sqrt de todos os 4 elementos
        "movaps %%xmm0, (%0)"
        :
        : "r"(out), "r"(c)
        : "%xmm0", "memory"
    );
    fmt::print("  SQRTPS √[4,9,16,25] = [{:.0f},{:.0f},{:.0f},{:.0f}]\n",
               out[0], out[1], out[2], out[3]);
}

// ── Demo 3: Produto interno (dot product) ─────────────────────
void demo_dot_product() {
    section("8.3 Produto interno 4D via SSE");

    alignas(16) float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    alignas(16) float b[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    float result;

    // a·b = 1*5 + 2*6 + 3*7 + 4*8 = 5+12+21+32 = 70
    asm_dot4(a, b, &result);

    fmt::print("  a = [1,2,3,4], b = [5,6,7,8]\n");
    fmt::print("  a·b = 1×5 + 2×6 + 3×7 + 4×8 = {:.0f}  (esperado: 70)\n",
               result);

    // Usando intrínseca para comparar:
    __m128 va = _mm_load_ps(a);
    __m128 vb = _mm_load_ps(b);
    __m128 vd = _mm_dp_ps(va, vb, 0xFF);  // DPPS — dot product SSE4.1
    fmt::print("  DPPS (intrínseca): {:.0f}\n", _mm_cvtss_f32(vd));
}

// ── Demo 4: Escalar array (scalar × vetor) ───────────────────
void demo_scale_array() {
    section("8.4 Multiplicar array por escalar — SIMD loop");

    float arr[8] = {1,2,3,4,5,6,7,8};
    asm_scale_array(arr, 3.14f, 8);

    fmt::print("  arr × 3.14:\n  [");
    for (int i = 0; i < 8; ++i)
        fmt::print("{:.2f}{}", arr[i], i<7 ? ", " : "");
    fmt::print("]\n");
}

// ── Demo 5: Comparações vetoriais ────────────────────────────
void demo_vector_compare() {
    section("8.5 Comparações vetoriais — máscara de bits");

    alignas(16) float a[4] = {1.0f, 5.0f, 3.0f, 7.0f};
    alignas(16) float b[4] = {2.0f, 4.0f, 3.0f, 6.0f};
    alignas(16) uint32_t mask[4];

    asm volatile (
        "movaps (%1), %%xmm0\n\t"
        "movaps (%2), %%xmm1\n\t"
        "cmpltps %%xmm1, %%xmm0\n\t"   // compara: a[i] < b[i]?
                                         // resultado: 0xFFFFFFFF se true, 0 se false
        "movaps %%xmm0, (%0)"
        :
        : "r"(mask), "r"(a), "r"(b)
        : "%xmm0", "%xmm1", "memory"
    );

    fmt::print("  a = [1, 5, 3, 7]\n");
    fmt::print("  b = [2, 4, 3, 6]\n");
    fmt::print("  a < b?  [");
    for (int i = 0; i < 4; ++i)
        fmt::print("{}{}", mask[i] ? "T" : "F", i<3?",":"");
    fmt::print("]  (máscaras: 0x{:X} 0x{:X} 0x{:X} 0x{:X})\n",
               mask[0], mask[1], mask[2], mask[3]);

    // MAXPS / MINPS — máximo/mínimo vetorial
    alignas(16) float mx[4], mn[4];
    asm volatile (
        "movaps (%2), %%xmm0\n\t"
        "movaps (%3), %%xmm1\n\t"
        "movaps %%xmm0, %%xmm2\n\t"
        "maxps  %%xmm1, %%xmm0\n\t"   // elementwise max
        "minps  %%xmm1, %%xmm2\n\t"   // elementwise min
        "movaps %%xmm0, (%0)\n\t"
        "movaps %%xmm2, (%1)"
        :
        : "r"(mx), "r"(mn), "r"(a), "r"(b)
        : "%xmm0","%xmm1","%xmm2","memory"
    );
    fmt::print("  max(a,b) = [{:.0f},{:.0f},{:.0f},{:.0f}]\n",
               mx[0],mx[1],mx[2],mx[3]);
    fmt::print("  min(a,b) = [{:.0f},{:.0f},{:.0f},{:.0f}]\n",
               mn[0],mn[1],mn[2],mn[3]);
}

// ── Demo 6: Inteiros em XMM ───────────────────────────────────
void demo_integer_simd() {
    section("8.6 Inteiros em XMM — SSE2 packed integers");

    alignas(16) int32_t a[4] = {10, 20, 30, 40};
    alignas(16) int32_t b[4] = {1,  2,  3,  4};
    alignas(16) int32_t out[4];

    asm volatile (
        "movdqa (%1), %%xmm0\n\t"     // load aligned (int)
        "movdqa (%2), %%xmm1\n\t"
        "paddd  %%xmm1, %%xmm0\n\t"   // packed add dword
        "movdqa %%xmm0, (%0)"
        :
        : "r"(out), "r"(a), "r"(b)
        : "%xmm0", "%xmm1", "memory"
    );
    fmt::print("  PADDD [10,20,30,40]+[1,2,3,4] = [{},{},{},{}]\n",
               out[0],out[1],out[2],out[3]);

    asm volatile (
        "movdqa (%1), %%xmm0\n\t"
        "movdqa (%2), %%xmm1\n\t"
        "pmulld %%xmm1, %%xmm0\n\t"   // packed mul dword (SSE4.1)
        "movdqa %%xmm0, (%0)"
        :
        : "r"(out), "r"(a), "r"(b)
        : "%xmm0", "%xmm1", "memory"
    );
    fmt::print("  PMULLD [10,20,30,40]×[1,2,3,4] = [{},{},{},{}]\n",
               out[0],out[1],out[2],out[3]);
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 08 · SIMD SSE/SSE2/SSE4        ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_load_xmm();
    demo_packed_arithmetic();
    demo_dot_product();
    demo_scale_array();
    demo_vector_compare();
    demo_integer_simd();

    fmt::print("\n\033[1;32m✓ Módulo 08 completo\033[0m\n\n");
}
