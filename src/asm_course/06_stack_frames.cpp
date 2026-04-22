// ============================================================
// src/asm_course/06_stack_frames.cpp
//
// MÓDULO 6 — Stack Frames: PUSH, POP, CALL, RET
//
// ┌─────────────────────────────────────────────────────────┐
// │  A stack (pilha) cresce para BAIXO no x86-64.           │
// │  RSP sempre aponta para o topo (menor endereço ativo).  │
// │                                                         │
// │  PUSH val  → RSP -= 8; [RSP] = val                      │
// │  POP  dst  → dst = [RSP]; RSP += 8                      │
// │                                                         │
// │  CALL label → PUSH(RIP+instr_size); JMP label           │
// │  RET        → POP RIP                                   │
// │                                                         │
// │  Stack frame de uma função:                             │
// │                                                         │
// │  Endereço alto                                          │
// │  ┌────────────────────┐  ← RSP antes do CALL            │
// │  │  endereço de ret.  │  ← CALL empurra isso            │
// │  ├────────────────────┤  ← RSP ao entrar na função      │
// │  │  RBP anterior      │  ← PUSH RBP (prólogo)           │
// │  ├────────────────────┤  ← RBP = RSP (base do frame)    │
// │  │  variável local 1  │  -8(%rbp)                       │
// │  │  variável local 2  │  -16(%rbp)                      │
// │  │  ...               │                                 │
// │  └────────────────────┘  ← RSP atual                    │
// │  Endereço baixo                                         │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <fmt/core.h>

