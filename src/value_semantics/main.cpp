// =============================================================================
//  src/value_semantics/main.cpp
//  Ponto de entrada — executa todos os demos de semântica de valor em sequência
// =============================================================================
#include "demo_value_categories.hpp"
#include "demo_rule_of_five.hpp"
#include "demo_copy_elision.hpp"
#include "demo_perfect_forwarding.hpp"
#include "demo_move_only.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::orange) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Categorias de valor — lvalue, prvalue, xvalue");
    demo_value_categories::run();

    section("2 · Rule of 0/3/5 — as operações especiais");
    demo_rule_of_five::run();

    section("3 · Copy elision — RVO, NRVO e o move ausente");
    demo_copy_elision::run();

    section("4 · Perfect forwarding — T&& + std::forward");
    demo_perfect_forwarding::run();

    section("5 · Move-only, ref-qualifiers e sink parameters");
    demo_move_only::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de semântica de valor concluídos!\n\n");
}
