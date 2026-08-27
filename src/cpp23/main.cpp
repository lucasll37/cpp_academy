// =============================================================================
//  src/cpp23/main.cpp
//  Ponto de entrada — C++23 neste toolchain
//
//  Este subprojeto é diferente dos outros: além de ENSINAR as features, ele
//  MEDE quais estão disponíveis, usando as feature-test macros. A tabela de
//  suporte em src/ROADMAP.md é gerada a partir do que este programa imprime —
//  e teve seis entradas corrigidas quando a medição substituiu a suposição.
// =============================================================================
#include "demo_inventario.hpp"
#include "demo_linguagem.hpp"
#include "demo_biblioteca.hpp"
#include "demo_disponivel.hpp"
#include "demo_o_que_falta.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::spring_green) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Inventário — o que este toolchain tem");
    demo_inventario::run();

    section("2 · Linguagem — if consteval, [[assume]], uz");
    demo_linguagem::run();

    section("3 · Biblioteca — to_underlying, zip, unreachable");
    demo_biblioteca::run();

    section("4 · Presentes e pouco usadas");
    demo_disponivel::run();

    section("5 · O que falta, e o que usar no lugar");
    demo_o_que_falta::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de C++23 concluídos!\n\n");
}
