// ============================================================
// src/asm_course/15_capstone.cpp
//
// MÓDULO 15 — Projeto Final: Funções de Produção em Assembly
//
// Implementações completas e corretas de:
//   1. asm_memcpy  — cópia eficiente com SIMD + alinhamento
//   2. asm_strlen  — comprimento de string com SSE4.2
//   3. asm_memset  — preenchimento com REP STOSQ
//   4. asm_memcmp  — comparação de memória
//   5. asm_isort   — insertion sort (demonstra acesso a array)
//
// Cada função é verificada contra a implementação da libc.
// ============================================================

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <algorithm>
#include <fmt/core.h>

extern "C" {
    void*   asm_memcpy (void* dst, const void* src, size_t n);
    void*   asm_memset (void* dst, int c, size_t n);
    int     asm_memcmp (const void* a, const void* b, size_t n);
    size_t  asm_strlen (const char* s);
    void    asm_isort  (int32_t* arr, int32_t n);
}

static void section(const char* t) {
    fmt::print("\n\033[1;33m━━━ {} ━━━\033[0m\n", t);
}
static void ok(bool b, const char* msg) {
    fmt::print("  {} {}\n", b ? "\033[32m✓\033[0m" : "\033[31m✗\033[0m", msg);
}

void test_memcpy() {
    section("15.1 asm_memcpy — cópia com SIMD");

    // Teste com vários tamanhos (inclui não-alinhado)
    for (int n : {0, 1, 3, 7, 15, 16, 17, 32, 63, 64, 127, 1024}) {
        std::vector<uint8_t> src(n), dst(n, 0xFF);
        for (int i = 0; i < n; ++i) src[i] = (uint8_t)(i * 7 + 13);

        asm_memcpy(dst.data(), src.data(), n);
        bool igual = (memcmp(dst.data(), src.data(), n) == 0);
        ok(igual, fmt::format("n={:4}: conteúdo correto", n).c_str());
    }

    // Teste de endereço de retorno (deve retornar dst)
    char src2[8] = "ABCDEFG", dst2[8] = {};
    void* ret = asm_memcpy(dst2, src2, 8);
    ok(ret == dst2, "retorno == dst");
}

void test_memset() {
    section("15.2 asm_memset — preenchimento rápido");

    uint8_t buf[128];
    asm_memset(buf, 0xAB, sizeof(buf));
    bool ok_all = true;
    for (auto b : buf) ok_all &= (b == 0xAB);
    ok(ok_all, "todos os bytes == 0xAB");

    asm_memset(buf, 0, sizeof(buf));
    ok_all = true;
    for (auto b : buf) ok_all &= (b == 0);
    ok(ok_all, "zeragem completa");

    // Retorno deve ser dst
    void* ret = asm_memset(buf, 0, sizeof(buf));
    ok(ret == buf, "retorno == dst");
}

void test_memcmp() {
    section("15.3 asm_memcmp — comparação de memória");

    const char* a = "hello world";
    const char* b = "hello world";
    const char* c = "hello WORLD";

    ok(asm_memcmp(a, b, 11) == 0, "strings iguais → 0");
    ok(asm_memcmp(a, c, 11) != 0, "strings diferentes → != 0");
    ok(asm_memcmp(a, c, 5)  == 0, "prefixo igual → 0");

    // Sinal correto (como memcmp da libc)
    int r_asm  = asm_memcmp("abc", "abd", 3);
    int r_libc = memcmp("abc", "abd", 3);
    ok((r_asm < 0) == (r_libc < 0), "sinal correto (negativo se a < b)");
}

void test_strlen() {
    section("15.4 asm_strlen — comprimento de string");

    struct Caso { const char* s; size_t len; };
    for (auto [s, expected] : std::initializer_list<Caso>{
        {"",           0},
        {"a",          1},
        {"hello",      5},
        {"hello world",11},
        {"cpp_academy", 11},
    }) {
        size_t got = asm_strlen(s);
        ok(got == expected,
           fmt::format("strlen(\"{}\") = {} (esperado {})", s, got, expected).c_str());
    }
}

void test_isort() {
    section("15.5 asm_isort — insertion sort");

    // Pequeno array
    int32_t arr[] = {5, 3, 8, 1, 9, 2, 7, 4, 6};
    int n = sizeof(arr)/sizeof(arr[0]);
    asm_isort(arr, n);

    bool sorted = true;
    for (int i = 1; i < n; ++i)
        sorted &= (arr[i-1] <= arr[i]);
    ok(sorted, "array ordenado corretamente");

    fmt::print("  resultado: [");
    for (int i = 0; i < n; ++i)
        fmt::print("{}{}", arr[i], i<n-1?",":"");
    fmt::print("]\n");

    // Array grande aleatório
    std::vector<int32_t> big(256);
    for (auto& x : big) x = rand() % 1000;
    std::vector<int32_t> ref = big;

    asm_isort(big.data(), (int32_t)big.size());
    std::sort(ref.begin(), ref.end());

    ok(big == ref, "array grande: resultado igual ao std::sort");
}

int main() {
    fmt::print("\033[1;35m");
    fmt::print("╔══════════════════════════════════════════════════════╗\n");
    fmt::print("║  ASM COURSE · Módulo 15 · Projeto Final (Capstone)  ║\n");
    fmt::print("╚══════════════════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    test_memcpy();
    test_memset();
    test_memcmp();
    test_strlen();
    test_isort();

    fmt::print("\n\033[1;32m✓ Módulo 15 completo — curso finalizado!\033[0m\n\n");
}
