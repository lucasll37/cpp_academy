#include "formas.hpp"
#include "renderizador.hpp"

#include <fmt/core.h>

#include <cmath>

namespace bridge {

// ── Implementações ───────────────────────────────────────────────────────────
std::string RenderizadorSvg::desenhar_circulo(double x, double y, double r) const {
    return fmt::format("<circle cx=\"{:.0f}\" cy=\"{:.0f}\" r=\"{:.0f}\"/>", x, y, r);
}
std::string RenderizadorSvg::desenhar_linha(double x1, double y1,
                                            double x2, double y2) const {
    return fmt::format("<line x1=\"{:.0f}\" y1=\"{:.0f}\" x2=\"{:.0f}\" y2=\"{:.0f}\"/>",
                       x1, y1, x2, y2);
}

std::string RenderizadorAscii::desenhar_circulo(double, double, double r) const {
    return fmt::format("( o ) raio {:.0f}", r);
}
std::string RenderizadorAscii::desenhar_linha(double, double, double, double) const {
    return "-----";
}

std::string RenderizadorOpenGl::desenhar_circulo(double x, double y, double r) const {
    return fmt::format("glDrawArrays(GL_TRIANGLE_FAN, círculo({:.0f},{:.0f},{:.0f}))",
                       x, y, r);
}
std::string RenderizadorOpenGl::desenhar_linha(double x1, double y1,
                                               double x2, double y2) const {
    return fmt::format("glDrawArrays(GL_LINES, [{:.0f},{:.0f}]→[{:.0f},{:.0f}])",
                       x1, y1, x2, y2);
}

// ── Abstrações ───────────────────────────────────────────────────────────────
double Circulo::area() const { return 3.14159265358979323846 * raio_ * raio_; }

std::string Quadrado::desenhar() const {
    // Um quadrado é quatro linhas — a abstração COMPÕE as primitivas
    return fmt::format("{} {} {} {}",
                       r_.desenhar_linha(x_, y_, x_ + lado_, y_),
                       r_.desenhar_linha(x_ + lado_, y_, x_ + lado_, y_ + lado_),
                       r_.desenhar_linha(x_ + lado_, y_ + lado_, x_, y_ + lado_),
                       r_.desenhar_linha(x_, y_ + lado_, x_, y_));
}

std::string Triangulo::desenhar() const {
    const double h = base_ * std::sqrt(3.0) / 2.0;
    return fmt::format("{} {} {}",
                       r_.desenhar_linha(x_, y_, x_ + base_, y_),
                       r_.desenhar_linha(x_ + base_, y_, x_ + base_ / 2, y_ + h),
                       r_.desenhar_linha(x_ + base_ / 2, y_ + h, x_, y_));
}

double Triangulo::area() const { return base_ * base_ * std::sqrt(3.0) / 4.0; }

}  // namespace bridge
