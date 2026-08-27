// =============================================================================
//  src/solid/dip/main.cpp — Subprojeto: Dependency Inversion Principle
// =============================================================================
//
//  a) "High-level modules should not depend on low-level modules.
//      Both should depend on abstractions."
//  b) "Abstractions should not depend on details.
//      Details should depend on abstractions."
//                                              — Robert C. Martin
//
//  Demos neste subprojeto:
//    1. Violação vs. Conformidade  — a seta de dependência invertida
//    2. Formas de injeção          — construtor, parâmetro, template, e o
//                                    composition root (onde o `new` mora)
//    3. DIP em escala              — a regra da dependência e Clean Architecture
//
//  O nome engana: "inversão" não é sobre injetar — é sobre a DIREÇÃO da seta
//  no diagrama de módulos. O teste decisivo é o `#include`.
//
//  Referências:
//    • Robert C. Martin, "Agile Software Development" (2003) — Cap. 11
//    • Robert C. Martin, "Clean Architecture" (2017) — Cap. 11 e 22
//    • Alistair Cockburn, "Hexagonal Architecture" (2005)
//
// =============================================================================

#include <fmt/core.h>
#include <string>

#include "demo_violation.hpp"
#include "demo_injection.hpp"
#include "demo_architecture.hpp"

static void header(const std::string& title) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", title);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│      cpp_academy — Dependency Inversion Principle       │\n");
    fmt::print("│   \"Depend upon abstractions, not concretions.\"           │\n");
    fmt::print("│                                  — Robert C. Martin      │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    header("1. Violação vs. Conformidade — a seta invertida");
    demo_violation::run();

    header("2. Formas de injeção e composition root");
    demo_injection::run();

    header("3. DIP em escala — a regra da dependência");
    demo_architecture::run();

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║              Resumo do DIP                               ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Definição  : dependa de abstrações, não de concretudes   ║\n");
    fmt::print("║ Chave      : a interface pertence ao módulo de ALTO nível║\n");
    fmt::print("║ Teste      : grep '#include' na camada de negócio        ║\n");
    fmt::print("║ Formas     : construtor, parâmetro, template (custo 0)   ║\n");
    fmt::print("║ O `new`    : confinado no composition root (o main)      ║\n");
    fmt::print("║ Anti-padrão: service locator — o objeto BUSCA, não recebe║\n");
    fmt::print("║ Inverta    : o que é VOLÁTIL (banco, rede, relógio)      ║\n");
    fmt::print("║ Não inverta: o que é estável (std::string, fmt)          ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ dip — todos os demos concluídos\n\n");

    return 0;
}
