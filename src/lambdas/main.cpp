// =============================================================================
//  src/lambdas/main.cpp
//  Ponto de entrada — lambdas e objetos chamáveis
//
//  ⚠ Compila com C++23 (lambdas sem captura como argumento de template em
//    contexto não avaliado). O `deducing this` NÃO está disponível no GCC 13 —
//    ver demo_padroes.hpp, seção 5.3.
// =============================================================================
#include "demo_capturas.hpp"
#include "demo_genericas.hpp"
#include "demo_function.hpp"
#include "demo_invoke.hpp"
#include "demo_padroes.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::turquoise) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Capturas — o que a lambda leva consigo");
    demo_capturas::run();

    section("2 · Lambdas genéricas");
    demo_genericas::run();

    section("3 · std::function e o type erasure");
    demo_function::run();

    section("4 · std::invoke e ponteiros para membro");
    demo_invoke::run();

    section("5 · Padrões que as lambdas destravaram");
    demo_padroes::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de lambdas concluídos!\n\n");
}
