// =============================================================================
//  src/concurrency/demo_futures.hpp
//
//  future & promise — comunicação assíncrona com resultado
//  ─────────────────────────────────────────────────────────
//  Enquanto mutex/condition_variable sincronizam ACESSO a dados,
//  future/promise transferem um RESULTADO de uma thread para outra —
//  sem precisar de variáveis compartilhadas nem mutex.
//
//  Os três mecanismos:
//  ┌─────────────────┬────────────────────────────────────────────────┐
//  │ std::async      │ jeito mais simples: "rode isso e me dê futuro" │
//  │ std::promise    │ controle manual: você define quando/o que      │
//  │ packaged_task   │ empacota um callable para uso posterior        │
//  └─────────────────┴────────────────────────────────────────────────┘
//
//  std::future<T>: representa um valor T que ainda está sendo calculado.
//  future.get()  : bloqueia até o valor estar disponível, retorna T.
//                  Pode ser chamado apenas UMA vez.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <future>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <vector>
#include <numeric>

namespace demo_futures {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. std::async — o jeito mais simples de rodar algo assincronamente
// ─────────────────────────────────────────────────────────────────────────────
void demo_async() {
    fmt::print("\n── 4.1 std::async ──\n");

    // std::launch::async  → garante nova thread
    // std::launch::deferred → executa lazy quando .get() for chamado (mesma thread)
    // sem flag             → SO decide (pode ser qualquer um — evitar em produção)

    auto futuro = std::async(std::launch::async, []() {
        fmt::print("  [async task] iniciando cálculo...\n");
        std::this_thread::sleep_for(20ms);
        return 42;
    });

    fmt::print("  [main] fazendo outras coisas enquanto task roda...\n");
    std::this_thread::sleep_for(5ms);
    fmt::print("  [main] chamando .get() — vai bloquear se task ainda não terminou\n");

    int resultado = futuro.get(); // bloqueia aqui se necessário
    fmt::print("  [main] resultado = {}\n", resultado);

    // Múltiplos futuros em paralelo
    fmt::print("\n  Múltiplos async em paralelo:\n");
    auto f1 = std::async(std::launch::async, []() { std::this_thread::sleep_for(30ms); return 1; });
    auto f2 = std::async(std::launch::async, []() { std::this_thread::sleep_for(10ms); return 2; });
    auto f3 = std::async(std::launch::async, []() { std::this_thread::sleep_for(20ms); return 3; });

    // get() bloqueia individualmente — todos rodam em paralelo
    fmt::print("  f1={}, f2={}, f3={}\n", f1.get(), f2.get(), f3.get());
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. std::promise — controle manual do valor futuro
//     promise é o "escritor", future é o "leitor"
//     Útil quando o resultado vem de um callback, evento, ou múltiplas etapas
// ─────────────────────────────────────────────────────────────────────────────
void demo_promise() {
    fmt::print("\n── 4.2 std::promise ──\n");

    std::promise<int> promessa;
    std::future<int>  futuro = promessa.get_future(); // conectados

    std::thread produtor([](std::promise<int> p) {
        fmt::print("  [produtor] calculando...\n");
        std::this_thread::sleep_for(20ms);
        p.set_value(99); // "cumpre" a promessa — futuro fica pronto
        fmt::print("  [produtor] promessa cumprida\n");
    }, std::move(promessa)); // promise é movível, não copiável

    fmt::print("  [main] aguardando resultado...\n");
    int val = futuro.get();
    fmt::print("  [main] recebido: {}\n", val);

    produtor.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Exceções via future — propagação cross-thread
//     Se a task lançar uma exceção, ela é capturada e relançada em .get()
// ─────────────────────────────────────────────────────────────────────────────
void demo_excecao() {
    fmt::print("\n── 4.3 Exceções propagadas via future ──\n");

    // Via async
    auto futuro = std::async(std::launch::async, []() -> int {
        std::this_thread::sleep_for(10ms);
        throw std::runtime_error("algo deu errado na thread!");
        return 0; // nunca alcançado
    });

    try {
        int val = futuro.get(); // exceção é relançada aqui
        fmt::print("  val = {} (não deveria chegar aqui)\n", val);
    } catch (const std::exception& e) {
        fmt::print("  exceção capturada em get(): {}\n", e.what());
    }

    // Via promise
    std::promise<double> p;
    auto f = p.get_future();

    std::thread t([](std::promise<double> p) {
        try {
            throw std::range_error("valor fora do intervalo");
        } catch (...) {
            // captura e armazena no future — será relançada em .get()
            p.set_exception(std::current_exception());
        }
    }, std::move(p));

    try {
        f.get();
    } catch (const std::exception& e) {
        fmt::print("  exceção via promise: {}\n", e.what());
    }

    t.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. std::packaged_task — empacota callable para execução posterior
//     Diferente de async (que executa imediatamente), packaged_task
//     separa a criação da tarefa de quando ela será executada.
//     Útil em thread pools — você enfileira tasks e threads as executam.
// ─────────────────────────────────────────────────────────────────────────────
int calcular(int a, int b) {
    std::this_thread::sleep_for(10ms);
    return a * b + a + b;
}

void demo_packaged_task() {
    fmt::print("\n── 4.4 std::packaged_task ──\n");

    // Empacota a função — ainda não executa
    std::packaged_task<int(int, int)> task(calcular);
    std::future<int> futuro = task.get_future();

    // Transfere para uma thread para executar
    std::thread t(std::move(task), 6, 7);

    fmt::print("  [main] task enfileirada, aguardando resultado...\n");
    int resultado = futuro.get();
    fmt::print("  [main] calcular(6,7) = {}\n", resultado);

    t.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. std::shared_future — resultado compartilhado por múltiplos "leitores"
//     future.get() só pode ser chamado UMA vez.
//     shared_future.get() pode ser chamado por múltiplas threads.
// ─────────────────────────────────────────────────────────────────────────────
void demo_shared_future() {
    fmt::print("\n── 4.5 std::shared_future ──\n");

    std::promise<std::string> p;
    std::shared_future<std::string> sf = p.get_future().share(); // .share() converte

    auto leitor = [sf](int id) {
        // shared_future pode ser copiado — cada thread tem sua cópia
        auto resultado = sf.get(); // todos os leitores recebem o mesmo valor
        fmt::print("  [leitor {}] recebeu: '{}'\n", id, resultado);
    };

    std::thread t1(leitor, 1);
    std::thread t2(leitor, 2);
    std::thread t3(leitor, 3);

    std::this_thread::sleep_for(20ms);
    p.set_value("resultado compartilhado");

    t1.join(); t2.join(); t3.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Paralelismo de dados com async — dividir e conquistar
// ─────────────────────────────────────────────────────────────────────────────
void demo_paralelo() {
    fmt::print("\n── 4.6 Soma paralela com async ──\n");

    std::vector<int> dados(1000);
    std::iota(dados.begin(), dados.end(), 1); // 1, 2, 3, ..., 1000

    // Divide em 4 partes e soma cada uma em paralelo
    auto soma_parcial = [&](int inicio, int fim) {
        return std::accumulate(dados.begin() + inicio, dados.begin() + fim, 0LL);
    };

    int n = static_cast<int>(dados.size());
    auto f1 = std::async(std::launch::async, soma_parcial, 0,       n/4);
    auto f2 = std::async(std::launch::async, soma_parcial, n/4,     n/2);
    auto f3 = std::async(std::launch::async, soma_parcial, n/2,   3*n/4);
    auto f4 = std::async(std::launch::async, soma_parcial, 3*n/4,    n);

    long long total = f1.get() + f2.get() + f3.get() + f4.get();
    long long esperado = (long long)n * (n + 1) / 2; // fórmula de Gauss

    fmt::print("  soma paralela = {}, esperado = {}, correto = {}\n",
               total, esperado, total == esperado);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_async();
    demo_promise();
    demo_excecao();
    demo_packaged_task();
    demo_shared_future();
    demo_paralelo();
}

} // namespace demo_futures
