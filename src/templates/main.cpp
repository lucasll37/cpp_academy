// =============================================================================
//  src/templates/main.cpp
//  Ponto de entrada — executa todos os demos de templates em sequência
// =============================================================================
#include "demo_basics.hpp"
#include "demo_specialization.hpp"
#include "demo_variadic.hpp"
#include "demo_sfinae.hpp"
#include "demo_type_traits.hpp"
#include "demo_crtp.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::medium_purple) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<52}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Fundamentos — function & class templates");
    demo_basics::run();

    section("2 · Specialization — full & partial");
    demo_specialization::run();

    section("3 · Variadic Templates & Parameter Packs");
    demo_variadic::run();

    section("4 · SFINAE & enable_if");
    demo_sfinae::run();

    section("5 · type_traits & if constexpr");
    demo_type_traits::run();

    section("6 · CRTP — Curiously Recurring Template Pattern");
    demo_crtp::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de templates concluídos!\n\n");
}
