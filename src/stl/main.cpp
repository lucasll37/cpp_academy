// =============================================================================
//  src/stl/main.cpp
//  Ponto de entrada — executa todos os demos da biblioteca padrão
// =============================================================================
#include "demo_containers.hpp"
#include "demo_iterators.hpp"
#include "demo_algorithms.hpp"
#include "demo_custom_container.hpp"
#include "demo_pitfalls.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::steel_blue) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Containers — escolher pela complexidade");
    demo_containers::run();

    section("2 · Iteradores — categorias e sentinelas");
    demo_iterators::run();

    section("3 · <algorithm> — o vocabulário sem loops crus");
    demo_algorithms::run();

    section("4 · Um container compatível com a STL");
    demo_custom_container::run();

    section("5 · Armadilhas — compila, roda, e está errado");
    demo_pitfalls::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos da STL concluídos!\n\n");
}
