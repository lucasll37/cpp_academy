// =============================================================================
//  src/casts/main.cpp
//  Ponto de entrada — executa todos os demos de cast em sequência
// =============================================================================
#include "demo_static_cast.hpp"
#include "demo_dynamic_cast.hpp"
#include "demo_const_cast.hpp"
#include "demo_reinterpret_cast.hpp"
#include "demo_c_style.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::dodger_blue) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════╗\n"
               "║  {:<48}  ║\n"
               "╚══════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · static_cast");
    demo_static_cast::run();

    section("2 · dynamic_cast");
    demo_dynamic_cast::run();

    section("3 · const_cast");
    demo_const_cast::run();

    section("4 · reinterpret_cast");
    demo_reinterpret_cast::run();

    section("5 · C-style cast (e por que evitar)");
    demo_c_style::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de cast concluídos!\n\n");
}
