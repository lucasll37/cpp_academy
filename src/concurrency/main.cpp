// =============================================================================
//  src/concurrency/main.cpp
//  Ponto de entrada — executa todos os demos de concorrência em sequência
// =============================================================================
#include "demo_threads.hpp"
#include "demo_mutex.hpp"
#include "demo_condition.hpp"
#include "demo_futures.hpp"
#include "demo_atomics.hpp"
#include "demo_patterns.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::coral) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<52}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · Threads — criação, join, detach, lifecycle");
    demo_threads::run();

    section("2 · Mutex & Locks — exclusão mútua");
    demo_mutex::run();

    section("3 · condition_variable — sincronização por estado");
    demo_condition::run();

    section("4 · future & promise — comunicação assíncrona");
    demo_futures::run();

    section("5 · atomics & memory model");
    demo_atomics::run();

    section("6 · Padrões — thread pool & producer/consumer");
    demo_patterns::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de concorrência concluídos!\n\n");
}
