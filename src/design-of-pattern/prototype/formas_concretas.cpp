#include "formas_concretas.hpp"

#include <fmt/core.h>

#include <cmath>

namespace prototype {

std::string Circulo::descrever() const {
    return fmt::format("Círculo(r={:.1f}, {})", raio_, cor_);
}
double Circulo::area() const { return 3.14159265358979323846 * raio_ * raio_; }

std::string Retangulo::descrever() const {
    return fmt::format("Retângulo({:.1f}x{:.1f}, {})", largura_, altura_, cor_);
}
double Retangulo::area() const { return largura_ * altura_; }

std::string Grupo::descrever() const {
    std::string s = fmt::format("Grupo[{} filhos, {}]: ", filhos_.size(), cor_);
    for (std::size_t i = 0; i < filhos_.size(); ++i) {
        if (i > 0) s += ", ";
        s += filhos_[i]->descrever();
    }
    return s;
}

double Grupo::area() const {
    double total = 0;
    for (const auto& f : filhos_) total += f->area();
    return total;
}

}  // namespace prototype
