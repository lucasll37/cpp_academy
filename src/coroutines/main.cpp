// =============================================================================
//  src/coroutines/main.cpp
//  Ponto de entrada — executa todos os demos de coroutines em sequência
// =============================================================================
#include "demo_generator.hpp"
#include "demo_task.hpp"
#include "demo_internals.hpp"
#include "demo_asio.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::cornflower_blue) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<52}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Generator — co_yield e sequências preguiçosas");
    demo_generator::run();

    section("2 · Task<T> — co_await e co_return");
    demo_task::run();

    section("3 · Internals — o que o compilador gera");
    demo_internals::run();

    section("4 · Asio — I/O assíncrono com coroutines");
    demo_asio::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de coroutines concluídos!\n\n");
}
