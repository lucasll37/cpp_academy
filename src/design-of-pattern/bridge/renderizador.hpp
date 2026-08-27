#pragma once
// =============================================================================
//  renderizador.hpp — a IMPLEMENTAÇÃO (o lado que varia por plataforma)
//
//  O Bridge separa uma ABSTRAÇÃO da sua IMPLEMENTAÇÃO, para que as duas possam
//  variar INDEPENDENTEMENTE.
//
//  O problema que ele resolve: quando duas dimensões variam ao mesmo tempo, a
//  herança sozinha produz explosão combinatória.
//
//      3 formas × 3 renderizadores = 9 classes com herança
//                                  = 3 + 3 com Bridge
//
//  E o pior não é o número — é que acrescentar UM renderizador vira 3 classes
//  novas, uma por forma.
// =============================================================================
#include <string>

namespace bridge {

// A "implementação": as primitivas de desenho de cada plataforma
class Renderizador {
public:
    virtual ~Renderizador() = default;

    virtual std::string desenhar_circulo(double x, double y, double r) const = 0;
    virtual std::string desenhar_linha(double x1, double y1,
                                       double x2, double y2) const = 0;
    virtual std::string nome() const = 0;
};

class RenderizadorSvg final : public Renderizador {
public:
    std::string desenhar_circulo(double x, double y, double r) const override;
    std::string desenhar_linha(double x1, double y1, double x2, double y2) const override;
    std::string nome() const override { return "SVG"; }
};

class RenderizadorAscii final : public Renderizador {
public:
    std::string desenhar_circulo(double x, double y, double r) const override;
    std::string desenhar_linha(double x1, double y1, double x2, double y2) const override;
    std::string nome() const override { return "ASCII"; }
};

class RenderizadorOpenGl final : public Renderizador {
public:
    std::string desenhar_circulo(double x, double y, double r) const override;
    std::string desenhar_linha(double x1, double y1, double x2, double y2) const override;
    std::string nome() const override { return "OpenGL"; }
};

}  // namespace bridge
