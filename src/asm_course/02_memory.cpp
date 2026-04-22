// ============================================================
// src/asm_course/02_memory.cpp
//
// MÓDULO 2 — Modos de Endereçamento e Acesso à Memória
//
// ┌─────────────────────────────────────────────────────────┐
// │  Sintaxe geral de endereçamento (AT&T):                 │
// │                                                         │
// │    disp(base, índice, escala)                           │
// │                                                         │
// │    Endereço efetivo = base + índice×escala + disp       │
// │                                                         │
// │    disp   → deslocamento constante (imediato)           │
// │    base   → registrador base                            │
// │    índice → registrador índice (≠ rsp)                  │
// │    escala → 1, 2, 4 ou 8                                │
// └─────────────────────────────────────────────────────────┘
//
// Exemplos de endereçamento:
//   (%rax)              → [rax]
//   8(%rax)             → [rax + 8]
//   (%rax, %rbx)        → [rax + rbx]
//   (%rax, %rbx, 4)     → [rax + rbx*4]
//   8(%rax, %rbx, 4)    → [rax + rbx*4 + 8]
//   -4(%rbp)            → [rbp - 4]  ← variáveis locais!
// ============================================================

#include <cstdint>
#include <array>
#include <fmt/core.h>

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// ── Demo 1: Endereçamento direto e indireto ───────────────────
void demo_direto_indireto() {
    section("2.1 Direto vs Indireto");

    int valor = 42;
    int resultado;

    // Endereçamento DIRETO: o operando é um valor imediato
    asm volatile (
        "movl $99, %0"       // $99 = imediato (valor literal)
        : "=r"(resultado)
    );
    fmt::print("  Direto:   movl $99    → {}\n", resultado);

    // Endereçamento INDIRETO via registrador: (%reg)
    // %0 já é tratado como memória pelo compilador quando usamos "=m"
    // Vamos fazer manualmente:
    int* ptr = &valor;
    asm volatile (
        "movl (%1), %0"      // (%1) = conteúdo do endereço em %1
        : "=r"(resultado)
        : "r"(ptr)
    );
    fmt::print("  Indireto: movl (%ptr) → {}   (ptr → valor=42)\n", resultado);
}

// ── Demo 2: Deslocamento (displacement) ──────────────────────
void demo_displacement() {
    section("2.2 Deslocamento: disp(%reg)");

    // Simula acesso a campos de struct via deslocamento
    struct Ponto { int x; int y; int z; };
    Ponto p = {10, 20, 30};

    int rx, ry, rz;
    asm volatile (
        "movl   (%3), %0\n\t"    // offset 0: x
        "movl  4(%3), %1\n\t"    // offset 4: y
        "movl  8(%3), %2"        // offset 8: z
        : "=r"(rx), "=r"(ry), "=r"(rz)   // outputs: %0, %1, %2
        : "r"(&p)                          // input:   %3
    );
    // Nota: a sintaxe "1"(&p) significa "use o mesmo reg do operando 1"
    // Vamos fazer corretamente:
    const Ponto* pp = &p;
    asm volatile (
        "movl  0(%1), %0\n\t"
        "movl  4(%1), %2\n\t"
        "movl  8(%1), %3"
        : "=r"(rx), "+r"(pp), "=r"(ry), "=r"(rz)
        :
    );
    fmt::print("  struct Ponto {{ int x={}, y={}, z={} }}\n", p.x, p.y, p.z);
    fmt::print("  0(%ptr) → x = {}\n", rx);
    fmt::print("  4(%ptr) → y = {}\n", ry);
    fmt::print("  8(%ptr) → z = {}\n", rz);
    fmt::print("  ↑ Isso é exatamente o que C++ faz para p.x, p.y, p.z\n");
}

// ── Demo 3: Indexação de array ────────────────────────────────
void demo_array_index() {
    section("2.3 Indexação: (%base, %idx, escala)");

    int arr[5] = {100, 200, 300, 400, 500};
    int resultado;

    // Acessa arr[3] = base + índice * sizeof(int) = base + 3*4
    int64_t idx = 3;
    asm volatile (
        "movl (%1, %2, 4), %0"    // base + idx*4  (4 = sizeof(int))
        : "=r"(resultado)
        : "r"(arr), "r"(idx)
    );
    fmt::print("  arr = [100, 200, 300, 400, 500]\n");
    fmt::print("  (%arr, 3, 4) → arr[3] = {}  (esperado: 400)\n", resultado);

    // Loop: soma todos os elementos via indexação
    int64_t soma = 0;
    int     n    = 5;
    asm volatile (
        "xorl %%ecx, %%ecx\n\t"        // ecx = i = 0
        "xorq %0, %0\n\t"              // soma = 0
        ".loop_array%=:\n\t"
        "  movslq (%2, %%rcx, 4), %%rax\n\t"  // rax = arr[i] (sign-ext)
        "  addq %%rax, %0\n\t"         // soma += arr[i]
        "  incl %%ecx\n\t"             // i++
        "  cmpl %3, %%ecx\n\t"         // i == n?
        "  jl .loop_array%=\n\t"       // não → repete
        : "+r"(soma)
        : "0"(soma), "r"(arr), "r"(n)
        : "%rax", "%rcx", "cc"
    );
    fmt::print("  soma de todos os elementos = {}  (esperado: 1500)\n", soma);
}

