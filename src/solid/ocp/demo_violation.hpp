#pragma once
// =============================================================================
//  demo_violation.hpp — OCP: violação vs. conformidade
// =============================================================================
//
//  Open/Closed Principle, formulado por Bertrand Meyer (1988) e popularizado
//  por Robert C. Martin:
//
//    "Software entities should be OPEN for extension,
//     but CLOSED for modification."
//
//  Traduzindo o que isso quer dizer na prática:
//    • ABERTO para extensão  → dá para acrescentar comportamento novo
//    • FECHADO para modificação → SEM editar o código que já funciona e já
//                                 foi testado
//
//  O sintoma de violação é sempre o mesmo: um `switch` ou uma cascata de
//  `if/else if` sobre um TIPO ou um ENUM. Cada caso novo obriga a reabrir a
//  função — e a função vai crescendo, acumulando responsabilidades e riscos
//  de regressão.
//
// =============================================================================

#include <fmt/core.h>
#include <memory>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

namespace demo_violation {

// ─────────────────────────────────────────────────────────────────────────────
//  ❌ VIOLAÇÃO — a cascata de if/else sobre um enum
//
//  Cada forma geométrica nova exige:
//    1. um valor novo no enum
//    2. um campo novo no struct (que as outras formas não usam)
//    3. um `else if` novo em CADA função que consome o struct
//
//  Repare que `area()` e `perimetro()` precisam ser editadas juntas — e nada
//  garante que o programador lembre da segunda.
// ─────────────────────────────────────────────────────────────────────────────
namespace bad {

enum class TipoForma { Circulo, Retangulo };   // ← Triangulo entraria aqui

struct Forma {
    TipoForma tipo;
    double    raio    = 0;   // só o círculo usa
    double    largura = 0;   // só o retângulo usa
    double    altura  = 0;   // só o retângulo usa
    // ...e um triângulo traria base e altura, inutilizados pelos outros dois
};

double area(const Forma& f) {
    if (f.tipo == TipoForma::Circulo) {
        return 3.14159265 * f.raio * f.raio;
    } else if (f.tipo == TipoForma::Retangulo) {
        return f.largura * f.altura;
    }
    // ← e aqui, um `else if` por forma nova. Para sempre.
    return 0.0;
}

double perimetro(const Forma& f) {
    if (f.tipo == TipoForma::Circulo) {
        return 2 * 3.14159265 * f.raio;
    } else if (f.tipo == TipoForma::Retangulo) {
        return 2 * (f.largura + f.altura);
    }
    // ← a MESMA cascata, repetida. Esquecer de atualizar uma delas
    //   não gera erro de compilação — gera resultado errado.
    return 0.0;
}

void demo() {
    fmt::print("  ❌ VIOLAÇÃO — cascata de if/else\n");

    std::vector<Forma> formas{
        {TipoForma::Circulo,   2.0, 0, 0},
        {TipoForma::Retangulo, 0,   3.0, 4.0},
    };

    for (const auto& f : formas) {
        fmt::print("     área={:6.2f}  perímetro={:6.2f}\n", area(f), perimetro(f));
    }

    fmt::print("     Para somar um triângulo é preciso editar:\n");
    fmt::print("       • o enum TipoForma\n");
    fmt::print("       • o struct Forma (mais 2 campos que ninguém mais usa)\n");
    fmt::print("       • a função area()\n");
    fmt::print("       • a função perimetro()\n");
    fmt::print("     Quatro arquivos já testados, reabertos. E o compilador\n");
    fmt::print("     não avisa se você esquecer um deles.\n");
}

} // namespace bad

// ─────────────────────────────────────────────────────────────────────────────
//  ✅ CONFORMIDADE — polimorfismo: o tipo novo TRAZ o comportamento
//
//  A interface fica FECHADA (ninguém mais edita `Forma`), e o sistema fica
//  ABERTO: um triângulo é um arquivo NOVO, e o compilador OBRIGA a
//  implementar os dois métodos.
// ─────────────────────────────────────────────────────────────────────────────
namespace good {

class Forma {
public:
    virtual ~Forma() = default;
    virtual double      area()      const = 0;
    virtual double      perimetro() const = 0;
    virtual std::string nome()      const = 0;
};

class Circulo final : public Forma {
public:
    explicit Circulo(double r) : raio_(r) {}
    double      area()      const override { return 3.14159265 * raio_ * raio_; }
    double      perimetro() const override { return 2 * 3.14159265 * raio_; }
    std::string nome()      const override { return "Círculo"; }
private:
    double raio_;
};

class Retangulo final : public Forma {
public:
    Retangulo(double l, double a) : largura_(l), altura_(a) {}
    double      area()      const override { return largura_ * altura_; }
    double      perimetro() const override { return 2 * (largura_ + altura_); }
    std::string nome()      const override { return "Retângulo"; }
private:
    double largura_, altura_;
};

// ── A EXTENSÃO: um arquivo novo, zero edições no que já existia ──────────────
class Triangulo final : public Forma {
public:
    Triangulo(double a, double b, double c) : a_(a), b_(b), c_(c) {}
    double area() const override {
        const double s = (a_ + b_ + c_) / 2;              // fórmula de Heron
        return std::sqrt(s * (s - a_) * (s - b_) * (s - c_));
    }
    double      perimetro() const override { return a_ + b_ + c_; }
    std::string nome()      const override { return "Triângulo"; }
private:
    double a_, b_, c_;
};

// Esta função NUNCA precisou ser editada, nem quando o triângulo chegou.
double area_total(const std::vector<std::unique_ptr<Forma>>& formas) {
    return std::accumulate(formas.begin(), formas.end(), 0.0,
                           [](double soma, const auto& f) { return soma + f->area(); });
}

void demo() {
    fmt::print("  ✅ CONFORMIDADE — polimorfismo\n");

    std::vector<std::unique_ptr<Forma>> formas;
    formas.push_back(std::make_unique<Circulo>(2.0));
    formas.push_back(std::make_unique<Retangulo>(3.0, 4.0));
    formas.push_back(std::make_unique<Triangulo>(3.0, 4.0, 5.0));   // ← o novo

    for (const auto& f : formas) {
        fmt::print("     {:<11} área={:6.2f}  perímetro={:6.2f}\n",
                   f->nome(), f->area(), f->perimetro());
    }
    fmt::print("     área total = {:.2f}\n", area_total(formas));

    fmt::print("     O Triangulo foi um arquivo NOVO. `area_total`, `Circulo` e\n");
    fmt::print("     `Retangulo` não foram tocados — e o compilador OBRIGOU a\n");
    fmt::print("     implementar os três métodos (`= 0` não deixa esquecer).\n");
}

} // namespace good

// ─────────────────────────────────────────────────────────────────────────────
//  ⚠ Quando a cascata é a resposta CERTA
//
//  OCP não é dogma. Herança custa: vtable, alocação, indireção, e um arquivo
//  por caso. Se o conjunto de casos é FECHADO e não vai crescer (os 4 naipes
//  de um baralho, os 3 estados de um semáforo), o switch é mais simples,
//  mais rápido e mais legível.
//
//  E há uma terceira via: `std::variant` + `std::visit` dá despacho sem
//  herança E com verificação de exaustividade pelo compilador — o melhor dos
//  dois mundos quando o conjunto é fechado.
// ─────────────────────────────────────────────────────────────────────────────
void nota_sobre_excessos() {
    fmt::print("  ⚠  Quando NÃO aplicar\n");
    fmt::print("     OCP paga-se com vtable, alocação e um arquivo por caso.\n");
    fmt::print("     Se o conjunto é FECHADO (4 naipes, 3 estados de semáforo),\n");
    fmt::print("     o switch é mais simples e mais rápido.\n");
    fmt::print("     Pergunta que decide: \"casos novos vão chegar depois do\n");
    fmt::print("     deploy, vindos de quem não controla este arquivo?\"\n");
    fmt::print("     Se sim → OCP. Se não → switch, e siga em frente.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 1: Violação vs. Conformidade ─────────────────────┐\n");
    bad::demo();
    fmt::print("\n");
    good::demo();
    fmt::print("\n");
    nota_sobre_excessos();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_violation
