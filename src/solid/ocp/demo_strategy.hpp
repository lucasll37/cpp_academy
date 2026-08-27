#pragma once
// =============================================================================
//  demo_strategy.hpp — OCP por INJEÇÃO: estratégias e políticas
// =============================================================================
//
//  A herança clássica não é a única forma de ficar aberto para extensão.
//  Quando o que varia é um ALGORITMO (e não uma família de entidades), a
//  ferramenta certa é injetar o algoritmo — e aí o C++ oferece três níveis:
//
//    1. std::function        → extensível em RUNTIME, custo de indireção
//    2. template (policy)    → extensível em COMPILAÇÃO, custo zero
//    3. concepts (C++20)     → policy com contrato verificado e erro legível
//
//  Todos os três deixam o consumidor FECHADO: `Processador` nunca precisa ser
//  editado, independentemente de quantas políticas surgirem.
//
// =============================================================================

#include <fmt/core.h>
#include <algorithm>
#include <concepts>
#include <functional>
#include <map>
#include <numeric>
#include <string>
#include <vector>

namespace demo_strategy {

// ─────────────────────────────────────────────────────────────────────────────
//  Nível 1 — std::function: extensão em RUNTIME
//
//  A política é um VALOR. Pode vir de um arquivo de configuração, de um
//  plugin carregado com dlopen, ou da escolha do usuário na interface.
// ─────────────────────────────────────────────────────────────────────────────
namespace runtime {

using PoliticaDeDesconto = std::function<double(double)>;

class Carrinho {
public:
    // O carrinho não sabe QUAIS descontos existem — nem precisa saber.
    explicit Carrinho(PoliticaDeDesconto politica) : politica_(std::move(politica)) {}

    void adicionar(double preco) { itens_.push_back(preco); }

    double total() const {
        const double bruto = std::accumulate(itens_.begin(), itens_.end(), 0.0);
        return politica_(bruto);
    }

private:
    std::vector<double> itens_;
    PoliticaDeDesconto  politica_;
};

void demo() {
    fmt::print("  1. std::function — extensão em RUNTIME\n");

    // Um registro de políticas montado em runtime. Acrescentar uma entrada
    // aqui NÃO exige editar a classe Carrinho.
    const std::map<std::string, PoliticaDeDesconto> catalogo{
        {"sem desconto",   [](double t) { return t; }},
        {"black friday",   [](double t) { return t * 0.70; }},
        {"frete grátis",   [](double t) { return t > 200 ? t : t + 25; }},
        {"cupom fixo R$50",[](double t) { return std::max(0.0, t - 50); }},
    };

    for (const auto& [nome, politica] : catalogo) {
        Carrinho c(politica);
        c.adicionar(120.0);
        c.adicionar(80.0);
        fmt::print("     {:<18} bruto=200.00 → total={:6.2f}\n", nome, c.total());
    }

    fmt::print("     A política pode vir de config, de plugin, da escolha do\n");
    fmt::print("     usuário. Custo: uma indireção e possível alocação.\n");
}

} // namespace runtime

// ─────────────────────────────────────────────────────────────────────────────
//  Nível 2 — template (policy-based design): extensão em COMPILAÇÃO
//
//  A política é um TIPO. O compilador inlina tudo: custo zero em runtime.
//  Em troca, o conjunto de políticas é fixado na compilação.
// ─────────────────────────────────────────────────────────────────────────────
namespace compilacao {

struct SemDesconto     { static double aplicar(double t) { return t; } };
struct BlackFriday     { static double aplicar(double t) { return t * 0.70; } };
struct CupomFixo50     { static double aplicar(double t) { return std::max(0.0, t - 50); } };

template <typename Politica>
class Carrinho {
public:
    void adicionar(double preco) { itens_.push_back(preco); }

