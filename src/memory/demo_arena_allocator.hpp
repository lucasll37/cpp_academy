// =============================================================================
//  src/memory/demo_arena_allocator.hpp
//
//  Arena Allocator (Bump Allocator) — o mais rápido possível
//  ──────────────────────────────────────────────────────────
//  Ideia: pré-aloca um bloco grande de memória.
//  Cada alocação apenas avança um ponteiro (bump).
//  Liberação individual NÃO existe — tudo é liberado de uma vez com reset().
//
//  alloc(n):
//    ptr = cursor
//    cursor += n  (alinhado)
//    return ptr
//    → literalmente uma soma, O(1), sem lock, sem busca
//
//  reset():
//    cursor = início
//    → "libera" tudo instantaneamente — sem chamar destrutores
//
//  Quando usar:
//  • Dados que vivem por um frame de jogo (partículas, draw calls)
//  • Parsing de um arquivo (AST temporária)
//  • Resposta de uma requisição HTTP (buffers temporários)
//  • Qualquer coisa com lifetime "tudo junto"
//
//  Quando NÃO usar:
//  • Objetos com lifetimes independentes
//  • Quando liberação individual importa
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <vector>
#include <cstring>
#include <functional>
#include <new>

namespace demo_arena_allocator {

// =============================================================================
//  Arena — bump allocator genérico
// =============================================================================
class Arena {
public:
    explicit Arena(std::size_t capacidade)
        : capacidade_(capacidade), usado_(0)
    {
        base_ = static_cast<std::byte*>(std::malloc(capacidade));
        assert(base_ && "Arena: falha na alocação inicial");
    }

    ~Arena() { std::free(base_); }

    Arena(const Arena&)            = delete;
    Arena& operator=(const Arena&) = delete;

    // ── alloc: avança o ponteiro, respeita alinhamento ───────────────────────
    void* alloc(std::size_t tamanho, std::size_t alinhamento = alignof(std::max_align_t)) {
        // Calcula offset necessário para alinhar
        std::size_t offset = alinhado(usado_, alinhamento);

        if (offset + tamanho > capacidade_) {
            fmt::print("    [Arena] ESGOTADA — pedido: {}, disponível: {}\n",
                       tamanho, capacidade_ - offset);
            return nullptr;
        }

        void* ptr = base_ + offset;
        usado_ = offset + tamanho;
        return ptr;
    }

    // ── alloc tipado — alloc + placement new ──────────────────────────────────
    template <typename T, typename... Args>
    T* criar(Args&&... args) {
        void* mem = alloc(sizeof(T), alignof(T));
        if (!mem) return nullptr;
        return new(mem) T(std::forward<Args>(args)...);
    }

    // ── reset: "libera" tudo — O(1) ──────────────────────────────────────────
    // ATENÇÃO: não chama destrutores — garanta que objetos foram destruídos antes
    void reset() {
        usado_ = 0;
    }

    // ── Diagnóstico ──────────────────────────────────────────────────────────
    std::size_t usado()       const { return usado_; }
    std::size_t disponivel()  const { return capacidade_ - usado_; }
    std::size_t capacidade()  const { return capacidade_; }
    float       uso_percent() const {
        return 100.0f * static_cast<float>(usado_) / capacidade_;
    }

private:
    // Arredonda 'n' para o próximo múltiplo de 'alinhamento'
    static std::size_t alinhado(std::size_t n, std::size_t alinhamento) {
        return (n + alinhamento - 1) & ~(alinhamento - 1);
    }

