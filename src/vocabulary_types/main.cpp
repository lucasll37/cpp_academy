// =============================================================================
//  src/vocabulary_types/main.cpp
//  Ponto de entrada — os tipos de vocabulário da biblioteca padrão
//
//  ⚠ Compila com C++23: as operações monádicas de std::optional
//    (and_then/transform/or_else) chegaram nesse padrão.
// =============================================================================
#include "demo_optional.hpp"
#include "demo_variant.hpp"
#include "demo_any.hpp"
#include "demo_tuple.hpp"
#include "demo_span.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::golden_rod) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · std::optional — pode não haver valor");
    demo_optional::run();

    section("2 · std::variant — união com tipo seguro");
    demo_variant::run();

    section("3 · std::any — qualquer tipo, e o seu preço");
    demo_any::run();

    section("4 · tuple e pair — agregados anônimos");
    demo_tuple::run();

    section("5 · std::span — visão sobre sequência contígua");
    demo_span::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de tipos de vocabulário concluídos!\n\n");
}
