// =============================================================================
//  src/smart-pointers/main.cpp
//  Ponto de entrada — executa todos os demos em sequência
// =============================================================================
#include "demo_unique.hpp"
#include "demo_shared.hpp"
#include "demo_weak.hpp"
#include "demo_patterns.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::gold) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════╗\n"
               "║  {:^40}  ║\n"
               "╚══════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · unique_ptr");
    demo_unique::run();

    section("2 · shared_ptr");
    demo_shared::run();

    section("3 · weak_ptr");
    demo_weak::run();

    section("4 · Padrões Modernos");
    demo_patterns::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos concluídos sem leaks!\n\n");
}
