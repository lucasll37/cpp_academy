// =============================================================================
//  src/operators/main.cpp
//  Ponto de entrada — sobrecarga de operadores
//
//  ⚠ Compila com C++23: subscript multidimensional e `static operator()`
//    (demo_chamada) exigem esse padrão.
// =============================================================================
#include "demo_aritmeticos.hpp"
#include "demo_comparacao.hpp"
#include "demo_conversao.hpp"
#include "demo_chamada.hpp"
#include "demo_literais.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::orchid) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Aritméticos — a forma canônica");
    demo_aritmeticos::run();

    section("2 · operator<=> — o spaceship");
    demo_comparacao::run();

    section("3 · Conversões — explicit e operator T()");
    demo_conversao::run();

    section("4 · operator(), operator[] e operator->");
    demo_chamada::run();

    section("5 · Literais definidos pelo usuário");
    demo_literais::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de operadores concluídos!\n\n");
}
