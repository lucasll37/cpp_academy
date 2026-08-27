// =============================================================================
//  src/strings/main.cpp
//  Ponto de entrada — texto em C++
//
//  ⚠ Compila com C++23 (`std::string::contains`). Usa `fmt` em vez de
//    `std::format` porque é o que já está no conanfile — a API é a mesma.
// =============================================================================
#include "demo_string.hpp"
#include "demo_string_view.hpp"
#include "demo_format.hpp"
#include "demo_charconv.hpp"
#include "demo_regex.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::salmon) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · std::string — SSO, capacity, concatenação");
    demo_string::run();

    section("2 · std::string_view — a janela que não possui");
    demo_string_view::run();

    section("3 · std::format — validado na compilação");
    demo_format::run();

    section("4 · <charconv> — sem alocar, sem locale");
    demo_charconv::run();

    section("5 · std::regex — a API e o custo");
    demo_regex::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de strings concluídos!\n\n");
}
