// ============================================================
// src/asm_course/05_control_flow.cpp
//
// MÓDULO 5 — Controle de Fluxo
//
// ┌─────────────────────────────────────────────────────────┐
// │  Jumps condicionais — dependem das flags após CMP/TEST  │
// │                                                         │
// │  Unsigned:                 Signed:                      │
// │   JA  — acima (>)          JG  — maior (>)             │
// │   JAE — acima/igual (>=)   JGE — maior/igual (>=)      │
// │   JB  — abaixo (<)         JL  — menor (<)             │
// │   JBE — abaixo/igual (<=)  JLE — menor/igual (<=)      │
// │   JE  — igual   (ZF=1)                                  │
// │   JNE — diferente (ZF=0)                                │
// │   JZ  — zero (alias JE)                                 │
// │   JS  — negativo (SF=1)                                 │
// │   JO  — overflow (OF=1)                                 │
// │   JC  — carry (CF=1)                                    │
// │                                                         │
// │  CMOV — conditional move (sem branch, sem pipeline stall)│
// │  SETcc — set byte on condition                          │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <fmt/core.h>

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// ── Demo 1: JMP incondicional ─────────────────────────────────
void demo_jmp() {
    section("5.1 JMP — salto incondicional");

    int resultado = 0;
    asm volatile (
        "jmp .fim_jmp%=\n\t"       // pula para .fim_jmp
        "movl $999, %0\n\t"        // ← NUNCA executado
        ".fim_jmp%=:\n\t"
        "movl $42, %0"             // executado após o salto
        : "=r"(resultado)
    );
    fmt::print("  JMP pulou sobre 'movl $999' → resultado = {}\n", resultado);
    fmt::print("  (Labels com %= são únicos por instância de asm)\n");
}

// ── Demo 2: Jumps condicionais signed vs unsigned ─────────────
void demo_conditional_jumps() {
    section("5.2 Jumps condicionais: JL vs JB (signed vs unsigned)");

    // -1 como int32 = 0xFFFFFFFF
    // Como unsigned: 0xFFFFFFFF = 4294967295 (MAIOR que 5)
    // Como signed:   0xFFFFFFFF = -1          (MENOR que 5)

    int32_t  a = -1;
    uint32_t u = (uint32_t)-1;
    int32_t  b = 5;

    int signed_result, unsigned_result;

    // JL: signed less than (usa SF != OF)
    asm volatile (
        "movl %2, %%eax\n\t"
        "cmpl %3, %%eax\n\t"
        "jl .yes_signed%=\n\t"
        "movl $0, %0\n\t"
        "jmp .end_signed%=\n\t"
        ".yes_signed%=:\n\t"
        "movl $1, %0\n\t"
        ".end_signed%=:"
        : "=r"(signed_result)
        : "0"(0), "r"(a), "r"(b)
        : "%eax"
    );

    // JB: unsigned below (usa CF)
    asm volatile (
        "movl %2, %%eax\n\t"
        "cmpl %3, %%eax\n\t"
        "jb .yes_unsigned%=\n\t"
        "movl $0, %0\n\t"
        "jmp .end_unsigned%=\n\t"
        ".yes_unsigned%=:\n\t"
        "movl $1, %0\n\t"
        ".end_unsigned%=:"
        : "=r"(unsigned_result)
        : "0"(0), "r"(u), "r"((uint32_t)b)
        : "%eax"
    );

    fmt::print("  -1 (0xFFFFFFFF) vs 5:\n");
    fmt::print("    JL  (signed  <): {} → -1 < 5 = {}\n",
               signed_result ? "SIM" : "NÃO", signed_result ? "verdadeiro" : "falso");
    fmt::print("    JB  (unsigned<): {} → 4294967295 < 5 = {}\n",
               unsigned_result ? "SIM" : "NÃO", unsigned_result ? "verdadeiro" : "falso");
    fmt::print("  ↑ Mesmos bits, interpretações OPOSTAS!\n");
}

