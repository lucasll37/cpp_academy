// ============================================================
// src/asm_course/14_disassembly.cpp
//
// MÓDULO 14 — Lendo o Assembly Gerado pelo Compilador
//
// ┌─────────────────────────────────────────────────────────┐
// │  Ferramentas:                                           │
// │                                                         │
// │  objdump -d -M intel <binário>                          │
// │    → disassembly de todo o texto do binário             │
// │                                                         │
// │  objdump -d -M intel <binário> | c++filt               │
// │    → demangle dos nomes C++                             │
// │                                                         │
// │  g++ -O2 -S -masm=intel arquivo.cpp -o arquivo.s       │
// │    → gera .s direto do compilador                       │
// │                                                         │
// │  godbolt.org — Compiler Explorer (interativo)           │
// │                                                         │
// │  gdb: disassemble /m <funcao>                           │
// │    → mistura fonte e assembly                           │
// └─────────────────────────────────────────────────────────┘
//
// Este módulo mostra funções comuns e explica o padrão
// de assembly que o compilador gera para cada construção.
// ============================================================

#include <cstdint>
#include <vector>
#include <fmt/core.h>

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// ── Padrão 1: if/else simples ─────────────────────────────────
// Compilador tipicamente gera:
//   cmp  edi, esi
//   jge  .else_branch    ← ou usa CMOV para branchless
//   ... then branch ...
//   jmp  .end
//   .else_branch:
//   ... else branch ...
//   .end:
int exemplo_if(int a, int b) {
    return (a < b) ? a * 2 : b * 3;
}

// ── Padrão 2: loop for simples ────────────────────────────────
// Compilador gera:
//   xor  eax, eax        ← soma = 0
//   test edi, edi        ← n > 0?
//   jle  .out
//   .loop:
//   add  eax, ecx        ← soma += i (ou usa LEA)
//   inc  ecx
//   cmp  ecx, edi
//   jl   .loop
//   .out:
//   ret
int64_t exemplo_loop(int n) {
    int64_t soma = 0;
    for (int i = 0; i < n; ++i) soma += i;
    return soma;
}

// ── Padrão 3: virtual dispatch ────────────────────────────────
// Compilador gera:
//   mov  rax, [rdi]      ← carrega vptr
//   call [rax + N]       ← indirect call via vtable
struct Base {
    virtual int compute(int x) = 0;
    virtual ~Base() = default;
};
struct Derived : Base {
    int compute(int x) override { return x * x; }
};

// ── Padrão 4: lambda / std::function ─────────────────────────
// Lambda sem captura → função normal em .text
// Lambda com captura → objeto com operator() + dados capturados

// ── Padrão 5: inlining ────────────────────────────────────────
// Com -O2, funções pequenas são inlined → desaparecem do assembly
__attribute__((noinline))  // força não-inlining para fins didáticos
int funcao_nao_inlined(int x) {
    return x * 7 + 3;
}

// ── Padrão 6: tail call ───────────────────────────────────────
// Se a última ação de uma função é chamar outra e retornar seu valor,
// o compilador pode usar JMP (tail call) em vez de CALL+RET:
//   jmp  outra_funcao    ← em vez de call + ret
int64_t helper(int64_t x) { return x * 2; }

__attribute__((noinline))
int64_t tail_caller(int64_t x) {
    return helper(x + 1);  // compilador pode gerar: jmp helper
}

// ── Demo: inspeciona seu próprio binário ─────────────────────
void demo_self_inspection() {
    section("14.1 Inspeção de funções deste binário");

    // Mostra os endereços das funções
    fmt::print("  Endereços das funções:\n");
    fmt::print("    exemplo_if:          {:p}\n", (void*)exemplo_if);
    fmt::print("    exemplo_loop:        {:p}\n", (void*)exemplo_loop);
    fmt::print("    funcao_nao_inlined:  {:p}\n", (void*)funcao_nao_inlined);
    fmt::print("    tail_caller:         {:p}\n", (void*)tail_caller);
    fmt::print("    helper:              {:p}\n", (void*)helper);

    // Mostra os primeiros bytes de cada função (prólogo típico)
    auto print_bytes = [](const char* name, void* fn) {
        auto* p = reinterpret_cast<uint8_t*>(fn);
        fmt::print("\n  {}:\n    ", name);
        for (int i = 0; i < 24; ++i)
            fmt::print("{:02X} ", p[i]);
        fmt::print("\n");

        // Decodifica prólogo manualmente
        if (p[0] == 0x55) fmt::print("    [0] 55        → push rbp\n");
        if (p[1] == 0x48 && p[2] == 0x89 && p[3] == 0xE5)
            fmt::print("    [1] 48 89 E5  → mov rbp, rsp\n");
        if (p[0] == 0xF3 || p[0] == 0xEB || p[0] == 0xE9)
            fmt::print("    [0] {:02X}        → salto/otimização\n", p[0]);
    };

    print_bytes("exemplo_if",   (void*)exemplo_if);
    print_bytes("exemplo_loop", (void*)exemplo_loop);
}

