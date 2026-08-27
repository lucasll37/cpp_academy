// =============================================================================
//  src/concurrency2/demo_jthread.hpp
//
//  std::jthread — a thread que se junta sozinha, e sabe parar
//  ──────────────────────────────────────────────────────────
//  `std::thread` tem dois defeitos que o C++20 corrigiu:
//
//    1. o destrutor chama `std::terminate` se a thread ainda for joinable.
//       Ou seja: esquecer um `join()` mata o programa. Uma exceção no meio
//       do escopo também.
//
//    2. não há forma padrão de PEDIR que a thread pare. Cada projeto
//       reinventava um `std::atomic<bool>` compartilhado.
//
//  `jthread` resolve os dois: RAII no destrutor (join automático) e um
//  `stop_token` cooperativo embutido.
//
//  Tópicos:
//  • O destrutor que não mata o programa
//  • stop_token: parada COOPERATIVA
//  • stop_callback — reagir ao pedido de parada
//  • ⚠ jthread não INTERROMPE: ela PEDE
//  • Integração com condition_variable_any
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace demo_jthread {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. O destrutor que não mata o programa
// ─────────────────────────────────────────────────────────────────────────────
void demo_raii() {
    fmt::print("\n── 1.1 O destrutor ──\n");

    fmt::print("      std::thread:\n");
    fmt::print("        {{\n");
    fmt::print("            std::thread t{{trabalho}};\n");
    fmt::print("        }}   // ← ~thread() com joinable == true → std::terminate()\n");
    fmt::print("      O programa ABORTA. Não é vazamento, não é UB: é morte.\n");
    fmt::print("      (o comitê escolheu isso de propósito: detach silencioso\n");
    fmt::print("       seria pior, porque a thread sobreviveria aos objetos que\n");
    fmt::print("       ela captura por referência)\n");

    fmt::print("\n      std::jthread:\n");
    {
        std::jthread t{[] {
            fmt::print("        [jthread] trabalhando...\n");
            std::this_thread::sleep_for(20ms);
            fmt::print("        [jthread] terminei\n");
        }};
        fmt::print("        saindo do escopo sem chamar join()...\n");
    }   // ← ~jthread() faz request_stop() + join()
    fmt::print("        ✓ o destrutor fez join(). Nenhum terminate.\n");

    // E com exceção no meio
    fmt::print("\n      e com exceção no meio do escopo:\n");
    try {
        std::jthread t{[] { std::this_thread::sleep_for(10ms); }};
        throw std::runtime_error("falhou");
    } catch (const std::exception& e) {
        fmt::print("        capturado '{}' — a jthread foi juntada no unwinding\n",
                   e.what());
    }
    fmt::print("      É o mesmo RAII de src/errors/demo_exceptions.hpp, 1.2.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. stop_token — parada cooperativa
//
//     A lambda recebe um `std::stop_token` como PRIMEIRO parâmetro, se ela o
//     declarar. A `jthread` o passa automaticamente.
// ─────────────────────────────────────────────────────────────────────────────
void demo_stop_token() {
    fmt::print("\n── 1.2 stop_token ──\n");

    std::atomic<int> iteracoes{0};

    {
        // O `std::stop_token st` no primeiro parâmetro é o que ativa o mecanismo
        std::jthread trabalhador{[&iteracoes](std::stop_token st) {
            while (!st.stop_requested()) {          // ← o ponto de checagem
                ++iteracoes;
                std::this_thread::sleep_for(5ms);
            }
            fmt::print("      [worker] parada pedida, saindo limpo\n");
        }};

        std::this_thread::sleep_for(40ms);
        fmt::print("      pedindo parada explicitamente...\n");
        trabalhador.request_stop();
    }   // o destrutor também pediria, se não tivéssemos pedido

    fmt::print("      o worker rodou {} iterações\n", iteracoes.load());

    fmt::print("\n      Antes do C++20, isto era:\n");
    fmt::print("        std::atomic<bool> parar{{false}};\n");
    fmt::print("        std::thread t{{[&parar]{{ while(!parar) {{...}} }}}};\n");
    fmt::print("        parar = true;  t.join();\n");
    fmt::print("      Funcionava, mas cada projeto tinha a sua versão, e nenhuma\n");
    fmt::print("      conversava com a biblioteca padrão.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. ⚠ jthread não INTERROMPE — ela PEDE
// ─────────────────────────────────────────────────────────────────────────────
void demo_cooperativa() {
    fmt::print("\n── 1.3 ⚠ A parada é COOPERATIVA ──\n");

    fmt::print("      `request_stop()` só liga um flag. Se a thread nunca\n");
    fmt::print("      consultar `stop_requested()`, ela NÃO para:\n\n");
    fmt::print("        std::jthread t{{[](std::stop_token) {{\n");
    fmt::print("            while (true) {{ /* nunca checa */ }}\n");
    fmt::print("        }}}};\n");
    fmt::print("        // o destrutor pede parada e depois faz join()\n");
    fmt::print("        // → TRAVA PARA SEMPRE\n\n");

    fmt::print("      Não existe `thread::kill()` em C++, e isso é deliberado:\n");
    fmt::print("      matar uma thread no meio deixaria mutexes travados,\n");
    fmt::print("      destrutores sem rodar e invariantes quebrados.\n");
    fmt::print("      (o `pthread_cancel` do POSIX existe e é considerado\n");
    fmt::print("       inseguro em C++ justamente por isso)\n\n");

    fmt::print("      A responsabilidade é sua: checar em pontos SEGUROS.\n");

    // Demonstração de um laço que checa nos pontos certos
    std::atomic<int> processados{0};
    const std::vector<int> tarefas(1000, 1);

    {
        std::jthread t{[&](std::stop_token st) {
            for (std::size_t i = 0; i < tarefas.size(); ++i) {
                if (st.stop_requested()) {         // ← entre itens, não no meio de um
                    fmt::print("      [worker] parou em {} de {} — estado consistente\n",
                               i, tarefas.size());
                    return;
                }
                ++processados;
                std::this_thread::sleep_for(100us);
            }
        }};
        std::this_thread::sleep_for(15ms);
    }

    fmt::print("      processou {} itens antes de parar\n", processados.load());
    fmt::print("      A checagem fica ENTRE unidades de trabalho — nunca no\n");
    fmt::print("      meio de uma, ou o estado ficaria pela metade.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. stop_callback — reagir ao pedido
// ─────────────────────────────────────────────────────────────────────────────
void demo_callback() {
    fmt::print("\n── 1.4 stop_callback ──\n");

    std::atomic<bool> limpou{false};

    {
        std::jthread t{[&limpou](std::stop_token st) {
            // O callback dispara NO MOMENTO do request_stop(), na thread que
            // pediu — não nesta. É útil para acordar algo que está bloqueado.
            std::stop_callback ao_parar{st, [&limpou] {
                fmt::print("      [callback] disparou — liberando recurso\n");
                limpou = true;
            }};

            while (!st.stop_requested()) std::this_thread::sleep_for(5ms);
            fmt::print("      [worker] laço encerrado\n");
        }};

        std::this_thread::sleep_for(20ms);
        fmt::print("      pedindo parada...\n");
    }

    fmt::print("      o callback rodou? {}\n", limpou.load());

    fmt::print("\n      Para que serve: acordar quem está BLOQUEADO.\n");
    fmt::print("      Um `read()` de socket não checa `stop_requested()`. O\n");
    fmt::print("      callback pode fechar o socket, e o `read` retorna com erro.\n");
    fmt::print("      ⚠  o callback roda na thread que chamou `request_stop()`,\n");
    fmt::print("         então ele precisa ser rápido e thread-safe.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. condition_variable_any + stop_token
//
//     Uma thread bloqueada num `cv.wait()` normal não vê o pedido de parada.
//     A sobrecarga de `condition_variable_any` que aceita `stop_token` acorda.
// ─────────────────────────────────────────────────────────────────────────────
void demo_cv_any() {
    fmt::print("\n── 1.5 condition_variable_any + stop_token ──\n");

    std::mutex                    m;
    std::condition_variable_any   cv;
    std::vector<int>              fila;
    std::atomic<int>              consumidos{0};

    {
        std::jthread consumidor{[&](std::stop_token st) {
            while (true) {
                std::unique_lock lk{m};

                // Esta sobrecarga devolve `false` se a parada foi pedida.
                // Um `cv.wait(lk, pred)` normal ficaria bloqueado para sempre.
                if (!cv.wait(lk, st, [&fila] { return !fila.empty(); })) {
                    fmt::print("      [consumidor] parada pedida durante o wait\n");
                    return;
                }
                fila.pop_back();
                ++consumidos;
            }
        }};

        for (int i = 0; i < 3; ++i) {
            { std::scoped_lock lk{m}; fila.push_back(i); }
            cv.notify_one();
            std::this_thread::sleep_for(5ms);
        }
        fmt::print("      produzi 3 itens; agora o consumidor vai bloquear...\n");
        std::this_thread::sleep_for(20ms);
        fmt::print("      pedindo parada com o consumidor BLOQUEADO no wait\n");
    }

    fmt::print("      consumiu {} itens e saiu limpo\n", consumidos.load());

    fmt::print("\n      | classe                  | aceita stop_token |\n");
    fmt::print("      |-------------------------|-------------------|\n");
    fmt::print("      | condition_variable      | NÃO               |\n");
    fmt::print("      | condition_variable_any  | SIM               |\n");
    fmt::print("      A `_any` é mais lenta (funciona com qualquer lockable),\n");
    fmt::print("      mas é a única que integra com `stop_token`.\n");
    fmt::print("      Ver src/concurrency/demo_condition.hpp para a versão básica.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_raii();
    demo_stop_token();
    demo_cooperativa();
    demo_callback();
    demo_cv_any();
}

}  // namespace demo_jthread
