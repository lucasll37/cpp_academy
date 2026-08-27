// =============================================================================
//  src/chrono/main.cpp
//  Ponto de entrada — tempo, datas e medição
//
//  ⚠ Compila com C++23. O calendário do C++20 é usado via `operator<<` da
//    libstdc++, porque a fmt 10 (a versão deste repositório) ainda não traz
//    formatters para `year_month_day` e companhia — isso chegou na fmt 11.
// =============================================================================
#include "demo_duration.hpp"
#include "demo_clocks.hpp"
#include "demo_time_point.hpp"
#include "demo_calendario.hpp"
#include "demo_medicao.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::sky_blue) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · duration — unidades de tempo no tipo");
    demo_duration::run();

    section("2 · Os relógios e suas garantias");
    demo_clocks::run();

    section("3 · time_point — instantes");
    demo_time_point::run();

    section("4 · O calendário do C++20");
    demo_calendario::run();

    section("5 · Como cronometrar sem se enganar");
    demo_medicao::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de chrono concluídos!\n\n");
}
