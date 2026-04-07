// =============================================================================
//  src/concurrency/demo_atomics.hpp
//
//  std::atomic & Memory Model
//  ───────────────────────────
//  std::atomic<T> garante que operações em T sejam indivisíveis (atômicas).
//  Sem mutex, sem overhead de context-switch — implementado com instruções
//  especiais da CPU (LOCK prefix, CMPXCHG, etc.).
//
//  Mas atomics têm um detalhe sutil: o MEMORY ORDER — que controla quanta
//  reordenação de instruções o compilador/CPU podem fazer.
//
//  Tópicos:
//  • atomic<int> — contador sem mutex
//  • atomic<bool> — flag de cancelamento
//  • fetch_add, compare_exchange (CAS — compare and swap)
//  • Memory orders: relaxed, acquire/release, seq_cst
//  • Quando usar atomic vs mutex
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>

namespace demo_atomics {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. O problema resolvido — contador sem mutex
// ─────────────────────────────────────────────────────────────────────────────
void demo_contador_atomico() {
    fmt::print("\n── 5.1 Contador atômico (sem mutex) ──\n");

    std::atomic<int> contador{0};
    constexpr int N = 10000;

    auto incrementar = [&]() {
        for (int i = 0; i < N; ++i)
            ++contador; // atômico: leia-some-escreva como UMA instrução da CPU
    };

    std::thread t1(incrementar);
    std::thread t2(incrementar);
    t1.join();
    t2.join();

    fmt::print("  esperado: {}, obtido: {} {}\n",
               2 * N, contador.load(),
               contador == 2 * N ? "✓" : "✗");

    // Operações disponíveis
    fmt::print("  load():          {}\n", contador.load());       // lê o valor
    contador.store(0);                                             // escreve
    fmt::print("  após store(0):   {}\n", contador.load());
    int anterior = contador.fetch_add(10);                         // += e retorna anterior
    fmt::print("  fetch_add(10):   anterior={}, atual={}\n", anterior, contador.load());
    anterior = contador.fetch_sub(3);                              // -= e retorna anterior
    fmt::print("  fetch_sub(3):    anterior={}, atual={}\n", anterior, contador.load());
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. atomic<bool> — flag de cancelamento/sinalização
//     Padrão muito comum: thread principal sinaliza para threads filhas pararem
// ─────────────────────────────────────────────────────────────────────────────
void demo_flag_cancelamento() {
    fmt::print("\n── 5.2 Flag de cancelamento com atomic<bool> ──\n");

    std::atomic<bool> cancelar{false};
    std::atomic<int>  iteracoes{0};

    std::thread worker([&]() {
        while (!cancelar.load()) { // lê atomicamente
            ++iteracoes;
            std::this_thread::sleep_for(1ms);
        }
        fmt::print("  [worker] cancelado após {} iterações\n", iteracoes.load());
    });

    std::this_thread::sleep_for(15ms);
    cancelar.store(true); // sinaliza — atomicamente
    fmt::print("  [main] sinal de cancelamento enviado\n");

    worker.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Compare-and-Swap (CAS) — a operação fundamental de algoritmos lock-free
//
//  compare_exchange_strong(expected, desired):
//    SE *this == expected → *this = desired, retorna true
//    SE *this != expected → expected = *this, retorna false
//
//  É a base de estruturas lock-free: "tente atualizar; se alguém mudou
//  o valor antes de mim, leia o novo valor e tente de novo"
// ─────────────────────────────────────────────────────────────────────────────
void demo_cas() {
    fmt::print("\n── 5.3 Compare-and-Swap (CAS) ──\n");

    std::atomic<int> valor{10};

    // Tentativa de atualização com CAS
    int esperado = 10;
    bool sucesso = valor.compare_exchange_strong(esperado, 20);
    fmt::print("  CAS(10→20): sucesso={}, valor={}\n", sucesso, valor.load());

    // Falha: valor já mudou para 20, não é mais 10
    esperado = 10; // tentamos com 10 novamente
    sucesso = valor.compare_exchange_strong(esperado, 30);
    fmt::print("  CAS(10→30): sucesso={}, esperado foi atualizado para={}\n",
               sucesso, esperado); // esperado agora contém o valor atual (20)

    // Padrão de retry — o núcleo de algoritmos lock-free
    fmt::print("\n  Padrão lock-free com CAS:\n");
    std::atomic<int> contador{0};

    auto incrementar_lock_free = [&](int vezes) {
        for (int i = 0; i < vezes; ++i) {
            int atual = contador.load(std::memory_order_relaxed);
            while (!contador.compare_exchange_weak(
                       atual, atual + 1,                    // tenta: atual → atual+1
                       std::memory_order_release,           // sucesso: release
                       std::memory_order_relaxed)) {        // falha: relaxed
                // atual foi atualizado com o valor real — tenta de novo
            }
        }
    };

    std::thread t1(incrementar_lock_free, 5000);
    std::thread t2(incrementar_lock_free, 5000);
    t1.join(); t2.join();
    fmt::print("  contador lock-free = {} (esperado 10000)\n", contador.load());
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Memory orders — quanto o compilador/CPU podem reordenar
//
//  CPUs modernas e compiladores reordenam instruções para performance.
//  Isso é invisível em código single-thread mas causa bugs em multithread.
//
//  ┌─────────────────────┬─────────────────────────────────────────────────┐
//  │ memory_order        │ Significado                                     │
//  ├─────────────────────┼─────────────────────────────────────────────────┤
//  │ relaxed             │ Apenas atomicidade — sem restrição de ordem     │
//  │ acquire             │ Nada depois desta leitura pode ser movido antes │
//  │ release             │ Nada antes desta escrita pode ser movido depois │
//  │ acq_rel             │ acquire + release (para RMW operations)         │
//  │ seq_cst             │ Ordem total — o mais forte, o default           │
//  └─────────────────────┴─────────────────────────────────────────────────┘
//
//  Regra prática:
//  • seq_cst (default): sempre correto, menor performance
//  • acquire/release: padrão para producer/consumer — bom equilíbrio
//  • relaxed: apenas para contadores onde ordem não importa (stats, telemetria)
// ─────────────────────────────────────────────────────────────────────────────
void demo_memory_order() {
    fmt::print("\n── 5.4 Memory orders ──\n");

    // Padrão acquire/release — producer/consumer sem mutex
    std::atomic<bool> publicado{false};
    int dados = 0; // dado não-atômico protegido pelo handshake

    std::thread produtor([&]() {
        dados = 42; // escreve dado
        // release: garante que dados=42 seja visível ANTES de publicado=true
        publicado.store(true, std::memory_order_release);
        fmt::print("  [produtor] publicou dados={}\n", dados);
    });

    std::thread consumidor([&]() {
        // acquire: garante que após ler publicado=true,
        // a leitura de dados reflete o que o produtor escreveu
        while (!publicado.load(std::memory_order_acquire))
            std::this_thread::yield();
        fmt::print("  [consumidor] dados={} (deve ser 42)\n", dados);
    });

    produtor.join();
    consumidor.join();

    // relaxed — apenas contagem, ordem não importa
    std::atomic<long> hits{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j)
                hits.fetch_add(1, std::memory_order_relaxed);
        });
    }
    for (auto& t : threads) t.join();
    fmt::print("  contador relaxed = {} (esperado 4000)\n", hits.load());
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Atomic vs Mutex — quando usar cada um
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando_usar() {
    fmt::print("\n── 5.5 Quando usar atomic vs mutex ──\n");
    fmt::print(
        "  atomic<T>:\n"
        "    ✓ T é simples: int, bool, pointer, flag\n"
        "    ✓ Operação é uma única instrução (load, store, fetch_add)\n"
        "    ✓ Performance crítica, sem bloqueio (lock-free)\n"
        "    ✗ Operações compostas (leia-modifique-escreva múltiplos campos)\n"
        "    ✗ Proteger estruturas de dados complexas\n"
        "\n"
        "  mutex:\n"
        "    ✓ Proteger múltiplas variáveis como unidade\n"
        "    ✓ Operações não-triviais na seção crítica\n"
        "    ✓ Integração com condition_variable\n"
        "    ✗ Overhead de context-switch quando há contenção\n"
        "\n"
        "  Regra prática: comece com mutex. Só migre para atomic\n"
        "  se o profiler mostrar contenção real no mutex.\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_contador_atomico();
    demo_flag_cancelamento();
    demo_cas();
    demo_memory_order();
    demo_quando_usar();
}

} // namespace demo_atomics
