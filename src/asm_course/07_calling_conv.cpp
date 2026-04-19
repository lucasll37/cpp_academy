// ============================================================
// src/asm_course/07_calling_conv.cpp
//
// MÓDULO 7 — System V AMD64 ABI: Convenção de Chamada Completa
//
// ┌─────────────────────────────────────────────────────────┐
// │  System V AMD64 ABI — a lei que governa como C/C++      │
// │  e ASM se comunicam no Linux x86-64.                    │
// │                                                         │
// │  Argumentos inteiros/ponteiros (1º ao 6º):              │
// │    1. RDI   2. RSI   3. RDX                             │
// │    4. RCX   5. R8    6. R9                              │
// │    7+: passados na stack, direita para esquerda          │
// │                                                         │
// │  Argumentos float/double (1º ao 8º):                    │
// │    XMM0 … XMM7                                          │
// │                                                         │
// │  Retorno:                                               │
// │    inteiro/ponteiro → RAX (e RDX para 128 bits)         │
// │    float/double    → XMM0 (e XMM1 para par)             │
// │                                                         │
// │  Caller-saved (voláteis — função pode destruir):        │
// │    RAX, RCX, RDX, RSI, RDI, R8-R11, XMM0-XMM15         │
// │                                                         │
// │  Callee-saved (não-voláteis — função DEVE preservar):   │
// │    RBX, RBP, R12-R15                                    │
// │                                                         │
// │  Stack: alinhada a 16 bytes no ponto do CALL            │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <cstdio>
#include <fmt/core.h>

extern "C" {
    // Funções em 07_calling_asm.s
    int64_t asm_soma6(int64_t a, int64_t b, int64_t c,
                      int64_t d, int64_t e, int64_t f);
    int64_t asm_soma8(int64_t a, int64_t b, int64_t c, int64_t d,
                      int64_t e, int64_t f, int64_t g, int64_t h);
    double  asm_hypot(double x, double y);
    void    asm_swap(int64_t* a, int64_t* b);
    int64_t asm_apply(int64_t (*fn)(int64_t), int64_t x);
    void    asm_print_variadic(const char* fmt, ...);

    // Callback chamado pelo asm
    int64_t cpp_callback(int64_t x);
}

int64_t cpp_callback(int64_t x) {
    fmt::print("    [cpp_callback chamado com {}]\n", x);
    return x * x;
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// ── Demo 1: Os 6 argumentos em registrador ────────────────────
void demo_six_args() {
    section("7.1 Os 6 primeiros argumentos em registradores");

    int64_t r = asm_soma6(1, 2, 3, 4, 5, 6);
    fmt::print("  asm_soma6(1,2,3,4,5,6) = {} (esperado: 21)\n", r);
    fmt::print("  Passagem: rdi=1, rsi=2, rdx=3, rcx=4, r8=5, r9=6\n");
}

// ── Demo 2: Argumentos na stack (7º em diante) ───────────────
void demo_stack_args() {
    section("7.2 Argumentos extras passados pela stack");

    int64_t r = asm_soma8(1, 2, 3, 4, 5, 6, 7, 8);
    fmt::print("  asm_soma8(1..8) = {} (esperado: 36)\n", r);
    fmt::print("  args 7 e 8 foram empurrados na stack pelo caller\n");
    fmt::print("  na função: acessados via 16(%rbp) e 24(%rbp)\n");
}

// ── Demo 3: Argumentos float em XMM ──────────────────────────
void demo_float_args() {
    section("7.3 Float/Double em registradores XMM");

    // hypot(3.0, 4.0) = 5.0
    double r = asm_hypot(3.0, 4.0);
    fmt::print("  asm_hypot(3.0, 4.0) = {:.4f} (esperado: 5.0000)\n", r);
    fmt::print("  passagem: xmm0=3.0, xmm1=4.0\n");
    fmt::print("  retorno:  xmm0 = resultado\n");
}

// ── Demo 4: Passagem de ponteiros ────────────────────────────
void demo_pointer_args() {
    section("7.4 Ponteiros como argumentos (rdi, rsi)");

    int64_t a = 100, b = 200;
    fmt::print("  antes: a={}, b={}\n", a, b);
    asm_swap(&a, &b);
    fmt::print("  após asm_swap(&a, &b): a={}, b={}\n", a, b);
    fmt::print("  (ponteiros passados em rdi e rsi)\n");
}

// ── Demo 5: Passagem de função (ponteiro de função) ───────────
void demo_function_pointer() {
    section("7.5 Ponteiro de função como argumento");
    //
    // Um ponteiro de função é apenas um inteiro de 64 bits (endereço).
    // Passa em RDI como qualquer outro ponteiro.
    // A função ASM chama via: call *%rdi  (indirect call)

    fmt::print("  asm_apply(cpp_callback, 7):\n");
    int64_t r = asm_apply(cpp_callback, 7);
    fmt::print("  resultado = {} (esperado: 49 = 7²)\n", r);
}

// ── Demo 6: Convenção de retorno ──────────────────────────────
void demo_return_values() {
    section("7.6 Convenção de retorno — RAX e RDX:RAX");

    // Retorno de valor de 64 bits (normal → só RAX)
    int64_t r64;
    asm volatile (
        "movq $0xDEADBEEFCAFEBABE, %0"
        : "=a"(r64)    // "a" → rax
    );
    fmt::print("  retorno 64 bits (rax): 0x{:016X}\n", (uint64_t)r64);

    // Retorno de struct pequena → dois campos em rax:rdx
    struct Pair { int64_t lo, hi; };
    // Simula uma função que retorna pair via rax:rdx
    int64_t lo, hi;
    asm volatile (
        "movq $0x1111111111111111, %%rax\n\t"
        "movq $0x2222222222222222, %%rdx\n\t"
        "movq %%rax, %0\n\t"
        "movq %%rdx, %1"
        : "=r"(lo), "=r"(hi)
        :
        : "%rax", "%rdx"
    );
    fmt::print("  retorno 128 bits: rax=0x{:X}, rdx=0x{:X}\n",
               (uint64_t)lo, (uint64_t)hi);
    fmt::print("  (structs pequenas retornam em rax:rdx ou xmm0:xmm1)\n");
}

// ── Demo 7: Alinhamento da stack ──────────────────────────────
void demo_stack_alignment() {
    section("7.7 Alinhamento de 16 bytes da stack no CALL");
    //
    // RSP deve estar alinhado em 16 bytes NO MOMENTO DO CALL.
    // O CALL empurra 8 bytes (return address) → logo após CALL,
    // RSP está misaligned por 8. O prólogo (push rbp) corrige.
    //
    // Se você faz sub rsp, N: N deve ser múltiplo de 16.
    // (ou múltiplo de 8 se você fez push rbp antes — net = 16)

    uint64_t rsp_val;
    asm volatile ("movq %%rsp, %0" : "=r"(rsp_val));

    fmt::print("  RSP atual: 0x{:016X}\n", rsp_val);
    fmt::print("  RSP mod 16 = {}\n", rsp_val % 16);
    fmt::print("  Alinhado a 16 bytes? {}\n",
               (rsp_val % 16 == 0) ? "SIM (após push rbp)" : "NÃO");
    fmt::print("  ⚠ SSE/AVX movaps FALHAM com misalignment (segfault!)\n");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 07 · Convenção de Chamada ABI  ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_six_args();
    demo_stack_args();
    demo_float_args();
    demo_pointer_args();
    demo_function_pointer();
    demo_return_values();
    demo_stack_alignment();

    fmt::print("\n\033[1;32m✓ Módulo 07 completo\033[0m\n\n");
}
