// ============================================================
// src/asm_course/04_bitwise.cpp
//
// MÓDULO 4 — Operações Bit a Bit
//
// ┌─────────────────────────────────────────────────────────┐
// │  AND  → bit=1 somente se ambos são 1                    │
// │  OR   → bit=1 se pelo menos um é 1                      │
// │  XOR  → bit=1 se são diferentes                         │
// │  NOT  → inverte todos os bits                           │
// │  SHL  → desloca à esquerda  (×2 por bit)                │
// │  SHR  → desloca à direita   (÷2 sem sinal)              │
// │  SAR  → desloca à direita   (÷2 com sinal, preserva SF) │
// │  ROL  → rotação à esquerda                              │
// │  ROR  → rotação à direita                               │
// │  RCL  → rotação à esquerda com carry                    │
// │  RCR  → rotação à direita com carry                     │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <fmt/core.h>

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

static void print_bits(const char* label, uint32_t v) {
    fmt::print("  {:12}: ", label);
    for (int i = 31; i >= 0; --i) {
        if (i == 23 || i == 15 || i == 7) fmt::print(" ");
        fmt::print("{}", (v >> i) & 1);
    }
    fmt::print(" (0x{:08X})\n", v);
}

// ── Demo 1: AND — mascaramento de bits ───────────────────────
void demo_and() {
    section("4.1 AND — mascaramento, extração de bits");

    uint32_t valor = 0b10110101'11001010'01010101'11110000;
    uint32_t mask  = 0x0000FFFF;  // mantém apenas os 16 bits baixos
    uint32_t result;

    asm volatile ("andl %2, %0" : "=r"(result) : "0"(valor), "r"(mask));

    print_bits("valor", valor);
    print_bits("mask ", mask);
    print_bits("AND  ", result);

    // Usos clássicos de AND:
    fmt::print("\n  Truques com AND:\n");

    // 1. Testar bit específico
    uint32_t bit3;
    asm volatile (
        "movl %1, %0\n\t"
        "andl $8, %0"            // bit 3 = 0b1000 = 8
        : "=r"(bit3) : "r"(valor)
    );
    fmt::print("    bit 3 está ativo? {}\n", bit3 ? "SIM" : "NÃO");

    // 2. Limpar bit específico
    uint32_t cleared;
    asm volatile (
        "movl %1, %0\n\t"
        "andl $~(1<<4), %0"      // limpa bit 4
        : "=r"(cleared) : "r"(valor)
    );
    fmt::print("    após limpar bit 4: 0x{:08X}\n", cleared);

    // 3. Verificar se número é par (bit 0 == 0)
    asm volatile ("andl $1, %0" : "=r"(result) : "0"(valor));
    fmt::print("    {} é {}\n", valor, result == 0 ? "par" : "ímpar");

    // 4. Alinhamento: addr & ~(N-1) = addr arredondado para baixo para N
    uint64_t addr = 0x1234567;
    uint64_t aligned;
    asm volatile (
        "movq %1, %0\n\t"
        "andq $~(64-1), %0"      // alinha para 64 bytes
        : "=r"(aligned) : "r"(addr)
    );
    fmt::print("    0x{:X} alinhado p/ 64B → 0x{:X}\n", addr, aligned);
}

// ── Demo 2: OR — setar bits ───────────────────────────────────
void demo_or() {
    section("4.2 OR — ativar bits");

    uint32_t valor = 0b00000000;
    uint32_t result;

    // Seta bits 0, 2, 4 (flags: READ=1, WRITE=4, EXEC=16)
    asm volatile (
        "movl %1, %0\n\t"
        "orl $0b00010101, %0"
        : "=r"(result) : "r"(valor)
    );
    print_bits("antes", valor);
    print_bits("OR    ", result);
    fmt::print("    ativar READ(0)|WRITE(2)|EXEC(4): 0x{:02X}\n", result);

    // TEST = AND que só afeta flags (não modifica operando)
    uint32_t flags_val;
    asm volatile (
        "movl %1, %%eax\n\t"
        "testl $1, %%eax\n\t"    // testa bit 0 sem modificar eax
        "setne %%al\n\t"         // al = 1 se bit estava setado
        "movzbl %%al, %0"
        : "=r"(flags_val) : "r"(result)
        : "%eax"
    );
    fmt::print("    TEST (bit 0 ativo?): {}\n", flags_val ? "SIM" : "NÃO");
}

// ── Demo 3: XOR — flip e zeragem ──────────────────────────────
void demo_xor() {
    section("4.3 XOR — flip, zeragem, troca sem temporário");

    uint32_t a = 0xAAAAAAAA, b = 0x55555555, result;

    // XOR para zerar registrador (mais rápido que MOV $0)
    asm volatile ("xorl %%eax, %%eax; movl %%eax, %0"
                  : "=r"(result) : : "%eax");
    fmt::print("  xor eax,eax → {} (zeragem rápida)\n", result);

    // XOR para flip de bits
    asm volatile (
        "movl %1, %0\n\t"
        "xorl $0xFF, %0"         // inverte os 8 bits baixos
        : "=r"(result) : "r"(a)
    );
    fmt::print("  0x{:08X} XOR 0xFF → 0x{:08X}\n", a, result);

    // Troca sem temporário (XOR swap)
    uint32_t x = 0xDEAD, y = 0xBEEF;
    asm volatile (
        "xorl %1, %0\n\t"        // x ^= y
        "xorl %0, %1\n\t"        // y ^= x  (y agora tem o x original)
        "xorl %1, %0"            // x ^= y  (x agora tem o y original)
        : "+r"(x), "+r"(y)
    );
    fmt::print("  XOR swap: x=0x{:04X}, y=0x{:04X}  (eram DEAD/BEEF)\n", x, y);

    // Encriptar byte simples (chave XOR)
    uint8_t msg = 'A', key = 0x42, enc, dec;
    asm volatile ("xorb %2, %0" : "=r"(enc) : "0"(msg), "r"(key));
    asm volatile ("xorb %2, %0" : "=r"(dec) : "0"(enc), "r"(key));
    fmt::print("  '{}' XOR 0x{:02X} → 0x{:02X} → '{}' (encriptar/decriptar)\n",
               (char)msg, key, enc, (char)dec);
}

