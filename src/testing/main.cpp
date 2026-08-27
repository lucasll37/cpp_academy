// =============================================================================
//  src/testing/main.cpp
//  Ponto de entrada — testes automatizados com GoogleTest
//
//  ⚠ Este executável é a NARRATIVA. Os testes de verdade estão nos arquivos
//    test_*.cpp, compilados em binários próprios e registrados com `test()`
//    no meson.build. Rode-os com:
//
//        meson test -C build                      # todos
//        meson test -C build testing_mock -v      # um, com a saída
//
//    O sistema sob teste é `loja::Carrinho`, em carrinho.hpp/.cpp.
// =============================================================================
#include "demo_porque.hpp"
#include "demo_anatomia.hpp"
#include "demo_organizacao.hpp"
#include "demo_dubles.hpp"
#include "demo_cobertura.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::pale_violet_red) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Por que testar, e o que é um teste bom");
    demo_porque::run();

    section("2 · Anatomia de um teste e as asserções");
    demo_anatomia::run();

    section("3 · Fixtures, parametrização e organização");
    demo_organizacao::run();

    section("4 · Dublês e a testabilidade do desenho");
    demo_dubles::run();

    section("5 · Cobertura, sanitizers e além");
    demo_cobertura::run();

    fmt::print(fmt::fg(fmt::color::gold) | fmt::emphasis::bold,
               "\n→ Os testes DE VERDADE estão em test_*.cpp. Rode-os com:\n"
               "    meson test -C build\n"
               "    meson test -C build testing_parametrizado -v\n");

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de testing concluídos!\n\n");
    return 0;
}
