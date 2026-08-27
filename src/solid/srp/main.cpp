// =============================================================================
//  src/srp/main.cpp — Subprojeto: Single Responsibility Principle
// =============================================================================
//
//  O Princípio da Responsabilidade Única (SRP) estabelece que um módulo
//  deve ter exatamente uma razão para mudar — ou seja, deve ser responsável
//  perante um único ator (grupo de stakeholders).
//
//  Demos neste subprojeto:
//    1. Violação vs. Conformidade  — a "God Class" e sua decomposição
//    2. Pipeline SRP               — processamento de pedidos em etapas
//    3. SRP em Funções             — o princípio além das classes + CQS
//    4. Atores (Clean Architecture)— Employee, COO, CFO, CTO
//    5. SRP + Interfaces           — testabilidade via injeção de dependência
//
//  Referências:
//    • Robert C. Martin, "Clean Code" (2008) — Cap. 10
//    • Robert C. Martin, "Clean Architecture" (2017) — Cap. 7
//    • Robert C. Martin, "Agile Software Development" (2003)
//
// =============================================================================

#include <fmt/core.h>
#include <string>

#include "demo_violation.hpp"
#include "demo_pipeline.hpp"
#include "demo_functions.hpp"
#include "demo_actors.hpp"
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
    fmt::print("│       cpp_academy — Single Responsibility Principle      │\n");
    fmt::print("│   \"A module should be responsible to one, and only       │\n");
    fmt::print("│    one, actor.\" — Robert C. Martin                       │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    header("1. Violação vs. Conformidade — God Class");
    demo_violation::run();

    header("2. Pipeline — Processamento de Pedidos");
    demo_pipeline::run();

    header("3. SRP em Funções + CQS");
    demo_functions::run();

    header("4. Atores — COO / CFO / CTO (Clean Architecture)");
    demo_actors::run();

    header("5. Interfaces + Injeção + Testabilidade");
    demo_testability::run();

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║              Resumo do SRP                               ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Definição  : um módulo → um ator                         ║\n");
    fmt::print("║ Heurística : \"por quantas razões isto pode mudar?\"        ║\n");
    fmt::print("║ Sinal      : classe longa, método 'e'/'ou' na descrição  ║\n");
    fmt::print("║ Ferramenta : interfaces + injeção de dependência         ║\n");
    fmt::print("║ Benefício  : testabilidade, manutenibilidade, coesão     ║\n");
    fmt::print("║ Cuidado    : não fragmente demais — coesão > granulidade ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ srp — todos os demos concluídos\n\n");

    return 0;
}
