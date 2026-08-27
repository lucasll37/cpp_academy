// =============================================================================
//  src/ranges/main.cpp
//  Ponto de entrada — executa todos os demos de Ranges (C++20)
// =============================================================================
#include "demo_views.hpp"
#include "demo_pipelines.hpp"
#include "demo_algorithms.hpp"
#include "demo_custom_range.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::medium_spring_green) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Views — preguiçosas e sem posse");
    demo_views::run();

    section("2 · Pipelines, composição e tempo de vida");
    demo_pipelines::run();

    section("3 · ranges::* e as projeções");
    demo_ranges_algorithms::run();

    section("4 · Escrevendo os seus próprios ranges");
    demo_custom_range::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de Ranges concluídos!\n\n");
}
