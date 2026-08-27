// =============================================================================
//  src/memory/demo_pmr.hpp
//
//  std::pmr — Polymorphic Memory Resources (C++17)
//  ─────────────────────────────────────────────────
//  std::pmr permite trocar o alocador de containers STL em runtime,
//  sem mudar o tipo do container — e sem templates de alocador.
//
//  Antes do pmr, trocar o alocador de um vector mudava seu tipo:
//      std::vector<int>                     // alocador padrão
//      std::vector<int, MeuAllocator<int>>  // tipo completamente diferente!
//      → Incompatíveis, não podem ser passados para a mesma função
//
//  Com pmr:
//      std::pmr::vector<int>  // sempre o mesmo tipo
//      // mas o alocador subjacente pode ser qualquer memory_resource
//
//  memory_resource é uma interface com três métodos:
//      do_allocate(bytes, alignment)   → void*
//      do_deallocate(ptr, bytes, align)
//      do_is_equal(other)              → bool
//
//  Resources embutidos na STL:
//  • monotonic_buffer_resource  → arena (bump allocator) — o mais rápido
//  • unsynchronized_pool_resource → pool para tamanhos variados
//  • synchronized_pool_resource   → versão thread-safe
//  • new_delete_resource()        → usa new/delete (padrão)
//  • null_memory_resource()       → sempre lança bad_alloc (teste)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory_resource>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <array>
#include <chrono>

