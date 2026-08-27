// =============================================================================
//  src/memory/demo_pool_allocator.hpp
//
//  Pool Allocator — blocos de tamanho fixo, zero fragmentação
//  ────────────────────────────────────────────────────────────
//  Problema do malloc para objetos pequenos e frequentes:
//  • Overhead de metadados por bloco (~16 bytes por alloc no glibc)
//  • Fragmentação: buracos entre blocos libertos
//  • Custo de busca: malloc percorre free list para achar bloco adequado
//  • Sincronização: malloc é thread-safe → lock em cada chamada
//
//  Pool Allocator resolve isso para um ÚNICO tamanho de objeto:
//  • Pré-aloca N blocos contíguos de tamanho sizeof(T)
//  • alloc() → O(1): retira da free list interna
//  • free()  → O(1): devolve à free list interna
//  • Sem fragmentação: todos os blocos têm o mesmo tamanho
//  • Cache-friendly: blocos contíguos na memória
//
//  Casos de uso reais:
//  • Partículas em jogos (criar/destruir milhares por frame)
//  • Nós de árvore / grafo (tamanho fixo, muitas instâncias)
//  • Mensagens de rede (tamanho máximo conhecido)
//  • ComponentStore do ECS (o que você já implementou!)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <vector>
#include <new>

namespace demo_pool_allocator {

// =============================================================================
//  PoolAllocator<T, N> — pool tipado para N objetos do tipo T
//
//  Layout de memória:
//  ┌────────────────────────────────────────────────────────┐
//  │ bloco[0] │ bloco[1] │ bloco[2] │ ... │ bloco[N-1]     │
//  └────────────────────────────────────────────────────────┘
//  Blocos livres formam uma linked list usando o próprio espaço do bloco:
//  livre → [ptr_próximo | ....] → [ptr_próximo | ....] → nullptr
// =============================================================================
template <typename T, std::size_t N>
class PoolAllocator {
    // Cada bloco precisa caber pelo menos um ponteiro (para a free list)
    static constexpr std::size_t BLOCO = sizeof(T) >= sizeof(void*) ?
                                         sizeof(T) : sizeof(void*);
public:
    PoolAllocator() {
        // Aloca o pool inteiro de uma vez — N blocos contíguos
        pool_ = static_cast<std::byte*>(
            std::aligned_alloc(alignof(T), N * BLOCO));
        assert(pool_ && "PoolAllocator: falha na alocação inicial");

        // Inicializa a free list: cada bloco aponta para o próximo
        livre_ = pool_;
        for (std::size_t i = 0; i < N - 1; ++i) {
            void** ptr = reinterpret_cast<void**>(pool_ + i * BLOCO);
            *ptr = pool_ + (i + 1) * BLOCO;
        }
        // Último bloco aponta para nullptr
        void** ultimo = reinterpret_cast<void**>(pool_ + (N - 1) * BLOCO);
        *ultimo = nullptr;

        fmt::print("    pool criado: {} blocos de {} bytes = {} bytes totais\n",
                   N, BLOCO, N * BLOCO);
    }

    ~PoolAllocator() {
        std::free(pool_);
    }

    // Não copiável — ownership do pool é exclusivo
    PoolAllocator(const PoolAllocator&)            = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    // ── alloc(): retira o primeiro bloco da free list — O(1) ─────────────────
    T* alloc() {
        if (!livre_) return nullptr; // pool esgotado

        void* bloco = livre_;
        livre_ = *reinterpret_cast<void**>(livre_); // avança free list
        ++em_uso_;
        return static_cast<T*>(bloco); // memória crua — construtor não chamado ainda
    }

