// =============================================================================
//  src/memory/main.cpp
//  Ponto de entrada — executa todos os demos de memória em sequência
// =============================================================================
#include "demo_new_delete.hpp"
#include "demo_pool_allocator.hpp"
#include "demo_arena_allocator.hpp"
#include "demo_pmr.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::medium_aquamarine) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<52}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · new/delete — o que realmente acontece");
    demo_new_delete::run();

    section("2 · Pool Allocator — blocos fixos, zero fragmentação");
    demo_pool_allocator::run();

    section("3 · Arena Allocator — aloca tudo, libera tudo");
    demo_arena_allocator::run();

    section("4 · std::pmr — alocadores polimórficos da STL");
    demo_pmr::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de memória concluídos!\n\n");
}
