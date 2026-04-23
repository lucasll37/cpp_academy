// =============================================================================
//  src/coroutines/demo_internals.hpp
//
//  Demo 3 — Internals: o que o compilador gera
//  ─────────────────────────────────────────────
//  Coroutines em C++20 são uma transformação do compilador.
//  Uma função coroutine é reescrita em uma máquina de estados.
//
//  Este demo expõe os mecanismos internos:
//   • coroutine_handle<> — ponteiro tipado para o frame da coroutine
//   • coroutine_handle<>::from_address() / address()
//   • resume() / destroy() / done()
//   • O que é o "frame" (coroutine state) na memória
//   • std::suspend_always vs std::suspend_never
//   • std::noop_coroutine() — coroutine que não faz nada (sentinela)
//
//  Tópicos avançados:
//   • Symmetric transfer — evitar stack overflow em cadeias de coroutines
//   • Heap elision — quando o compilador evita alocação heap
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <coroutine>

namespace demo_internals {

// ─────────────────────────────────────────────────────────────────────────────
//  Coroutine minimal — passo a passo manual
//
//  Normalmente usamos `for (auto v : gen)` ou `co_await task`.
//  Aqui controlamos o handle diretamente para ver cada etapa.
// ─────────────────────────────────────────────────────────────────────────────
struct SimpleTask {
    struct promise_type {
        int checkpoint = 0; // acompanhar em que ponto está a coroutine

