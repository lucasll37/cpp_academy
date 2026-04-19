// ============================================================
// src/asm_course/03_arithmetic.cpp
//
// MÓDULO 3 — Aritmética e Flags
//
// ┌─────────────────────────────────────────────────────────┐
// │  RFLAGS — o registrador de estado da CPU                │
// │                                                         │
// │  Bit  Nome  Significado                                 │
// │   0   CF    Carry Flag    — overflow UNSIGNED           │
// │   2   PF    Parity Flag   — paridade do byte baixo      │
// │   4   AF    Aux Carry     — carry do nibble baixo (BCD) │
// │   6   ZF    Zero Flag     — resultado == 0              │
// │   7   SF    Sign Flag     — bit mais alto == 1          │
// │   8   TF    Trap Flag     — debug single-step           │
// │   9   IF    Interrupt     — habilita interrupções       │
// │  10   DF    Direction     — direção de string ops       │
// │  11   OF    Overflow Flag — overflow SIGNED             │
// └─────────────────────────────────────────────────────────┘
//
// Instruções que afetam flags: ADD, SUB, AND, OR, XOR, CMP, TEST
// Instruções que NÃO afetam flags: MOV, LEA, PUSH, POP
// ============================================================

#include <cstdint>
#include <fmt/core.h>

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// Helper: lê RFLAGS atual
static uint64_t read_flags() {
    uint64_t f;
    asm volatile ("pushfq; popq %0" : "=r"(f));
    return f;
}

static void print_flags(uint64_t f) {
    fmt::print("    CF={} PF={} ZF={} SF={} OF={}\n",
        (f>>0)&1, (f>>2)&1, (f>>6)&1, (f>>7)&1, (f>>11)&1);
}

// ── Demo 1: ADD e seus efeitos nas flags ─────────────────────
void demo_add_flags() {
    section("3.1 ADD — flags em diferentes cenários");

    struct Caso {
        int32_t a, b;
        const char* desc;
    };
    Caso casos[] = {
        {  10,   20, "resultado positivo normal"},
        {  -1,    1, "resultado zero (ZF=1)"},
        { 100, -200, "resultado negativo (SF=1)"},
        { (int32_t)0x7FFFFFFF, 1, "overflow com sinal (OF=1)"},
        { (int32_t)0xFFFFFFFF, 1, "carry sem sinal (CF=1)"},
    };

    for (auto& c : casos) {
        int32_t result;
        uint64_t flags;
        asm volatile (
            "movl %2, %%eax\n\t"
            "addl %3, %%eax\n\t"
            "movl %%eax, %0\n\t"
            "pushfq\n\t"
            "popq %1"
            : "=r"(result), "=r"(flags)
            : "r"(c.a), "r"(c.b)
            : "%eax"
        );
        fmt::print("  {:11} + {:11} = {:12} │ {}\n",
                   c.a, c.b, result, c.desc);
        print_flags(flags);
    }
}

// ── Demo 2: SUB e CMP ─────────────────────────────────────────
void demo_sub_cmp() {
    section("3.2 SUB vs CMP — a diferença crucial");
    //
    // SUB dst, src  →  dst = dst - src  (ESCREVE resultado)
    // CMP dst, src  →  calcula dst - src (DESCARTA resultado, só afeta flags)
    //
    // CMP é fundamental para jumps condicionais (jz, jl, jg, etc.)

    int a = 10, b = 10;
    uint64_t flags_sub, flags_cmp;
    int sub_result;

    asm volatile (
        "movl %2, %%eax\n\t"
        "subl %3, %%eax\n\t"    // eax = a - b
        "movl %%eax, %0\n\t"
        "pushfq; popq %1"
        : "=r"(sub_result), "=r"(flags_sub)
        : "r"(a), "r"(b)
        : "%eax"
    );

    asm volatile (
        "movl %1, %%eax\n\t"
        "cmpl %2, %%eax\n\t"    // flags = a - b, descarta resultado
        "pushfq; popq %0"
        : "=r"(flags_cmp)
        : "r"(a), "r"(b)
        : "%eax"
    );

    fmt::print("  SUB {}-{}: resultado={}, ZF={}\n",
               a, b, sub_result, (flags_sub>>6)&1);
    fmt::print("  CMP {},{}: sem resultado,  ZF={}  (flags idênticas!)\n",
               a, b, (flags_cmp>>6)&1);
    fmt::print("  → ZF=1 significa a == b\n");
}

