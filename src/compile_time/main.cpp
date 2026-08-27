// =============================================================================
//  src/compile_time/main.cpp
//  Ponto de entrada — computação em tempo de compilação
//
//  ⚠ Compila com C++23: `if consteval` (demo_consteval) exige esse padrão.
// =============================================================================
#include "demo_constexpr.hpp"
#include "demo_consteval.hpp"
#include "demo_static_assert.hpp"
#include "demo_nttp.hpp"
#include "demo_tabelas.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::dark_orange) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · constexpr — permissão, não garantia");
    demo_constexpr::run();

    section("2 · consteval e constinit — as garantias");
    demo_consteval::run();

    section("3 · static_assert e type_traits");
    demo_static_assert::run();

    section("4 · NTTP — templates com valores");
    demo_nttp::run();

    section("5 · Gerando dados na compilação");
    demo_tabelas::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de compile-time concluídos!\n\n");
}
