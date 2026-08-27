// =============================================================================
//  src/linkage/main.cpp
//  Ponto de entrada — unidades de tradução, ODR, linkage e ABI
//
//  ⚠ Este subprojeto é o único que precisa de VÁRIAS unidades de tradução para
//    existir: os endereços impressos em 2.1 e os contadores de 2.2 vêm de
//    unidade_a.cpp e unidade_b.cpp compilados separadamente, e a seção 4 chama
//    c_api.c, compilado pelo compilador C. Ver o meson.build.
// =============================================================================
#include "demo_traducao.hpp"
#include "demo_odr.hpp"
#include "demo_linkage.hpp"
#include "demo_extern_c.hpp"
#include "demo_abi.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::dark_sea_green) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · A unidade de tradução");
    demo_traducao::run();

    section("2 · A One Definition Rule, medida");
    demo_odr::run();

    section("3 · Os três tipos de linkage");
    demo_linkage::run();

    section("4 · extern \"C\" e o name mangling");
    demo_extern_c::run();

    section("5 · ABI — o contrato binário");
    demo_abi::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de linkage concluídos!\n\n");
    return 0;
}
