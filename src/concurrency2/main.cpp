// =============================================================================
//  src/concurrency2/main.cpp
//  Ponto de entrada — concorrência moderna (C++20)
//
//  Continua src/concurrency/, que cobre `thread`, mutex, `condition_variable`,
//  future/promise, atomics básicos e thread pool. Aqui entram os primitivos
//  que o C++20 acrescentou e o modelo de memória por baixo de todos eles.
// =============================================================================
#include "demo_jthread.hpp"
#include "demo_sincronizadores.hpp"
#include "demo_memory_model.hpp"
#include "demo_atomicos.hpp"
#include "demo_lockfree.hpp"

#include <fmt/core.h>
#include <fmt/color.h>

static void section(const char* title) {
    fmt::print(fmt::fg(fmt::color::medium_purple) | fmt::emphasis::bold,
               "\n╔══════════════════════════════════════════════════════╗\n"
               "║  {:<50}  ║\n"
               "╚══════════════════════════════════════════════════════╝\n", title);
}

int main() {
    section("1 · jthread — RAII e parada cooperativa");
    demo_jthread::run();

    section("2 · latch, barrier e semaphore");
    demo_sincronizadores::run();

    section("3 · O modelo de memória");
    demo_memory_model::run();

    section("4 · CAS, atomic_flag e espera atômica");
    demo_atomicos::run();

    section("5 · Lock-free na prática (e quando não vale)");
    demo_lockfree::run();

    fmt::print(fmt::fg(fmt::color::lime_green) | fmt::emphasis::bold,
               "\n✓ Todos os demos de concurrency2 concluídos!\n\n");
    return 0;
}
