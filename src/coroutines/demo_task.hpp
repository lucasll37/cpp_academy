// =============================================================================
//  src/coroutines/demo_task.hpp
//
//  Demo 2 — Task<T>: co_await e computação assíncrona
//  ────────────────────────────────────────────────────
//  `co_yield` é para gerar sequências.
//  `co_await` é para ESPERAR por resultados — sem bloquear a thread.
//
//  Task<T> representa uma coroutine que:
//   1. Retorna um valor T quando concluída
//   2. Pode ser "aguardada" por outra coroutine via co_await
//   3. Executa de forma lazy (só começa quando alguém faz co_await nela)
//
//  Diferença fundamental de std::future:
//   • future.get() → BLOQUEIA a thread (thread fica parada esperando)
//   • co_await task → SUSPENDE a coroutine (thread fica livre para outro trabalho)
//
//  Tópicos:
//  • co_await expression — como funciona a suspensão
//  • Awaitable / Awaiter — o protocolo que co_await usa
//  • Task<T> — coroutine que retorna um valor
//  • Encadeamento de Tasks (co_await dentro de co_await)
//  • co_return — como a coroutine entrega seu resultado
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <coroutine>
#include <exception>
#include <stdexcept>
#include <variant>
#include <thread>
#include <chrono>

namespace demo_task {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
//  Awaitable simples — SuspendFor
//
//  co_await exige que a expressão seja um "Awaitable" — um tipo com três
//  métodos que o compilador chama automaticamente:
//
//    bool await_ready()          → true = não suspende (já tem resultado)
//    void await_suspend(handle)  → o que fazer quando suspende
//    T    await_resume()         → valor que co_await retorna
//
//  Esse awaitable suspende a coroutine por um intervalo de tempo.
//  É um exemplo minimal — em produção usaria um event loop.
// ─────────────────────────────────────────────────────────────────────────────
struct SuspendFor {
    std::chrono::milliseconds duracao;

    // await_ready() → false = sempre suspende (nunca pula a espera)
    bool await_ready() const noexcept { return false; }

    // await_suspend(handle): chamado quando a coroutine vai suspender.
    // Aqui simplesmente dormimos em outra thread e depois retomamos.
    // Em produção: registraria em um event loop (libuv, io_uring, etc.)
    void await_suspend(std::coroutine_handle<> handle) const {
        // Retoma após a duração em uma thread auxiliar
        std::thread([handle, d = duracao]() mutable {
            std::this_thread::sleep_for(d);
            handle.resume(); // retoma a coroutine suspensa
        }).detach();
    }

    // await_resume() → valor que `co_await SuspendFor{...}` retorna
    // Aqui retornamos void — só queríamos esperar, sem valor
    void await_resume() const noexcept {}
};

// ─────────────────────────────────────────────────────────────────────────────
//  Task<T> — coroutine que produz um valor T ao final
//
//  Diferente de Generator<T> que produz múltiplos valores via co_yield,
//  Task<T> produz UM valor ao final via co_return.
//
//  Suporta co_await: outra coroutine pode fazer `co_await minhaTask()`
//  e receberá o resultado quando minhaTask() terminar.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
struct Task {

    struct promise_type {
        std::variant<std::monostate, T, std::exception_ptr> resultado_;

        // A coroutine que está aguardando esta Task (parent)
        // Quando esta Task terminar, retoma o parent.
        std::coroutine_handle<> continuacao_;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // suspend_always: execução lazy — só começa quando alguém faz co_await
        std::suspend_always initial_suspend() noexcept { return {}; }

        // Ao terminar: suspende e retoma o chamador (se houver)
        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }

            // Quando esta Task termina, retomamos a coroutine que estava
            // esperando por ela (a "continuação")
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept {
                auto cont = h.promise().continuacao_;
                if (cont) return cont;             // retoma o chamador
                return std::noop_coroutine();       // ninguém esperando, para
            }

            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }

        // co_return valor → armazena o resultado
        void return_value(T value) {
            resultado_.template emplace<1>(std::move(value));
        }

        void unhandled_exception() {
            resultado_.template emplace<2>(std::current_exception());
        }

        T& get() {
            if (auto* exc = std::get_if<2>(&resultado_))
                std::rethrow_exception(*exc);
            return std::get<1>(resultado_);
        }
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle handle_;