    // ── free(): devolve bloco à free list — O(1) ─────────────────────────────
    void free(T* ptr) {
        // Devolve à frente da free list
        *reinterpret_cast<void**>(ptr) = livre_;
        livre_ = ptr;
        --em_uso_;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────
    std::size_t em_uso()    const { return em_uso_; }
    std::size_t capacidade() const { return N; }
    std::size_t disponiveis() const { return N - em_uso_; }

    bool pertence(T* ptr) const {
        auto p = reinterpret_cast<std::byte*>(ptr);
        return p >= pool_ && p < pool_ + N * BLOCO;
    }

private:
    std::byte*  pool_   = nullptr;
    void*       livre_  = nullptr;
    std::size_t em_uso_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Pool básico — alloc/free O(1)
// ─────────────────────────────────────────────────────────────────────────────
void demo_basico() {
    fmt::print("\n── 2.1 Pool básico ──\n");

    struct Particula {
        float x, y, vx, vy;
        float vida;
        int   cor;
    };

    PoolAllocator<Particula, 8> pool;
    fmt::print("  disponíveis: {}\n", pool.disponiveis());

    // Aloca 3 partículas
    Particula* p1 = pool.alloc();
    Particula* p2 = pool.alloc();
    Particula* p3 = pool.alloc();

    // Constrói manualmente (ou usa placement new)
    new(p1) Particula{0.0f, 0.0f, 1.0f, 0.5f, 3.0f, 0xFF0000};
    new(p2) Particula{5.0f, 5.0f, 0.0f, 1.0f, 2.0f, 0x00FF00};
    new(p3) Particula{10.f, 0.0f, -1.f, 0.0f, 1.0f, 0x0000FF};

    fmt::print("  em uso: {}, disponíveis: {}\n", pool.em_uso(), pool.disponiveis());
    fmt::print("  p1 pertence ao pool? {}\n", pool.pertence(p1));

    // Libera p2 — bloco volta para a free list
    p2->~Particula();
    pool.free(p2);
    fmt::print("  após free(p2) — disponíveis: {}\n", pool.disponiveis());

    // p4 reutiliza o bloco de p2
    Particula* p4 = pool.alloc();
    fmt::print("  p4 == antigo p2? {} (reutilização de bloco)\n", p4 == p2);

    p1->~Particula();
    p3->~Particula();
    p4->~Particula();
    pool.free(p1);
    pool.free(p3);
    pool.free(p4);
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Pool vs heap — benchmark de performance
// ─────────────────────────────────────────────────────────────────────────────
void demo_benchmark() {
    fmt::print("\n── 2.2 Pool vs heap — benchmark ──\n");

    struct No { int valor; No* esquerdo; No* direito; };
    constexpr int N = 50000;
    using Clock = std::chrono::high_resolution_clock;

    // ── Heap (new/delete) ─────────────────────────────────────────────────────
    {
        auto t0 = Clock::now();
        std::vector<No*> nos;
        nos.reserve(N);
        for (int i = 0; i < N; ++i)
            nos.push_back(new No{i, nullptr, nullptr});
        for (auto* n : nos) delete n;
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      Clock::now() - t0).count();
        fmt::print("  heap  ({} new/delete): {}µs\n", N, us);
    }

    // ── Pool ──────────────────────────────────────────────────────────────────
    {
        PoolAllocator<No, N> pool;
        auto t0 = Clock::now();
        std::vector<No*> nos;
        nos.reserve(N);
        for (int i = 0; i < N; ++i) {
            No* n = pool.alloc();
            new(n) No{i, nullptr, nullptr};
            nos.push_back(n);
        }
        for (auto* n : nos) {
            n->~No();
            pool.free(n);
        }
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      Clock::now() - t0).count();
        fmt::print("  pool  ({} alloc/free): {}µs\n", N, us);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Pool com wrapper RAII — alloc + construtor, free + destruidor
// ─────────────────────────────────────────────────────────────────────────────
template <typename T, std::size_t N>
class PoolRaii {
public:
    // acquire<Args...>: alloc + placement new — retorna unique_ptr com deleter
    template <typename... Args>
    auto acquire(Args&&... args) {
        T* ptr = pool_.alloc();
        if (!ptr) throw std::bad_alloc{};
        new(ptr) T(std::forward<Args>(args)...);

        // Deleter customizado: destruidor + pool.free
        auto deleter = [this](T* p) {
            p->~T();
            pool_.free(p);
        };
        return std::unique_ptr<T, decltype(deleter)>(ptr, deleter);
    }

    std::size_t em_uso() const { return pool_.em_uso(); }

private:
    PoolAllocator<T, N> pool_;
};

void demo_raii() {
    fmt::print("\n── 2.3 Pool com RAII (unique_ptr + deleter customizado) ──\n");

    struct Conexao {
        int id;
        explicit Conexao(int i) : id(i) {
            fmt::print("    [Conexao {}] aberta\n", id);
        }
        ~Conexao() {
            fmt::print("    [Conexao {}] fechada\n", id);
        }
    };

    PoolRaii<Conexao, 4> pool;

    {
        auto c1 = pool.acquire(1);
        auto c2 = pool.acquire(2);
        fmt::print("  em uso: {}\n", pool.em_uso());
        // c1 e c2 destruídos aqui → destruidor + pool.free automático
    }
    fmt::print("  em uso após escopo: {}\n", pool.em_uso());
}

inline void run() {
    demo_basico();
    demo_benchmark();
    demo_raii();
}

} // namespace demo_pool_allocator
