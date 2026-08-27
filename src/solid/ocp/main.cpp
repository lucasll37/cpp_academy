// =============================================================================
//  src/solid/ocp/main.cpp — Subprojeto: Open/Closed Principle
// =============================================================================
//
//  "Software entities should be open for extension, but closed for
//   modification." — Bertrand Meyer (1988) / Robert C. Martin
//
//  Demos neste subprojeto:
//    1. Violação vs. Conformidade  — a cascata de if/else e o polimorfismo
//    2. OCP por injeção            — std::function, template policy, concepts
//    3. Registro auto-populado     — extensão sem tocar nem no main
//    4. Custo e testabilidade      — e a armadilha da abstração especulativa
//
//  Referências:
//    • Bertrand Meyer, "Object-Oriented Software Construction" (1988)
//    • Robert C. Martin, "Agile Software Development" (2003) — Cap. 9
//    • Robert C. Martin, "Clean Architecture" (2017) — Cap. 8
//
// =============================================================================

#include <fmt/core.h>
#include <string>

#include "demo_violation.hpp"
#include "demo_strategy.hpp"
#include "demo_registry.hpp"
#include "demo_testability.hpp"

static void header(const std::string& title) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", title);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│            cpp_academy — Open/Closed Principle          │\n");
    fmt::print("│   \"Open for extension, closed for modification.\"         │\n");
    fmt::print("│                              — Bertrand Meyer            │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    header("1. Violação vs. Conformidade — a cascata");
    demo_violation::run();

    header("2. OCP por injeção — função, template, concept");
    demo_strategy::run();

    header("3. Registro auto-populado");
    demo_registry::run();

    header("4. Custo, testabilidade e excessos");
    demo_testability::run();

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║              Resumo do OCP                               ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Definição  : aberto p/ extensão, fechado p/ modificação  ║\n");
    fmt::print("║ Sintoma    : switch/if-else sobre TIPO, repetido         ║\n");
    fmt::print("║ Ferramentas: virtual, std::function, template, concept   ║\n");
    fmt::print("║ Benefício  : código não editado não pode regredir        ║\n");
    fmt::print("║ Cuidado    : abstração especulativa — use a Rule of Three║\n");
    fmt::print("║ Alternativa: std::variant + visit p/ conjunto FECHADO    ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ ocp — todos os demos concluídos\n\n");

    return 0;
}
