// ============================================================
// src/asm_course/10_string_ops.cpp
//
// MÓDULO 10 — Operações de String: REP MOV/STO/SCA/CMPS
//
// ┌─────────────────────────────────────────────────────────┐
// │  Instruções de string x86 usam registradores implícitos │
// │                                                         │
// │  MOVS (move string):                                    │
// │    movsb/movsw/movsl/movsq                              │
// │    [rdi] ← [rsi];  rdi ± size;  rsi ± size             │
// │                                                         │
// │  STOS (store string):                                   │
// │    stosb/stosw/stosl/stosq                              │
// │    [rdi] ← al/ax/eax/rax;  rdi ± size                  │
// │                                                         │
// │  SCAS (scan string):                                    │
// │    scasb/scasw/scasl/scasq                              │
// │    flags ← [rdi] - al/ax/eax/rax;  rdi ± size          │
// │                                                         │
// │  CMPS (compare string):                                 │
// │    cmpsb/cmpsw/cmpsl/cmpsq                              │
// │    flags ← [rdi] - [rsi];  rdi ± size;  rsi ± size     │
// │                                                         │
// │  Prefixos REP:                                          │
// │    REP    — repete enquanto RCX != 0                    │
// │    REPE   — repete enquanto RCX != 0 E ZF = 1           │
// │    REPNE  — repete enquanto RCX != 0 E ZF = 0           │
// │                                                         │
// │  DF (Direction Flag):                                   │
// │    CLD → DF=0 → rdi/rsi incrementam (para frente) ✓    │
// │    STD → DF=1 → rdi/rsi decrementam (para trás)        │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <cstring>
#include <fmt/core.h>

extern "C" {
    void    asm_rep_movsq(void* dst, const void* src, size_t qwords);
    void    asm_rep_stosb(void* dst, uint8_t byte, size_t n);
    size_t  asm_repne_scasb(const char* s);
    int     asm_repe_cmpsb(const char* a, const char* b, size_t n);
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}
static void ok(bool b, const char* msg) {
    fmt::print("  {} {}\n", b?"\033[32m✓\033[0m":"\033[31m✗\033[0m", msg);
}

void demo_rep_movs() {
    section("10.1 REP MOVSQ — cópia eficiente de memória");

    uint64_t src[8] = {1,2,3,4,5,6,7,8};
    uint64_t dst[8] = {};

    asm_rep_movsq(dst, src, 8);

    bool correct = true;
    for (int i = 0; i < 8; ++i) correct &= (dst[i] == src[i]);
    ok(correct, "REP MOVSQ copiou 8 qwords corretamente");
    fmt::print("  dst = [{},{},{},{},{},{},{},{}]\n",
               dst[0],dst[1],dst[2],dst[3],dst[4],dst[5],dst[6],dst[7]);

    // Demonstra comportamento inline com CLD garantido
    uint8_t src2[16], dst2[16];
    for (int i = 0; i < 16; ++i) src2[i] = (uint8_t)(i * 11);
    asm volatile (
        "cld\n\t"               // DF=0: direção para frente (SEMPRE faça isso!)
        "rep movsb"             // copia rcx bytes de [rsi] para [rdi]
        :
        : "D"(dst2), "S"(src2), "c"((size_t)16)
        : "memory"
    );
    ok(memcmp(dst2, src2, 16) == 0, "REP MOVSB inline copiou 16 bytes");
}

void demo_rep_stos() {
    section("10.2 REP STOSB — preenchimento de memória");

    uint8_t buf[64];
    memset(buf, 0xFF, sizeof(buf));

    asm_rep_stosb(buf, 0xAB, sizeof(buf));
    bool ok_all = true;
    for (auto b : buf) ok_all &= (b == 0xAB);
    ok(ok_all, "REP STOSB preencheu todos os 64 bytes com 0xAB");

    // REP STOSQ para zeragem rápida (8 bytes por iteração)
    uint64_t buf64[8];
    for (auto& x : buf64) x = 0xDEADBEEF;
    asm volatile (
        "xorq %%rax, %%rax\n\t"  // rax = 0 (valor a escrever)
        "cld\n\t"
        "rep stosq"               // zera rcx qwords a partir de [rdi]
        :
        : "D"(buf64), "c"((size_t)8)
        : "%rax", "memory"
    );
    ok_all = true;
    for (auto x : buf64) ok_all &= (x == 0);
    ok(ok_all, "REP STOSQ zerou 8 qwords");
}

