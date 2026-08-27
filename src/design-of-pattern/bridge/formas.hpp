#pragma once
// =============================================================================
//  formas.hpp — a ABSTRAÇÃO (o lado que varia por conceito)
//
//  A ponte: cada forma GUARDA uma referência ao renderizador, e delega as
//  primitivas a ele. As duas hierarquias evoluem sem se tocar.
//
//  ⚠ A diferença para o Strategy, que confunde muita gente:
//    Strategy → troca um ALGORITMO; a interface tem tipicamente 1 método
//    Bridge   → separa duas HIERARQUIAS que variam nos dois eixos
//    O Bridge é estrutural (sobre COMO as classes se organizam); o Strategy é
//    comportamental (sobre O QUE elas fazem).
// =============================================================================
#include "renderizador.hpp"

#include <string>

namespace bridge {

// A abstração base guarda a ponte
class Forma {
public:
    explicit Forma(const Renderizador& r) : r_(r) {}
    virtual ~Forma() = default;

    virtual std::string desenhar() const = 0;
    virtual double      area() const = 0;

    // A abstração pode ser REFINADA sem tocar na implementação
    virtual void redimensionar(double fator) = 0;

protected:
    const Renderizador& r_;      // ← a PONTE
};

class Circulo final : public Forma {
public:
    Circulo(const Renderizador& r, double x, double y, double raio)
        : Forma(r), x_(x), y_(y), raio_(raio) {}

    std::string desenhar() const override { return r_.desenhar_circulo(x_, y_, raio_); }
    double      area() const override;
    void        redimensionar(double f) override { raio_ *= f; }

private:
    double x_, y_, raio_;
};

class Quadrado final : public Forma {
public:
    Quadrado(const Renderizador& r, double x, double y, double lado)
        : Forma(r), x_(x), y_(y), lado_(lado) {}

    std::string desenhar() const override;
    double      area() const override { return lado_ * lado_; }
    void        redimensionar(double f) override { lado_ *= f; }

private:
    double x_, y_, lado_;
};

class Triangulo final : public Forma {
public:
    Triangulo(const Renderizador& r, double x, double y, double base)
        : Forma(r), x_(x), y_(y), base_(base) {}

    std::string desenhar() const override;
    double      area() const override;
    void        redimensionar(double f) override { base_ *= f; }

private:
    double x_, y_, base_;
};

}  // namespace bridge