    double total() const {
        const double bruto = std::accumulate(itens_.begin(), itens_.end(), 0.0);
        return Politica::aplicar(bruto);      // resolvido em compilação
    }

private:
    std::vector<double> itens_;
};

void demo() {
    fmt::print("\n  2. Template (policy) — extensão em COMPILAÇÃO\n");

    Carrinho<SemDesconto> a;  a.adicionar(120); a.adicionar(80);
    Carrinho<BlackFriday> b;  b.adicionar(120); b.adicionar(80);
    Carrinho<CupomFixo50> c;  c.adicionar(120); c.adicionar(80);

    fmt::print("     Carrinho<SemDesconto>  → {:6.2f}\n", a.total());
    fmt::print("     Carrinho<BlackFriday>  → {:6.2f}\n", b.total());
    fmt::print("     Carrinho<CupomFixo50>  → {:6.2f}\n", c.total());

    fmt::print("     Nenhuma vtable, nenhuma alocação, tudo inlined.\n");
    fmt::print("     ⚠  cada política gera uma classe DIFERENTE: não dá para\n");
    fmt::print("        guardar os três num mesmo std::vector.\n");
}

} // namespace compilacao

// ─────────────────────────────────────────────────────────────────────────────
//  Nível 3 — concepts (C++20): a policy com contrato explícito
//
//  O problema do nível 2 é a mensagem de erro: passar uma política inválida
//  produz um erro no FUNDO da instanciação. Um concept move o erro para a
//  chamada e diz o que faltou.
// ─────────────────────────────────────────────────────────────────────────────
namespace com_concepts {

template <typename P>
concept PoliticaDeDesconto = requires(double total) {
    { P::aplicar(total) } -> std::convertible_to<double>;
    { P::nome() }         -> std::convertible_to<std::string>;
};

struct Progressivo {
    static double aplicar(double t) {
        if (t > 500) return t * 0.80;
        if (t > 200) return t * 0.90;
        return t;
    }
    static std::string nome() { return "progressivo"; }
};

struct Fidelidade {
    static double aplicar(double t) { return t * 0.95; }
    static std::string nome() { return "fidelidade -5%"; }
};

// Política INCOMPLETA — falta nome(). Não satisfaz o concept.
struct Quebrada {
    static double aplicar(double t) { return t; }
};

template <PoliticaDeDesconto P>          // ← o contrato, na assinatura
class Carrinho {
public:
    void adicionar(double preco) { itens_.push_back(preco); }
    double total() const {
        return P::aplicar(std::accumulate(itens_.begin(), itens_.end(), 0.0));
    }
    static std::string politica() { return P::nome(); }
private:
    std::vector<double> itens_;
};

void demo() {
    fmt::print("\n  3. Concepts — a policy com contrato verificado\n");

    Carrinho<Progressivo> a; a.adicionar(300); a.adicionar(300);
    Carrinho<Fidelidade>  b; b.adicionar(300); b.adicionar(300);

    fmt::print("     {:<16} bruto=600.00 → {:6.2f}\n", Carrinho<Progressivo>::politica(), a.total());
    fmt::print("     {:<16} bruto=600.00 → {:6.2f}\n", Carrinho<Fidelidade>::politica(),  b.total());

    // O concept é um predicado consultável:
    fmt::print("     PoliticaDeDesconto<Progressivo> = {}\n", PoliticaDeDesconto<Progressivo>);
    fmt::print("     PoliticaDeDesconto<Fidelidade>  = {}\n", PoliticaDeDesconto<Fidelidade>);
    fmt::print("     PoliticaDeDesconto<Quebrada>    = {}  ← falta nome()\n",
               PoliticaDeDesconto<Quebrada>);

    // Carrinho<Quebrada> c;      // ✗ erro NA LINHA, dizendo o que faltou:
    //   note: the required expression 'P::nome()' is invalid
    fmt::print("     `Carrinho<Quebrada>` não compila, e o erro aponta a LINHA\n");
    fmt::print("     e a expressão que falhou — não 300 linhas de instanciação.\n");
}

} // namespace com_concepts

// ─────────────────────────────────────────────────────────────────────────────
//  A tabela de decisão
// ─────────────────────────────────────────────────────────────────────────────
void comparar() {
    fmt::print("\n  Escolhendo o nível:\n");
    fmt::print("     | mecanismo      | extensão   | custo      | homogêneo? |\n");
    fmt::print("     |----------------|------------|------------|------------|\n");
    fmt::print("     | herança        | runtime    | vtable     | sim        |\n");
    fmt::print("     | std::function  | runtime    | indireção  | sim        |\n");
    fmt::print("     | template       | compilação | ZERO       | não        |\n");
    fmt::print("     | concept        | compilação | ZERO       | não        |\n");
    fmt::print("\n     \"homogêneo\" = dá para guardar variantes num mesmo vector.\n");
    fmt::print("     Precisa disso em compilação? Use type erasure (src/ecs/).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 2: OCP por injeção — estratégias e políticas ──────┐\n");
    runtime::demo();
    compilacao::demo();
    com_concepts::demo();
    comparar();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_strategy
