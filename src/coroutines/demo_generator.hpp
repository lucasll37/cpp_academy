// =============================================================================
//  src/coroutines/demo_generator.hpp
//
//  Demo 1 — Generator: produzir valores sob demanda
//  ─────────────────────────────────────────────────
//  Um generator é a forma mais simples de coroutine: uma função que pode
//  "pausar" e devolver um valor intermediário, depois continuar de onde parou.
//
//  Antes de coroutines (C++17 e anterior), gerar sequências preguiçosas
//  requeria threads, callbacks ou máquinas de estado manuais.
//  Com coroutines, o código é direto — parece um loop normal.
//
//  Tópicos:
//  • co_yield — pausa a coroutine e entrega um valor
//  • promise_type — personaliza o comportamento da coroutine
//  • Generator<T> — handle RAII que gerencia o ciclo de vida
//  • Sequências infinitas e finitas
//  • Range-for sobre uma coroutine
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>

namespace demo_generator {

// ─────────────────────────────────────────────────────────────────────────────
//  Generator<T> — tipo retornado por funções geradoras
//
//  Para que uma função seja uma coroutine, o tipo de retorno deve ter
//  um membro `promise_type` que controla:
//    • Como a coroutine começa (suspend_always → preguiçoso)
//    • O que acontece com valores de co_yield
//    • Como a coroutine termina
//
//  Usamos suspend_always em initial_suspend → a coroutine NÃO começa
//  a executar quando criada; só executa quando pedimos o próximo valor.
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
struct Generator {

    // ── promise_type ─────────────────────────────────────────────────────────
    //  O compilador cria uma instância de promise_type para cada chamada da
    //  função coroutine. É o "estado interno" invisível ao chamador.
    struct promise_type {
        std::optional<T>   value_;      // valor atual produzido por co_yield
        std::exception_ptr exception_;  // exceção propagada para o chamador

        // get_return_object() → cria o Generator que o chamador recebe.
        // coroutine_handle<promise_type>::from_promise(*this) é o "handle"
        // que permite retomar e destruir a coroutine.
        Generator get_return_object() {
            return Generator{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }

        // initial_suspend → suspend_always: a coroutine não executa nada
        // ao ser criada. Só começa quando chamamos .next() pela primeira vez.
        // (Alternativa: suspend_never = executa até o primeiro co_yield logo)
        std::suspend_always initial_suspend() noexcept { return {}; }

        // final_suspend → suspend_always: ao terminar, a coroutine suspende
        // em vez de se destruir automaticamente. Isso é necessário para que
        // o Generator (que guarda o handle) possa destruí-la de forma segura.
        std::suspend_always final_suspend() noexcept { return {}; }

        // yield_value(v) é chamado quando a coroutine executa `co_yield v`.
        // Armazenamos o valor e suspendemos — o chamador pode então lê-lo.
        std::suspend_always yield_value(T value) {
            value_ = std::move(value);
            return {};
        }

        // return_void() é chamado quando a função coroutine chega ao fim
        // sem um co_return explícito (ou com `co_return;`).
        void return_void() noexcept { value_.reset(); }

        // unhandled_exception() captura exceções lançadas dentro da coroutine
        // para que possamos relançá-las no chamador via .next() ou .value().
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };

    // ── Handle RAII ───────────────────────────────────────────────────────────
    using Handle = std::coroutine_handle<promise_type>;
    Handle handle_;

    explicit Generator(Handle h) : handle_(h) {}

