// =============================================================================
//  src/bits/main.cpp
//  Ponto de entrada — manipulação de bits
//
//  ⚠ Compila com C++23 (`std::byteswap`). O `<bit>` em si é C++20.
// =============================================================================
#include "demo_operadores.hpp"
#include "demo_bit.hpp"
#include "demo_bitset.hpp"
#include "demo_bitfields.hpp"
#include "demo_endianness.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::slate_gray) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Operadores de bit e as regras que mordem");
    demo_operadores::run();

    section("2 · <bit> — o que o hardware já sabia fazer");
    demo_bit::run();

    section("3 · std::bitset — um conjunto de bits com nome");
    demo_bitset::run();

    section("4 · Campos de bits — a sintaxe nativa");
    demo_bitfields::run();

    section("5 · Endianness e serialização binária");
    demo_endianness::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de bits concluídos!\n\n");
}
