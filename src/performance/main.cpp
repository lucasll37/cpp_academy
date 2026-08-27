// =============================================================================
//  src/performance/main.cpp
//  Ponto de entrada — desempenho: cache, layout e medição
//
//  ⚠ Este executável MEDE. Ele leva alguns segundos e aloca algumas centenas
//    de MB — os conjuntos de dados precisam ser maiores que o cache L3 para
//    que os números signifiquem alguma coisa.
//
//  ⚠ Todos os números impressos são desta máquina, agora. Eles variam com o
//    processador, a carga e o compilador. O que NÃO varia é a ordem de
//    grandeza entre os casos comparados — e é isso que se deve levar.
//
//  Compile sempre com -O2 ou mais: a seção 4.1 mostra um efeito que aparece
//  em -O0 e desaparece em -O1.
// =============================================================================
#include "demo_cache.hpp"
#include "demo_aos_soa.hpp"
#include "demo_false_sharing.hpp"
#include "demo_branch.hpp"
#include "demo_metodo.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::dark_turquoise) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    fmt::print(fmt::fg(fmt::color::gray),
               "\n(este subprojeto MEDE — leva alguns segundos e aloca "
               "algumas centenas de MB)\n");

    section("1 · Localidade de cache");
    demo_cache::run();

    section("2 · AoS vs SoA");
    demo_aos_soa::run();

    section("3 · False sharing");
    demo_false_sharing::run();

    section("4 · Predição de desvio");
    demo_branch::run();

    section("5 · Como medir sem se enganar");
    demo_metodo::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de performance concluídos!\n\n");
    return 0;
}