namespace demo_pmr {

// ─────────────────────────────────────────────────────────────────────────────
//  1. monotonic_buffer_resource — arena na stack, sem heap
// ─────────────────────────────────────────────────────────────────────────────
void demo_monotonic() {
    fmt::print("\n── 4.1 monotonic_buffer_resource (arena na stack) ──\n");

    // Buffer na stack — sem alocação dinâmica alguma
    std::array<std::byte, 4096> buf;
    std::pmr::monotonic_buffer_resource arena(buf.data(), buf.size());

    // vector usando a arena — sem malloc
    std::pmr::vector<int> v(&arena);
    for (int i = 0; i < 10; ++i)
        v.push_back(i * i);

    fmt::print("  vector<int> com arena na stack:\n  ");
    for (int x : v) fmt::print("{} ", x);
    fmt::print("\n");

    // map também funciona
    std::pmr::map<std::string, int> m(&arena);
    m["alpha"] = 1;
    m["beta"]  = 2;
    m["gamma"] = 3;
    for (auto& [k, val] : m)
        fmt::print("  {}={}\n", k, val);

    // Quando arena sai de escopo, tudo liberado de uma vez
    // (sem chamar delete para cada elemento individualmente)
    fmt::print("  sem nenhum malloc/free para os elementos!\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Fallback chain — arena com fallback para heap
//     Se a arena encher, cai para o alocador seguinte
// ─────────────────────────────────────────────────────────────────────────────
void demo_fallback() {
    fmt::print("\n── 4.2 Fallback: arena pequena → heap ──\n");

    std::array<std::byte, 128> buf_pequeno; // arena pequena de propósito
    std::pmr::monotonic_buffer_resource arena(
        buf_pequeno.data(),
        buf_pequeno.size(),
        std::pmr::new_delete_resource()  // fallback: usa new/delete quando encher
    );

    std::pmr::vector<int> v(&arena);

    // Primeiros elementos usam a arena na stack
    for (int i = 0; i < 5; ++i) v.push_back(i);
    fmt::print("  5 elementos (provavelmente na arena)\n");

    // Quando a arena encher, automaticamente usa o fallback (heap)
    for (int i = 5; i < 50; ++i) v.push_back(i);
    fmt::print("  50 elementos (arena esgotou, caiu para heap)\n");
    fmt::print("  tamanho final: {}\n", v.size());
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. unsynchronized_pool_resource — pool para tamanhos variados
//     Mantém múltiplos pools internos, um por tamanho de bloco
// ─────────────────────────────────────────────────────────────────────────────
void demo_pool_resource() {
    fmt::print("\n── 4.3 unsynchronized_pool_resource ──\n");

    std::pmr::unsynchronized_pool_resource pool;

    // Múltiplos containers com tamanhos diferentes — todos usando o pool
    std::pmr::vector<int>         vi(&pool);
    std::pmr::vector<double>      vd(&pool);
    std::pmr::list<std::string>   ls(&pool);

    vi.push_back(1); vi.push_back(2); vi.push_back(3);
    vd.push_back(3.14); vd.push_back(2.71);
    ls.push_back("hello"); ls.push_back("pool");

    fmt::print("  vi: {} elementos\n", vi.size());
    fmt::print("  vd: {} elementos\n", vd.size());
    fmt::print("  ls: {} elementos\n", ls.size());
    fmt::print("  todos usando o mesmo pool sem fragmentação\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. memory_resource customizado — implementando a interface
//     Qualquer classe que herda de memory_resource pode ser usada como alocador
// ─────────────────────────────────────────────────────────────────────────────
class LoggingResource : public std::pmr::memory_resource {
public:
    explicit LoggingResource(std::pmr::memory_resource* upstream
                             = std::pmr::new_delete_resource())
        : upstream_(upstream) {}

    std::size_t total_alocado()   const { return total_alocado_; }
    std::size_t total_liberado()  const { return total_liberado_; }
    int         num_alocacoes()   const { return num_alloc_; }

private:
    void* do_allocate(std::size_t bytes, std::size_t align) override {
        void* ptr = upstream_->allocate(bytes, align);
        total_alocado_ += bytes;
        ++num_alloc_;
        fmt::print("    [alloc] {} bytes em {}\n", bytes, ptr);
        return ptr;
    }

    void do_deallocate(void* ptr, std::size_t bytes, std::size_t align) override {
        upstream_->deallocate(ptr, bytes, align);
        total_liberado_ += bytes;
        fmt::print("    [free]  {} bytes em {}\n", bytes, ptr);
    }

    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }

    std::pmr::memory_resource* upstream_;
    std::size_t total_alocado_  = 0;
    std::size_t total_liberado_ = 0;
    int         num_alloc_      = 0;
};

void demo_custom_resource() {
    fmt::print("\n── 4.4 memory_resource customizado (logging) ──\n");

    LoggingResource logger;

    {
        std::pmr::vector<int> v(&logger);
        v.reserve(4);
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
    } // v destruído → deallocate chamado

    fmt::print("  total alocado:  {} bytes\n", logger.total_alocado());
    fmt::print("  total liberado: {} bytes\n", logger.total_liberado());
    fmt::print("  num alocações:  {}\n", logger.num_alocacoes());
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. pmr vs heap — benchmark
// ─────────────────────────────────────────────────────────────────────────────
void demo_benchmark() {
    fmt::print("\n── 4.5 pmr monotonic vs heap — benchmark ──\n");

    constexpr int N = 100000;
    using Clock = std::chrono::high_resolution_clock;

    // Heap padrão
    {
        auto t0 = Clock::now();
        std::vector<int> v;
        v.reserve(N);
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      Clock::now() - t0).count();
        fmt::print("  std::vector (heap):       {}µs\n", us);
    }

    // pmr com monotonic (arena)
    {
        std::array<std::byte, sizeof(int) * N + 256> buf;
        std::pmr::monotonic_buffer_resource arena(buf.data(), buf.size());

        auto t0 = Clock::now();
        std::pmr::vector<int> v(&arena);
        v.reserve(N);
        for (int i = 0; i < N; ++i) v.push_back(i);
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      Clock::now() - t0).count();
        fmt::print("  pmr::vector (monotonic):  {}µs\n", us);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Tabela de quando usar cada resource
// ─────────────────────────────────────────────────────────────────────────────
void demo_guia() {
    fmt::print("\n── 4.6 Guia de escolha ──\n");
    fmt::print(
        "  ┌──────────────────────────────┬────────────────────────────────┐\n"
        "  │ Resource                     │ Quando usar                    │\n"
        "  ├──────────────────────────────┼────────────────────────────────┤\n"
        "  │ new_delete_resource()        │ padrão — sem requisito especial│\n"
        "  │ monotonic_buffer_resource    │ lifetime único, reset em lote  │\n"
        "  │                              │ (frame, request, parsing)      │\n"
        "  │ unsynchronized_pool_resource │ muitos objetos, tamanhos vars  │\n"
        "  │                              │ single-thread                  │\n"
        "  │ synchronized_pool_resource   │ mesmo, mas multi-thread        │\n"
        "  │ null_memory_resource()       │ testes: garante que não aloca  │\n"
        "  │ customizado                  │ logging, limite de memória,    │\n"
        "  │                              │ shared memory, mmap            │\n"
        "  └──────────────────────────────┴────────────────────────────────┘\n"
    );
}

inline void run() {
    demo_monotonic();
    demo_fallback();
    demo_pool_resource();
    demo_custom_resource();
    demo_benchmark();
    demo_guia();
}

} // namespace demo_pmr