    std::byte*  base_;
    std::size_t capacidade_;
    std::size_t usado_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Arena básica — alloc e reset
// ─────────────────────────────────────────────────────────────────────────────
void demo_basico() {
    fmt::print("\n── 3.1 Arena básica ──\n");

    Arena arena(1024); // 1KB
    fmt::print("  arena: {} bytes\n", arena.capacidade());

    // Aloca vários tipos
    int*   n1 = static_cast<int*>(arena.alloc(sizeof(int)));
    float* f1 = static_cast<float*>(arena.alloc(sizeof(float), alignof(float)));
    char*  s1 = static_cast<char*>(arena.alloc(32));

    *n1 = 42;
    *f1 = 3.14f;
    std::strncpy(s1, "hello arena", 31);

    fmt::print("  *n1={}, *f1={:.2f}, s1='{}'\n", *n1, *f1, s1);
    fmt::print("  usado: {} bytes ({:.1f}%)\n", arena.usado(), arena.uso_percent());

    // reset: "libera" tudo em O(1) — ponteiro volta ao início
    arena.reset();
    fmt::print("  após reset: usado={} bytes\n", arena.usado());

    // Memória reutilizada imediatamente
    int* n2 = static_cast<int*>(arena.alloc(sizeof(int)));
    *n2 = 99;
    fmt::print("  n2 no mesmo endereço de n1? {} (reutilização)\n", n2 == n1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Arena tipada com criar<T>()
// ─────────────────────────────────────────────────────────────────────────────
void demo_tipada() {
    fmt::print("\n── 3.2 Arena com criar<T>() ──\n");

    struct No {
        int   valor;
        No*   esquerdo = nullptr;
        No*   direito  = nullptr;
        No(int v) : valor(v) {}
    };

    // Arena grande o suficiente para uma árvore
    Arena arena(sizeof(No) * 16);

    // Constrói uma árvore binária simples — tudo na arena
    No* raiz  = arena.criar<No>(10);
    No* esq   = arena.criar<No>(5);
    No* dir   = arena.criar<No>(15);
    No* ee    = arena.criar<No>(2);
    No* ed    = arena.criar<No>(7);

    raiz->esquerdo = esq;
    raiz->direito  = dir;
    esq->esquerdo  = ee;
    esq->direito   = ed;

    // Traversal inorder
    std::function<void(No*)> inorder = [&](No* n) {
        if (!n) return;
        inorder(n->esquerdo);
        fmt::print("{} ", n->valor);
        inorder(n->direito);
    };

    fmt::print("  inorder: ");
    inorder(raiz);
    fmt::print("\n");
    fmt::print("  usado: {} / {} bytes\n", arena.usado(), arena.capacidade());

    // "Libera" toda a árvore de uma vez — sem percorrer nós
    // (No não tem destruidor não-trivial — se tivesse, precisaria chamar manualmente)
    arena.reset();
    fmt::print("  reset: árvore inteira liberada em O(1)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Arena por frame — padrão de jogos
//     Cada frame usa a arena, no fim do frame faz reset
// ─────────────────────────────────────────────────────────────────────────────
void demo_por_frame() {
    fmt::print("\n── 3.3 Arena por frame (padrão de jogos) ──\n");

    struct DrawCall {
        float x, y, w, h;
        int   sprite_id;
        float alpha;
    };

    Arena arena_frame(sizeof(DrawCall) * 1000);

    for (int frame = 1; frame <= 3; ++frame) {
        // Início do frame: arena está limpa (ou acaba de ser resetada)
        int num_draws = frame * 3; // número variável por frame

        std::vector<DrawCall*> draws;
        for (int i = 0; i < num_draws; ++i) {
            DrawCall* dc = arena_frame.criar<DrawCall>();
            if (dc) {
                dc->x = static_cast<float>(i * 10);
                dc->y = 0.0f;
                dc->sprite_id = i;
                draws.push_back(dc);
            }
        }

        fmt::print("  frame {}: {} draw calls, {}B usados\n",
                   frame, draws.size(), arena_frame.usado());

        // Fim do frame: reset — sem chamar destruidor (DrawCall é POD)
        arena_frame.reset();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Arena vs heap — benchmark
// ─────────────────────────────────────────────────────────────────────────────
void demo_benchmark() {
    fmt::print("\n── 3.4 Arena vs heap — benchmark ──\n");

    struct Item {
        int a, b, c, d;
        Item() = default;
        Item(int v, int, int, int) : a(v), b(v), c(v), d(v) {}
    };
    constexpr int N = 100000;
    using Clock = std::chrono::high_resolution_clock;

    // Heap
    {
        auto t0 = Clock::now();
        std::vector<Item*> items;
        items.reserve(N);
        for (int i = 0; i < N; ++i)
            items.push_back(new Item{i, i, i, i});
        for (auto* p : items) delete p;
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      Clock::now() - t0).count();
        fmt::print("  heap  ({} new/delete): {}µs\n", N, us);
    }

    // Arena
    {
        Arena arena(sizeof(Item) * N + 64);
        auto t0 = Clock::now();
        std::vector<Item*> items;
        items.reserve(N);
        for (int i = 0; i < N; ++i)
            items.push_back(arena.criar<Item>(i, i, i, i));
        arena.reset(); // libera tudo — O(1)
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      Clock::now() - t0).count();
        fmt::print("  arena ({} criar+reset):{}µs\n", N, us);
    }
}

inline void run() {
    demo_basico();
    demo_tipada();
    demo_por_frame();
    demo_benchmark();
}

} // namespace demo_arena_allocator