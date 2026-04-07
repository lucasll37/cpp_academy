// =============================================================================
//  src/smart-pointers/demo_unique.hpp
//
//  std::unique_ptr — ownership EXCLUSIVO
//  ─────────────────────────────────────
//  • Exatamente um dono em qualquer instante.
//  • Destruído automaticamente quando sai de escopo (RAII).
//  • NÃO pode ser copiado — apenas MOVIDO (transfer de ownership).
//  • Overhead zero em relação a um raw pointer (sem ref-count).
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <vector>
#include <cstdio>   // fopen / fclose

namespace demo_unique {

// ── Tipo simples para observar construção/destruição ─────────────────────────
struct Recurso {
    int id;
    explicit Recurso(int i) : id(i) {
        fmt::print("  [Recurso #{}] construído\n", id);
    }
    ~Recurso() {
        fmt::print("  [Recurso #{}] destruído\n", id);
    }
    void usar() const { fmt::print("  [Recurso #{}] em uso\n", id); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Criação e destruição automática
// ─────────────────────────────────────────────────────────────────────────────
void demo_basico() {
    fmt::print("\n── 1.1 Criação / destruição automática ──\n");

    {
        // make_unique é a forma preferida: nunca vaza em caso de exceção
        auto p = std::make_unique<Recurso>(10);
        p->usar();
        fmt::print("  p.get() = {}\n", static_cast<void*>(p.get()));
    } // <- destruidor chamado aqui automaticamente
    fmt::print("  (bloco encerrado — recurso já foi liberado)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Move semantics — transferência de ownership
// ─────────────────────────────────────────────────────────────────────────────
void demo_move() {
    fmt::print("\n── 1.2 Move semantics ──\n");

    auto original = std::make_unique<Recurso>(20);
    fmt::print("  original válido? {}\n", original != nullptr);

    // std::move transfere a posse; original vira nullptr
    auto novo_dono = std::move(original);
    fmt::print("  original válido após move? {}\n", original != nullptr);
    fmt::print("  novo_dono válido? {}\n", novo_dono != nullptr);
    novo_dono->usar();
    // novo_dono destruído ao sair deste escopo
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Passagem para funções
//     (a) por referência  → apenas empréstimo, sem transferência
//     (b) por valor (move) → transferência permanente de ownership
// ─────────────────────────────────────────────────────────────────────────────
void apenas_usa(const Recurso& r) {
    fmt::print("  apenas_usa: recurso #{}\n", r.id);
}

void toma_posse(std::unique_ptr<Recurso> p) {
    fmt::print("  toma_posse: recebi recurso #{}\n", p->id);
    // p destruído ao sair desta função
}

void demo_passagem() {
    fmt::print("\n── 1.3 Passagem para funções ──\n");

    auto p = std::make_unique<Recurso>(30);

    // Empréstimo — derreferência ou .get()
    apenas_usa(*p);
    fmt::print("  p ainda válido após empréstimo? {}\n", p != nullptr);

    // Transferência — caller perde o ponteiro
    toma_posse(std::move(p));
    fmt::print("  p válido após transferência? {}\n", p != nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. unique_ptr para arrays
// ─────────────────────────────────────────────────────────────────────────────
void demo_array() {
    fmt::print("\n── 1.4 Array ownership ──\n");

    // unique_ptr<T[]> usa delete[] corretamente
    constexpr int N = 4;
    auto arr = std::make_unique<int[]>(N);

    for (int i = 0; i < N; ++i) arr[i] = i * i;

    fmt::print("  arr = [ ");
    for (int i = 0; i < N; ++i) fmt::print("{} ", arr[i]);
    fmt::print("]\n");
}
// ─────────────────────────────────────────────────────────────────────────────
//  5. Custom deleter — quando "liberar" não é só "delete"
//     Exemplo: gerenciar um FILE* com RAII
// ─────────────────────────────────────────────────────────────────────────────
void demo_custom_deleter() {
    fmt::print("\n── 1.5 Custom deleter (FILE* com RAII) ──\n");

    // O deleter é parte do TIPO: unique_ptr<FILE, decltype(&fclose)>
    auto fechar_arquivo = [](FILE* f) {
        if (f) {
            fmt::print("  [deleter] fclose chamado\n");
            std::fclose(f);
        }
    };

    {
        std::unique_ptr<FILE, decltype(fechar_arquivo)>
            arquivo(std::fopen("/dev/null", "w"), fechar_arquivo);

        if (arquivo) {
            fmt::print("  arquivo aberto com sucesso\n");
        }
    } // <- fechar_arquivo chamado aqui automaticamente
    fmt::print("  (bloco encerrado — arquivo fechado)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. release() e reset() — controle explícito
// ─────────────────────────────────────────────────────────────────────────────
void demo_release_reset() {
    fmt::print("\n── 1.6 release() e reset() ──\n");

    auto p = std::make_unique<Recurso>(40);

    // release(): abandona ownership SEM destruir — você é responsável depois!
    Recurso* raw = p.release();
    fmt::print("  p válido após release()? {}\n", p != nullptr);
    fmt::print("  raw pointer = {}\n", static_cast<void*>(raw));
    delete raw;  // obrigatório — senão vaza!
    fmt::print("  (delete manual feito)\n");

    // reset(): destrói o objeto atual e opcionalmente assume um novo
    auto q = std::make_unique<Recurso>(50);
    fmt::print("  chamando reset() em q...\n");
    q.reset();   // destrói Recurso(50)
    fmt::print("  q válido após reset()? {}\n", q != nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada do demo
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_move();
    demo_passagem();
    demo_array();
    demo_custom_deleter();
    demo_release_reset();
}

} // namespace demo_unique
