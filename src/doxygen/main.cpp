// =============================================================================
//  src/doxygen/main.cpp — Ponto de entrada do subprojeto doxygen
// =============================================================================
//
//  Propósito deste subprojeto:
//    Demonstrar como documentar código C++ usando Doxygen, cobrindo:
//      • Comentários de arquivo, classe, função, enum, macro e membro
//      • Comandos: @brief, @details, @param, @return, @throws, @pre, @post
//      • Comandos: @note, @warning, @bug, @todo, @deprecated, @see, @code
//      • Agrupamento com @defgroup / @ingroup / @addtogroup
//      • Documentação de templates com @tparam
//      • Página principal com @mainpage
//      • Geração de HTML com Doxyfile
//
// =============================================================================

/**
 * @mainpage cpp_academy — Doxygen Demo
 *
 * @section intro Introdução
 *
 * Este subprojeto demonstra as práticas de documentação com **Doxygen**
 * aplicadas a código C++ moderno (C++20).
 *
 * @section estrutura Estrutura do Subprojeto
 *
 * | Arquivo           | Conteúdo                                           |
 * |-------------------|----------------------------------------------------|
 * | `math_utils.hpp`  | Funções e classes com documentação Doxygen completa |
 * | `main.cpp`        | Exercita a API e demonstra `@mainpage`             |
 * | `Doxyfile`        | Configuração do gerador de documentação            |
 * | `meson.build`     | Target `docs` que invoca `doxygen`                 |
 *
 * @section gerar Como Gerar a Documentação
 *
 * @code{.sh}
 * # Via Meson/Ninja (target configurado em meson.build):
 * make configure
 * cd build && ninja docs
 * xdg-open docs/html/index.html
 *
 * # Diretamente:
 * doxygen Doxyfile
 * @endcode
 *
 * @section convencoes Convenções de Comentário
 *
 * Doxygen reconhece três estilos:
 *
 * @code{.cpp}
 * // 1. Bloco JavaDoc (recomendado para blocos):
 * /**
 *  * @brief Descrição curta.
 *  * @param x Valor de entrada.
 *  *\/
 *
 * // 2. Barra tripla (bom para membros inline):
 * double x; ///< Coordenada horizontal.
 *
 * // 3. Exclamação (alternativa ao JavaDoc):
 * /*! @brief Descrição curta. *\/
 * @endcode
 */

/**
 * @file main.cpp
 * @brief Demonstração do uso da API math e geração de documentação Doxygen.
 */

#include <fmt/core.h>
#include <vector>
#include <numbers>   // std::numbers::pi

#include "math_utils.hpp"

// ─────────────────────────────────────────────────────────────────────────────
//  Separador visual
// ─────────────────────────────────────────────────────────────────────────────
static void secao(const char* titulo) {
    fmt::print("\n╔══════════════════════════════════════════════╗\n");
    fmt::print("║  {:<44}║\n", titulo);
    fmt::print("╚══════════════════════════════════════════════╝\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Demonstrações
// ─────────────────────────────────────────────────────────────────────────────

static void demo_funcoes_livres() {
    secao("Funções livres: abs, clamp, lerp, nearly_equal");

    // math::abs
    fmt::print("  abs(-7)     = {}\n",   math::abs(-7));
    fmt::print("  abs(-3.14f) = {:.2f}\n", math::abs(-3.14f));

    // math::clamp
    fmt::print("  clamp(15, 0, 10) = {}\n", math::clamp(15, 0, 10));
    fmt::print("  clamp(-3, 0, 10) = {}\n", math::clamp(-3, 0, 10));
    fmt::print("  clamp( 5, 0, 10) = {}\n", math::clamp( 5, 0, 10));

    // math::lerp
    fmt::print("  lerp(0.0, 100.0, 0.25) = {:.1f}\n",
        math::lerp(0.0, 100.0, 0.25));

    // math::nearly_equal
    double a = 0.1 + 0.2;
    double b = 0.3;
    fmt::print("  0.1+0.2 == 0.3         → {} (exact)\n",   a == b);
    fmt::print("  nearly_equal(0.1+0.2)  → {} (approx)\n",
        math::nearly_equal(a, b));

    // Conversão de unidades
    fmt::print("  90° em rad = {:.6f}\n", math::deg_to_rad(90.0));
    fmt::print("  π rad em ° = {:.1f}\n", math::rad_to_deg(std::numbers::pi));
}

static void demo_stats() {
    secao("Stats: mean, variance, stddev, median");

    std::vector<double> dados = {4.0, 8.0, 15.0, 16.0, 23.0, 42.0};
    math::Stats s(dados);

    fmt::print("  dataset:  [4, 8, 15, 16, 23, 42]\n");
    fmt::print("  count:    {}\n",    s.count());
    fmt::print("  min:      {:.1f}\n", s.min());
    fmt::print("  max:      {:.1f}\n", s.max());
    fmt::print("  mean:     {:.4f}\n", s.mean());
    fmt::print("  variance: {:.4f}\n", s.variance());
    fmt::print("  stddev:   {:.4f}\n", s.stddev());
    fmt::print("  median:   {:.1f}\n", s.median());
}

static void demo_vector2d() {
    secao("Vector2D: dot, cross, length, normalized, rotated");

    math::Vector2D a{3.0, 0.0};
    math::Vector2D b{0.0, 4.0};

    fmt::print("  a = ({}, {})\n", a.x, a.y);
    fmt::print("  b = ({}, {})\n", b.x, b.y);

    // Produto escalar
    fmt::print("  a.dot(b)       = {:.1f}\n", a.dot(b));       // 0 (perpendiculares)

    // Comprimento do vetor soma (teorema de Pitágoras: 3² + 4² = 25 → √25 = 5)
    auto soma = a + b;
    fmt::print("  (a+b).length() = {:.1f}\n", soma.length());   // 5.0

    // Normalização
    auto n = soma.normalized();
    fmt::print("  (a+b).normalized() = ({:.3f}, {:.3f})\n", n.x, n.y);
    fmt::print("  |normalized|       = {:.1f}\n", n.length());

    // Rotação de 90° (anti-horário)
    auto rot = a.rotated(std::numbers::pi / 2.0);
    fmt::print("  a rotacionado 90°  = ({:.1f}, {:.1f})\n", rot.x, rot.y); // (0, 3)

    // Produto vetorial (componente z)
    fmt::print("  a.cross(b)         = {:.1f}\n", a.cross(b));

    // Distância
    math::Vector2D p{1.0, 1.0};
    math::Vector2D q{4.0, 5.0};
    fmt::print("  dist({},{}) → ({},{}) = {:.1f}\n",
        p.x, p.y, q.x, q.y, p.distance_to(q));  // 5.0
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    fmt::print("\n");
    fmt::print("┌───────────────────────────────────────────────┐\n");
    fmt::print("│    cpp_academy — doxygen subproject           │\n");
    fmt::print("│    Documentação de código C++ com Doxygen     │\n");
    fmt::print("└───────────────────────────────────────────────┘\n");

    demo_funcoes_livres();
    demo_stats();
    demo_vector2d();

    fmt::print("\n✓ doxygen demo concluído\n");
    fmt::print("  Gere a doc com: cd build && ninja docs\n");
    fmt::print("  Abra: docs/html/index.html\n\n");
    return 0;
}