// ── Demo 3: CMOV — conditional move (branchless) ─────────────
void demo_cmov() {
    section("5.3 CMOV — conditional move (sem branch)");
    //
    // Jumps condicionais causam "branch misprediction":
    //   se a CPU apostou no caminho errado, desperdiça ~15 ciclos
    //   para esvaziar e recarregar o pipeline.
    //
    // CMOV executa um MOV condicional SEM branch:
    //   cmovl src, dst  → if (SF!=OF): dst = src
    //   (a instrução sempre executa, mas só escreve se condição verdadeira)

    // max(a, b) com branch tradicional vs CMOV:
    auto max_branch = [](int a, int b) -> int {
        int r;
        asm volatile (
            "movl %1, %0\n\t"
            "cmpl %2, %1\n\t"
            "jge .skip_branch%=\n\t"  // if a >= b: pula
            "movl %2, %0\n\t"         // r = b
            ".skip_branch%=:"
            : "=r"(r) : "r"(a), "r"(b)
        );
        return r;
    };

    auto max_cmov = [](int a, int b) -> int {
        int r;
        asm volatile (
            "movl %1, %0\n\t"        // r = a
            "cmpl %2, %0\n\t"        // compara a com b
            "cmovll %2, %0"          // se a < b: r = b (cmovl = "move if less")
            : "=r"(r) : "r"(a), "r"(b)
        );
        return r;
    };

    struct Caso { int a, b; };
    for (auto [a, b] : std::initializer_list<Caso>{{3,7},{7,3},{5,5},{-1,1}}) {
        fmt::print("  max({:3},{:3}): branch={}, cmov={}\n",
                   a, b, max_branch(a,b), max_cmov(a,b));
    }

    // abs(n) branchless com CMOV
    auto abs_cmov = [](int n) -> int {
        int r, neg;
        asm volatile (
            "movl %1, %0\n\t"        // r = n
            "movl %1, %2\n\t"
            "negl %2\n\t"            // neg = -n
            "testl %1, %1\n\t"       // SF = (n < 0)?
            "cmovsl %2, %0"          // se negativo: r = -n
            : "=r"(r), "=r"(neg)
            : "1"(n), "2"(0)
        );
        return r;
    };
    fmt::print("\n  abs(-7) = {}, abs(7) = {}\n", abs_cmov(-7), abs_cmov(7));
}

// ── Demo 4: SETcc — byte condicional ─────────────────────────
void demo_setcc() {
    section("5.4 SETcc — produz 0 ou 1 baseado em flags");
    //
    // SETcc escreve 0 ou 1 (byte) no operando, baseado em uma condição.
    // Muito usado para converter comparações em valores booleanos
    // sem branch. O compilador C++ usa isso para (a < b) em ints.

    auto is_equal = [](int a, int b) -> uint8_t {
        uint8_t result;
        asm volatile (
            "cmpl %2, %1\n\t"
            "sete %0"            // result = (ZF==1) ? 1 : 0
            : "=r"(result) : "r"(a), "r"(b)
        );
        return result;
    };

    auto is_less = [](int a, int b) -> uint8_t {
        uint8_t result;
        asm volatile (
            "cmpl %2, %1\n\t"
            "setl %0"            // result = (SF!=OF) ? 1 : 0
            : "=r"(result) : "r"(a), "r"(b)
        );
        return result;
    };

    fmt::print("  sete: 3==3={}, 3==4={}\n", is_equal(3,3), is_equal(3,4));
    fmt::print("  setl: 3<5={},  5<3={}\n",  is_less(3,5),  is_less(5,3));

    // Tabela completa de SETcc:
    int x = 3, y = 5;
    uint8_t r;
    fmt::print("\n  Comparando {} vs {}:\n", x, y);
    asm volatile ("cmpl %1,%0; sete  %%al; movzbl %%al,%0" : "=r"(r) : "r"(x),"r"(y) : "%al"); fmt::print("    sete  (==): {}\n", r);
    asm volatile ("cmpl %1,%0; setne %%al; movzbl %%al,%0" : "=r"(r) : "r"(x),"r"(y) : "%al"); fmt::print("    setne (!=): {}\n", r);
    asm volatile ("cmpl %1,%0; setl  %%al; movzbl %%al,%0" : "=r"(r) : "r"(x),"r"(y) : "%al"); fmt::print("    setl  (<) : {}\n", r);
    asm volatile ("cmpl %1,%0; setle %%al; movzbl %%al,%0" : "=r"(r) : "r"(x),"r"(y) : "%al"); fmt::print("    setle (<=): {}\n", r);
    asm volatile ("cmpl %1,%0; setg  %%al; movzbl %%al,%0" : "=r"(r) : "r"(x),"r"(y) : "%al"); fmt::print("    setg  (>) : {}\n", r);
    asm volatile ("cmpl %1,%0; setge %%al; movzbl %%al,%0" : "=r"(r) : "r"(x),"r"(y) : "%al"); fmt::print("    setge (>=): {}\n", r);
}