extern "C" {
    // Funções definidas em 06_stack_asm.s
    int64_t asm_fibonacci(int64_t n);
    int64_t asm_with_locals(int64_t a, int64_t b, int64_t c);
    void    asm_inspect_frame(uint64_t* out_rsp, uint64_t* out_rbp,
                              uint64_t* out_ret_addr);
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// ── Demo 1: PUSH/POP básico ───────────────────────────────────
void demo_push_pop() {
    section("6.1 PUSH / POP — manipulação direta da stack");

    uint64_t val1 = 0xAAAAAAAA;
    uint64_t val2 = 0xBBBBBBBB;
    uint64_t pop1, pop2;
    uint64_t rsp_before, rsp_after;

    asm volatile (
        "movq %%rsp, %2\n\t"     // %2 = rsp_before
        "pushq %4\n\t"
        "pushq %5\n\t"
        "popq  %1\n\t"           // %1 = pop1 (val2, LIFO)
        "popq  %0\n\t"           // %0 = pop2 (val1)
        "movq %%rsp, %3"         // %3 = rsp_after
        : "=r"(pop2), "=r"(pop1), "=r"(rsp_before), "=r"(rsp_after)  // %0–%3
        : "r"(val1), "r"(val2)                                          // %4, %5
    );

    // Versão sem a confusão acima — mais clara:
    uint64_t popped_first, popped_second;
    uint64_t sp_before, sp_after_push, sp_after_pop;
    asm volatile (
        "movq %%rsp, %0\n\t"     // sp_before
        "pushq %5\n\t"
        "pushq %6\n\t"
        "movq %%rsp, %1\n\t"     // sp_after_push
        "popq  %3\n\t"           // popped_first  = val2
        "popq  %4\n\t"           // popped_second = val1
        "movq %%rsp, %2"         // sp_after_pop
        : "=r"(sp_before), "=r"(sp_after_push), "=r"(sp_after_pop),
        "=r"(popped_first), "=r"(popped_second)                      // %0–%4
        : "r"(val1), "r"(val2)                                          // %5, %6
    );

    fmt::print("  RSP antes:       0x{:016X}\n", sp_before);
    fmt::print("  RSP após 2 PUSH: 0x{:016X}  (decrementou {:+d} bytes)\n",
               sp_after_push, (int64_t)(sp_after_push - sp_before));
    fmt::print("  RSP após 2 POP:  0x{:016X}  (restaurado)\n", sp_after_pop);
    fmt::print("  Primeiro pop: 0x{:08X} (era val2={:X})\n",
               (uint32_t)popped_first, val2);
    fmt::print("  Segundo pop:  0x{:08X} (era val1={:X})\n",
               (uint32_t)popped_second, val1);
    fmt::print("  → LIFO: Last In, First Out\n");
}

// ── Demo 2: Prólogo/Epílogo — frame padrão ────────────────────
void demo_prologue_epilogue() {
    section("6.2 Prólogo/Epílogo da função — frame convencional");
    //
    // Toda função com variáveis locais ou que chama outra função
    // usa o padrão:
    //
    // PRÓLOGO:            EPÍLOGO:
    //   push rbp            leave       (ou: mov rsp,rbp; pop rbp)
    //   mov  rbp, rsp       ret
    //   sub  rsp, N         (N = espaço para variáveis locais, alinhado 16)
    //
    // Com -fomit-frame-pointer: compilador pode eliminar rbp e usar rsp direto.

    uint64_t captured_rsp, captured_rbp, captured_ret;
    asm_inspect_frame(&captured_rsp, &captured_rbp, &captured_ret);

    fmt::print("  Dentro de asm_inspect_frame():\n");
    fmt::print("    RSP = 0x{:016X}  (topo da stack)\n", captured_rsp);
    fmt::print("    RBP = 0x{:016X}  (base do frame)\n", captured_rbp);
    fmt::print("    [RBP+8] = endereço de retorno = 0x{:016X}\n", captured_ret);
    fmt::print("    diferença RBP-RSP = {} bytes (variáveis locais)\n",
               (int64_t)(captured_rbp - captured_rsp));
}

// ── Demo 3: Fibonacci recursivo em ASM ───────────────────────
void demo_recursive() {
    section("6.3 Recursão em ASM — fibonacci(n)");
    //
    // asm_fibonacci(n) mostra como uma função recursiva preserva
    // registradores callee-saved (rbx, r12-r15) na stack.

    fmt::print("  Fibonacci calculado em assembly puro:\n");
    for (int i = 0; i <= 10; ++i) {
        fmt::print("    fib({:2}) = {}\n", i, asm_fibonacci(i));
    }
}

// ── Demo 4: Variáveis locais no frame ────────────────────────
void demo_locals() {
    section("6.4 Variáveis locais via stack frame");
    //
    // asm_with_locals demonstra alocação de variáveis locais
    // no stack frame (sub rsp, N) e acesso via -N(%rbp).

    int64_t result = asm_with_locals(10, 20, 30);
    fmt::print("  asm_with_locals(10, 20, 30):\n");
    fmt::print("    (a*b) + (b*c) + (a+c) = (10*20)+(20*30)+(10+30)\n");
    fmt::print("    = 200 + 600 + 40 = 840\n");
    fmt::print("    resultado = {}\n", result);
}

// ── Demo 5: Red zone — zona proibida abaixo do RSP ────────────
void demo_red_zone() {
    section("6.5 Red Zone — os 128 bytes abaixo do RSP");
    //
    // System V AMD64 ABI garante que os 128 bytes abaixo do RSP
    // (endereços [RSP-128, RSP)) são preservados pelo OS.
    //
    // Isso permite que funções folha (que não chamam outras) usem
    // essa área SEM ajustar o RSP — economizando o sub/add rsp.
    //
    // Funções que chamam outras NÃO podem confiar na red zone.

    int var_in_red_zone;
    asm volatile (
        "movl $0xABCD, -4(%%rsp)"   // escreve 4 bytes na red zone
        ::: "memory"
    );
    asm volatile (
        "movl -4(%%rsp), %0"        // lê de volta da red zone
        : "=r"(var_in_red_zone)
    );
    fmt::print("  Escreveu/leu 0x{:04X} via red zone (abaixo do RSP)\n",
               (uint32_t)var_in_red_zone);
    fmt::print("  Isso é válido SOMENTE em funções folha!\n");
    fmt::print("  ⚠ Interrupções/signals podem corromper a red zone\n");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 06 · Stack Frames              ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_push_pop();
    demo_prologue_epilogue();
    demo_recursive();
    demo_locals();
    demo_red_zone();

    fmt::print("\n\033[1;32m✓ Módulo 06 completo\033[0m\n\n");
}
