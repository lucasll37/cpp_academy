// =============================================================================
//  src/concepts/main.cpp
//  Ponto de entrada — executa todos os demos de concepts em sequência
// =============================================================================
#include "demo_basics.hpp"
#include "demo_requires.hpp"
#include "demo_constraints.hpp"
#include "demo_real_world.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::cornflower_blue) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<52}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    fmt::print(fmt::fg(fmt::color::cornflower_blue) | fmt::emphasis::bold,
               "\n  C++20 Concepts — Constraints & Requirements\n");

    section("1 · Fundamentos — concept, requires, stdlib concepts");
    demo_basics::run();

    section("2 · Requires Expressions — simple, type, compound, nested");
    demo_requires::run();

    section("3 · Constraints Avancadas — subsumption, classes, lambdas");
    demo_constraints::run();

    section("4 · Casos Reais — algoritmos, policies, contratos");
    demo_real_world::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de concepts concluidos!\n\n");
}
