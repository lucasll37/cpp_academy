// ============================================================
// src/asm_course/01_registers.cpp
//
// MÓDULO 1 — Registradores x86-64
//
// ┌─────────────────────────────────────────────────────────┐
// │  O que é um registrador?                                │
// │                                                         │
// │  É uma célula de memória DENTRO da CPU — latência zero  │
// │  (comparado a cache L1 ~4 ciclos, RAM ~200 ciclos).     │
// │  x86-64 tem 16 registradores de propósito geral de      │
// │  64 bits, mais registradores especiais e vetoriais.     │
// └─────────────────────────────────────────────────────────┘
//
// Mapa de aliases (o mesmo registrador físico, visões diferentes):
//
//   64 bits │ 32 bits │ 16 bits │  8 alto │  8 baixo
//   ────────┼─────────┼─────────┼─────────┼──────────
//    rax    │  eax    │   ax    │   ah    │   al
//    rbx    │  ebx    │   bx    │   bh    │   bl
//    rcx    │  ecx    │   cx    │   ch    │   cl
//    rdx    │  edx    │   dx    │   dh    │   dl
//    rsi    │  esi    │   si    │    -    │   sil
//    rdi    │  edi    │   di    │    -    │   dil
//    rbp    │  ebp    │   bp    │    -    │   bpl
//    rsp    │  esp    │   sp    │    -    │   spl
//    r8     │  r8d    │   r8w   │    -    │   r8b
//    r9     │  r9d    │   r9w   │    -    │   r9b
//    r10    │  r10d   │   r10w  │    -    │   r10b
//    r11    │  r11d   │   r11w  │    -    │   r11b
//    r12    │  r12d   │   r12w  │    -    │   r12b
//    r13    │  r13d   │   r13w  │    -    │   r13b
//    r14    │  r14d   │   r14w  │    -    │   r14b
//    r15    │  r15d   │   r15w  │    -    │   r15b
//
// ARMADILHA CLÁSSICA: escrever em %eax ZERA os 32 bits altos de %rax.
// Escrever em %ax NÃO zera — apenas substitui os 16 bits baixos.
// ============================================================

