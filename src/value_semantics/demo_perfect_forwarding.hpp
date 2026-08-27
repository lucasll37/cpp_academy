// =============================================================================
//  src/value_semantics/demo_perfect_forwarding.hpp
//
//  Perfect Forwarding — repassar argumentos sem perder informação
//  ──────────────────────────────────────────────────────────────
//  O problema: uma função genérica que recebe argumentos e os repassa para
//  outra precisa preservar TRÊS coisas — o tipo, a constância e a categoria
//  de valor. Sem isso, um rvalue vira lvalue no caminho e o move some.
//
//  A solução tem duas peças que só funcionam juntas:
//    1. o parâmetro `T&&` numa função template (referência universal)
//    2. o `std::forward<T>` na hora de repassar
//
//  Tópicos:
//  • T&& em template ≠ T&& em tipo concreto
//  • Colapso de referências — a tabela que explica tudo
//  • std::forward vs std::move
//  • auto&& e o for-range genérico
//  • Fábricas variádicas (como make_unique é feito)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo_perfect_forwarding {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O alvo: uma função com overloads que distinguem a categoria
// ─────────────────────────────────────────────────────────────────────────────
void alvo(std::string&)       { fmt::print("      alvo(std::string&)       ← lvalue\n"); }
void alvo(const std::string&) { fmt::print("      alvo(const std::string&) ← const lvalue\n"); }
void alvo(std::string&&)      { fmt::print("      alvo(std::string&&)      ← rvalue\n"); }

// ─────────────────────────────────────────────────────────────────────────────
//  2. Três tentativas de escrever um repassador
// ─────────────────────────────────────────────────────────────────────────────

// ✗ Tentativa 1: por valor — sempre copia, e sempre entrega um lvalue
template <typename T>
void repassa_por_valor(T arg) {
    alvo(arg);
}

// ✗ Tentativa 2: T&& sem forward — o parâmetro TEM NOME, logo é lvalue
template <typename T>
void repassa_sem_forward(T&& arg) {
    alvo(arg);               // sempre chama o overload de lvalue!
}

// ✓ Tentativa 3: T&& + std::forward — preserva a categoria original
template <typename T>
void repassa_certo(T&& arg) {
    alvo(std::forward<T>(arg));
}