        SimpleTask get_return_object() {
            return SimpleTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        // suspend_always → começa suspensa
        std::suspend_always initial_suspend() noexcept {
            fmt::print("    [promise] initial_suspend — coroutine criada, ainda não executou\n");
            return {};
        }

        // suspend_always → termina suspensa (precisamos destruir manualmente)
        std::suspend_always final_suspend() noexcept {
            fmt::print("    [promise] final_suspend — coroutine terminou, aguardando destroy()\n");
            return {};
        }

        // co_yield int
        std::suspend_always yield_value(int v) {
            checkpoint = v;
            fmt::print("    [promise] yield_value({}) — suspendendo\n", v);
            return {};
        }

        void return_void() noexcept {
            fmt::print("    [promise] return_void — fim da função\n");
        }

        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;
    explicit SimpleTask(std::coroutine_handle<promise_type> h) : handle(h) {}
    SimpleTask(const SimpleTask&) = delete;
    ~SimpleTask() { /* não destruímos aqui — fazemos manualmente no demo */ }
};

// A função coroutine — será transformada numa máquina de estados
SimpleTask maquina_de_estados() {
    fmt::print("    [coroutine] checkpoint A — antes do primeiro co_yield\n");
    co_yield 1;
    fmt::print("    [coroutine] checkpoint B — entre os co_yields\n");
    co_yield 2;
    fmt::print("    [coroutine] checkpoint C — após o segundo co_yield\n");
    // co_return implícito (return_void chamado)
}

void demo_handle_manual() {
    fmt::print("\n── 3.1 coroutine_handle — controle manual do ciclo de vida ──\n\n");

    fmt::print("  >> Criando a coroutine (maquina_de_estados()):\n");
    auto task = maquina_de_estados();
    auto& h = task.handle;

    fmt::print("\n  Estado: done={}, address={}\n",
               h.done(), h.address());

    fmt::print("\n  >> Primeira resume():\n");
    h.resume(); // executa até o primeiro co_yield 1
    fmt::print("  Checkpoint atual: {}\n", h.promise().checkpoint);
    fmt::print("  done={}\n", h.done());

    fmt::print("\n  >> Segunda resume():\n");
    h.resume(); // executa até o segundo co_yield 2
    fmt::print("  Checkpoint atual: {}\n", h.promise().checkpoint);

    fmt::print("\n  >> Terceira resume():\n");
    h.resume(); // executa até o fim → final_suspend
    fmt::print("  done={} (suspenso no final)\n", h.done());

    fmt::print("\n  >> destroy() — libera o frame da coroutine:\n");
    h.destroy(); // se não destruirmos, há memory leak!
    task.handle = nullptr;
    fmt::print("  Frame destruído\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  suspend_always vs suspend_never — o que muda no comportamento
// ─────────────────────────────────────────────────────────────────────────────
struct EagerTask {
    struct promise_type {
        EagerTask get_return_object() {
            return EagerTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        // suspend_never → executa imediatamente ao ser criada
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;
    explicit EagerTask(std::coroutine_handle<promise_type> h) : handle(h) {}
};

EagerTask coroutine_eager() {
    fmt::print("    [eager] executa IMEDIATAMENTE ao ser criada (suspend_never)\n");
    // sem co_yield → termina instantaneamente
    co_return;
}

void demo_suspend_never() {
    fmt::print("\n── 3.2 suspend_never vs suspend_always ──\n");
    fmt::print("  Criando EagerTask (suspend_never em initial_suspend):\n");
    auto t = coroutine_eager(); // executa o corpo imediatamente aqui
    fmt::print("  Após a criação — a coroutine já terminou\n");
    fmt::print("  handle.done() = {}\n\n", t.handle.done());

    fmt::print("  Contraste:\n");
    fmt::print("    suspend_always (lazy):  coroutine só executa com .resume()\n");
    fmt::print("    suspend_never  (eager): coroutine executa na construção\n");
    fmt::print("  Lazy é mais comum em Task<T> para controlar quando começa\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Symmetric transfer — retornar um handle em await_suspend
//
//  Em vez de resumir o chamador via handle.resume() (que adiciona um frame
//  na stack), retornamos o handle → o compilador faz goto direto.
//  Isso evita stack overflow em cadeias longas de coroutines.
// ─────────────────────────────────────────────────────────────────────────────
void demo_symmetric_transfer() {
    fmt::print("\n── 3.3 Symmetric transfer — evitar stack overflow em cadeias ──\n");
    fmt::print("  Problema sem symmetric transfer:\n");
    fmt::print("    corA resume → corB resume → corC resume\n");
    fmt::print("    cada resume() adiciona um frame → stack cresce!\n\n");
    fmt::print("  Solução — await_suspend retorna coroutine_handle<>:\n");
    fmt::print("    em vez de: void await_suspend(h) {{{{ outro_handle.resume(); }}}}\n");
    fmt::print("    use:       auto await_suspend(h) {{{{ return outro_handle; }}}}\n\n");
    fmt::print("  O compilador transforma em goto (tail call) — stack constante\n");
    fmt::print("  É isso que o FinalAwaiter em Task<T> faz:\n");
    fmt::print("    return cont ? cont : std::noop_coroutine();\n");
    fmt::print("  noop_coroutine() é uma coroutine sentinela que não faz nada\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  O que o compilador gera (pseudocódigo)
// ─────────────────────────────────────────────────────────────────────────────
void demo_transformacao_compilador() {
    fmt::print("\n── 3.4 O que o compilador gera (equivalente manual) ──\n");
    fmt::print(R"(
  Função original (coroutine):
    Generator<int> contar(int n) {{
        while (true) co_yield n++;
    }}

  Equivalente manual gerado pelo compilador:
    struct contar_frame {{
        contar_promise promise;
        int n;
        int __state = 0;   // em qual co_yield estamos
    }};

    void contar_resume(contar_frame* f) {{
        switch (f->__state) {{
            case 0: goto label_0;
            case 1: goto label_1;
        }}
        label_0:
            while (true) {{
                f->promise.yield_value(f->n++);  // co_yield
                f->__state = 1;
                return;  // suspende (volta ao chamador)
                label_1:;
            }}
    }}

  Cada co_yield vira um `return` + `case` na máquina de estados.
  O frame é alocado na heap (ou otimizado para stack pelo compilador).
)");
}

inline void run() {
    demo_handle_manual();
    demo_suspend_never();
    demo_symmetric_transfer();
    demo_transformacao_compilador();
}

} // namespace demo_internals