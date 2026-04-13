// ============================================================
// src/outline_asm/main.cpp
//
// Chama funções implementadas em asm_funcs.s e verifica
// que os resultados batem com a implementação C++ equivalente.
//
// Do ponto de vista do compilador, asm_funcs.s.o é uma
// biblioteca de objetos como qualquer outra — ele só vê as
// declarações extern "C" do header.
// ============================================================

#include <cstdint>
#include <cstring>
#include <cassert>
#include <vector>
#include <fmt/core.h>
#include "asm_funcs.hpp"

static void section(const char* t) {
    fmt::print("\n\033[1;36m── {} ──\033[0m\n", t);
}

static void ok(bool cond, const char* expr) {
    if (cond)
        fmt::print("  \033[32m✓\033[0m  {}\n", expr);
    else
        fmt::print("  \033[31m✗\033[0m  FALHOU: {}\n", expr);
}

// ============================================================
int main() {
    fmt::print("\033[1;33m");
    fmt::print("╔══════════════════════════════════════════╗\n");
    fmt::print("║  outline_asm — funções .s linkadas ao C++║\n");
    fmt::print("╚══════════════════════════════════════════╝\n");
    fmt::print("\033[0m");

    // ── 1. soma int32 ─────────────────────────────────────
    section("1. asm_soma_int");
    {
        int r = asm_soma_int(17, 25);
        fmt::print("  asm_soma_int(17, 25) = {}\n", r);
        ok(r == 42, "17 + 25 == 42");

        r = asm_soma_int(-10, 3);
        fmt::print("  asm_soma_int(-10, 3) = {}\n", r);
        ok(r == -7, "-10 + 3 == -7");
    }

    // ── 2. soma int64 ─────────────────────────────────────
    section("2. asm_soma_int64");
    {
        int64_t r = asm_soma_int64(1'000'000'000LL, 2'000'000'000LL);
        fmt::print("  asm_soma_int64(1e9, 2e9) = {}\n", r);
        ok(r == 3'000'000'000LL, "resultado == 3.000.000.000");

        // overflow de int32 mas não de int64
        r = asm_soma_int64(2'147'483'647LL, 1LL);
        fmt::print("  INT32_MAX + 1 = {}\n", r);
        ok(r == 2'147'483'648LL, "sem overflow em int64");
    }

    // ── 3. valor absoluto ─────────────────────────────────
    section("3. asm_valor_absoluto");
    {
        for (int n : {0, 1, -1, 42, -42, -2147483647}) {
            int r = asm_valor_absoluto(n);
            int esperado = n < 0 ? -n : n;
            fmt::print("  |{}| = {}\n", n, r);
            ok(r == esperado, "resultado correto");
        }
    }

    // ── 4. fatorial ───────────────────────────────────────
    section("4. asm_fatorial");
    {
        // Referência C++
        auto fat_cpp = [](int n) -> int64_t {
            int64_t r = 1;
            for (int i = 2; i <= n; ++i) r *= i;
            return r;
        };
        for (int n : {0, 1, 2, 5, 10, 15, 20}) {
            int64_t r = asm_fatorial(n);
            fmt::print("  {}! = {}\n", n, r);
            ok(r == fat_cpp(n), "igual à referência C++");
        }
    }

    // ── 5. máximo branchless ──────────────────────────────
    section("5. asm_maximo (branchless CMOV)");
    {
        struct Caso { int a, b, esperado; };
        for (auto [a, b, esp] : std::vector<Caso>{
            {3, 7, 7}, {7, 3, 7}, {0, 0, 0}, {-5, -3, -3}, {-1, 1, 1}
        }) {
            int r = asm_maximo(a, b);
            fmt::print("  max({}, {}) = {}\n", a, b, r);
            ok(r == esp, "resultado correto");
        }
    }

    // ── 6. popcount ───────────────────────────────────────
    section("6. asm_popcount (instrução POPCNT)");
    {
        struct Caso { uint32_t x; int bits; };
        for (auto [x, bits] : std::vector<Caso>{
            {0u, 0}, {1u, 1}, {0xFFFFFFFFu, 32},
            {0b10101010u, 4}, {255u, 8}
        }) {
            int r = asm_popcount(x);
            fmt::print("  popcount(0x{:08X}) = {}\n", x, r);
            ok(r == bits, "contagem correta");
        }
    }

    // ── 7. strlen ─────────────────────────────────────────
    section("7. asm_strlen (REPNE SCASB)");
    {
        for (const char* s : {"", "a", "hello", "cpp_academy"}) {
            int64_t r   = asm_strlen(s);
            int64_t ref = (int64_t)strlen(s);
            fmt::print("  strlen(\"{}\") = {}\n", s, r);
            ok(r == ref, "igual a strlen() da libc");
        }
    }

    // ── 8. memcpy_64 ──────────────────────────────────────
    section("8. asm_memcpy_64 (REP MOVSQ)");
    {
        uint64_t src[4] = {0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xABCD1234};
        uint64_t dst[4] = {};
        asm_memcpy_64(dst, src, 4);

        bool ok_all = true;
        for (int i = 0; i < 4; ++i)
            ok_all = ok_all && (dst[i] == src[i]);

        fmt::print("  src → dst:\n");
        for (int i = 0; i < 4; ++i)
            fmt::print("    [{}] 0x{:016X} == 0x{:016X}  {}\n",
                       i, src[i], dst[i], src[i]==dst[i]?"✓":"✗");
        ok(ok_all, "todos os qwords copiados corretamente");
    }

    // ── 9. soma array ─────────────────────────────────────
    section("9. asm_soma_array");
    {
        int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        int n = sizeof(arr) / sizeof(arr[0]);
        int64_t r = asm_soma_array(arr, n);
        fmt::print("  soma(1..10) = {}\n", r);
        ok(r == 55, "soma == 55");

        // array vazio
        r = asm_soma_array(arr, 0);
        ok(r == 0, "array vazio → 0");

        // array com negativos
        int arr2[] = {-3, -2, -1, 0, 1, 2, 3};
        r = asm_soma_array(arr2, 7);
        fmt::print("  soma(-3..3) = {}\n", r);
        ok(r == 0, "soma simétrica == 0");
    }

    // ── 10. primo ─────────────────────────────────────────
    section("10. asm_eh_primo (preserva %rbx)");
    {
        // Referência C++
        auto primo_cpp = [](int n) -> bool {
            if (n < 2) return false;
            if (n == 2) return true;
            if (n % 2 == 0) return false;
            for (int i = 3; i * i <= n; i += 2)
                if (n % i == 0) return false;
            return true;
        };

        for (int n : {0, 1, 2, 3, 4, 5, 17, 18, 97, 100, 7919}) {
            int r = asm_eh_primo(n);
            bool esp = primo_cpp(n);
            fmt::print("  {} → {}\n", n, r ? "primo" : "não primo");
            ok((bool)r == esp, "igual à referência C++");
        }
    }

    fmt::print("\n\033[1;32m✓ Todas as demos concluídas\033[0m\n\n");
    return 0;
}
