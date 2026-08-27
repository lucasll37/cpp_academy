// ============================================================
// src/asm_course/13_optimization.cpp
//
// MÓDULO 13 — Otimizações de Baixo Nível
//
// ┌─────────────────────────────────────────────────────────┐
// │  Por que assembly manual ainda importa em 2025?         │
// │                                                         │
// │  1. O compilador não conhece o contexto do problema     │
// │  2. Instruções específicas (POPCNT, LZCNT, FMA) que     │
// │     o compilador pode não emitir                        │
// │  3. Controle de alinhamento e prefetch                  │
// │  4. Eliminar branch mispredictions                      │
// │  5. Unrolling manual para loops críticos                │
// │  6. Entender o que o compilador gera (e por quê)        │
// └─────────────────────────────────────────────────────────┘
// ============================================================

#include <cstdint>
#include <ctime>
#include <array>
#include <fmt/core.h>

extern "C" {
    int64_t asm_sum_unrolled(const int32_t* arr, int64_t n);
    int64_t asm_sum_simd(const int32_t* arr, int64_t n);
    void    asm_prefetch_copy(const int64_t* src, int64_t* dst, int64_t n);
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}

// Mede ciclos de CPU com RDTSC
static uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// ── Demo 1: Loop unrolling ────────────────────────────────────
void demo_unrolling() {
    section("13.1 Loop Unrolling — reduz overhead do loop");
    //
    // Um loop simples tem overhead:
    //   - incremento do contador
    //   - comparação com limite
    //   - salto condicional
    //
    // Unrolling: processa N elementos por iteração → menos saltos
    // O compilador faz isso com -O2, mas às vezes precisa de ajuda.

    static const int N = 1024;
    alignas(64) int32_t arr[N];
    for (int i = 0; i < N; ++i) arr[i] = i;

    // Loop simples
    auto loop_simples = [&]() -> int64_t {
        int64_t soma = 0;
        asm volatile (
            "xorq %0, %0\n\t"
            "xorl %%ecx, %%ecx\n\t"
            ".Lloop_simples%=:\n\t"
            "  movslq (%2,%%rcx,4), %%rax\n\t"
            "  addq %%rax, %0\n\t"
            "  incl %%ecx\n\t"
            "  cmpl %3, %%ecx\n\t"
            "  jl .Lloop_simples%="
            : "+r"(soma)
            : "0"(0LL), "r"(arr), "r"(N)
            : "%rax", "%rcx", "cc"
        );
        return soma;
    };

    // Loop unrolled (4×)
    int64_t r_simples = loop_simples();
    int64_t r_unrolled = asm_sum_unrolled(arr, N);
    int64_t r_simd     = asm_sum_simd(arr, N);

    fmt::print("  N={}, soma esperada: {}\n", N, (int64_t)N*(N-1)/2);
    fmt::print("  loop simples:  {}\n", r_simples);
    fmt::print("  loop unrolled: {}\n", r_unrolled);
    fmt::print("  loop SIMD:     {}\n", r_simd);

    // Benchmark básico com RDTSC
    auto bench = [&](auto fn, const char* label) {
        uint64_t t0 = rdtsc();
        volatile int64_t r = 0;
        for (int rep = 0; rep < 10000; ++rep) r += fn();
        uint64_t t1 = rdtsc();
        fmt::print("  {:15}: {:8} ciclos/10k iterações\n", label, t1-t0);
        return r;
    };

    bench(loop_simples,    "simples");
    bench([&]{ return asm_sum_unrolled(arr,N); }, "unrolled(4x)");
    bench([&]{ return asm_sum_simd(arr,N);     }, "SIMD(SSE2)");
}

// ── Demo 2: Branchless tricks ─────────────────────────────────
void demo_branchless() {
    section("13.2 Código branchless — elimina mispredictions");

    // min/max branchless (já vimos CMOV, aqui vemos alternativas)
    auto clamp_branchy = [](int v, int lo, int hi) -> int {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    };

    auto clamp_branchless = [](int v, int lo, int hi) -> int {
        int r;
        asm volatile (
            "movl %1, %0\n\t"        // r = v
            "cmpl %2, %0\n\t"        // r < lo?
            "cmovll %2, %0\n\t"      // se sim: r = lo
            "cmpl %3, %0\n\t"        // r > hi?
            "cmovgl %3, %0"          // se sim: r = hi
            : "=r"(r)
            : "r"(v), "r"(lo), "r"(hi)
        );
        return r;
    };

    fmt::print("  clamp(5, 0, 10) branchy={} branchless={}\n",
               clamp_branchy(5,0,10), clamp_branchless(5,0,10));
    fmt::print("  clamp(-3, 0, 10) branchy={} branchless={}\n",
               clamp_branchy(-3,0,10), clamp_branchless(-3,0,10));
    fmt::print("  clamp(15, 0, 10) branchy={} branchless={}\n",
               clamp_branchy(15,0,10), clamp_branchless(15,0,10));

    // Seleção condicional sem branch
    auto select_branchless = [](int cond, int a, int b) -> int {
        int r;
        asm volatile (
            "testl %1, %1\n\t"       // cond == 0?
            "movl %2, %0\n\t"        // r = a
            "cmovzl %3, %0"          // se cond==0: r = b
            : "=r"(r)
            : "r"(cond), "r"(a), "r"(b)
        );
        return r;
    };
    fmt::print("  select(1, 100, 200) = {}  (esperado: 100)\n",
               select_branchless(1, 100, 200));
    fmt::print("  select(0, 100, 200) = {}  (esperado: 200)\n",
               select_branchless(0, 100, 200));
}