void demo_repne_scasb() {
    section("10.3 REPNE SCASB — busca de byte (strlen)");

    const char* s = "hello, assembly!";
    size_t len = asm_repne_scasb(s);
    ok(len == strlen(s), fmt::format("strlen(\"{}\") = {}", s, len).c_str());

    // Demonstra REPNE SCASB para buscar caractere específico
    const char* haystack = "find the X in this string";
    char needle = 'X';
    size_t pos;
    asm volatile (
        "cld\n\t"
        "repne scasb\n\t"        // procura al em [rdi]; rdi++ por byte
        "subq %2, %0\n\t"        // pos = rdi_final - rdi_original
        "decq %0"                // -1 pois rdi passou do byte encontrado
        : "=D"(pos)
        : "D"(haystack), "r"(haystack), "a"((int)needle), "c"((size_t)~0ULL)
        : "cc", "memory"
    );
    ok(haystack[pos] == needle,
       fmt::format("'{}' encontrado em pos {}", needle, pos).c_str());
}

void demo_repe_cmpsb() {
    section("10.4 REPE CMPSB — comparação de strings");

    const char* a = "hello world";
    const char* b = "hello world";
    const char* c = "hello WORLD";

    int r1 = asm_repe_cmpsb(a, b, 11);
    int r2 = asm_repe_cmpsb(a, c, 11);

    ok(r1 == 0, "strings iguais → 0");
    ok(r2 != 0, "strings diferentes → != 0");
    fmt::print("  repe_cmpsb(\"{}\", \"{}\") = {}\n", a, c, r2);
    fmt::print("  (para no primeiro byte diferente: 'w'=119 vs 'W'=87, diff={})\n", r2);
}

void demo_direction_flag() {
    section("10.5 Direction Flag — CLD vs STD");
    //
    // CLD: DF=0 → strings caminham para frente (endereços crescentes)
    // STD: DF=1 → strings caminham para trás  (endereços decrescentes)
    //
    // REGRA: SEMPRE use CLD antes de REP MOVS/STOS.
    // A ABI System V garante DF=0 ao entrar em funções.
    // Se você usar STD, deve restaurar CLD antes de retornar!

    uint8_t arr[8] = {1,2,3,4,5,6,7,8};
    uint8_t rev[8] = {};

    // Copia ao contrário: rdi aponta para o fim de rev, rsi para o fim de arr
    asm volatile (
        "std\n\t"               // DF=1: decrementar após cada byte
        "rep movsb\n\t"         // copia 8 bytes de trás para frente
        "cld"                   // OBRIGATÓRIO: restaura DF=0 antes de retornar
        :
        : "D"(&rev[7]), "S"(&arr[7]), "c"((size_t)8)
        : "memory"
    );

    bool reversed = true;
    for (int i = 0; i < 8; ++i) reversed &= (rev[i] == arr[7-i]);
    ok(reversed, "STD+REP MOVSB inverteu o array");
    fmt::print("  original: [1,2,3,4,5,6,7,8]\n");
    fmt::print("  invertido:[{},{},{},{},{},{},{},{}]\n",
               rev[0],rev[1],rev[2],rev[3],rev[4],rev[5],rev[6],rev[7]);
    fmt::print("  ⚠ STD é raro e perigoso — sempre restaure CLD!\n");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 10 · String Operations         ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_rep_movs();
    demo_rep_stos();
    demo_repne_scasb();
    demo_repe_cmpsb();
    demo_direction_flag();

    fmt::print("\n\033[1;32m✓ Módulo 10 completo\033[0m\n\n");
}
