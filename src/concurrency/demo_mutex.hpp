// =============================================================================
//  src/concurrency/demo_mutex.hpp
//
//  Mutex & Locks — exclusão mútua
//  ────────────────────────────────
//  Quando múltiplas threads acessam dados compartilhados, precisamos garantir
//  que apenas uma execute a seção crítica por vez.
//
//  Race condition: duas threads lendo e escrevendo sem sincronização
//  → resultado depende do escalonamento do SO → não-determinístico → bugs.
//
//  Tópicos:
//  • Race condition demonstrada
//  • std::mutex + lock()/unlock() manual (e por que evitar)
//  • std::lock_guard — RAII simples
//  • std::unique_lock — RAII flexível (lock/unlock manual, try_lock, defer)
//  • std::shared_mutex — múltiplos leitores, um escritor
//  • std::call_once — inicialização thread-safe única
//  • Deadlock — como acontece e como evitar
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <chrono>
#include <string>

namespace demo_mutex {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. Race condition — o problema
//     contador++ parece atômico mas NÃO É:
//     É na verdade: leia valor → some 1 → escreva de volta (3 operações!)
//     Duas threads podem ler o mesmo valor antes de qualquer uma escrever.
// ─────────────────────────────────────────────────────────────────────────────
void demo_race_condition() {
    fmt::print("\n── 2.1 Race condition (sem proteção) ──\n");

    int contador = 0;
    constexpr int N = 10000;

    auto incrementar = [&]() {
        for (int i = 0; i < N; ++i)
            ++contador; // ← RACE CONDITION: leia-some-escreva não é atômico
    };

    std::thread t1(incrementar);
    std::thread t2(incrementar);
    t1.join();
    t2.join();

    fmt::print("  esperado: {}, obtido: {} {}\n",
               2 * N, contador,
               contador == 2 * N ? "✓" : "✗ (race condition!)");
    // Quase sempre obtido < 20000 — algumas incrementações foram perdidas
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. std::mutex — a solução, e por que não usar lock/unlock diretamente
// ─────────────────────────────────────────────────────────────────────────────
void demo_mutex_manual() {
    fmt::print("\n── 2.2 mutex manual (lock/unlock) — EVITAR ──\n");

    int contador = 0;
    std::mutex mtx;
    constexpr int N = 10000;

    auto incrementar = [&]() {
        for (int i = 0; i < N; ++i) {
            mtx.lock();
            ++contador; // seção crítica protegida
            mtx.unlock();
            // ⚠ Se uma exceção for lançada entre lock() e unlock()
            //   → mutex fica bloqueado para sempre → deadlock!
        }
    };

    std::thread t1(incrementar);
    std::thread t2(incrementar);
    t1.join();
    t2.join();

    fmt::print("  resultado correto: {} == {} ? {}\n",
               contador, 2 * N, contador == 2 * N);
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. std::lock_guard — RAII simples, o mais comum
//     Faz lock no construtor, unlock no destruidor.
//     Não pode fazer unlock manualmente — use unique_lock para isso.
// ─────────────────────────────────────────────────────────────────────────────
void demo_lock_guard() {
    fmt::print("\n── 2.3 lock_guard (RAII simples) ──\n");

    int contador = 0;
    std::mutex mtx;
    constexpr int N = 10000;

    auto incrementar = [&]() {
        for (int i = 0; i < N; ++i) {
            std::lock_guard<std::mutex> lock(mtx); // lock no construtor
            ++contador;
        } // ← unlock automático no destruidor, mesmo com exceção
    };

    std::thread t1(incrementar);
    std::thread t2(incrementar);
    t1.join();
    t2.join();

    fmt::print("  resultado correto: {} == {} ? {}\n",
               contador, 2 * N, contador == 2 * N);

    // C++17: deduction guide — não precisa especificar o tipo
    // std::lock_guard lock(mtx); // equivalente
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. std::unique_lock — RAII flexível
//     • Pode fazer unlock/lock manualmente
//     • Pode ser movido (lock_guard não pode)
//     • Necessário para condition_variable
//     • Suporta try_lock e lock com timeout
// ─────────────────────────────────────────────────────────────────────────────
void demo_unique_lock() {
    fmt::print("\n── 2.4 unique_lock (RAII flexível) ──\n");

    std::mutex mtx;
    int valor = 0;

    std::thread t([&]() {
        // std::defer_lock: cria o unique_lock SEM fazer lock ainda
        std::unique_lock<std::mutex> lock(mtx, std::defer_lock);

        // ... faz trabalho sem o lock ...
        fmt::print("  [t] fazendo trabalho sem lock\n");

        lock.lock(); // adquire o lock manualmente quando necessário
        valor = 42;
        fmt::print("  [t] valor escrito: {}\n", valor);
        lock.unlock(); // libera antes de terminar (opcional com RAII, mas explícito)

        fmt::print("  [t] mais trabalho sem lock\n");
    });

    std::this_thread::sleep_for(5ms);

    {
        std::unique_lock<std::mutex> lock(mtx);
        fmt::print("  [main] valor lido: {}\n", valor);
    }

    t.join();

    // try_lock: tenta adquirir sem bloquear
    std::mutex mtx2;
    std::unique_lock<std::mutex> lock(mtx2, std::try_to_lock);
    fmt::print("  try_lock bem-sucedido: {}\n", lock.owns_lock());
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. std::shared_mutex — readers/writer lock
//     Múltiplas threads podem LER simultaneamente.
//     Apenas uma thread pode ESCREVER (e nenhuma pode ler durante a escrita).
//     Uso: caches, mapas de configuração, dados lidos muito e escritos pouco.
// ─────────────────────────────────────────────────────────────────────────────
void demo_shared_mutex() {
    fmt::print("\n── 2.5 shared_mutex (readers/writer lock) ──\n");

    std::shared_mutex rw_mtx;
    std::string dado = "valor inicial";

    // Múltiplos leitores simultâneos — shared_lock
    auto ler = [&](int id) {
        std::shared_lock lock(rw_mtx); // múltiplas threads podem ter isso ao mesmo tempo
        fmt::print("  [leitor {}] dado = '{}'\n", id, dado);
        std::this_thread::sleep_for(5ms);
    };

    // Apenas um escritor — unique_lock (exclusivo)
    auto escrever = [&](const std::string& novo) {
        std::unique_lock lock(rw_mtx); // bloqueia todos os leitores e escritores
        fmt::print("  [escritor] atualizando...\n");
        dado = novo;
        fmt::print("  [escritor] novo valor: '{}'\n", dado);
    };

    std::vector<std::thread> threads;
    threads.emplace_back(ler, 1);
    threads.emplace_back(ler, 2);
    threads.emplace_back(ler, 3);
    threads.emplace_back(escrever, "valor atualizado");
    threads.emplace_back(ler, 4);

    for (auto& t : threads) t.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. std::call_once — inicialização thread-safe, executada exatamente uma vez
//     Útil para: singleton, inicialização lazy de recursos caros
// ─────────────────────────────────────────────────────────────────────────────
void demo_call_once() {
    fmt::print("\n── 2.6 call_once — inicialização única ──\n");

    std::once_flag flag;
    int recurso = 0;

    auto inicializar_se_necessario = [&]() {
        std::call_once(flag, [&]() {
            fmt::print("  inicializando recurso (só uma vez!)...\n");
            recurso = 42;
        });
        fmt::print("  usando recurso: {}\n", recurso);
    };

    std::thread t1(inicializar_se_necessario);
    std::thread t2(inicializar_se_necessario);
    std::thread t3(inicializar_se_necessario);
    t1.join(); t2.join(); t3.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Deadlock — o que é e como evitar
//     Acontece quando thread A espera por mutex de B, e B espera por mutex de A.
//     → Nenhuma avança → programa trava para sempre.
// ─────────────────────────────────────────────────────────────────────────────
void demo_deadlock_evitado() {
    fmt::print("\n── 2.7 Deadlock — como evitar ──\n");

    std::mutex mtx_a, mtx_b;

    // ✗ Receita para deadlock:
    // Thread 1: lock(mtx_a) → lock(mtx_b)
    // Thread 2: lock(mtx_b) → lock(mtx_a)  ← ordem inversa!

    // ✓ Solução 1: sempre adquirir na mesma ordem
    // Thread 1: lock(mtx_a) → lock(mtx_b)
    // Thread 2: lock(mtx_a) → lock(mtx_b)  ← mesma ordem

    // ✓ Solução 2: std::lock() — adquire múltiplos mutexes sem deadlock
    //   usa algoritmo que tenta ordens diferentes até conseguir ambos
    auto transferir = [&](bool invertido) {
        if (!invertido) {
            std::lock(mtx_a, mtx_b); // atomic: adquire os dois ou nenhum
        } else {
            std::lock(mtx_b, mtx_a); // mesmo resultado — std::lock é inteligente
        }
        // adopt_lock: diz ao lock_guard "o mutex já está lockado, só gerencie"
        std::lock_guard<std::mutex> la(mtx_a, std::adopt_lock);
        std::lock_guard<std::mutex> lb(mtx_b, std::adopt_lock);

        fmt::print("  transferência segura realizada\n");
    };

    std::thread t1(transferir, false);
    std::thread t2(transferir, true);
    t1.join();
    t2.join();

    fmt::print("  nenhum deadlock!\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_race_condition();
    demo_mutex_manual();
    demo_lock_guard();
    demo_unique_lock();
    demo_shared_mutex();
    demo_call_once();
    demo_deadlock_evitado();
}

} // namespace demo_mutex
