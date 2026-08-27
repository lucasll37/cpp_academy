// =============================================================================
//  src/memory/demo_new_delete.hpp
//
//  new / delete — o que realmente acontece
//  ─────────────────────────────────────────
//  new e delete não são funções — são operadores com comportamento em duas fases.
//
//  new T(args):
//    1. operator new(sizeof(T))   → aloca bytes (chama malloc internamente)
//    2. T::T(args)                → construtor roda na memória obtida
//
//  delete p:
//    1. p->~T()                   → destruidor
//    2. operator delete(p)        → libera bytes (chama free internamente)
//
//  Tópicos:
//  • Sobrecarga de operator new/delete — rastrear alocações
//  • placement new — construir objeto em memória já alocada
//  • new[] vs delete[] — arrays e o erro clássico
//  • nothrow new — sem exceção, retorna nullptr
//  • Stack vs heap — diferença de custo medida
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstdlib>
#include <cstddef>
#include <new>
#include <chrono>
#include <vector>

namespace demo_new_delete {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Rastreando alocações com operator new/delete global sobrescrito
//     Útil para detectar leaks e medir pressão no heap
// ─────────────────────────────────────────────────────────────────────────────

// Contador global de alocações — declarado aqui, definido uma vez
static std::size_t total_alocado    = 0;
static std::size_t total_liberado   = 0;
static std::size_t num_alocacoes    = 0;
static std::size_t num_liberacoes   = 0;
static bool        rastreando       = false;

// Sobrescrevemos operator new/delete LOCALMENTE com wrappers
// (Em produção, isso seria feito globalmente em um .cpp separado)
void* meu_malloc(std::size_t n) {
    void* ptr = std::malloc(n);
    if (rastreando && ptr) {
        total_alocado += n;
        ++num_alocacoes;
    }
    return ptr;
}

void meu_free(void* ptr, std::size_t n) {
    if (rastreando && ptr) {
        total_liberado += n;
        ++num_liberacoes;
    }
    std::free(ptr);
}

void demo_rastreamento() {
    fmt::print("\n── 1.1 Rastreando alocações (operator new/delete) ──\n");

    // Demonstra as duas fases manualmente para ser didático
    struct Recurso {
        int  id;
        char nome[32];
        Recurso(int i, const char* n) : id(i) {
            std::snprintf(nome, sizeof(nome), "%s", n);
            fmt::print("    [ctor] Recurso id={}\n", id);
        }
        ~Recurso() {
            fmt::print("    [dtor] Recurso id={}\n", id);
        }
    };

    fmt::print("  Fase 1: alocação de bytes (operator new)\n");
    void* mem = meu_malloc(sizeof(Recurso));
    fmt::print("    {} bytes alocados em {}\n", sizeof(Recurso), mem);

    fmt::print("  Fase 2: construção (placement new)\n");
    Recurso* r = new(mem) Recurso(42, "teste");  // constrói na memória já alocada

    fmt::print("  r->id={}, r->nome='{}'\n", r->id, r->nome);

    fmt::print("  Fase 3: destruição manual + liberação\n");
    r->~Recurso();                    // destruidor manual (necessário com placement new)
    meu_free(mem, sizeof(Recurso));   // libera a memória

    fmt::print("  (equivalente ao que new/delete fazem automaticamente)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Placement new — construir objeto em memória pré-alocada
//     Caso de uso real: pool allocator, shared memory, memory-mapped files
// ─────────────────────────────────────────────────────────────────────────────
void demo_placement_new() {
    fmt::print("\n── 1.2 Placement new ──\n");

    struct Vec3 {
        float x, y, z;
        Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
        ~Vec3() { fmt::print("    ~Vec3({:.1f},{:.1f},{:.1f})\n", x, y, z); }
    };

    // Buffer na stack — sem alocação dinâmica
    alignas(Vec3) std::byte buffer[sizeof(Vec3)];

    fmt::print("  buffer na stack: {} bytes em {}\n",
               sizeof(buffer), static_cast<void*>(buffer));

    // Constrói Vec3 dentro do buffer — zero malloc
    Vec3* v = new(buffer) Vec3(1.0f, 2.0f, 3.0f);
    fmt::print("  Vec3 em {}: ({:.1f},{:.1f},{:.1f})\n",
               static_cast<void*>(v), v->x, v->y, v->z);

    // IMPORTANTE: com placement new, destruidor deve ser chamado manualmente
    // delete v seria ERRADO — tentaria liberar a stack
    v->~Vec3();

    // alignas é necessário: objetos têm requisitos de alinhamento
    // float requer alinhamento de 4 bytes, por exemplo
    fmt::print("  alignof(Vec3) = {}, sizeof(Vec3) = {}\n",
               alignof(Vec3), sizeof(Vec3));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. new[] e delete[] — o erro clássico de misturar
// ─────────────────────────────────────────────────────────────────────────────
void demo_array_new() {
    fmt::print("\n── 1.3 new[] vs new — não misture! ──\n");

    // new[] aloca N objetos E guarda N em metadado interno
    // delete[] usa esse metadado para chamar N destrutores
    int* arr = new int[5]{1, 2, 3, 4, 5};
    fmt::print("  arr[0..4]: ");
    for (int i = 0; i < 5; ++i) fmt::print("{} ", arr[i]);
    fmt::print("\n");

    delete[] arr;  // ← CORRETO: usa delete[] para new[]
    // delete arr;  // ← ERRADO: undefined behavior — só chama 1 destruidor

    // unique_ptr<T[]> — faz a coisa certa automaticamente
    auto arr2 = std::unique_ptr<int[]>(new int[5]{10, 20, 30, 40, 50});
    fmt::print("  arr2[2] = {}\n", arr2[2]);
    // delete[] chamado automaticamente no destruidor de unique_ptr<T[]>

    // Preferível em código moderno: std::vector ou std::array
    std::vector<int> vec = {100, 200, 300};
    fmt::print("  vector: {} elementos, sem new/delete manual\n", vec.size());
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. nothrow new — sem exceção quando memória acaba
// ─────────────────────────────────────────────────────────────────────────────
void demo_nothrow() {
    fmt::print("\n── 1.4 nothrow new ──\n");

    // new padrão: lança std::bad_alloc se memória acabar
    // new(std::nothrow): retorna nullptr em vez de lançar

    int* p = new(std::nothrow) int[1];
    if (p) {
        fmt::print("  alocação pequena bem-sucedida\n");
        delete[] p;
    }

    // Tentativa de alocação gigante — sem nothrow causaria std::bad_alloc
    constexpr std::size_t GIGANTE = std::size_t(1) << 40; // 1 TB
    int* enorme = new(std::nothrow) int[GIGANTE];
    if (!enorme) {
        fmt::print("  alocação de {}GB falhou graciosamente (nullptr)\n",
                   GIGANTE / (1024*1024*1024));
    }
    // sem nothrow, o programa terminaria com exceção aqui
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Stack vs Heap — custo real medido
// ─────────────────────────────────────────────────────────────────────────────
void demo_stack_vs_heap() {
    fmt::print("\n── 1.5 Stack vs Heap — custo medido ──\n");

    constexpr int N = 100000;
    using Clock = std::chrono::high_resolution_clock;

    // Stack: alocação é apenas mover o stack pointer
    {
        auto t0 = Clock::now();
        volatile int soma = 0;
        for (int i = 0; i < N; ++i) {
            int x = i * 2;   // stack — zero custo de alocação
            soma += x;
        }
        auto t1 = Clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        fmt::print("  stack  ({} ints locais): {}µs\n", N, us);
        (void)soma;
    }

    // Heap: cada new chama malloc, potencialmente syscall
    {
        auto t0 = Clock::now();
        volatile int soma = 0;
        for (int i = 0; i < N; ++i) {
            int* x = new int(i * 2);  // heap — malloc por iteração
            soma += *x;
            delete x;
        }
        auto t1 = Clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        fmt::print("  heap   ({} new/delete):  {}µs\n", N, us);
        (void)soma;
    }

    fmt::print("  (heap tipicamente 10-100x mais lento por alocação individual)\n");
}

inline void run() {
    demo_rastreamento();
    demo_placement_new();
    demo_array_new();
    demo_nothrow();
    demo_stack_vs_heap();
}

} // namespace demo_new_delete