// ── Demo 4: SHL, SHR, SAR ────────────────────────────────────
void demo_shifts() {
    section("4.4 SHL / SHR / SAR — deslocamentos");

    int32_t pos =  8;
    int32_t neg = -8;
    int32_t result;

    // SHL: desloca à esquerda (multiplica por 2^n)
    asm volatile ("movl %1,%0; shll $3,%0" : "=r"(result) : "r"(pos));
    fmt::print("  SHL {} por 3 = {} ({} * 8)\n", pos, result, pos);

    // SHR: desloca à direita sem sinal (preenche 0 à esquerda)
    asm volatile ("movl %1,%0; shrl $3,%0" : "=r"(result) : "r"(pos));
    fmt::print("  SHR {} por 3 = {} ({} / 8)\n", pos, result, pos);

    // SAR: desloca à direita com sinal (propaga bit de sinal)
    asm volatile ("movl %1,%0; sarl $3,%0" : "=r"(result) : "r"(neg));
    fmt::print("  SAR {} por 3 = {} (÷8 com sinal, preserva negativo)\n", neg, result);

    // SHR vs SAR em negativo — a diferença crítica:
    uint32_t shr_neg;
    asm volatile ("movl %1,%0; shrl $3,%0" : "=r"(shr_neg) : "r"(neg));
    fmt::print("  SHR {} por 3 = {} (trata como unsigned!)\n", neg, (int32_t)shr_neg);

    // Deslocamento variável: CL contém o número de bits
    uint32_t val = 0x01;
    uint8_t  nbits = 7;
    asm volatile (
        "movb %2, %%cl\n\t"      // cl = número de bits (SHL usa cl)
        "shll %%cl, %0"
        : "+r"(val) : "0"(val), "r"(nbits)
        : "%cl"
    );
    fmt::print("  0x01 SHL {} = 0x{:08X}\n", nbits, val);
}

// ── Demo 5: Rotações ──────────────────────────────────────────
void demo_rotations() {
    section("4.5 ROL / ROR — rotações (bits não se perdem)");

    uint32_t val = 0x80000001;  // bit 31 e bit 0 ativos
    uint32_t result;

    // ROL: bits que saem pela esquerda entram pela direita
    asm volatile ("movl %1,%0; roll $1,%0" : "=r"(result) : "r"(val));
    print_bits("original", val);
    print_bits("ROL 1   ", result);

    // ROR
    asm volatile ("movl %1,%0; rorl $1,%0" : "=r"(result) : "r"(val));
    print_bits("ROR 1   ", result);

    // Uso clássico: hash e criptografia (ROL/ROR preservam bits)
    uint32_t hash = 0xDEADBEEF;
    asm volatile (
        "movl %1, %0\n\t"
        "roll $5, %0\n\t"        // ROL 5 (parte de muitos algoritmos de hash)
        "xorl $0xA5A5A5A5, %0"   // XOR com constante
        : "=r"(result) : "r"(hash)
    );
    fmt::print("  hash simples de 0x{:08X}: 0x{:08X}\n", hash, result);
}

// ── Demo 6: BSF, BSR, LZCNT, TZCNT ──────────────────────────
void demo_bit_scan() {
    section("4.6 BSF/BSR/LZCNT/TZCNT — localizar bits");
    //
    // BSF (Bit Scan Forward):  encontra o índice do bit 1 MENOS significativo
    // BSR (Bit Scan Reverse):  encontra o índice do bit 1 MAIS significativo
    // TZCNT: conta trailing zeros (BSF mais seguro — funciona com 0)
    // LZCNT: conta leading  zeros (log2 em inteiro)

    uint32_t val = 0b00001010'00000000'00000000'00000000u;
    uint32_t bsf, bsr;

    asm volatile ("bsfl %1, %0" : "=r"(bsf) : "r"(val));
    asm volatile ("bsrl %1, %0" : "=r"(bsr) : "r"(val));

    print_bits("val", val);
    fmt::print("  BSF (1° bit ativo, da direita) = {}\n", bsf);
    fmt::print("  BSR (1° bit ativo, da esquerda) = {}\n", bsr);

    // LZCNT: floor(log2(n)) para potências de 2
    uint32_t n = 64, lzc;
    asm volatile ("lzcntl %1, %0" : "=r"(lzc) : "r"(n));
    fmt::print("  LZCNT({}) = {} leading zeros → log2({}) = {}\n",
               n, lzc, n, 31 - lzc);

    // TZCNT: trailing zeros = expoente na fatoração por 2
    uint32_t m = 48, tzc;  // 48 = 16 * 3 = 2^4 * 3
    asm volatile ("tzcntl %1, %0" : "=r"(tzc) : "r"(m));
    fmt::print("  TZCNT({}) = {} → {} = 2^{} * {}\n",
               m, tzc, m, tzc, m >> tzc);
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 04 · Operações Bit a Bit       ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_and();
    demo_or();
    demo_xor();
    demo_shifts();
    demo_rotations();
    demo_bit_scan();

    fmt::print("\n\033[1;32m✓ Módulo 04 completo\033[0m\n\n");
}