// ── Demo 3: Prefetch ──────────────────────────────────────────
void demo_prefetch() {
    section("13.3 PREFETCHNTA — pré-carrega dados no cache");
    //
    // Latência de memória:
    //   L1 cache: ~4 ciclos
    //   L2 cache: ~12 ciclos
    //   L3 cache: ~40 ciclos
    //   RAM:      ~200 ciclos
    //
    // PREFETCH instrui a CPU a carregar a linha de cache antecipadamente.
    // Enquanto processamos dado[i], pedimos ao hardware que carregue dado[i+N].

    static const int N = 4096;
    alignas(64) int64_t src[N], dst[N];
    for (int i = 0; i < N; ++i) src[i] = i * i;

    uint64_t t0 = rdtsc();
    for (int i = 0; i < N; ++i) dst[i] = src[i];
    uint64_t t1 = rdtsc();

    uint64_t t2 = rdtsc();
    asm_prefetch_copy(src, dst, N);
    uint64_t t3 = rdtsc();

    fmt::print("  Cópia simples:     {} ciclos\n", t1-t0);
    fmt::print("  Cópia c/ prefetch: {} ciclos\n", t3-t2);
    fmt::print("  (diferença depende do cache state — re-execute para ver variação)\n");
}

// ── Demo 4: Lendo o que o compilador gera ────────────────────
void demo_compiler_output() {
    section("13.4 Como ver o assembly gerado pelo compilador");

    fmt::print("  Para ver o assembly do seu binário:\n\n");
    fmt::print("  objdump -d -M intel build/src/asm_course/asm_13_optimization\n\n");
    fmt::print("  Para ver o assembly por função:\n");
    fmt::print("  objdump -d -M intel build/... | grep -A 30 '<nome_funcao>'\n\n");
    fmt::print("  Para compilar e ver assembly diretamente:\n");
    fmt::print("  g++ -O2 -S -masm=intel arquivo.cpp -o arquivo.s\n\n");
    fmt::print("  Ferramenta online: https://godbolt.org (Compiler Explorer)\n\n");

    // Auto-disassembly rudimentar via /proc/self/maps
    fmt::print("  Endereço desta função: {:p}\n", (void*)demo_compiler_output);
    fmt::print("  Primeiros bytes: ");
    auto* ptr = reinterpret_cast<uint8_t*>(demo_compiler_output);
    for (int i = 0; i < 16; ++i)
        fmt::print("{:02X} ", ptr[i]);
    fmt::print("\n  (prólogo típico: 55=push rbp, 48 89 E5=mov rbp,rsp)\n");
}

// ── Demo 5: Alinhamento de código ────────────────────────────
void demo_code_alignment() {
    section("13.5 Alinhamento de código — .p2align");
    //
    // CPUs buscam código em blocos de 16/32/64 bytes.
    // Se um loop começa no meio de um bloco, a primeira iteração
    // desperdiça ciclos esperando o fetch completo.
    //
    // .p2align 4  → alinha o label ao próximo múltiplo de 2^4=16 bytes
    //
    // Em asm inline não temos controle, mas em .s podemos:
    //   .p2align 4       # alinha a 16 bytes (ótimo para loops SSE)
    //   .p2align 5       # alinha a 32 bytes (ótimo para loops AVX)
    // .loop:
    //   ...

    fmt::print("  Regras de alinhamento de código:\n");
    fmt::print("    Funções:   .p2align 4  (16 bytes) — padrão GCC -O2\n");
    fmt::print("    Loops SSE: .p2align 4  (16 bytes)\n");
    fmt::print("    Loops AVX: .p2align 5  (32 bytes)\n");
    fmt::print("    NOP padding: GCC insere NOPs para preencher\n");
    fmt::print("    Multi-byte NOP: 66 90 (melhor que 2× 90 90)\n");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 13 · Otimizações               ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    demo_unrolling();
    demo_branchless();
    demo_prefetch();
    demo_compiler_output();
    demo_code_alignment();

    fmt::print("\n\033[1;32m✓ Módulo 13 completo\033[0m\n\n");
}