    // Não copiável — ownership exclusivo do handle (como unique_ptr)
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    // Movível
    Generator(Generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    // Destrutor: destrói a coroutine suspensa (libera stack frame + estado)
    ~Generator() {
        if (handle_) handle_.destroy();
    }

    // ── Interface pública ─────────────────────────────────────────────────────

    // next() → avança a coroutine até o próximo co_yield (ou fim).
    // Retorna true se produziu um valor, false se terminou.
    bool next() {
        if (!handle_ || handle_.done()) return false;

        handle_.resume(); // retoma até o próximo ponto de suspensão

        // Se a coroutine lançou exceção, relança aqui no chamador
        if (handle_.promise().exception_)
            std::rethrow_exception(handle_.promise().exception_);

        return !handle_.done();
    }

    // value() → o valor produzido pelo último co_yield
    const T& value() const {
        return *handle_.promise().value_;
    }

    // ── Suporte a range-for ───────────────────────────────────────────────────
    //  Implementamos begin()/end() para que Generator funcione diretamente
    //  em `for (auto v : gen)` — sem copiar os valores.
    struct iterator {
        Handle handle_;

        iterator& operator++() {
            handle_.resume();
            return *this;
        }

        const T& operator*() const {
            return *handle_.promise().value_;
        }

        bool operator==(std::default_sentinel_t) const {
            return !handle_ || handle_.done();
        }
    };

    iterator begin() {
        if (handle_) handle_.resume(); // avança até o primeiro co_yield
        return iterator{handle_};
    }

    std::default_sentinel_t end() { return {}; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Exemplos de funções geradoras
// ─────────────────────────────────────────────────────────────────────────────

// Sequência infinita de inteiros a partir de `start`
// A função parece um loop normal — a mágica é o co_yield
Generator<int> contar(int start = 0) {
    int n = start;
    while (true) {
        co_yield n++; // pausa aqui, entrega n, depois continua
    }
}

// Sequência finita — Fibonacci até o limite dado
Generator<long long> fibonacci(long long limite) {
    long long a = 0, b = 1;
    while (a <= limite) {
        co_yield a;
        auto prox = a + b;
        a = b;
        b = prox;
    }
    // ao sair do while, a coroutine termina → return_void() chamado
}

// Filtro: só entrega valores do outro generator que satisfazem o predicado
// (generators compostos — como pipes de Unix)
template <typename T, typename Pred>
Generator<T> filtrar(Generator<T> src, Pred pred) {
    for (auto val : src) {
        if (pred(val)) co_yield val;
    }
}

// Transforma valores de um generator aplicando uma função
template <typename T, typename F>
auto mapear(Generator<T> src, F func) -> Generator<decltype(func(std::declval<T>()))> {
    for (auto val : src) {
        co_yield func(val);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Generator com exceção interna
// ─────────────────────────────────────────────────────────────────────────────
Generator<int> com_erro(int ate) {
    for (int i = 0; i < ate; ++i) {
        if (i == 3) throw std::runtime_error("valor proibido: 3");
        co_yield i;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demos
// ─────────────────────────────────────────────────────────────────────────────
void demo_sequencia_infinita() {
    fmt::print("\n── 1.1 Sequência infinita — co_yield com loop while(true) ──\n");

    auto gen = contar(10);

    fmt::print("  Primeiros 5 valores a partir de 10: ");
    for (int i = 0; i < 5; ++i) {
        gen.next();
        fmt::print("{} ", gen.value());
    }
    fmt::print("\n");

    fmt::print("  A coroutine está suspensa e pode continuar: {}\n",
               !gen.handle_.done() ? "sim" : "não");
}

void demo_fibonacci() {
    fmt::print("\n── 1.2 Fibonacci até 1000 — range-for sobre Generator ──\n");

    fmt::print("  Fibonacci ≤ 1000: ");
    for (auto n : fibonacci(1000)) {
        fmt::print("{} ", n);
    }
    fmt::print("\n");
}

void demo_pipeline() {
    fmt::print("\n── 1.3 Pipeline: contar → filtrar pares → dobrar ──\n");

    // Pega os primeiros 10 inteiros, filtra os pares, dobra cada um
    auto numeros  = contar(0);
    auto pares    = filtrar(std::move(numeros), [](int n){ return n % 2 == 0; });
    auto dobrados = mapear(std::move(pares),   [](int n){ return n * 2; });

    fmt::print("  Resultados: ");
    int count = 0;
    for (auto v : dobrados) {
        fmt::print("{} ", v);
        if (++count == 6) break; // limita para não rodar infinito
    }
    fmt::print("\n");
    fmt::print("  (pipeline: contar(0) | filtrar(par) | mapear(x2))\n");
}

void demo_excecao() {
    fmt::print("\n── 1.4 Exceção dentro da coroutine ──\n");

    auto gen = com_erro(6);
    try {
        while (gen.next()) {
            fmt::print("  valor: {}\n", gen.value());
        }
    } catch (const std::exception& e) {
        fmt::print("  Exceção capturada no chamador: {}\n", e.what());
        fmt::print("  A exceção foi propagada da coroutine para cá transparentemente\n");
    }
}

inline void run() {
    demo_sequencia_infinita();
    demo_fibonacci();
    demo_pipeline();
    demo_excecao();
}

} // namespace demo_generator