    explicit Task(Handle h) : handle_(h) {}

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }

    ~Task() { if (handle_) handle_.destroy(); }

    // ── Awaitable: permite `co_await task` ───────────────────────────────────
    //
    //  Quando outra coroutine faz `co_await task`:
    //   1. await_ready() → false = não está pronto ainda
    //   2. await_suspend(chamador) → salva o chamador como continuação,
    //      começa a executar esta Task
    //   3. Quando Task termina → FinalAwaiter retoma o chamador
    //   4. await_resume() → retorna o valor produzido por co_return
    //
    bool await_ready() const noexcept {
        return handle_.done(); // já terminou? não precisa suspender
    }

    void await_suspend(std::coroutine_handle<> chamador) noexcept {
        handle_.promise().continuacao_ = chamador; // "me avise quando terminar"
        handle_.resume();                           // começa a executar
    }

    T await_resume() {
        return handle_.promise().get();
    }

    // Execução síncrona (para o main() que não é coroutine)
    T get_sync() {
        handle_.resume();
        // Spin simples — em produção usaria event loop
        while (!handle_.done()) {
            std::this_thread::sleep_for(1ms);
        }
        return handle_.promise().get();
    }
};

// Especialização para Task<void>
template <>
struct Task<void> {
    struct promise_type {
        std::exception_ptr exception_;
        std::coroutine_handle<> continuacao_;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct FinalAwaiter {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(
                std::coroutine_handle<promise_type> h) noexcept {
                auto cont = h.promise().continuacao_;
                return cont ? cont : std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { exception_ = std::current_exception(); }

        void get() {
            if (exception_) std::rethrow_exception(exception_);
        }
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle handle_;

    explicit Task(Handle h) : handle_(h) {}
    Task(const Task&) = delete;
    Task(Task&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }
    ~Task() { if (handle_) handle_.destroy(); }

    bool await_ready() const noexcept { return handle_.done(); }
    void await_suspend(std::coroutine_handle<> c) noexcept {
        handle_.promise().continuacao_ = c;
        handle_.resume();
    }
    void await_resume() { handle_.promise().get(); }

    void get_sync() {
        handle_.resume();
        while (!handle_.done())
            std::this_thread::sleep_for(1ms);
        handle_.promise().get();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Funções de exemplo usando co_await e co_return
// ─────────────────────────────────────────────────────────────────────────────

// Simula uma operação assíncrona (ex: leitura de disco, chamada de rede)
Task<int> buscar_dado(int id) {
    fmt::print("  [Task] buscando dado {}... (aguardando 50ms)\n", id);
    co_await SuspendFor{50ms}; // suspende sem bloquear a thread
    fmt::print("  [Task] dado {} pronto!\n", id);
    co_return id * 10; // entrega o resultado
}

// Coroutine que encadeia outras Tasks com co_await
Task<int> processar() {
    fmt::print("  [processar] iniciando\n");

    // co_await suspende processar() enquanto buscar_dado(1) executa
    // A thread fica livre — não está bloqueada esperando!
    int a = co_await buscar_dado(1);
    int b = co_await buscar_dado(2);

    fmt::print("  [processar] a={}, b={}, soma={}\n", a, b, a + b);
    co_return a + b;
}

// Task<void> — coroutine que não retorna valor
Task<void> exibir_mensagem(std::string msg) {
    co_await SuspendFor{10ms};
    fmt::print("  [mensagem] {}\n", msg);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demos
// ─────────────────────────────────────────────────────────────────────────────
void demo_co_return() {
    fmt::print("\n── 2.1 co_return — Task retorna um valor ──\n");

    // buscar_dado(5) é uma coroutine — não executa ao ser criada
    // get_sync() a executa até o fim de forma síncrona (para poder usar no main)
    auto task = buscar_dado(5);
    int resultado = task.get_sync();
    fmt::print("  Resultado final: {}\n", resultado);
}

void demo_encadeamento() {
    fmt::print("\n── 2.2 Encadeamento — co_await dentro de coroutine ──\n");

    auto task = processar();
    int resultado = task.get_sync();
    fmt::print("  Soma final recebida no main: {}\n", resultado);
}

void demo_task_void() {
    fmt::print("\n── 2.3 Task<void> — co_return sem valor ──\n");

    auto t = exibir_mensagem("Coroutines são compostas naturalmente!");
    t.get_sync();
}

void demo_awaitable_custom() {
    fmt::print("\n── 2.4 Custom Awaitable — qualquer tipo pode ser co_await-ado ──\n");
    fmt::print("  Protocolo do Awaitable:\n");
    fmt::print("    • await_ready()       → bool: já tem resultado? pula suspensão\n");
    fmt::print("    • await_suspend(h)    → o que fazer ao suspender\n");
    fmt::print("    • await_resume()      → valor que co_await retorna\n\n");
    fmt::print("  SuspendFor{{50ms}} implementa esse protocolo:\n");
    fmt::print("    • await_ready()  → false  (sempre suspende)\n");
    fmt::print("    • await_suspend  → dorme em outra thread, depois retoma\n");
    fmt::print("    • await_resume() → void   (só queríamos esperar)\n");
}

inline void run() {
    demo_co_return();
    demo_encadeamento();
    demo_task_void();
    demo_awaitable_custom();
}

} // namespace demo_task