// ── Demo 4: LEA — Load Effective Address ─────────────────────
void demo_lea() {
    section("2.4 LEA — calcula endereço SEM acessar memória");
    //
    // LEA (Load Effective Address) é uma das instruções mais versáteis
    // do x86-64. Ela CALCULA o endereço usando a unidade de endereçamento
    // da CPU — mas NÃO acessa a memória.
    //
    // Usos:
    //   1. Ponteiro aritmético
    //   2. Multiplicação por 2, 3, 4, 5, 8, 9 em UMA instrução
    //   3. Adição de múltiplos registradores + constante (sem flags!)

    int64_t x = 7, y = 3;
    int64_t res;

    // LEA como multiplicação rápida: x*4
    asm volatile (
        "leaq (,%1,4), %0"    // endereço = 0 + x*4
        : "=r"(res)
        : "r"(x)
    );
    fmt::print("  leaq (,x,4) → x*4 = {}  (x={})\n", res, x);

    // LEA como x*5 = x*4 + x
    asm volatile (
        "leaq (%1,%1,4), %0"  // endereço = x + x*4
        : "=r"(res)
        : "r"(x)
    );
    fmt::print("  leaq (x,x,4) → x*5 = {}  (x={})\n", res, x);

    // LEA como x*9
    asm volatile (
        "leaq (%1,%1,8), %0"  // endereço = x + x*8
        : "=r"(res)
        : "r"(x)
    );
    fmt::print("  leaq (x,x,8) → x*9 = {}\n", res);

    // LEA para adição de 3 valores + constante SEM tocar RFLAGS
    asm volatile (
        "leaq 10(%1,%2), %0"  // endereço = x + y + 10
        : "=r"(res)
        : "r"(x), "r"(y)
    );
    fmt::print("  leaq 10(x,y) → x+y+10 = {}  (x={}, y={})\n", res, x, y);
    fmt::print("  ↑ LEA não altera RFLAGS — vantagem sobre ADD/IMUL!\n");
}

// ── Demo 5: Modos RIP-relative (posição independente) ────────
void demo_rip_relative() {
    section("2.5 RIP-relative addressing — código PIC");
    //
    // Em código de 64 bits, acessos a variáveis globais/estáticas usam
    // endereçamento relativo ao RIP (instrução atual).
    //
    //   movq  var(%rip), %rax
    //
    // O endereço efetivo = RIP_depois_desta_instrucao + offset
    // Isso permite Position-Independent Code (PIC) — executável pode
    // ser carregado em qualquer endereço (ASLR).

    static int global_var = 0xBEEF;
    int lido;
    asm volatile (
        "movl %1, %0"          // o compilador já emite RIP-relative para o acesso
        : "=r"(lido)
        : "m"(global_var)      // constraint "m" = acesso a memória — o compilador
                            // gera global_var(%rip) automaticamente em PIE
    );
    // Nota: na prática o compilador gera isso automaticamente para globais.
    // Mostramos aqui apenas para didática.
    fmt::print("  global_var = 0x{:X}\n", global_var);
    fmt::print("  lido via RIP-relative = 0x{:X}\n", lido);
    fmt::print("  (o compilador usa isso automaticamente para globais/statics)\n");
}

// ── Demo 6: Alinhamento de memória e seu impacto ─────────────
void demo_alignment() {
    section("2.6 Alinhamento de memória");
    //
    // Acessos NÃO alinhados são penalizados (ou proibidos em algumas instruções).
    // SIMD (SSE/AVX) exige alinhamento de 16/32 bytes.
    //
    // alignas(N) força alinhamento no C++.

    alignas(16) int arr_aligned[4]   = {1, 2, 3, 4};
    int             arr_unaligned[5] = {0, 1, 2, 3, 4};
    int* unaligned_ptr = &arr_unaligned[1]; // offset 1 elemento = +4 bytes

    fmt::print("  &arr_aligned[0]   = {:p}  (último hex: {:X})\n",
               (void*)arr_aligned, (uintptr_t)arr_aligned & 0xF);
    fmt::print("  &arr_unaligned[1] = {:p}  (último hex: {:X})\n",
               (void*)unaligned_ptr, (uintptr_t)unaligned_ptr & 0xF);

    fmt::print("  Alinhado 16B: endereço & 0xF == 0? {}\n",
               ((uintptr_t)arr_aligned & 0xF) == 0 ? "SIM ✓" : "NÃO ✗");

    // Leitura alinhada normal:
    int val;
    asm volatile ("movl (%1), %0" : "=r"(val) : "r"(arr_aligned));
    fmt::print("  Leitura via asm de arr_aligned[0] = {}\n", val);
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 02 · Memória e Endereçamento   ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_direto_indireto();
    demo_displacement();
    demo_array_index();
    demo_lea();
    demo_rip_relative();
    demo_alignment();

    fmt::print("\n\033[1;32m✓ Módulo 02 completo\033[0m\n\n");
}
