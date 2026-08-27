// =============================================================================
//  src/type_erasure/demo_conceito.hpp
//
//  O que é apagamento de tipo
//  ──────────────────────────
//  Type erasure é guardar um objeto de tipo T num lugar que NÃO menciona T,
//  preservando as operações que interessam. O tipo desaparece da assinatura;
//  o comportamento fica.
//
//  Você já usa isso o tempo todo sem chamar pelo nome:
//
//      std::function<int(int)> f;   // aceita lambda, functor, ponteiro...
//      std::any               a;    // aceita qualquer coisa copiável
//      std::shared_ptr<void>  p;    // guarda o deleter de QUALQUER tipo
//      std::unique_ptr<Base>  b;    // a herança é a forma mais antiga
//
//  Tópicos:
//  • O problema: heterogêneo sem hierarquia comum
//  • As três formas de resolver
//  • A herança já é type erasure (só que intrusiva)
//  • Custo: indireção, alocação, cache
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace demo_conceito {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O problema
// ─────────────────────────────────────────────────────────────────────────────
void demo_problema() {
    fmt::print("\n── 1.1 O problema ──\n");

    fmt::print("      Três tipos que NÃO se conhecem e não herdam de nada:\n\n");
    fmt::print("        struct Circulo {{ double r; double area() const; }};\n");
    fmt::print("        struct Quadrado {{ double l; double area() const; }};\n");
    fmt::print("        struct Triangulo {{ double b, h; double area() const; }};\n\n");
    fmt::print("      Como guardar os três num único `std::vector`?\n\n");

    fmt::print("      Tentativa ingênua:\n");
    fmt::print("        std::vector<...> formas;   // ← que tipo vai aqui?\n\n");

    fmt::print("      Não há resposta boa dentro do sistema de tipos estático:\n");
    fmt::print("        • `vector<Circulo>` só guarda círculos;\n");
    fmt::print("        • um template `template<class T> void f(vector<T>&)` é\n");
    fmt::print("          resolvido em COMPILAÇÃO — não dá heterogeneidade;\n");
    fmt::print("        • `void*` perde toda a informação, inclusive como destruir.\n\n");

    fmt::print("      É esse buraco que o apagamento de tipo preenche.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. A herança já é type erasure (intrusiva)
// ─────────────────────────────────────────────────────────────────────────────
namespace com_heranca {

struct Forma {                          // ← o "concept" virou uma classe base
    virtual ~Forma() = default;
    virtual double area() const = 0;
    virtual std::string nome() const = 0;
};

struct Circulo : Forma {
    double r;
    explicit Circulo(double raio) : r{raio} {}
    double area() const override { return 3.14159265 * r * r; }
    std::string nome() const override { return "círculo"; }
};

struct Quadrado : Forma {
    double l;
    explicit Quadrado(double lado) : l{lado} {}
    double area() const override { return l * l; }
    std::string nome() const override { return "quadrado"; }
};

}  // namespace com_heranca

void demo_heranca() {
    fmt::print("\n── 1.2 A herança já é type erasure ──\n");

    std::vector<std::unique_ptr<com_heranca::Forma>> formas;
    formas.push_back(std::make_unique<com_heranca::Circulo>(1.0));
    formas.push_back(std::make_unique<com_heranca::Quadrado>(2.0));

    for (const auto& f : formas)
        fmt::print("      {:<10} área = {:.4f}\n", f->nome(), f->area());

    fmt::print("\n      Repare no que aconteceu: `unique_ptr<Forma>` NÃO menciona\n");
    fmt::print("      `Circulo` nem `Quadrado`. O tipo foi apagado. A vtable é o\n");
    fmt::print("      mecanismo — ver src/oop/demo_virtual.hpp.\n\n");

    fmt::print("      O defeito: é INTRUSIVO. Para entrar no vetor, o tipo precisa\n");
    fmt::print("      HERDAR de `Forma`. Isso significa:\n");
    fmt::print("        • não dá para usar `int`, `std::string` ou um tipo de\n");
    fmt::print("          biblioteca de terceiros que você não pode alterar;\n");
    fmt::print("        • o tipo passa a ter vptr, deixa de ser trivial e de\n");
    fmt::print("          caber num registrador (ver src/layout/);\n");
    fmt::print("        • você precisa alocar no heap e usar ponteiro, ou sofre\n");
    fmt::print("          object slicing (src/oop/demo_slicing.hpp).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. As três formas de resolver
// ─────────────────────────────────────────────────────────────────────────────
void demo_tres_formas() {
    fmt::print("\n── 1.3 As três formas ──\n");

    fmt::print("      | forma                | conjunto de tipos | despacho    |\n");
    fmt::print("      |----------------------|-------------------|-------------|\n");
    fmt::print("      | herança + virtual    | ABERTO, intrusivo | vtable      |\n");
    fmt::print("      | std::variant + visit | FECHADO           | índice/jump |\n");
    fmt::print("      | type erasure manual  | ABERTO, externo   | vtable manual|\n\n");

    fmt::print("      ABERTO  = qualquer um pode acrescentar um tipo novo depois,\n");
    fmt::print("                sem tocar no seu código.\n");
    fmt::print("      FECHADO = a lista de tipos está fixa na declaração.\n\n");

    fmt::print("      Este é o \"problema da expressão\", já discutido em\n");
    fmt::print("      src/design-of-pattern/visitor/. Você escolhe qual eixo é\n");
    fmt::print("      fácil de estender:\n\n");
    fmt::print("        virtual  → tipos novos são fáceis, OPERAÇÕES novas são caras\n");
    fmt::print("                   (mexer na base quebra todos os derivados)\n");
    fmt::print("        variant  → operações novas são fáceis (basta outro visitor),\n");
    fmt::print("                   TIPOS novos são caros (recompila tudo)\n\n");

    fmt::print("      Type erasure manual fica no meio: aberto como a herança, mas\n");
    fmt::print("      NÃO intrusivo — o tipo não precisa saber que existe.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. std::function como exemplo canônico
// ─────────────────────────────────────────────────────────────────────────────
void demo_function() {
    fmt::print("\n── 1.4 std::function, o exemplo canônico ──\n");

    int contador = 0;

    struct Somador { int operator()(int x) const { return x + 100; } };
    auto dobra = [](int x) { return x * 2; };
    auto conta = [&contador](int x) { ++contador; return x; };
    static const auto ponteiro = +[](int x) { return -x; };   // ponteiro de função

    // Quatro tipos COMPLETAMENTE diferentes, o mesmo container:
    std::vector<std::function<int(int)>> fs;
    fs.emplace_back(Somador{});
    fs.emplace_back(dobra);
    fs.emplace_back(conta);
    fs.emplace_back(ponteiro);

    fmt::print("      quatro callables, um vetor:\n");
    for (std::size_t i = 0; i < fs.size(); ++i)
        fmt::print("        fs[{}](7) = {:4}\n", i, fs[i](7));
    fmt::print("      a lambda com captura incrementou o contador: {}\n", contador);

    fmt::print("\n      Os quatro tipos não têm base comum, não herdam de nada e\n");
    fmt::print("      nem sabem que `std::function` existe. Nenhum deles foi\n");
    fmt::print("      alterado. Isso é apagamento de tipo NÃO intrusivo.\n\n");

    fmt::print("      O contrato exigido é só: \"invocável com (int) devolvendo\n");
    fmt::print("      algo conversível para int\". Um concept implícito.\n");
    fmt::print("      A seção 2 mostra como isso é implementado por dentro.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_problema();
    demo_heranca();
    demo_tres_formas();
    demo_function();
}

}  // namespace demo_conceito