// ── Demo 3: MUL vs IMUL ───────────────────────────────────────
void demo_mul() {
    section("3.3 MUL (unsigned) vs IMUL (signed)");
    //
    // MUL src   — multiplica rAX * src → resultado 128 bits em rDX:rAX
    //   (parte alta em rdx, parte baixa em rax)
    //
    // IMUL tem 3 formas:
    //   imul src           → rdx:rax = rax * src  (como MUL mas com sinal)
    //   imul dst, src      → dst = dst * src       (64 bits, truncado)
    //   imul dst, src, imm → dst = src * imm       (3 operandos)

    uint32_t a = 0xFFFFFFFF;  // 4294967295
    uint32_t b = 2;
    uint32_t hi, lo;

    asm volatile (
        "movl %2, %%eax\n\t"
        "mull %3\n\t"           // edx:eax = eax * b (unsigned)
        "movl %%eax, %0\n\t"
        "movl %%edx, %1"
        : "=r"(lo), "=r"(hi)
        : "r"(a), "r"(b)
        : "%eax", "%edx"
    );
    uint64_t resultado = ((uint64_t)hi << 32) | lo;
    fmt::print("  MUL (unsigned): 0x{:X} * {} = 0x{:X}\n",
               a, b, resultado);
    fmt::print("    edx (alto) = 0x{:08X}, eax (baixo) = 0x{:08X}\n", hi, lo);

    // IMUL com 2 operandos (comum, truncado a 64 bits)
    int64_t x = -5, y = 6, prod;
    asm volatile (
        "imulq %2, %0"          // dest *= src
        : "=r"(prod)
        : "0"(x), "r"(y)
    );
    fmt::print("  IMUL (signed): {} * {} = {}\n", x, y, prod);

    // IMUL com 3 operandos (imediato)
    int64_t base = 7, escala;
    asm volatile (
        "imulq $9, %1, %0"      // dest = src * 9
        : "=r"(escala)
        : "r"(base)
    );
    fmt::print("  IMUL 3-op: {} * 9 = {}\n", base, escala);
}

// ── Demo 4: DIV e IDIV ────────────────────────────────────────
void demo_div() {
    section("3.4 DIV / IDIV — divisão e resto");
    //
    // DIV src   — divide rDX:rAX por src
    //   quociente → rAX,  resto → rDX
    //
    // ANTES de DIV, zere rDX se dividendo cabe em rAX:
    //   xorl %edx, %edx   (para divisão 32-bit)
    //   xorq %rdx, %rdx   (para divisão 64-bit)
    //
    // IDIV é igual mas com sinal. Use CDQ/CQO para sign-extend rAX → rDX:rAX

    uint32_t dividendo = 100, divisor = 7;
    uint32_t quociente, resto;

    asm volatile (
        "movl %2, %%eax\n\t"
        "xorl %%edx, %%edx\n\t"  // zera edx (parte alta do dividendo)
        "divl %3\n\t"             // eax = 100/7 = 14, edx = 100%7 = 2
        "movl %%eax, %0\n\t"
        "movl %%edx, %1"
        : "=r"(quociente), "=r"(resto)
        : "r"(dividendo), "r"(divisor)
        : "%eax", "%edx"
    );
    fmt::print("  {} / {} = {} (quociente), {} (resto)\n",
               dividendo, divisor, quociente, resto);

    // IDIV com sinal — usa CDQ para sign-extend
    int32_t sdiv = -17, sdivisor = 5;
    int32_t sq, sr;
    asm volatile (
        "movl %2, %%eax\n\t"
        "cdq\n\t"                 // sign-extend eax → edx:eax
        "idivl %3\n\t"
        "movl %%eax, %0\n\t"
        "movl %%edx, %1"
        : "=r"(sq), "=r"(sr)
        : "r"(sdiv), "r"(sdivisor)
        : "%eax", "%edx"
    );
    fmt::print("  {} / {} = {} (quociente), {} (resto)\n",
               sdiv, sdivisor, sq, sr);
    fmt::print("  (em C++: {}/{} = {}, {}%{} = {})\n",
               sdiv, sdivisor, sdiv/sdivisor, sdiv, sdivisor, sdiv%sdivisor);
}

