// =============================================================================
//  src/solid/isp/main.cpp — Subprojeto: Interface Segregation Principle
// =============================================================================
//
//  "Clients should not be forced to depend upon interfaces that they do not
//   use."                                        — Robert C. Martin
//
//  Demos neste subprojeto:
//    1. Violação vs. Conformidade  — a "fat interface" e a segregação
//    2. Interfaces do cliente      — role interfaces e o que elas compram
//
//  ISP e LSP são o mesmo problema visto de dois lados:
//    LSP: "o derivado consegue cumprir tudo que a base promete?"
//    ISP: "a base está prometendo coisas demais?"
//
//  Referências:
//    • Robert C. Martin, "Agile Software Development" (2003) — Cap. 12
//    • Robert C. Martin, "Clean Architecture" (2017) — Cap. 10
//
// =============================================================================

#include <fmt/core.h>
#include <string>

#include "demo_violation.hpp"
#include "demo_client_side.hpp"

static void header(const std::string& title) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", title);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│      cpp_academy — Interface Segregation Principle      │\n");
    fmt::print("│   \"Clients should not be forced to depend upon           │\n");
    fmt::print("│    interfaces that they do not use.\"   — Robert Martin   │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    header("1. Violação vs. Conformidade — a fat interface");
    demo_violation::run();

    header("2. A interface pertence ao cliente");
    demo_client_side::run();

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║              Resumo do ISP                               ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Definição  : ninguém depende do que não usa              ║\n");
    fmt::print("║ Sintoma    : override que lança, ou que não faz nada     ║\n");
    fmt::print("║ Sintoma    : dublê de teste com 12 métodos p/ testar 2   ║\n");
    fmt::print("║ Ferramenta : herança múltipla de interfaces puras        ║\n");
    fmt::print("║ Regra      : a interface pertence ao CLIENTE             ║\n");
    fmt::print("║ Nomeação   : pelo PAPEL, não pela classe concreta        ║\n");
    fmt::print("║ Benefício  : erro de compilação no lugar de erro runtime ║\n");
    fmt::print("║ Cuidado    : segregar demais vira sopa de interfaces     ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ isp — todos os demos concluídos\n\n");

    return 0;
}
