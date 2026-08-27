// =============================================================================
//  src/design-of-pattern/bridge/main.cpp
//
//  BRIDGE — separar duas hierarquias que variam nos dois eixos
//  ───────────────────────────────────────────────────────────
//  O problema: quando DUAS dimensões variam ao mesmo tempo, a herança sozinha
//  produz explosão combinatória.
//
//      CirculoSvg, CirculoAscii, CirculoOpenGl,
//      QuadradoSvg, QuadradoAscii, QuadradoOpenGl,
//      TrianguloSvg, TrianguloAscii, TrianguloOpenGl     ← 3×3 = 9
//
//  E o custo real não é o número: acrescentar UM renderizador vira TRÊS classes
//  novas, uma por forma. Com Bridge, vira UMA.
// =============================================================================
#include "formas.hpp"
#include "renderizador.hpp"

#include <fmt/core.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  A versão moderna: a implementação como um struct de funções
// ─────────────────────────────────────────────────────────────────────────────
namespace moderno {

struct Primitivas {
    std::string nome;
    std::function<std::string(double, double, double)> circulo;
    std::function<std::string(double, double, double, double)> linha;
};

class Circulo {
public:
    Circulo(const Primitivas& p, double x, double y, double r)
        : p_(p), x_(x), y_(y), r_(r) {}

    std::string desenhar() const { return p_.circulo(x_, y_, r_); }

private:
    const Primitivas& p_;
    double x_, y_, r_;
};

}  // namespace moderno