// ── Demo 5: INC, DEC, NEG, NOT ───────────────────────────────
void demo_unary() {
    section("3.5 Operadores unários: INC, DEC, NEG, NOT");
    //
    // INC dst  → dst++  (NÃO afeta CF! Diferença de ADD $1)
    // DEC dst  → dst--  (NÃO afeta CF!)
    // NEG dst  → dst = 0 - dst  (negação em complemento 2)
    // NOT dst  → dst = ~dst     (inversão bit a bit)

    int32_t v = 5, resultado;

    asm volatile ("movl %1,%0; incl %0" : "=r"(resultado) : "r"(v));
    fmt::print("  INC {}: {}\n", v, resultado);

    asm volatile ("movl %1,%0; decl %0" : "=r"(resultado) : "r"(v));
    fmt::print("  DEC {}: {}\n", v, resultado);

    asm volatile ("movl %1,%0; negl %0" : "=r"(resultado) : "r"(v));
    fmt::print("  NEG {}: {}\n", v, resultado);

    uint32_t u = 0x0F0F0F0F, ur;
    asm volatile ("movl %1,%0; notl %0" : "=r"(ur) : "r"(u));
    fmt::print("  NOT 0x{:08X}: 0x{:08X}\n", u, ur);

    // ATENÇÃO: INC/DEC não alteram CF — isso pode causar bugs sutis
    // em loops que verificam CF após incrementar um contador:
    fmt::print("  ⚠ INC/DEC não alteram CF — use ADD/SUB se precisar de CF\n");
}

// ── Demo 6: ADC/SBB — aritmética de precisão múltipla ────────
void demo_adc_sbb() {
    section("3.6 ADC/SBB — aritmética 128-bit via CF");
    //
    // ADC (Add with Carry): dst = dst + src + CF
    // SBB (Sub with Borrow): dst = dst - src - CF
    //
    // Permitem soma/subtração de números maiores que 64 bits,
    // encadeando o carry entre partes.

    // Soma de dois números de 128 bits:
    //   A = {a_hi, a_lo},  B = {b_hi, b_lo}
    uint64_t a_lo = 0xFFFFFFFFFFFFFFFF;  // max uint64
    uint64_t a_hi = 0;
    uint64_t b_lo = 1;
    uint64_t b_hi = 0;
    uint64_t r_lo, r_hi;

    asm volatile (
        "addq %4, %2\n\t"    // r_lo = a_lo + b_lo (gera CF se overflow)
        "adcq %5, %3"        // r_hi = a_hi + b_hi + CF (propaga carry!)
        : "=r"(r_lo), "=r"(r_hi)
        : "0"(a_lo), "1"(a_hi), "r"(b_lo), "r"(b_hi)
    );

    fmt::print("  A = 0x{:016X}:{:016X} (= UINT64_MAX)\n", a_hi, a_lo);
    fmt::print("  B = 0x{:016X}:{:016X} (= 1)\n",          b_hi, b_lo);
    fmt::print("  A+B = 0x{:016X}:{:016X}\n", r_hi, r_lo);
    fmt::print("  → carry propagado: r_hi=1, r_lo=0  ✓\n");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 03 · Aritmética e Flags        ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_add_flags();
    demo_sub_cmp();
    demo_mul();
    demo_div();
    demo_unary();
    demo_adc_sbb();

    fmt::print("\n\033[1;32m✓ Módulo 03 completo\033[0m\n\n");
}
