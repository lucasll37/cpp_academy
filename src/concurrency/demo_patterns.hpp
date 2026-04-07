// =============================================================================
//  src/concurrency/demo_patterns.hpp
//
//  Padrões — Thread Pool & Pipeline
//  ──────────────────────────────────
//  Reúne tudo: thread, mutex, condition_variable, future, packaged_task.
//
//  Thread Pool:
//    Um conjunto fixo de threads que processam tarefas de uma fila.
//    Evita o overhead de criar/destruir threads a cada tarefa.
//    Base de frameworks como TBB, std::execution, Asio's thread pool.
//
//  Pipeline:
//    Estágios em série, cada um em sua própria thread.
//    A saída de um estágio é a entrada do próximo — como UNIX pipes.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <queue>
#include <vector>
#include <atomic>
#include <chrono>
#include <stdexcept>

namespace demo_patterns {

using namespace std::chrono_literals;

// =============================================================================
//  Thread Pool
// =============================================================================

class ThreadPool {
public:
    // ── Construtor: cria N worker threads ────────────────────────────────────
    explicit ThreadPool(std::size_t n_threads) : parar_(false) {
        workers_.reserve(n_threads);
        for (std::size_t i = 0; i < n_threads; ++i) {
            workers_.emplace_back([this, i]() { loop_worker(i); });
        }
        fmt::print("  [ThreadPool] {} threads criadas\n", n_threads);
    }

    // ── Destrutor: sinaliza parada e aguarda todas as threads ─────────────────
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            parar_ = true;
        }
        cv_.notify_all(); // acorda todas as threads para que saiam do loop
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        fmt::print("  [ThreadPool] encerrado\n");
    }

    // ── submit() — enfileira uma tarefa e retorna um future ──────────────────
    // O template permite aceitar qualquer callable com qualquer retorno
    template <typename Func, typename... Args>
    auto submit(Func&& f, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        // Empacota a tarefa com seus argumentos em um packaged_task
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            [f = std::forward<Func>(f),
             args_tuple = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply(std::move(f), std::move(args_tuple));
            }
        );

        std::future<ReturnType> futuro = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mtx_);
            if (parar_)
                throw std::runtime_error("ThreadPool encerrado — não aceita mais tarefas");

            // Enfileira como std::function<void()> — apaga o tipo concreto
            tarefas_.push([task]() { (*task)(); });
        }

        cv_.notify_one(); // acorda uma thread para processar
        return futuro;
    }

    std::size_t tamanho() const { return workers_.size(); }

private:
    // ── Loop de cada worker thread ────────────────────────────────────────────
    void loop_worker(std::size_t id) {
        while (true) {
            std::function<void()> tarefa;

            {
                std::unique_lock<std::mutex> lock(mtx_);
                // Dorme até: haver tarefa OU sinal de parada
                cv_.wait(lock, [this]() {
                    return parar_ || !tarefas_.empty();
                });

                // Se parar_ e fila vazia → encerra a thread
                if (parar_ && tarefas_.empty()) return;

                tarefa = std::move(tarefas_.front());
                tarefas_.pop();
            }

            tarefa(); // executa FORA do lock — outras threads podem pegar tarefas
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tarefas_;
    std::mutex                        mtx_;
    std::condition_variable           cv_;
    bool                              parar_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Demo 1: Thread Pool básico
// ─────────────────────────────────────────────────────────────────────────────
void demo_thread_pool() {
    fmt::print("\n── 6.1 Thread Pool ──\n");

    ThreadPool pool(4); // 4 threads prontas

    // Enfileira 8 tarefas — as 4 threads as distribuem
    std::vector<std::future<int>> futuros;

    for (int i = 0; i < 8; ++i) {
        futuros.push_back(pool.submit([i]() -> int {
            std::this_thread::sleep_for(10ms);
            int resultado = i * i;
            fmt::print("  [tarefa {}] resultado = {}\n", i, resultado);
            return resultado;
        }));
    }

    // Coleta todos os resultados
    int soma = 0;
    for (auto& f : futuros) soma += f.get();
    fmt::print("  soma de todos os resultados = {}\n", soma);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demo 2: Thread Pool com tarefas heterogêneas
// ─────────────────────────────────────────────────────────────────────────────
void demo_pool_heterogeneo() {
    fmt::print("\n── 6.2 Thread Pool — tarefas de tipos diferentes ──\n");

    ThreadPool pool(3);

    auto f_int    = pool.submit([]() -> int    { return 42; });
    auto f_string = pool.submit([]() -> std::string { return "resultado"; });
    auto f_double = pool.submit([](double x, double y) { return x * y; }, 3.14, 2.0);
    auto f_void   = pool.submit([]() {
        fmt::print("  [void task] executando sem retorno\n");
    });

    fmt::print("  int future:    {}\n", f_int.get());
    fmt::print("  string future: {}\n", f_string.get());
    fmt::print("  double future: {:.4f}\n", f_double.get());
    f_void.get(); // aguarda conclusão mesmo sem valor de retorno
}

// =============================================================================
//  Pipeline — processamento em estágios
//  Estágio 1: gerador → Estágio 2: transformador → Estágio 3: consumidor
// =============================================================================

// Fila thread-safe genérica com suporte a "encerramento"
template <typename T>
class Canal {
public:
    void enviar(T item) {
        std::lock_guard<std::mutex> lock(mtx_);
        fila_.push(std::move(item));
        cv_.notify_one();
    }

    // Retorna false quando o canal está fechado e vazio
    bool receber(T& out) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this]() { return !fila_.empty() || fechado_; });
        if (fila_.empty()) return false; // fechado e vazio
        out = std::move(fila_.front());
        fila_.pop();
        return true;
    }

    void fechar() {
        std::lock_guard<std::mutex> lock(mtx_);
        fechado_ = true;
        cv_.notify_all();
    }

private:
    std::queue<T>           fila_;
    std::mutex              mtx_;
    std::condition_variable cv_;
    bool                    fechado_ = false;
};

void demo_pipeline() {
    fmt::print("\n── 6.3 Pipeline de 3 estágios ──\n");

    Canal<int>         c1; // gerador → transformador
    Canal<std::string> c2; // transformador → consumidor

    // Estágio 1: Gerador — produz inteiros de 1 a 5
    std::thread gerador([&]() {
        for (int i = 1; i <= 5; ++i) {
            std::this_thread::sleep_for(5ms);
            fmt::print("  [gerador] enviando {}\n", i);
            c1.enviar(i);
        }
        c1.fechar();
    });

    // Estágio 2: Transformador — converte int → string formatada
    std::thread transformador([&]() {
        int val;
        while (c1.receber(val)) {
            auto msg = fmt::format("item-{:03d}", val * val); // i²
            fmt::print("  [transform] {} → '{}'\n", val, msg);
            c2.enviar(msg);
            std::this_thread::sleep_for(3ms);
        }
        c2.fechar();
    });

    // Estágio 3: Consumidor — exibe os resultados finais
    std::thread consumidor([&]() {
        std::string msg;
        int contagem = 0;
        while (c2.receber(msg)) {
            fmt::print("  [consumidor] '{}'\n", msg);
            ++contagem;
        }
        fmt::print("  [consumidor] processados {} itens\n", contagem);
    });

    gerador.join();
    transformador.join();
    consumidor.join();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_thread_pool();
    demo_pool_heterogeneo();
    demo_pipeline();
}

} // namespace demo_patterns
