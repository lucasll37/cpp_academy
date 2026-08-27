// =============================================================================
//  src/lambdas/demo_padroes.hpp
//
//  Padrões que só existem por causa das lambdas
//  ────────────────────────────────────────────
//  Alguns idiomas do C++ moderno não existiam antes do C++11 porque não havia
//  como escrever uma função anônima no ponto de uso. Este demo reúne os que
//  aparecem mais.
//
//  Tópicos:
//  • IIFE — inicializar um `const` complexo
//  • O overload idiom, revisitado
//  • Recursão: y-combinator e `deducing this` (C++23)
//  • Scope guard — RAII com uma lambda
//  • Lambdas como comparadores em containers ordenados
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <functional>
#include <map>
#include <stdexcept>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace demo_padroes {

// ─────────────────────────────────────────────────────────────────────────────
//  1. IIFE — Immediately Invoked Function Expression
//
//     O problema: inicializar um `const` que precisa de várias instruções.
//     Sem IIFE, ou você abre mão do `const`, ou escreve uma função auxiliar
//     usada uma vez só.
// ─────────────────────────────────────────────────────────────────────────────
void demo_iife() {
    fmt::print("\n── 5.1 IIFE ──\n");

    const int nivel_de_log = 2;

    // ✗ Sem IIFE: perde o const, e o estado intermediário fica visível
    std::string prefixo_mutavel;
    if (nivel_de_log >= 3)      prefixo_mutavel = "[DEBUG] ";
    else if (nivel_de_log >= 2) prefixo_mutavel = "[INFO] ";
    else                         prefixo_mutavel = "[ERRO] ";

    // ✓ Com IIFE: o resultado é const, e o cálculo fica encapsulado
    const std::string prefixo = [nivel_de_log] {
        if (nivel_de_log >= 3) return "[DEBUG] ";
        if (nivel_de_log >= 2) return "[INFO] ";
        return "[ERRO] ";
    }();                          // ← os parênteses invocam na hora

    fmt::print("      prefixo (via IIFE) = '{}'\n", prefixo);

    // O caso em que o IIFE é quase obrigatório: inicializar um MEMBRO const
    // numa lista de inicialização, com lógica.
    struct Config {
        const std::vector<int> portas;

        explicit Config(bool producao)
            : portas([producao] {
                  std::vector<int> p{80, 443};
                  if (!producao) { p.push_back(8080); p.push_back(3000); }
                  return p;
              }()) {}
    };

    const Config dev{false}, prod{true};
    fmt::print("      Config(dev).portas  = ");
    for (int p : dev.portas) fmt::print("{} ", p);
    fmt::print("\n      Config(prod).portas = ");
    for (int p : prod.portas) fmt::print("{} ", p);
    fmt::print("\n");

    fmt::print("\n      Sem IIFE, um membro `const vector` com lógica de\n");
    fmt::print("      inicialização exigiria uma função auxiliar `static` na\n");
    fmt::print("      classe — ou abrir mão do `const`.\n");
    fmt::print("      Ver src/compile_time/ para o IIFE em contexto constexpr.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O overload idiom, revisitado
//
//     Já apareceu em src/vocabulary_types/demo_variant.hpp. Aqui o foco é o
//     MECANISMO: por que herdar de lambdas funciona.
// ─────────────────────────────────────────────────────────────────────────────
template <typename... Ts>
struct Overload : Ts... { using Ts::operator()...; };

template <typename... Ts> Overload(Ts...) -> Overload<Ts...>;

void demo_overload() {
    fmt::print("\n── 5.2 O overload idiom ──\n");

    auto tratar = Overload{
        [](int i)                { return fmt::format("int {}", i); },
        [](double d)             { return fmt::format("double {:.2f}", d); },
        [](const std::string& s) { return fmt::format("string '{}'", s); },
    };

    fmt::print("      tratar(42)      → {}\n", tratar(42));
    fmt::print("      tratar(3.14)    → {}\n", tratar(3.14));
    fmt::print("      tratar(\"texto\") → {}\n", tratar(std::string{"texto"}));

    fmt::print("\n      Por que funciona:\n");
    fmt::print("        1. cada lambda é uma CLASSE com operator()\n");
    fmt::print("        2. `struct Overload : Ts...` herda de todas elas\n");
    fmt::print("        3. `using Ts::operator()...;` traz os operator() de\n");
    fmt::print("           todas as bases para o mesmo escopo\n");
    fmt::print("        4. a resolução de overload normal escolhe qual chamar\n");
    fmt::print("      Sem o passo 3, os operator() das bases ficariam ESCONDIDOS\n");
    fmt::print("      uns pelos outros — é o name hiding de src/oop/, 1.4.\n");

    fmt::print("\n      sizeof(Overload de 3 lambdas sem captura) = {}\n", sizeof(tratar));
    fmt::print("      ↑ classes vazias: a EBO faz o objeto inteiro caber em 1 byte.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Recursão numa lambda
//
//     Uma lambda não tem nome dentro de si mesma, então não pode se chamar
//     diretamente. Há três saídas, e a terceira (C++23) é a boa.
// ─────────────────────────────────────────────────────────────────────────────
void demo_recursao() {
    fmt::print("\n── 5.3 Recursão numa lambda ──\n");

    // (a) std::function — funciona, mas paga type erasure e captura por ref
    std::function<int(int)> fat_function = [&fat_function](int n) {
        return n <= 1 ? 1 : n * fat_function(n - 1);
    };
    fmt::print("      (a) std::function       → 5! = {}\n", fat_function(5));

    // (b) Y-combinator: passar a própria lambda como argumento.
    //     Funciona desde o C++14 (lambda genérica), sem alocação.
    auto fat_y = [](auto&& self, int n) -> int {
        return n <= 1 ? 1 : n * self(self, n - 1);
    };
    fmt::print("      (b) y-combinator        → 5! = {}\n", fat_y(fat_y, 5));

    // (c) `deducing this` (C++23): a lambda ganharia um nome para si mesma.
    //
    //     auto fat = [](this auto const& self, int n) -> int {
    //         return n <= 1 ? 1 : n * self(n - 1);
    //     };
    //     fat(5);                          // ← chamada natural, sem passar `self`
    //
    //     ⚠ O GCC 13 NÃO implementa isto. O parâmetro explícito de objeto
    //       (P0847) chegou no GCC 14; aqui o compilador responde
    //       "expected identifier before 'this'". Verifique com:
    //           #ifdef __cpp_explicit_this_parameter
    //       que no GCC 13 não está definido.
    fmt::print("      (c) deducing this (C++23)→ NÃO DISPONÍVEL no GCC 13\n");
    fmt::print("          (P0847 chegou no GCC 14 — ver src/ROADMAP.md)\n");

    fmt::print("\n      | forma          | aloca | inlina | chamada  | GCC 13 |\n");
    fmt::print("      |----------------|-------|--------|----------|--------|\n");
    fmt::print("      | std::function  | talvez| não    | f(n)     | ✓      |\n");
    fmt::print("      | y-combinator   | não   | sim    | f(f, n)  | ✓      |\n");
    fmt::print("      | deducing this  | não   | sim    | f(n)     | ✗      |\n");

    fmt::print("\n      Enquanto o `deducing this` não chega, o y-combinator é a\n");
    fmt::print("      opção certa em código quente: não aloca e inlina. A\n");
    fmt::print("      feiura do `f(f, n)` fica contida numa linha.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Scope guard — RAII com uma lambda
//
//     RAII costuma exigir uma classe por recurso. Com uma lambda, dá para
//     transformar QUALQUER ação de limpeza em RAII, no ponto de uso.
// ─────────────────────────────────────────────────────────────────────────────
template <typename F>
class GuardaDeEscopo {
public:
    explicit GuardaDeEscopo(F f) : limpeza_(std::move(f)) {}

    ~GuardaDeEscopo() { if (ativo_) limpeza_(); }

    // Move-only: dois guards não podem executar a mesma limpeza
    GuardaDeEscopo(const GuardaDeEscopo&)            = delete;
    GuardaDeEscopo& operator=(const GuardaDeEscopo&) = delete;
    GuardaDeEscopo(GuardaDeEscopo&& o) noexcept
        : limpeza_(std::move(o.limpeza_)), ativo_(o.ativo_) { o.ativo_ = false; }

    void dispensar() { ativo_ = false; }      // cancelar a limpeza (commit)

private:
    F    limpeza_;
    bool ativo_ = true;
};

// Deduction guide, para escrever `GuardaDeEscopo{[]{...}}`
template <typename F> GuardaDeEscopo(F) -> GuardaDeEscopo<F>;

void demo_scope_guard() {
    fmt::print("\n── 5.4 Scope guard ──\n");

    fmt::print("      caminho normal:\n");
    {
        GuardaDeEscopo g{[] { fmt::print("        [guard] recurso liberado\n"); }};
        fmt::print("        trabalhando...\n");
    }

    fmt::print("      com exceção no meio:\n");
    try {
        GuardaDeEscopo g{[] { fmt::print("        [guard] liberado no unwinding\n"); }};
        fmt::print("        trabalhando...\n");
        throw std::runtime_error("falhou");
    } catch (const std::exception& e) {
        fmt::print("        capturado: {}\n", e.what());
    }

    fmt::print("      com dispensar() — o padrão commit/rollback:\n");
    {
        GuardaDeEscopo desfazer{[] { fmt::print("        [guard] ROLLBACK\n"); }};
        fmt::print("        transação iniciada...\n");
        fmt::print("        commit!\n");
        desfazer.dispensar();               // deu certo: cancela o rollback
    }

    fmt::print("\n      O guard transforma qualquer ação em RAII, no ponto de uso —\n");
    fmt::print("      sem escrever uma classe por recurso:\n");
    fmt::print("        auto g = GuardaDeEscopo{{[&]{{ fclose(f); }}}};\n");
    fmt::print("        auto g = GuardaDeEscopo{{[&]{{ mutex.unlock(); }}}};\n");
    fmt::print("      É o `scope_exit` do Boost e a proposta `std::scope_exit`.\n");
    fmt::print("      Ver src/errors/ para a relação entre RAII e exceções.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Lambdas como comparadores em containers ordenados
//
//     Um `std::set` precisa do comparador no TIPO. Como cada lambda tem tipo
//     único e anônimo, isso exigia um functor nomeado — até o C++20 permitir
//     lambdas sem captura em contexto não avaliado.
// ─────────────────────────────────────────────────────────────────────────────
void demo_comparadores() {
    fmt::print("\n── 5.5 Lambdas como comparadores ──\n");

    // C++20: a lambda pode aparecer direto no argumento de template
    using PorTamanho = decltype([](const std::string& a, const std::string& b) {
        return a.size() != b.size() ? a.size() < b.size() : a < b;
    });

    std::set<std::string, PorTamanho> ordenado{
        "banana", "uva", "maçã", "abacaxi", "kiwi",
    };

    fmt::print("      set com comparador-lambda (por tamanho, depois alfabético):\n        ");
    for (const auto& s : ordenado) fmt::print("{} ", s);
    fmt::print("\n");

    // Antes do C++20, era preciso construir o set passando a lambda
    auto comp = [](int a, int b) { return a > b; };       // decrescente
    std::set<int, decltype(comp)> decrescente(comp);
    for (int x : {3, 1, 4, 1, 5, 9, 2, 6}) decrescente.insert(x);

    fmt::print("      set<int, decltype(comp)> decrescente: ");
    for (int x : decrescente) fmt::print("{} ", x);
    fmt::print("\n");

    fmt::print("\n      Antes do C++20:\n");
    fmt::print("        auto comp = [](...){{...}};\n");
    fmt::print("        std::set<T, decltype(comp)> s(comp);   ← precisava passar\n");
    fmt::print("      A partir do C++20, lambdas sem captura são default-\n");
    fmt::print("      constructible, então `std::set<T, decltype([]{{...}})>` basta.\n");

    fmt::print("\n      ⚠  o comparador FAZ PARTE DO TIPO: dois sets com lambdas\n");
    fmt::print("         diferentes são tipos incompatíveis, mesmo com a mesma\n");
    fmt::print("         lógica. Se precisar de heterogeneidade, use\n");
    fmt::print("         `std::function<bool(const T&, const T&)>` — e pague o\n");
    fmt::print("         type erasure (ver demo_function.hpp).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_iife();
    demo_overload();
    demo_recursao();
    demo_scope_guard();
    demo_comparadores();
}

} // namespace demo_padroes
