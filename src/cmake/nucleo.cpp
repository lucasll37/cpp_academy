// =============================================================================
//  src/cmake/nucleo.cpp — a implementação de referência
// =============================================================================
#include "nucleo.hpp"

#include <cmath>
#include <numeric>
#include <random>

namespace nucleo {

void saxpy(float a, std::span<const float> x, std::span<float> y) {
    const std::size_t n = std::min(x.size(), y.size());
    for (std::size_t i = 0; i < n; ++i) y[i] = a * x[i] + y[i];
}

double soma(std::span<const float> v) {
    // Acumula em `double` de propósito: somar um milhão de `float` em `float`
    // perde precisão de forma visível. Ver src/strings/ e src/chrono/ sobre a
    // mesma classe de armadilha.
    return std::accumulate(v.begin(), v.end(), 0.0,
                           [](double acc, float x) { return acc + x; });
}

double erro_maximo(std::span<const float> a, std::span<const float> b) {
    const std::size_t n = std::min(a.size(), b.size());
    double pior = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        pior = std::max(pior, std::fabs(static_cast<double>(a[i]) - b[i]));
    return pior;
}

std::vector<float> gerar(std::size_t n, unsigned semente) {
    std::mt19937 rng{semente};                   // semente FIXA: reprodutível
    std::uniform_real_distribution<float> dist{-1.0f, 1.0f};

    std::vector<float> v(n);
    for (auto& x : v) x = dist(rng);
    return v;
}

}  // namespace nucleo
