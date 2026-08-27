// =============================================================================
//  src/solid/lsp/main.cpp — Subprojeto: Liskov Substitution Principle
// =============================================================================
//
//  "Se S é subtipo de T, objetos de T podem ser substituídos por objetos de S
//   sem alterar nenhuma propriedade desejável do programa."
//                                              — Barbara Liskov (1987)
//
//  Demos neste subprojeto:
//    1. Violação vs. Conformidade  — o Quadrado/Retângulo e por que ele falha
//    2. As quatro regras           — pré, pós, invariante e history rule
//    3. Detecção e prevenção       — testes de contrato, NVI, alternativas
//
//  O ponto que se perde com mais frequência: LSP é sobre COMPORTAMENTO, não
//  sobre assinatura. O compilador verifica a assinatura; ninguém verifica o
//  comportamento — por isso a violação compila e só aparece em produção.
//
//  Referências:
//    • Barbara Liskov, "Data Abstraction and Hierarchy" (1987)
//    • Liskov & Wing, "A Behavioral Notion of Subtyping" (1994)
//    • Robert C. Martin, "Agile Software Development" (2003) — Cap. 10
//
// =============================================================================

#include <fmt/core.h>
#include <string>

#include "demo_violation.hpp"
#include "demo_contracts.hpp"
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
    fmt::print("│        cpp_academy — Liskov Substitution Principle      │\n");
    fmt::print("│   \"Subtipos devem ser substituíveis por seus tipos       │\n");
    fmt::print("│    base.\"                        — Barbara Liskov        │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    header("1. Violação vs. Conformidade — Quadrado/Retângulo");
    demo_violation::run();

    header("2. As quatro regras do contrato");
    demo_contracts::run();

    header("3. Detectando e prevenindo violações");
    demo_testability::run();

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║              Resumo do LSP                               ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Definição  : o derivado cumpre TODAS as promessas da base║\n");
    fmt::print("║ Regra 1    : pré-condições não podem ser fortalecidas    ║\n");
    fmt::print("║ Regra 2    : pós-condições não podem ser enfraquecidas   ║\n");
    fmt::print("║ Regra 3    : invariantes da base têm que valer           ║\n");
    fmt::print("║ Regra 4    : history rule — sem estados novos proibidos  ║\n");
    fmt::print("║ Mnemônico  : mais GENEROSO, nunca mais EXIGENTE          ║\n");
    fmt::print("║ Sintoma    : override que lança, ou que não faz nada     ║\n");
    fmt::print("║ Detecção   : suíte de contrato rodada em cada derivado   ║\n");
    fmt::print("║ Prevenção  : NVI, `final`, composição, segregar (→ ISP)  ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ lsp — todos os demos concluídos\n\n");

    return 0;
}
