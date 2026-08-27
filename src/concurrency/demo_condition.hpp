// =============================================================================
//  src/concurrency/demo_condition.hpp
//
//  std::condition_variable — sincronização por estado
//  ────────────────────────────────────────────────────
//  Mutex protege dados. condition_variable coordena QUANDO uma thread
//  deve agir — ela fica dormente até outra thread sinalizar que algo mudou.
//
//  Sem condition_variable, a alternativa seria busy-wait (spin):
//      while (!pronto) { /* verificar em loop */ }  ← desperdiça CPU
//
//  Com condition_variable, a thread dorme de verdade e só acorda
//  quando sinalizada — zero consumo de CPU enquanto espera.
//
//  Tópicos:
//  • wait() e o spurious wakeup — por que sempre usar predicado
//  • notify_one() vs notify_all()
//  • wait_for() / wait_until() — timeout
//  • Pipeline simples com condition_variable
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <chrono>

namespace demo_condition {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  1. Uso básico — uma thread espera, outra sinaliza
// ─────────────────────────────────────────────────────────────────────────────
void demo_basico() {
    fmt::print("\n── 3.1 wait() e notify_one() básico ──\n");

    std::mutex mtx;
    std::condition_variable cv;
    bool pronto = false;
    int dado = 0;

    // Thread consumidora — espera até pronto == true
    std::thread consumidor([&]() {
        std::unique_lock<std::mutex> lock(mtx);

        // wait() faz três coisas:
        //  1. Libera o mutex (para o produtor poder entrar)
        //  2. Dorme a thread
        //  3. Quando notificado: reacquire o mutex e verifica o predicado
        //
        // O predicado [&]{ return pronto; } protege contra SPURIOUS WAKEUP
        // (o SO pode acordar a thread sem notify — o predicado garante que
        //  só continuamos se a condição for realmente verdadeira)
        cv.wait(lock, [&]() { return pronto; });

        fmt::print("  [consumidor] acordou! dado = {}\n", dado);
    });

    // Thread produtora — prepara dados e sinaliza
    std::this_thread::sleep_for(20ms); // simula trabalho

    {
        std::lock_guard<std::mutex> lock(mtx);
        dado = 42;
        pronto = true;
        fmt::print("  [produtor] dado preparado, notificando...\n");
    } // ← libera o mutex ANTES de notificar (melhor prática)

    cv.notify_one(); // acorda UMA thread esperando
    consumidor.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. notify_one() vs notify_all()
//     notify_one()  → acorda apenas uma thread (a escolha é do SO)
//     notify_all()  → acorda todas as threads esperando
//
//  Use notify_all() quando:
//  • Múltiplas threads podem processar a mesma condição
//  • A condição mudou de forma relevante para todos
// ─────────────────────────────────────────────────────────────────────────────
void demo_notify_all() {
    fmt::print("\n── 3.2 notify_all() ──\n");

    std::mutex mtx;
    std::condition_variable cv;
    bool iniciar = false;

    auto corredor = [&](int id) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() { return iniciar; });
        fmt::print("  [corredor {}] largou!\n", id);
    };

    std::thread t1(corredor, 1);
    std::thread t2(corredor, 2);
    std::thread t3(corredor, 3);

    std::this_thread::sleep_for(20ms);

    {
        std::lock_guard<std::mutex> lock(mtx);
        iniciar = true;
        fmt::print("  [largada] GO!\n");
    }
    cv.notify_all(); // acorda todos os corredores simultaneamente

    t1.join(); t2.join(); t3.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. wait_for() — espera com timeout
//     Retorna cv_status::timeout se expirou, cv_status::no_timeout se notificado
// ─────────────────────────────────────────────────────────────────────────────
void demo_timeout() {
    fmt::print("\n── 3.3 wait_for() com timeout ──\n");

    std::mutex mtx;
    std::condition_variable cv;
    bool pronto = false;

    std::thread t([&]() {
        std::unique_lock<std::mutex> lock(mtx);

        // Espera até 50ms
        auto status = cv.wait_for(lock, 50ms, [&]() { return pronto; });

        if (status) {
            fmt::print("  [t] notificado antes do timeout\n");
        } else {
            fmt::print("  [t] timeout! pronto ainda é false\n");
        }
    });

    // Não notificamos — deixamos dar timeout
    std::this_thread::sleep_for(100ms);
    t.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Fila thread-safe — padrão producer/consumer clássico
//     O produtor enfileira itens; o consumidor os processa.
//     A condition_variable coordena sem busy-wait.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
class FilaSegura {
public:
    void enfileirar(T item) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            fila_.push(std::move(item));
        }
        cv_.notify_one(); // avisa consumidor que há novo item
    }

    // Bloqueia até haver um item disponível
    T desenfileirar() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !fila_.empty(); });
        T item = std::move(fila_.front());
        fila_.pop();
        return item;
    }

    bool vazia() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return fila_.empty();
    }

    std::size_t tamanho() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return fila_.size();
    }

private:
    mutable std::mutex        mtx_;
    std::condition_variable   cv_;
    std::queue<T>             fila_;
};

void demo_fila_segura() {
    fmt::print("\n── 3.4 Fila thread-safe (producer/consumer) ──\n");

    FilaSegura<std::string> fila;
    bool encerrar = false;
    std::mutex mtx_enc;
    std::condition_variable cv_enc;

    // Produtor: enfileira 5 mensagens
    std::thread produtor([&]() {
        for (int i = 1; i <= 5; ++i) {
            std::this_thread::sleep_for(10ms);
            auto msg = fmt::format("mensagem-{}", i);
            fmt::print("  [produtor] enfileirando '{}'\n", msg);
            fila.enfileirar(msg);
        }
        // Sinaliza encerramento
        {
            std::lock_guard<std::mutex> lock(mtx_enc);
            encerrar = true;
        }
        cv_enc.notify_one();
    });

    // Consumidor: processa mensagens até receber sinal de encerramento
    std::thread consumidor([&]() {
        while (true) {
            // Verifica se deve encerrar E fila está vazia
            {
                std::unique_lock<std::mutex> lock(mtx_enc);
                cv_enc.wait_for(lock, 5ms);
                if (encerrar && fila.vazia()) break;
            }
            if (!fila.vazia()) {
                auto msg = fila.desenfileirar();
                fmt::print("  [consumidor] processando '{}'\n", msg);
            }
        }
        fmt::print("  [consumidor] encerrado\n");
    });

    produtor.join();
    consumidor.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_notify_all();
    demo_timeout();
    demo_fila_segura();
}

} // namespace demo_condition