// ── Demo 5: Loop com LOOP instruction ────────────────────────
void demo_loop_instr() {
    section("5.5 LOOP — instrução de loop usando RCX");
    //
    // LOOP label: decrementa RCX, salta se RCX != 0
    // Equivalente a: dec rcx; jnz label
    // NÃO afeta flags (diferente de DEC+JNZ)

    int64_t soma = 0;
    int64_t n = 10;

    asm volatile (
        "movq %2, %%rcx\n\t"     // rcx = n (contador)
        ".loop_instr%=:\n\t"
        "  addq %%rcx, %0\n\t"   // soma += rcx
        "  loop .loop_instr%="   // rcx--; if rcx!=0 goto loop
        : "+r"(soma)
        : "0"(soma), "r"(n)
        : "%rcx"
    );
    fmt::print("  LOOP soma 1..{} = {} (esperado: {})\n", n, soma, n*(n+1)/2);
    fmt::print("  ⚠ LOOP é mais lento que DEC+JNZ em CPUs modernas\n");
    fmt::print("    (pipeline não pode prever o término facilmente)\n");
}

// ── Demo 6: Switch via tabela de saltos ───────────────────────
void demo_jump_table() {
    section("5.6 Tabela de saltos — implementação de switch");
    //
    // switch com muitos casos densos → compilador gera:
    //   jmp *table(,%rax,8)   ← indirect jump por tabela
    //
    // Aqui demonstramos manualmente como funciona:

    auto dispatch = [](int op, int a, int b) -> int {
        int result;
        asm volatile (
            "cmpl $0, %1\n\t"
            "je .op_add%=\n\t"
            "cmpl $1, %1\n\t"
            "je .op_sub%=\n\t"
            "cmpl $2, %1\n\t"
            "je .op_mul%=\n\t"
            "movl $-1, %0\n\t"       // default: erro
            "jmp .op_end%=\n\t"
            ".op_add%=: addl %3, %2; movl %2, %0; jmp .op_end%=\n\t"
            ".op_sub%=: subl %3, %2; movl %2, %0; jmp .op_end%=\n\t"
            ".op_mul%=: imull %3, %2; movl %2, %0\n\t"
            ".op_end%=:"
            : "=r"(result)
            : "r"(op), "r"(a), "r"(b)
        );
        return result;
    };

    fmt::print("  dispatch(ADD, 10, 3) = {}\n", dispatch(0, 10, 3));
    fmt::print("  dispatch(SUB, 10, 3) = {}\n", dispatch(1, 10, 3));
    fmt::print("  dispatch(MUL, 10, 3) = {}\n", dispatch(2, 10, 3));
    fmt::print("  dispatch(???:  3, 3) = {}\n", dispatch(9, 10, 3));
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 05 · Controle de Fluxo         ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_jmp();
    demo_conditional_jumps();
    demo_cmov();
    demo_setcc();
    demo_loop_instr();
    demo_jump_table();

    fmt::print("\n\033[1;32m✓ Módulo 05 completo\033[0m\n\n");
}
