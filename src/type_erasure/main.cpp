// =============================================================================
//  src/type_erasure/main.cpp
//  Ponto de entrada — apagamento de tipo
//
//  ⚠ Compila com C++23 por causa de `std::move_only_function` (demo_any, 3.3).
//    Todo o resto é C++20.
//
//  ⚠ demo_implementacao.hpp SUBSTITUI o `operator new` global para medir as
//    alocações da `std::function`. Por isso ele só pode ser incluído por UMA
//    unidade de tradução — esta. Ver src/linkage/ sobre a ODR.
// =============================================================================
#include "demo_conceito.hpp"
#include "demo_implementacao.hpp"
#include "demo_any.hpp"
#include "demo_alternativas.hpp"
#include "demo_pratica.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::coral) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · O que é apagamento de tipo");
    demo_conceito::run();

    section("2 · std::function por dentro");
    demo_implementacao::run();

    section("3 · O apagamento que já vem no padrão");
    demo_any::run();

    section("4 · virtual vs CRTP vs variant vs erasure");
    demo_alternativas::run();

    section("5 · Polimorfismo com semântica de valor");
    demo_pratica::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de type_erasure concluídos!\n\n");
    return 0;
}
