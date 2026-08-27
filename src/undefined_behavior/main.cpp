// =============================================================================
//  src/undefined_behavior/main.cpp
//  Ponto de entrada — comportamento indefinido e como se defender
//
//  ⚠ ESTE executável NÃO executa UB. Ele explica, e todas as afirmações sobre
//    o que cada sanitizer pega vieram de rodar ub_exemplos.cpp — um alvo
//    SEPARADO, não instalado, que contém UB de propósito e só dispara um caso
//    por vez, com argumento explícito.
//
//        g++ -O0 -g -fsanitize=address,undefined
//            src/undefined_behavior/ub_exemplos.cpp -o /tmp/ub
//        /tmp/ub          # lista os 12 casos
//        /tmp/ub uaf      # roda um
// =============================================================================
#include "demo_catalogo.hpp"
#include "demo_otimizador.hpp"
#include "demo_sanitizers.hpp"
#include "demo_aliasing.hpp"
#include "demo_defesas.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::orange_red) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · O que é comportamento indefinido");
    demo_catalogo::run();

    section("2 · Como o UB licencia o otimizador");
    demo_otimizador::run();

    section("3 · Os sanitizers, medidos");
    demo_sanitizers::run();

    section("4 · Strict aliasing");
    demo_aliasing::run();

    section("5 · As camadas de defesa");
    demo_defesas::run();

    fmt::print(fmt::fg(fmt::color::gold) | fmt::emphasis::bold,
               "\n→ Os exemplos EXECUTÁVEIS de UB estão em ub_exemplos.cpp:\n"
               "    g++ -O0 -g -fsanitize=address,undefined \\\n"
               "        src/undefined_behavior/ub_exemplos.cpp -o /tmp/ub\n"
               "    /tmp/ub uaf\n");

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de undefined_behavior concluídos!\n\n");
    return 0;
}
