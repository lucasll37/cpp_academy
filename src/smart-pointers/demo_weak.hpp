// =============================================================================
//  src/smart-pointers/demo_weak.hpp
//
//  std::weak_ptr — observador SEM ownership
//  ─────────────────────────────────────────
//  • Aponta para um objeto gerenciado por shared_ptr, mas NÃO incrementa
//    o ref-count → não mantém o objeto vivo.
//  • Principal uso: QUEBRAR CICLOS de referência que causariam memory leak.
//  • Para usar o objeto, é preciso "promover" para shared_ptr via lock().
//  • Se o objeto já foi destruído, lock() retorna nullptr.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <string>

namespace demo_weak {

// ── Tipo observável ───────────────────────────────────────────────────────────
struct Sensor {
    std::string nome;
    double valor = 0.0;
    explicit Sensor(std::string n, double v) : nome(std::move(n)), valor(v) {
        fmt::print("  [Sensor '{}'] construído\n", nome);
    }
    ~Sensor() {
        fmt::print("  [Sensor '{}'] destruído\n", nome);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. weak_ptr básico — lock() e expired()
// ─────────────────────────────────────────────────────────────────────────────
void demo_basico() {
    fmt::print("\n── 3.1 weak_ptr básico ──\n");

    std::weak_ptr<Sensor> observador;

    {
        auto forte = std::make_shared<Sensor>("temperatura", 36.5);
        observador = forte; // weak_ptr não altera use_count

        fmt::print("  forte.use_count() = {} (weak não conta)\n",
                   forte.use_count()); // 1, não 2!

        // Para usar o objeto: promover para shared_ptr com lock()
        if (auto temp = observador.lock()) {  // temp é shared_ptr<Sensor>
            fmt::print("  Sensor acessado via lock(): {} = {:.1f}\n",
                       temp->nome, temp->valor);
        }
    } // 'forte' destruído aqui → Sensor liberado

    // Depois que o último shared_ptr morreu, lock() retorna nullptr
    fmt::print("  observador.expired() = {}\n", observador.expired()); // true
    if (auto temp = observador.lock()) {
        fmt::print("  ERRO: não deveria chegar aqui\n");
    } else {
        fmt::print("  lock() retornou nullptr — objeto foi destruído\n");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O PROBLEMA: ciclo de referência com shared_ptr puro
//     A → B e B → A usando shared_ptr: nenhum é destruído!
// ─────────────────────────────────────────────────────────────────────────────

// ⚠ Demonstração de LEAK intencional (objetos nunca destruídos)
struct NodoComCiclo {
    std::string nome;
    std::shared_ptr<NodoComCiclo> proximo; // <- o problema

    explicit NodoComCiclo(std::string n) : nome(std::move(n)) {
        fmt::print("  [NodoComCiclo '{}'] construído\n", nome);
    }
    ~NodoComCiclo() {
        fmt::print("  [NodoComCiclo '{}'] destruído\n", nome);
    }
};

void demo_ciclo_com_leak() {
    fmt::print("\n── 3.2 Ciclo com shared_ptr → MEMORY LEAK ──\n");
    fmt::print("  (os destruidores abaixo NÃO serão chamados!)\n");

    auto a = std::make_shared<NodoComCiclo>("A-ciclo");
    auto b = std::make_shared<NodoComCiclo>("B-ciclo");

    a->proximo = b;  // A aponta para B → use_count(b) = 2
    b->proximo = a;  // B aponta para A → use_count(a) = 2

    fmt::print("  use_count(a) = {}\n", a.use_count()); // 2
    fmt::print("  use_count(b) = {}\n", b.use_count()); // 2
    // 'a' e 'b' saem de escopo → use_count cai para 1 (não zero!)
    // → objetos nunca destruídos → LEAK
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. A SOLUÇÃO: quebrar o ciclo com weak_ptr
// ─────────────────────────────────────────────────────────────────────────────
struct NodoSemCiclo {
    std::string nome;
    std::weak_ptr<NodoSemCiclo> proximo; // <- weak não segura o ciclo

    explicit NodoSemCiclo(std::string n) : nome(std::move(n)) {
        fmt::print("  [NodoSemCiclo '{}'] construído\n", nome);
    }
    ~NodoSemCiclo() {
        fmt::print("  [NodoSemCiclo '{}'] destruído\n", nome);
    }
};

void demo_ciclo_resolvido() {
    fmt::print("\n── 3.3 Ciclo com weak_ptr → SEM LEAK ──\n");

    auto a = std::make_shared<NodoSemCiclo>("A-ok");
    auto b = std::make_shared<NodoSemCiclo>("B-ok");

    a->proximo = b;  // weak não altera use_count
    b->proximo = a;  // weak não altera use_count

    fmt::print("  use_count(a) = {}\n", a.use_count()); // 1
    fmt::print("  use_count(b) = {}\n", b.use_count()); // 1

    // Navegar pela lista ainda é possível via lock()
    if (auto next = a->proximo.lock()) {
        fmt::print("  a->proximo = '{}'\n", next->nome);
    }
    // 'a' e 'b' saem de escopo → use_count = 0 → ambos destruídos corretamente
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Padrão Observer com weak_ptr — observadores opcionais
//     O subject não precisa saber se o observer ainda existe
// ─────────────────────────────────────────────────────────────────────────────
struct Observer {
    std::string tag;
    explicit Observer(std::string t) : tag(std::move(t)) {
        fmt::print("  [Observer '{}'] construído\n", tag);
    }
    ~Observer() {
        fmt::print("  [Observer '{}'] destruído\n", tag); 
    }
    void notificar(double val) const {
        fmt::print("  Observer '{}' recebeu: {:.2f}\n", tag, val);
    }
};

struct Subject {
    std::vector<std::weak_ptr<Observer>> observers;

    void registrar(std::shared_ptr<Observer> obs) {
        observers.push_back(obs);
    }

    void notificar_todos(double val) {
        fmt::print("  Notificando observers (mortos serão removidos)...\n");
        // Percorre e remove automaticamente os que morreram
        auto it = observers.begin();
        while (it != observers.end()) {
            if (auto obs = it->lock()) {
                obs->notificar(val);
                ++it;
            } else {
                fmt::print("  (observer morto removido)\n");
                it = observers.erase(it);
            }
        }
    }
};

void demo_observer() {
    fmt::print("\n── 3.4 Padrão Observer com weak_ptr ──\n");

    Subject subject;

    auto obs1 = std::make_shared<Observer>("Logger");
    auto obs2 = std::make_shared<Observer>("Dashboard");

    subject.registrar(obs1);
    subject.registrar(obs2);

    subject.notificar_todos(42.0);

    fmt::print("  (destruindo Dashboard...)\n");
    obs2.reset(); // mata obs2

    subject.notificar_todos(99.0); // obs2 será detectado como morto
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada do demo
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_basico();
    demo_ciclo_com_leak();    // ← intencional, para fins didáticos
    demo_ciclo_resolvido();
    demo_observer();
}

} // namespace demo_weak
