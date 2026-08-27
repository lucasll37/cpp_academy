// =============================================================================
//  src/oop/main.cpp
//  Ponto de entrada — executa todos os demos de orientação a objetos
// =============================================================================
#include "demo_inheritance.hpp"
#include "demo_virtual.hpp"
#include "demo_slicing.hpp"
#include "demo_multiple_inheritance.hpp"
#include "demo_rtti.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::medium_purple) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Herança — public, protected, private");
    demo_inheritance::run();

    section("2 · Funções virtuais — vtable, vptr, override");
    demo_virtual::run();

    section("3 · Object slicing — o polimorfismo que some");
    demo_slicing::run();

    section("4 · Herança múltipla e o diamante");
    demo_multiple_inheritance::run();

    section("5 · RTTI — typeid, type_index, dynamic_cast");
    demo_rtti::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de orientação a objetos concluídos!\n\n");
}
