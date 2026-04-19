// ============================================================
// src/asm_course/12_fp_x87.cpp
//
// MÓDULO 12 — FPU x87: A pilha de ponto flutuante legada
//
// ┌─────────────────────────────────────────────────────────┐
// │  x87 é a FPU original do x86, introduzida no 8087.     │
// │  Usa uma PILHA de 8 registradores de 80 bits:          │
// │    ST(0) = topo,  ST(1) = próximo, …, ST(7)             │
// │                                                         │
// │  Operações empurram/retiram da pilha:                   │
// │    FLD  → push na pilha x87                             │
// │    FST  → lê ST(0) sem retirar                          │
// │    FSTP → lê e retira ST(0) da pilha (p = pop)          │
// │                                                         │
// │  Ponto flutuante moderno usa SSE2 (XMM), mas x87:      │
// │    - Opera em 80 bits internamente (maior precisão)     │
// │    - Ainda usado em funções como sin/cos/atan2 (fsin)   │
// │    - Obrigatório em modo real/16-bit                    │
// │    - Às vezes gerado por código legado                  │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <cmath>
#include <fmt/core.h>

extern "C" {
    double asm_x87_sqrt(double x);
    double asm_x87_sin(double x);
    double asm_x87_atan2(double y, double x);
    double asm_x87_log2(double x);
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

void demo_x87_stack() {
    section("12.1 A pilha x87 — FLD, FADD, FSTP");

    double a = 3.0, b = 4.0, result;

    asm volatile (
        "fldl %1\n\t"        // push 3.0 → ST(0)=3.0
        "fldl %2\n\t"        // push 4.0 → ST(0)=4.0, ST(1)=3.0
        "faddp\n\t"          // ST(1) = ST(0)+ST(1) = 7.0, pop → ST(0)=7.0
        "fstpl %0"           // pop ST(0) para resultado
        : "=m"(result)
        : "m"(a), "m"(b)
    );
    fmt::print("  3.0 + 4.0 via x87 = {:.1f}\n", result);

    // Multiplicação
    asm volatile (
        "fldl %1\n\t"
        "fldl %2\n\t"
        "fmulp\n\t"          // ST(1) = ST(0)*ST(1), pop
        "fstpl %0"
        : "=m"(result)
        : "m"(a), "m"(b)
    );
    fmt::print("  3.0 × 4.0 via x87 = {:.1f}\n", result);
}

void demo_x87_trig() {
    section("12.2 Funções trigonométricas x87 (FSIN, FCOS, FPATAN)");

    double angle = M_PI / 6.0;  // 30 graus
    double sin_val = asm_x87_sin(angle);
    fmt::print("  sin(π/6) = {:.6f}  (esperado: 0.500000)\n", sin_val);

    double atan2_val = asm_x87_atan2(1.0, 1.0);
    fmt::print("  atan2(1,1) = {:.6f}  (esperado: π/4 = {:.6f})\n",
               atan2_val, M_PI/4);

    fmt::print("\n  Funções x87 disponíveis:\n");
    fmt::print("    FSIN  → seno\n");
    fmt::print("    FCOS  → cosseno\n");
    fmt::print("    FSINCOS → ambos simultaneamente\n");
    fmt::print("    FPTAN → tan(ST0) → ST0=tan, push 1.0\n");
    fmt::print("    FPATAN → atan(ST1/ST0), pop\n");
    fmt::print("    FYL2X → ST1 × log2(ST0), pop\n");
    fmt::print("    FSQRT → sqrt(ST0)\n");
    fmt::print("    FABS  → |ST0|\n");
    fmt::print("    FCHS  → nega ST0\n");
}

void demo_x87_precision() {
    section("12.3 Precisão 80 bits vs 64 bits");
    //
    // x87 opera internamente em 80 bits (64 bits de mantissa).
    // SSE2/double usa 64 bits (52 bits de mantissa).
    // Isso significa resultados LIGEIRAMENTE diferentes para cálculos longos.

    double x = 1.0;
    double result_x87, result_sse;

    // x87: 80 bits internos
    asm volatile (
        "fldl %1\n\t"
        "fldl %1\n\t"
        "fdivrp\n\t"         // ST(1) / ST(0) = 1.0 / 1.0
        "fstpl %0"
        : "=m"(result_x87) : "m"(x)
    );

    // SSE: 64 bits
    asm volatile (
        "movsd %1, %%xmm0\n\t"
        "divsd %1, %%xmm0\n\t"
        "movsd %%xmm0, %0"
        : "=m"(result_sse) : "m"(x) : "%xmm0"
    );

    fmt::print("  1.0/1.0 x87 = {:.20f}\n", result_x87);
    fmt::print("  1.0/1.0 SSE = {:.20f}\n", result_sse);
    fmt::print("  (podem diferir em cálculos de longa cadeia)\n");
    fmt::print("  Em código novo: prefira SSE2 (comportamento IEEE 754 estrito)\n");
}

void demo_x87_sqrt_log() {
    section("12.4 FSQRT e FYL2X — raiz e logaritmo");

    double val = 2.0;
    double sqr = asm_x87_sqrt(val);
    fmt::print("  fsqrt(2.0) = {:.10f}  (esperado: {:.10f})\n", sqr, sqrt(2.0));

    double log2_val = asm_x87_log2(val);
    fmt::print("  fyl2x(2.0) = {:.6f}  (esperado: 1.000000)\n", log2_val);
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 12 · FPU x87 Legada            ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_x87_stack();
    demo_x87_trig();
    demo_x87_precision();
    demo_x87_sqrt_log();

    fmt::print("\n\033[1;32m✓ Módulo 12 completo\033[0m\n\n");
}