void demo_tres_tentativas() {
    fmt::print("\n── 4.1 Por que std::forward existe ──\n");

    std::string       lv  = "lvalue";
    const std::string clv = "const lvalue";

    fmt::print("    repassa_sem_forward(lv):\n");            repassa_sem_forward(lv);
    fmt::print("    repassa_sem_forward(std::string{{}}):\n"); repassa_sem_forward(std::string("rv"));
    fmt::print("      ↑ os dois foram parar no overload de lvalue — o move se perdeu\n");

    fmt::print("\n    repassa_certo(lv):\n");                repassa_certo(lv);
    fmt::print("    repassa_certo(clv):\n");                 repassa_certo(clv);
    fmt::print("    repassa_certo(std::string{{}}):\n");       repassa_certo(std::string("rv"));
    fmt::print("      ↑ cada um chegou no overload correto\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Colapso de referências — a mecânica por trás
//
//     Quando `T` é deduzido numa referência universal `T&&`:
//       • argumento lvalue  de tipo X  →  T = X&   →  X& &&  colapsa em  X&
//       • argumento rvalue  de tipo X  →  T = X    →  X&&    permanece   X&&
//
//     Tabela de colapso ("só && && dá &&"; qualquer & vence):
//       X&  &   → X&        X&  &&  → X&
//       X&& &   → X&        X&& &&  → X&&
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
void mostra_deducao(T&& /*arg*/) {
    if constexpr (std::is_lvalue_reference_v<T>) {
        fmt::print("      T = X&  → o parâmetro colapsou para X&   (veio lvalue)\n");
    } else {
        fmt::print("      T = X   → o parâmetro continuou X&&      (veio rvalue)\n");
    }
}

void demo_colapso() {
    fmt::print("\n── 4.2 Colapso de referências ──\n");

    std::string s = "x";
    fmt::print("    mostra_deducao(s):\n");            mostra_deducao(s);
    fmt::print("    mostra_deducao(std::move(s)):\n"); mostra_deducao(std::move(s));

    fmt::print("\n    Tabela:  X& &  → X&    |  X& &&  → X&\n");
    fmt::print("             X&& & → X&    |  X&& && → X&&\n");
    fmt::print("    Só `&& &&` sobrevive como rvalue. Qualquer `&` contamina.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. ⚠ `T&&` só é referência universal quando T é DEDUZIDO nessa posição
//
//     Estes NÃO são referências universais:
//       void f(std::string&&)                    — tipo concreto
//       template<class T> void f(std::vector<T>&&) — T não está na posição certa
//       template<class T> struct S { void f(T&&); } — T já foi fixado na classe
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
void nao_universal(std::vector<T>&& v) {   // rvalue reference de verdade
    fmt::print("      nao_universal: aceita SÓ rvalue, {} elementos\n", v.size());
}

void demo_nao_universal() {
    fmt::print("\n── 4.3 Nem todo T&& é referência universal ──\n");

    std::vector<int> v{1, 2, 3};

    // nao_universal(v);            // ✗ erro: não liga lvalue a std::vector<T>&&
    nao_universal(std::move(v));    // ✓
    nao_universal(std::vector<int>{4, 5});

    fmt::print("      `std::vector<T>&&` é rvalue ref pura: T não está na\n");
    fmt::print("      posição deduzível que habilita o colapso.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. O bug clássico: std::move num parâmetro que será usado de novo
//
//     std::move é incondicional; std::forward é condicional.
//     Usar move onde cabia forward rouba o objeto do chamador sem avisar.
// ─────────────────────────────────────────────────────────────────────────────
struct Registro {
    std::string nome;
};

// ✗ move incondicional: destrói o argumento mesmo quando veio um lvalue
template <typename T>
Registro cria_errado(T&& nome) {
    return Registro{std::move(nome)};    // sempre rouba
}

// ✓ forward condicional: só rouba se o chamador entregou um rvalue
template <typename T>
Registro cria_certo(T&& nome) {
    return Registro{std::forward<T>(nome)};
}

void demo_move_vs_forward() {
    fmt::print("\n── 4.4 std::move incondicional vs std::forward condicional ──\n");

    {
        std::string meu = "ainda preciso disto";
        Registro r = cria_errado(meu);
        fmt::print("    após cria_errado(meu):  meu = '{}'  ← esvaziado sem avisar!\n", meu);
        fmt::print("                            r.nome = '{}'\n", r.nome);
    }
    {
        std::string meu = "ainda preciso disto";
        Registro r = cria_certo(meu);
        fmt::print("    após cria_certo(meu):   meu = '{}'  ← intacto\n", meu);
        fmt::print("                            r.nome = '{}'\n", r.nome);
    }

    fmt::print("    Regra: `std::move` em parâmetro concreto (T&&/const T&),\n");
    fmt::print("           `std::forward<T>` em parâmetro deduzido.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Fábrica variádica — o padrão de make_unique / emplace_back
// ─────────────────────────────────────────────────────────────────────────────
struct Widget {
    std::string nome;
    int         largura;

    Widget(std::string n, int l) : nome(std::move(n)), largura(l) {
        fmt::print("      Widget('{}', {}) construído in-place\n", nome, largura);
    }
};

// Repassa QUALQUER conjunto de argumentos ao construtor, preservando categorias.
// O `...` do forward expande para forward<A1>(a1), forward<A2>(a2), ...
template <typename T, typename... Args>
std::unique_ptr<T> meu_make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

void demo_fabrica_variadica() {
    fmt::print("\n── 4.5 Fábrica variádica (como make_unique funciona) ──\n");

    std::string nome = "botão";

    auto w1 = meu_make_unique<Widget>(nome, 120);              // lvalue → cópia
    fmt::print("      após passar lvalue:  nome = '{}'\n", nome);

    auto w2 = meu_make_unique<Widget>(std::move(nome), 240);   // rvalue → move
    fmt::print("      após passar rvalue:  nome = '{}'  (movido)\n", nome);

    fmt::print("      w1='{}' w2='{}'\n", w1->nome, w2->nome);
    fmt::print("      Uma única função template atende todas as assinaturas\n");
    fmt::print("      de construtor, sem overload combinatório.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. auto&& — referência universal fora de templates
// ─────────────────────────────────────────────────────────────────────────────
void demo_auto_ref_ref() {
    fmt::print("\n── 4.6 `auto&&` — o coringa que nunca copia ──\n");

    std::vector<std::string> palavras{"um", "dois", "três"};

    // `auto&&` liga a qualquer coisa: lvalue, rvalue, const ou não.
    // É a forma correta de escrever um for-range genérico.
    for (auto&& p : palavras) {
        fmt::print("      '{}' ", p);
    }
    fmt::print("\n");

    // Ligar a um prvalue também funciona e estende o tempo de vida.
    auto&& temporario = std::string("temporário estendido");
    fmt::print("      auto&& ligado a prvalue: '{}'\n", temporario);

    fmt::print("      Use `auto&&` em código genérico; `const auto&` quando\n");
    fmt::print("      você só lê; `auto` quando quer mesmo uma cópia.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_tres_tentativas();
    demo_colapso();
    demo_nao_universal();
    demo_move_vs_forward();
    demo_fabrica_variadica();
    demo_auto_ref_ref();
}

} // namespace demo_perfect_forwarding
