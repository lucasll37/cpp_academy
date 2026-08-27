#pragma once
// =============================================================================
//  demo_violation.hpp — LSP: o contrato que a herança promete
// =============================================================================
//
//  Liskov Substitution Principle, de Barbara Liskov (1987):
//
//    "Se S é subtipo de T, objetos de T podem ser substituídos por objetos
//     de S sem alterar nenhuma propriedade desejável do programa."
//
//  Em português de todo dia: se o código funciona com a Base, tem que
//  continuar funcionando quando você passa um Derivado — sem saber que passou.
//
//  O ponto que quase todo mundo perde: LSP é sobre COMPORTAMENTO, não sobre
//  assinatura. O compilador verifica a assinatura. Ninguém verifica o
//  comportamento — por isso violações de LSP compilam perfeitamente e só
//  aparecem em produção.
//
//  O exemplo canônico é o Quadrado/Retângulo, e ele é canônico justamente
//  porque a relação "quadrado É UM retângulo" é verdadeira na MATEMÁTICA e
//  falsa no CÓDIGO.
//
// =============================================================================

#include <fmt/core.h>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace demo_violation {

// ─────────────────────────────────────────────────────────────────────────────
//  ❌ VIOLAÇÃO — o Quadrado que se diz Retângulo
// ─────────────────────────────────────────────────────────────────────────────
namespace bad {

class Retangulo {
public:
    virtual ~Retangulo() = default;

    virtual void definir_largura(double l) { largura_ = l; }
    virtual void definir_altura(double a)  { altura_  = a; }

    double largura() const { return largura_; }
    double altura()  const { return altura_; }
    double area()    const { return largura_ * altura_; }

protected:
    double largura_ = 0, altura_ = 0;
};

// "Um quadrado é um retângulo com os lados iguais" — verdade em geometria.
// Para manter o invariante, cada setter mexe nos DOIS lados.
class Quadrado : public Retangulo {
public:
    void definir_largura(double l) override { largura_ = altura_ = l; }
    void definir_altura(double a)  override { largura_ = altura_ = a; }
};

// Esta função foi escrita e TESTADA contra Retangulo. Ela não sabe que
// Quadrado existe — e não deveria precisar saber.
void redimensionar_e_verificar(Retangulo& r) {
    r.definir_largura(5);
    r.definir_altura(4);

    const double esperado = 20.0;
    fmt::print("     largura=5, altura=4 → área esperada {:.0f}, obtida {:.0f}  {}\n",
               esperado, r.area(),
               r.area() == esperado ? "✓" : "✗ LSP VIOLADO");
}

void demo() {
    fmt::print("  ❌ VIOLAÇÃO — Quadrado que se diz Retângulo\n");

    Retangulo ret;
    fmt::print("     com Retangulo: ");
    redimensionar_e_verificar(ret);

    Quadrado quad;
    fmt::print("     com Quadrado:  ");
    redimensionar_e_verificar(quad);

    fmt::print("\n     A função compila e roda com os dois. Mas o Quadrado\n");
    fmt::print("     QUEBRA a expectativa de quem escreveu `redimensionar`.\n");
    fmt::print("     Nenhum compilador pega isso: as assinaturas batem.\n");
    fmt::print("     O que não bate é o CONTRATO.\n");
}

} // namespace bad

// ─────────────────────────────────────────────────────────────────────────────
//  ✅ CONFORMIDADE — a herança errada era a premissa
//
//  O erro não foi implementar mal o Quadrado. Foi afirmar que ele É UM
//  Retângulo mutável. A relação "é um" da geometria não sobrevive à
//  MUTABILIDADE: um retângulo mutável promete "posso mudar os lados
//  independentemente", e o quadrado não pode cumprir.
//
//  Duas saídas, e as duas são boas:
//    (a) tornar os tipos IMUTÁVEIS — sem setters, o conflito desaparece
//    (b) não herdar — irmãos sob uma interface comum
// ─────────────────────────────────────────────────────────────────────────────
namespace good {

// ── (a) Imutabilidade: sem setters, sem promessa a quebrar ──────────────────
namespace imutavel {

class Retangulo {
public:
    Retangulo(double l, double a) : largura_(l), altura_(a) {}
    double largura() const { return largura_; }
    double altura()  const { return altura_; }
    double area()    const { return largura_ * altura_; }

    // "Mudar" devolve um objeto NOVO — nenhum invariante alheio é violado
    Retangulo com_largura(double l) const { return {l, altura_}; }
    Retangulo com_altura(double a)  const { return {largura_, a}; }

private:
    double largura_, altura_;
};

class Quadrado {
public:
    explicit Quadrado(double lado) : lado_(lado) {}
    double area() const { return lado_ * lado_; }
    // Converte-se num Retangulo quando alguém precisa dessa visão
    Retangulo como_retangulo() const { return {lado_, lado_}; }
private:
    double lado_;
};

} // namespace imutavel

// ── (b) Irmãos sob uma interface que ambos PODEM cumprir ────────────────────
class Forma {
public:
    virtual ~Forma() = default;
    virtual double      area()  const = 0;
    virtual double      escala(double fator) = 0;   // ambos conseguem cumprir
    virtual std::string nome()  const = 0;
};

class Retangulo final : public Forma {
public:
    Retangulo(double l, double a) : largura_(l), altura_(a) {}
    double area() const override { return largura_ * altura_; }
    double escala(double f) override { largura_ *= f; altura_ *= f; return area(); }
    std::string nome() const override { return "Retângulo"; }
private:
    double largura_, altura_;
};

class Quadrado final : public Forma {
public:
    explicit Quadrado(double lado) : lado_(lado) {}
    double area() const override { return lado_ * lado_; }
    double escala(double f) override { lado_ *= f; return area(); }
    std::string nome() const override { return "Quadrado"; }
private:
    double lado_;
};

// Escrita contra Forma, funciona com QUALQUER forma — presente ou futura
void escalar_e_verificar(Forma& f) {
    const double antes = f.area();
    const double depois = f.escala(2.0);
    const double esperado = antes * 4;             // área escala com o quadrado
    fmt::print("     {:<11} área {:6.2f} → {:6.2f} (esperado {:6.2f})  {}\n",
               f.nome(), antes, depois, esperado,
               std::abs(depois - esperado) < 1e-9 ? "✓" : "✗");
}

void demo() {
    fmt::print("  ✅ CONFORMIDADE — a herança errada era a premissa\n");

    fmt::print("     (b) irmãos sob uma interface que os DOIS cumprem:\n");
    Retangulo r(5, 4);
    Quadrado  q(5);
    escalar_e_verificar(r);
    escalar_e_verificar(q);

    fmt::print("\n     (a) ou torne os tipos imutáveis — sem setter, sem conflito:\n");
    imutavel::Retangulo ir(5, 4);
    auto ir2 = ir.com_largura(10);
    fmt::print("     Retangulo(5,4).com_largura(10) → área {:.0f} (original intacto: {:.0f})\n",
               ir2.area(), ir.area());
    imutavel::Quadrado iq(5);
    fmt::print("     Quadrado(5).como_retangulo()   → área {:.0f}\n",
               iq.como_retangulo().area());

    fmt::print("\n     A lição: \"X é um Y\" na conversa não implica `class X : public Y`.\n");
    fmt::print("     A pergunta certa é: \"X consegue cumprir TODAS as promessas\n");
    fmt::print("     que Y faz aos seus clientes?\"\n");
}

} // namespace good

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 1: Violação vs. Conformidade ─────────────────────┐\n");
    bad::demo();
    fmt::print("\n");
    good::demo();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_violation