#include <cstdint>
#include <fmt/core.h>

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}
static void ok(bool b, const char* msg) {
    fmt::print("  {} {}\n", b ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m", msg);
}

// ── Demo 1: aliases e zero-extension ─────────────────────────
void demo_aliases() {
    section("1.1 Aliases — mesmo registrador, visões diferentes");

    uint64_t rax_val;

    // Carrega 0xDEADBEEFCAFEBABE em rax
    asm volatile (
        "movabsq $0xDEADBEEFCAFEBABE, %%rax\n\t"   // movabs: move imediato 64-bit
        "movq %%rax, %0"
        : "=r"(rax_val)
        :
        : "%rax"
    );
    fmt::print("  rax = 0x{:016X}\n", rax_val);
    fmt::print("        ^^^^^^^^^^^^^^^^ 64 bits\n");

    // Lê apenas eax (32 bits baixos)
    uint32_t eax_val;
    asm volatile (
        "movabsq $0xDEADBEEFCAFEBABE, %%rax\n\t"
        "movl %%eax, %0"
        : "=r"(eax_val)
        :
        : "%rax"
    );
    fmt::print("  eax = 0x{:08X}  (32 bits baixos de rax)\n", eax_val);

    // Lê apenas ax (16 bits baixos)
    uint16_t ax_val;
    asm volatile (
        "movabsq $0xDEADBEEFCAFEBABE, %%rax\n\t"
        "movw %%ax, %0"
        : "=r"(ax_val)
        :
        : "%rax"
    );
    fmt::print("  ax  = 0x{:04X}      (16 bits baixos de rax)\n", ax_val);

    // Lê al (8 bits mais baixos)
    uint8_t al_val;
    asm volatile (
        "movabsq $0xDEADBEEFCAFEBABE, %%rax\n\t"
        "movb %%al, %0"
        : "=r"(al_val)
        :
        : "%rax"
    );
    fmt::print("  al  = 0x{:02X}        (8 bits baixos de rax)\n", al_val);

    // Lê ah (bits 8-15)
    uint8_t ah_val;
    asm volatile (
        "movabsq $0xDEADBEEFCAFEBABE, %%rax\n\t"
        "movb %%ah, %0"
        : "=r"(ah_val)
        :
        : "%rax"
    );
    fmt::print("  ah  = 0x{:02X}        (bits 8-15 de rax)\n", ah_val);
}

// ── Demo 2: A armadilha do zero-extension ────────────────────
void demo_zero_extension() {
    section("1.2 Armadilha: escrever em eax ZERA os 32 bits altos de rax");

    uint64_t before, after;

    asm volatile (
        // Carrega valor grande em rax
        "movabsq $0xDEADBEEF00000000, %%rax\n\t"
        "movq %%rax, %0\n\t"         // salva antes
        // Agora escreve apenas na metade baixa (eax)
        "movl $0x12345678, %%eax\n\t" // ← ZERA os 32 bits altos!
        "movq %%rax, %1"              // salva depois
        : "=r"(before), "=r"(after)
        :
        : "%rax"
    );

    fmt::print("  antes:  rax = 0x{:016X}\n", before);
    fmt::print("  movl $0x12345678, %%eax\n");
    fmt::print("  depois: rax = 0x{:016X}\n", after);
    fmt::print("  ↑ Os 32 bits altos foram ZERADOS!\n");
    ok(after == 0x12345678, "bits altos zerados por escrita em eax");

    // Contraste: escrever em ax NÃO zera
    uint64_t after_ax;
    asm volatile (
        "movabsq $0xDEADBEEFCAFEBABE, %%rax\n\t"
        "movw $0x1234, %%ax\n\t"  // só substitui os 16 bits baixos
        "movq %%rax, %0"
        : "=r"(after_ax)
        :
        : "%rax"
    );
    fmt::print("\n  após movw $0x1234, %%ax:\n");
    fmt::print("  rax = 0x{:016X}\n", after_ax);
    fmt::print("  ↑ Apenas os 16 bits baixos mudaram, altos preservados\n");
}

// ── Demo 3: Registradores especiais ──────────────────────────
void demo_special_registers() {
    section("1.3 Registradores especiais: RIP, RFLAGS, RSP");

    // RIP: Instruction Pointer — endereço da próxima instrução
    // Não pode ser lido diretamente com MOV, mas podemos usar LEA ou CALL/POP
    uint64_t rip_val;
    asm volatile (
        "lea (%%rip), %0"   // LEA com RIP-relative → captura endereço atual
        : "=r"(rip_val)
    );
    fmt::print("  RIP (instrução atual) ≈ 0x{:016X}\n", rip_val);
    fmt::print("  RIP é automaticamente incrementado após cada instrução\n");

    // RSP: Stack Pointer — topo da stack
    uint64_t rsp_val;
    asm volatile (
        "movq %%rsp, %0"
        : "=r"(rsp_val)
    );
    fmt::print("  RSP (topo da stack)  = 0x{:016X}\n", rsp_val);

    // RFLAGS: registrador de flags
    // Flags principais:
    //   CF (bit 0)  — Carry Flag: overflow sem sinal
    //   PF (bit 2)  — Parity Flag: paridade do resultado
    //   AF (bit 4)  — Auxiliary Carry (BCD)
    //   ZF (bit 6)  — Zero Flag: resultado == 0
    //   SF (bit 7)  — Sign Flag: resultado negativo
    //   OF (bit 11) — Overflow Flag: overflow com sinal
    uint64_t flags;
    asm volatile (
        "pushfq\n\t"        // empurra RFLAGS na stack
        "popq %0"           // retira para variável
        : "=r"(flags)
    );
    fmt::print("  RFLAGS = 0x{:016X}\n", flags);
    fmt::print("    ZF (zero)     = {}\n", (flags >> 6) & 1);
    fmt::print("    SF (sinal)    = {}\n", (flags >> 7) & 1);
    fmt::print("    CF (carry)    = {}\n", (flags >> 0) & 1);
    fmt::print("    OF (overflow) = {}\n", (flags >> 11) & 1);
}

// ── Demo 4: Tamanhos e sufixos de instrução ───────────────────
void demo_suffixes() {
    section("1.4 Sufixos de instrução: b/w/l/q");
    //
    // AT&T syntax usa sufixos para indicar o tamanho do operando:
    //   b → byte       (8 bits)
    //   w → word       (16 bits)
    //   l → long/dword (32 bits)   ← "long" no sentido do GAS (não C long!)
    //   q → quad/qword (64 bits)
    //
    // Intel syntax usa o operando para inferir (ou prefixos BYTE PTR, etc.)

    uint8_t  val8;
    uint16_t val16;
    uint32_t val32;
    uint64_t val64;

    asm volatile ("movb $0xAB, %0"             : "=r"(val8));
    asm volatile ("movw $0xABCD, %0"           : "=r"(val16));
    asm volatile ("movl $0xABCDEF01, %0"       : "=r"(val32));
    asm volatile ("movabsq $0xABCDEF0123456789, %0" : "=r"(val64));

    fmt::print("  movb → {:3} bits → 0x{:02X}\n",  8,  val8);
    fmt::print("  movw → {:3} bits → 0x{:04X}\n", 16, val16);
    fmt::print("  movl → {:3} bits → 0x{:08X}\n", 32, val32);
    fmt::print("  movq → {:3} bits → 0x{:016X}\n",64, val64);
}

// ── Demo 5: MOVSX e MOVZX — extensão de sinal vs zero ────────
void demo_sign_extension() {
    section("1.5 MOVSX (sign-extend) vs MOVZX (zero-extend)");
    //
    // Ao mover um valor menor para um registrador maior, precisamos
    // decidir o que fazer com os bits extras:
    //   MOVZX → preenche com 0  (interpretação sem sinal)
    //   MOVSX → replica o bit de sinal (interpretação com sinal)

    int8_t  negativo = -1;   // 0xFF em 8 bits
    int8_t  positivo =  5;   // 0x05 em 8 bits

    int64_t zx_neg, sx_neg, zx_pos, sx_pos;

    asm volatile (
        "movsbq %1, %0"   // MOVSX byte→qword (sign-extend)
        : "=r"(sx_neg) : "m"(negativo)
    );
    asm volatile (
        "movzbq %1, %0"   // MOVZX byte→qword (zero-extend)
        : "=r"(zx_neg) : "m"(negativo)
    );
    asm volatile (
        "movsbq %1, %0"
        : "=r"(sx_pos) : "m"(positivo)
    );
    asm volatile (
        "movzbq %1, %0"
        : "=r"(zx_pos) : "m"(positivo)
    );

    fmt::print("  Valor original: -1 (0xFF como byte)\n");
    fmt::print("  MOVZX → 0x{:016X} = {}  (trata como unsigned)\n", (uint64_t)zx_neg, zx_neg);
    fmt::print("  MOVSX → 0x{:016X} = {}  (preserva sinal)\n",      (uint64_t)sx_neg, sx_neg);

    fmt::print("\n  Valor original: +5 (0x05 como byte)\n");
    fmt::print("  MOVZX → 0x{:016X} = {}\n", (uint64_t)zx_pos, zx_pos);
    fmt::print("  MOVSX → 0x{:016X} = {}\n", (uint64_t)sx_pos, sx_pos);
    fmt::print("  (para positivos, MOVZX == MOVSX)\n");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 01 · Registradores x86-64      ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_aliases();
    demo_zero_extension();
    demo_special_registers();
    demo_suffixes();
    demo_sign_extension();

    fmt::print("\n\033[1;32m✓ Módulo 01 completo\033[0m\n\n");
}
