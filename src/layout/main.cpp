// =============================================================================
//  src/layout/main.cpp
//  Ponto de entrada — layout de objetos
//
//  Como os bytes de um objeto são realmente arrumados na memória: alinhamento,
//  padding, as categorias de tipo que autorizam cada operação, e o que
//  `virtual` acrescenta.
// =============================================================================
#include "demo_tamanho.hpp"
#include "demo_alignas.hpp"
#include "demo_categorias.hpp"
#include "demo_no_unique_address.hpp"
#include "demo_polimorfico.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::steel_blue) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · sizeof, alinhamento e padding");
    demo_tamanho::run();

    section("2 · alignas e alinhamento sob demanda");
    demo_alignas::run();

    section("3 · Trivial, standard-layout, POD e agregado");
    demo_categorias::run();

    section("4 · EBO e [[no_unique_address]]");
    demo_no_unique_address::run();

    section("5 · O que virtual acrescenta ao objeto");
    demo_polimorfico::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de layout concluídos!\n\n");
    return 0;
}