static void secao(const char* titulo) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", titulo);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│               cpp_academy — Bridge (GoF)                 │\n");
    fmt::print("│   \"Desacople uma abstração da sua implementação, para    │\n");
    fmt::print("│    que as duas variem independentemente.\"       — GoF    │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. A explosão que ele evita");

    fmt::print("  Sem Bridge, com herança nos dois eixos:\n\n");
    fmt::print("    CirculoSvg    CirculoAscii    CirculoOpenGl\n");
    fmt::print("    QuadradoSvg   QuadradoAscii   QuadradoOpenGl\n");
    fmt::print("    TrianguloSvg  TrianguloAscii  TrianguloOpenGl\n\n");
    fmt::print("    3 formas × 3 renderizadores = 9 classes\n");
    fmt::print("    acrescentar 1 renderizador  = +3 classes\n");
    fmt::print("    acrescentar 1 forma         = +3 classes\n\n");
    fmt::print("  Com Bridge:\n");
    fmt::print("    3 formas + 3 renderizadores = 6 classes\n");
    fmt::print("    acrescentar 1 renderizador  = +1 classe\n");
    fmt::print("    acrescentar 1 forma         = +1 classe\n\n");
    fmt::print("  A conta piora rápido: 5×5 seria 25 contra 10.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. As duas hierarquias em ação");

    const bridge::RenderizadorSvg    svg;
    const bridge::RenderizadorAscii  ascii;
    const bridge::RenderizadorOpenGl gl;

    const std::vector<const bridge::Renderizador*> backends{&svg, &ascii, &gl};

    for (const auto* r : backends) {
        fmt::print("  renderizador {}:\n", r->nome());

        const bridge::Circulo   c{*r, 10, 20, 5};
        const bridge::Quadrado  q{*r, 0, 0, 4};
        const bridge::Triangulo t{*r, 0, 0, 6};

        fmt::print("    círculo   {}\n", c.desenhar());
        fmt::print("    quadrado  {}\n", q.desenhar());
        fmt::print("    triângulo {}\n", t.desenhar());
        fmt::print("\n");
    }

    fmt::print("  As MESMAS três classes de forma, com três backends. Nenhuma\n");
    fmt::print("  delas conhece SVG, ASCII ou OpenGL — todas falam com\n");
    fmt::print("  `Renderizador&`.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. As duas hierarquias variam de verdade");

    fmt::print("  o lado da ABSTRAÇÃO pode ser refinado:\n");
    bridge::Circulo c{ascii, 0, 0, 3};
    fmt::print("    antes de redimensionar: {} (área {:.2f})\n", c.desenhar(), c.area());
    c.redimensionar(2.0);
    fmt::print("    depois de ×2          : {} (área {:.2f})\n", c.desenhar(), c.area());
    fmt::print("  ↑ `redimensionar` é da ABSTRAÇÃO — nenhum renderizador soube.\n");

    fmt::print("\n  e o lado da IMPLEMENTAÇÃO pode ser trocado em runtime:\n");
    for (const auto* r : backends) {
        const bridge::Circulo mesmo{*r, 0, 0, 3};
        fmt::print("    {:<8} → {}\n", r->nome(), mesmo.desenhar());
    }

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. Bridge vs Strategy — a confusão comum");

    fmt::print("  Os dois têm a MESMA forma no diagrama de classes: um objeto\n");
    fmt::print("  guarda uma referência a uma interface e delega.\n");
    fmt::print("  A diferença é de INTENÇÃO e de ESCALA:\n\n");

    fmt::print("  | critério              | Strategy        | Bridge          |\n");
    fmt::print("  |-----------------------|-----------------|-----------------|\n");
    fmt::print("  | categoria             | comportamental  | ESTRUTURAL      |\n");
    fmt::print("  | o que varia           | um ALGORITMO    | uma HIERARQUIA  |\n");
    fmt::print("  | métodos na interface  | tipicamente 1   | vários          |\n");
    fmt::print("  | o outro lado é hier.? | não             | SIM             |\n");
    fmt::print("  | quando se decide      | por chamada     | na construção   |\n");
    fmt::print("  | motivação             | trocar política | evitar explosão |\n\n");

    fmt::print("  A pergunta que separa:\n");
    fmt::print("    \"o lado que eu injeto é uma HIERARQUIA que também cresce?\"\n");
    fmt::print("      não → Strategy (ver design-of-pattern/strategy/)\n");
    fmt::print("      sim → Bridge\n\n");
    fmt::print("  Aqui, `Renderizador` tem 3 implementações E 3 métodos, e a\n");
    fmt::print("  hierarquia de `Forma` também cresce. Duas dimensões → Bridge.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. O Bridge que você já usa: pimpl");

    fmt::print("  O idioma pimpl é um Bridge DEGENERADO — com exatamente uma\n");
    fmt::print("  implementação:\n\n");
    fmt::print("    // widget.hpp\n");
    fmt::print("    class Widget {{\n");
    fmt::print("        class Impl;                      // ← declaração só\n");
    fmt::print("        std::unique_ptr<Impl> pimpl_;    // ← a PONTE\n");
    fmt::print("    }};\n\n");
    fmt::print("  O objetivo ali não é evitar explosão combinatória: é quebrar\n");
    fmt::print("  a dependência de COMPILAÇÃO. Quem inclui `widget.hpp` não vê\n");
    fmt::print("  os membros de `Impl`, então mudá-los não recompila o mundo —\n");
    fmt::print("  e a ABI fica estável.\n");
    fmt::print("  Ver src/smart-pointers/demo_patterns.hpp.\n");

    fmt::print("\n  Outros Bridges no mundo real:\n");
    fmt::print("    JDBC/ODBC          → API única, N drivers de banco\n");
    fmt::print("    std::pmr           → containers × memory_resource\n");
    fmt::print("    backends gráficos  → Qt sobre X11/Wayland/Cocoa/Win32\n");
    fmt::print("    std::locale        → formatação × facetas\n");

    // ── 6 ────────────────────────────────────────────────────────────────────
    secao("6. A versão moderna: struct de funções");

    const moderno::Primitivas p_ascii{
        .nome    = "ASCII",
        .circulo = [](double, double, double r) {
            return fmt::format("( o ) raio {:.0f}", r);
        },
        .linha   = [](double, double, double, double) { return std::string{"-----"}; },
    };

    const moderno::Circulo mc{p_ascii, 0, 0, 7};
    fmt::print("  {}\n", mc.desenhar());

    fmt::print("\n    struct Primitivas {{\n");
    fmt::print("        std::string nome;\n");
    fmt::print("        std::function<std::string(double,double,double)> circulo;\n");
    fmt::print("        std::function<std::string(double,double,double,double)> linha;\n");
    fmt::print("    }};\n\n");

    fmt::print("  Ganha: montar um backend em runtime, misturando primitivas de\n");
    fmt::print("  origens diferentes; nenhuma hierarquia.\n");
    fmt::print("  Perde: o type erasure de cada `std::function` custa uma\n");
    fmt::print("  indireção por primitiva, e num laço de renderização isso é\n");
    fmt::print("  quente (ver src/lambdas/demo_function.hpp, 3.3 — 3,5× medido).\n\n");

    fmt::print("  Para um Bridge de RENDERIZAÇÃO, a hierarquia virtual costuma\n");
    fmt::print("  ganhar: a implementação é escolhida UMA vez na inicialização,\n");
    fmt::print("  e depois são milhões de chamadas. Uma vtable é mais barata\n");
    fmt::print("  que N `std::function`.\n");
    fmt::print("  E se a escolha for conhecida em COMPILAÇÃO, um template\n");
    fmt::print("  elimina os dois custos (src/solid/ocp/demo_strategy.hpp, 2).\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║                 Resumo do Bridge                         ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : duas hierarquias variando independentemente ║\n");
    fmt::print("║ Evita      : explosão combinatória (M×N → M+N)           ║\n");
    fmt::print("║ Forma      : a abstração GUARDA uma ref à implementação  ║\n");
    fmt::print("║ vs Strategy: o outro lado é uma HIERARQUIA, não 1 método ║\n");
    fmt::print("║ Caso especial: pimpl é um Bridge com 1 implementação     ║\n");
    fmt::print("║ Motivo lá  : quebrar dependência de compilação e ABI     ║\n");
    fmt::print("║ Alternativa: struct de function (runtime) ou template    ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ bridge — todos os demos concluídos\n\n");

    return 0;
}
