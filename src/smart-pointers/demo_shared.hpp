// =============================================================================
//  src/smart-pointers/demo_shared.hpp
//
//  std::shared_ptr — ownership COMPARTILHADO
//  ──────────────────────────────────────────
//  • Múltiplos ponteiros podem apontar para o mesmo objeto.
//  • Um contador de referências (ref-count) é mantido internamente.
//  • Objeto destruído quando o ÚLTIMO shared_ptr que o aponta é destruído.
//  • Pequeno overhead: incremento/decremento atômico do ref-count.
//  • CUIDADO: ciclos de referência causam memory leak! (ver demo_weak.hpp)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <vector>
#include <string>

namespace demo_shared {

// ── Tipo com rastreamento de vida ─────────────────────────────────────────────
struct Nodo {
    std::string nome;
    explicit Nodo(std::string n) : nome(std::move(n)) {
        fmt::print("  [Nodo '{}'] construído\n", nome);
    }
    ~Nodo() {
        fmt::print("  [Nodo '{}'] destruído\n", nome);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Criação e ref-count básico
// ─────────────────────────────────────────────────────────────────────────────
void demo_basico() {
    fmt::print("\n── 2.1 Criação e ref-count ──\n");

    auto p1 = std::make_shared<Nodo>("A");
    fmt::print("  use_count após make_shared: {}\n", p1.use_count()); // 1

    {
        auto p2 = p1; // cópia → compartilha ownership
        fmt::print("  use_count com p2: {}\n", p1.use_count());       // 2

        auto p3 = p1; // mais uma cópia
        fmt::print("  use_count com p2+p3: {}\n", p1.use_count());    // 3

    } // p2 e p3 destruídos → ref-count volta para 1
    fmt::print("  use_count após sair do bloco: {}\n", p1.use_count()); // 1

    // p1 destruído aqui → ref-count = 0 → objeto destruído
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. make_shared vs new (por que make_shared é preferível)
// ─────────────────────────────────────────────────────────────────────────────
void demo_make_shared() {
    fmt::print("\n── 2.2 make_shared vs shared_ptr(new ...) ──\n");

    // ✗ Evitar — duas alocações: objeto + bloco de controle separados
    std::shared_ptr<Nodo> p_old(new Nodo("old-style"));
    fmt::print("  (criado com new — 2 alocações)\n");

    // ✓ Preferir — única alocação: objeto + bloco de controle juntos
    auto p_new = std::make_shared<Nodo>("make_shared-style");
    fmt::print("  (criado com make_shared — 1 alocação, mais eficiente)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. shared_ptr em container
// ─────────────────────────────────────────────────────────────────────────────
void demo_container() {
    fmt::print("\n── 2.3 shared_ptr em vector ──\n");

    std::vector<std::shared_ptr<Nodo>> lista;

    auto n1 = std::make_shared<Nodo>("B");
    auto n2 = std::make_shared<Nodo>("C");

    lista.push_back(n1);   // ref-count de n1: 2
    lista.push_back(n2);
    lista.push_back(n1);   // ref-count de n1: 3 (duas entradas no vector!)

    fmt::print("  n1.use_count() = {} (original + 2 no vector)\n", n1.use_count());

    lista.clear(); // remove todas as entradas; n1 ref-count volta a 1
    fmt::print("  n1.use_count() após lista.clear() = {}\n", n1.use_count());
    // n1 e n2 destruídos quando saem do escopo desta função
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. shared_ptr com custom deleter
//     Diferente do unique_ptr, o deleter NÃO faz parte do tipo — maior
//     flexibilidade para armazenar em containers heterogêneos.
// ─────────────────────────────────────────────────────────────────────────────
void demo_custom_deleter() {
    fmt::print("\n── 2.4 Custom deleter em shared_ptr ──\n");

    auto deleter = [](Nodo* p) {
        fmt::print("  [custom deleter] liberando Nodo '{}'\n", p->nome);
        delete p;
    };

    // Note: o tipo é shared_ptr<Nodo> — deleter não aparece no tipo!
    std::shared_ptr<Nodo> p(new Nodo("D"), deleter);
    fmt::print("  p criado com custom deleter\n");
    // deleter chamado ao destruir p
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. shared_from_this — quando um objeto precisa criar um shared_ptr de si
//     mesmo de forma segura (sem criar um segundo bloco de controle)
// ─────────────────────────────────────────────────────────────────────────────
struct Observavel : std::enable_shared_from_this<Observavel> {
    std::string nome;
    explicit Observavel(std::string n) : nome(std::move(n)) {
        fmt::print("  [Observavel '{}'] construído\n", nome);
    }
    ~Observavel() {
        fmt::print("  [Observavel '{}'] destruído\n", nome);
    }

    // Retorna um shared_ptr para *this sem criar novo bloco de controle
    std::shared_ptr<Observavel> get_self() {
        return shared_from_this();
    }
};

void demo_shared_from_this() {
    fmt::print("\n── 2.5 enable_shared_from_this ──\n");

    auto obj = std::make_shared<Observavel>("E");
    fmt::print("  use_count original: {}\n", obj.use_count());        // 1

    {
        auto self = obj->get_self(); // seguro — mesmo bloco de controle
        fmt::print("  use_count com self: {}\n", obj.use_count());    // 2
    }
    fmt::print("  use_count após destruir self: {}\n", obj.use_count()); // 1
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. alias constructor — shared_ptr que aponta para membro interno
// ─────────────────────────────────────────────────────────────────────────────
struct Motor {
    int rpm = 3000;
};

struct Carro {
    Motor motor;
    std::string modelo = "Fusca";
    Carro() { fmt::print("  [Carro '{}'] construído\n", modelo); }
    ~Carro() { fmt::print("  [Carro '{}'] destruído\n", modelo); }
};

void demo_alias() {
    fmt::print("\n── 2.6 Alias constructor (ponteiro para membro) ──\n");

    auto carro = std::make_shared<Carro>();

    // motor_ptr mantém o Carro vivo mas aponta para o Motor interno!
    std::shared_ptr<Motor> motor_ptr(carro, &carro->motor);

    fmt::print("  carro.use_count() = {} (carro + motor_ptr)\n", carro.use_count());
    fmt::print("  motor_ptr->rpm = {}\n", motor_ptr->rpm);

    carro.reset(); // solta a referência do 'carro'
    fmt::print("  carro.use_count() após reset: {}\n", motor_ptr.use_count());
    // Carro ainda vivo! motor_ptr mantém o bloco de controle ativo.
    fmt::print("  motor_ptr->rpm ainda acessível: {}\n", motor_ptr->rpm);
    // Carro destruído aqui quando motor_ptr sai de escopo
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada do demo
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_make_shared();
    demo_container();
    demo_custom_deleter();
    demo_shared_from_this();
    demo_alias();
}

} // namespace demo_shared
