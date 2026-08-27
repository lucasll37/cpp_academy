// =============================================================================
//  src/oop/demo_slicing.hpp
//
//  Object Slicing — quando o polimorfismo some sem avisar
//  ──────────────────────────────────────────────────────
//  Copiar um objeto derivado para uma variável do tipo BASE não faz uma
//  conversão: faz uma AMPUTAÇÃO. Só a parte-base é copiada; tudo que o
//  derivado acrescentou é descartado, e o vptr do destino continua sendo
//  o da base. O polimorfismo desaparece, e o compilador não reclama.
//
//  Tópicos:
//  • O corte acontecendo numa atribuição
//  • Passagem de parâmetro por valor — o caso mais comum na prática
//  • Por que `std::vector<Base>` é sempre errado
//  • A correção: `vector<unique_ptr<Base>>`
//  • Como proibir o corte no próprio tipo
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace demo_slicing {

// ─────────────────────────────────────────────────────────────────────────────
//  Hierarquia de exemplo: o derivado acrescenta ESTADO e COMPORTAMENTO
// ─────────────────────────────────────────────────────────────────────────────
struct Animal {
    std::string nome;

    explicit Animal(std::string n) : nome(std::move(n)) {}
    virtual ~Animal() = default;

    virtual std::string falar() const { return "..."; }
    virtual std::string descrever() const { return fmt::format("Animal '{}'", nome); }
};

struct Cachorro : Animal {
    int  numero_de_truques;         // ← estado extra, some no corte

    Cachorro(std::string n, int truques)
        : Animal(std::move(n)), numero_de_truques(truques) {}

    std::string falar() const override { return "Au!"; }
    std::string descrever() const override {
        return fmt::format("Cachorro '{}' com {} truques", nome, numero_de_truques);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. O corte numa cópia
// ─────────────────────────────────────────────────────────────────────────────
void demo_corte_basico() {
    fmt::print("\n── 3.1 O corte acontecendo ──\n");

    Cachorro rex("Rex", 5);
    fmt::print("      original : {} → '{}'\n", rex.descrever(), rex.falar());

    // Aqui está o corte. `copia` é um Animal, não um Cachorro.
    // O construtor de cópia de Animal roda e copia SÓ os membros de Animal.
    Animal copia = rex;
    fmt::print("      Animal copia = rex;\n");
    fmt::print("      cortado  : {} → '{}'\n", copia.descrever(), copia.falar());
    fmt::print("      ↑ perdeu os truques E perdeu o comportamento de Cachorro\n");

    // Já a REFERÊNCIA não corta: não há cópia, o objeto continua o mesmo.
    Animal& ref = rex;
    fmt::print("      Animal& ref = rex;\n");
    fmt::print("      via ref  : {} → '{}'\n", ref.descrever(), ref.falar());
    fmt::print("      ↑ referências e ponteiros preservam o objeto real\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O caso mais comum: parâmetro por valor
//
//     Ninguém escreve `Animal copia = rex;` de propósito. Mas escrever
//     `void f(Animal a)` em vez de `void f(const Animal& a)` é fácil demais.
// ─────────────────────────────────────────────────────────────────────────────
void apresentar_cortado(Animal a) {        // ✗ por valor → corta
    fmt::print("      [por valor]      {} → '{}'\n", a.descrever(), a.falar());
}

void apresentar_certo(const Animal& a) {   // ✓ por referência → preserva
    fmt::print("      [por referência] {} → '{}'\n", a.descrever(), a.falar());
}

void demo_parametro_por_valor() {
    fmt::print("\n── 3.2 O corte na passagem de parâmetro ──\n");

    Cachorro rex("Rex", 5);

    apresentar_cortado(rex);
    apresentar_certo(rex);

    fmt::print("      A diferença entre as duas assinaturas é UM caractere: `&`\n");
    fmt::print("      Regra: em hierarquia polimórfica, NUNCA passe por valor.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. `std::vector<Base>` — o corte em escala
//
//     O container guarda objetos do tipo declarado, com o tamanho do tipo
//     declarado. Não há espaço físico para um derivado maior.
// ─────────────────────────────────────────────────────────────────────────────
void demo_vector_de_base() {
    fmt::print("\n── 3.3 Por que `vector<Base>` é sempre errado ──\n");

    fmt::print("      sizeof(Animal)   = {} bytes\n", sizeof(Animal));
    fmt::print("      sizeof(Cachorro) = {} bytes\n", sizeof(Cachorro));
    fmt::print("      Um vector<Animal> reserva {} bytes por slot — não cabe.\n",
               sizeof(Animal));

    std::vector<Animal> errado;
    errado.push_back(Cachorro("Rex", 5));     // corta na inserção
    errado.push_back(Cachorro("Bob", 3));

    fmt::print("      vector<Animal>:\n");
    for (const auto& a : errado) {
        fmt::print("        {} → '{}'\n", a.descrever(), a.falar());
    }
    fmt::print("      ↑ dois Cachorros entraram, dois Animals saíram\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. A correção — guardar ponteiros
//
//     Um `unique_ptr<Animal>` tem tamanho fixo (é um ponteiro) e aponta para
//     o objeto real, que vive na heap com o tamanho que precisar.
// ─────────────────────────────────────────────────────────────────────────────
void demo_vector_de_ponteiros() {
    fmt::print("\n── 3.4 A correção: vector<unique_ptr<Base>> ──\n");

    std::vector<std::unique_ptr<Animal>> certo;
    certo.push_back(std::make_unique<Cachorro>("Rex", 5));
    certo.push_back(std::make_unique<Cachorro>("Bob", 3));
    certo.push_back(std::make_unique<Animal>("genérico"));

    for (const auto& a : certo) {
        fmt::print("        {} → '{}'\n", a->descrever(), a->falar());
    }
    fmt::print("      ↑ cada objeto manteve o tipo real e o comportamento\n");
    fmt::print("      `unique_ptr` ainda garante a destruição correta —\n");
    fmt::print("      desde que Animal tenha destrutor virtual (e tem).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Proibindo o corte no próprio tipo
//
//     Se a base não pode ser copiada, o corte vira erro de compilação.
//     Três receitas, da mais forte para a mais frouxa.
// ─────────────────────────────────────────────────────────────────────────────

// (a) Base abstrata — não instanciável, logo não copiável para uma variável Base
struct FormaAbstrata {
    virtual double area() const = 0;
    virtual ~FormaAbstrata() = default;
};

// (b) Cópia deletada, cópia permitida só entre derivados concretos
struct BaseNaoCopiavel {
    BaseNaoCopiavel() = default;
    BaseNaoCopiavel(const BaseNaoCopiavel&)            = delete;
    BaseNaoCopiavel& operator=(const BaseNaoCopiavel&) = delete;
    virtual ~BaseNaoCopiavel() = default;
};

struct DerivadaCopiavel : BaseNaoCopiavel {
    int dados = 7;
    DerivadaCopiavel() = default;
    // A cópia entre DerivadaCopiavel pode ser reativada explicitamente,
    // porque aqui o tipo estático e o dinâmico coincidem.
    DerivadaCopiavel(const DerivadaCopiavel& o) : BaseNaoCopiavel(), dados(o.dados) {}
};

// (c) Cópia protegida — derivados copiam entre si, o mundo externo não corta
struct BaseProtegida {
    virtual ~BaseProtegida() = default;
protected:
    BaseProtegida()                                = default;
    BaseProtegida(const BaseProtegida&)            = default;
    BaseProtegida& operator=(const BaseProtegida&) = default;
};

struct DerivadaProtegida : BaseProtegida {
    int dados = 9;
};

void demo_proibindo_corte() {
    fmt::print("\n── 3.5 Tornando o corte incompilável ──\n");

    // FormaAbstrata f;                        // ✗ (a) abstrata: nem instancia

    DerivadaCopiavel d1;
    DerivadaCopiavel d2 = d1;                  // ✓ cópia entre iguais
    // BaseNaoCopiavel b = d1;                 // ✗ (b) cópia deletada
    fmt::print("      (b) cópia deletada na base: d2.dados = {}\n", d2.dados);

    DerivadaProtegida p1;
    DerivadaProtegida p2 = p1;                 // ✓ o derivado tem acesso
    // BaseProtegida bp = p1;                  // ✗ (c) ctor de cópia é protected
    fmt::print("      (c) cópia protegida na base: p2.dados = {}\n", p2.dados);

    fmt::print("\n      | receita                  | efeito                        |\n");
    fmt::print("      |--------------------------|-------------------------------|\n");
    fmt::print("      | base abstrata            | nem instancia — o mais forte  |\n");
    fmt::print("      | cópia = delete na base   | derivados também perdem cópia |\n");
    fmt::print("      | cópia protected na base  | derivados copiam entre si     |\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_corte_basico();
    demo_parametro_por_valor();
    demo_vector_de_base();
    demo_vector_de_ponteiros();
    demo_proibindo_corte();
}

} // namespace demo_slicing