void demo_reading_guide() {
    section("14.2 Guia de leitura de disassembly");

    fmt::print("  Como executar:\n");
    fmt::print("    objdump -d -M intel build/src/asm_course/asm_14_disassembly | c++filt\n\n");

    fmt::print("  Formato de saída:\n");
    fmt::print("    <endereço>: <bytes hex>   <mnemônico>  <operandos>\n");
    fmt::print("    4011a0:     55            push   rbp\n");
    fmt::print("    4011a1:     48 89 e5      mov    rbp,rsp\n");
    fmt::print("    4011a4:     8d 04 37      lea    eax,[rdi+rsi*1]\n\n");

    fmt::print("  Dicas de leitura:\n");
    fmt::print("    xor eax,eax      → zera eax (mais curto que mov eax,0)\n");
    fmt::print("    test eax,eax     → verifica se eax==0 (como cmp eax,0)\n");
    fmt::print("    lea eax,[rdi+rdi] → multiplica por 2 (sem flags!)\n");
    fmt::print("    movsx eax,edi    → sign-extend int→int\n");
    fmt::print("    rep ret          → ret com prefixo NOP (evita stall AMD)\n\n");

    fmt::print("  Flags de otimização e seus efeitos:\n");
    fmt::print("    -O0: prólogo/epílogo completo, sem inline, sem CMOV\n");
    fmt::print("    -O1: inline básico, eliminação de dead code\n");
    fmt::print("    -O2: CMOV, unrolling, reordenamento de instruções\n");
    fmt::print("    -O3: vetorização auto (SSE/AVX), aggressive inline\n");
    fmt::print("    -Os: otimiza para tamanho (menos unrolling)\n\n");

    fmt::print("  Para ver o assembly de uma função específica:\n");
    fmt::print("    objdump -d build/... | grep -A 40 '<exemplo_loop>'\n");
}

void demo_vtable() {
    section("14.3 VTable — como o compilador implementa virtual");

    Derived d;
    Base* b = &d;
    int r = b->compute(5);

    fmt::print("  b->compute(5) = {} (via vtable)\n", r);

    // Mostra o que o compilador gerou:
    // Na memória de d: [vptr | ...dados...]
    // vptr → &vtable_de_Derived
    // vtable_de_Derived[0] = endereço de Derived::compute
    // vtable_de_Derived[1] = endereço de Derived::~Derived

    void** vptr = *reinterpret_cast<void***>(&d);
    fmt::print("  vptr de Derived: {:p}\n", (void*)vptr);
    fmt::print("  vptr[0] (compute): {:p}\n", vptr[0]);
    fmt::print("  vptr[1] (~Derived): {:p}\n", vptr[1]);
    fmt::print("\n  Assembly do dispatch virtual:\n");
    fmt::print("    mov rax, [rdi]        ; rax = vptr\n");
    fmt::print("    mov edi, 5            ; arg\n");
    fmt::print("    call qword ptr [rax]  ; chama vptr[0] = compute\n");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 14 · Lendo Disassembly         ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    // Exercita as funções para que apareçam no binário
    fmt::print("  exemplo_if(3,5)    = {}\n", exemplo_if(3,5));
    fmt::print("  exemplo_loop(10)   = {}\n", exemplo_loop(10));
    fmt::print("  nao_inlined(6)     = {}\n", funcao_nao_inlined(6));
    fmt::print("  tail_caller(10)    = {}\n", tail_caller(10));

    demo_self_inspection();
    demo_reading_guide();
    demo_vtable();

    fmt::print("\n\033[1;32m✓ Módulo 14 completo\033[0m\n\n");
}
