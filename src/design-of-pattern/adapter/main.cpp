// =============================================================================
//  src/design-of-pattern/adapter/main.cpp
//
//  ADAPTER — fazer duas interfaces incompatíveis conversarem
//  ─────────────────────────────────────────────────────────
//  O cenário que dá sentido ao padrão: uma biblioteca de terceiros (ou legada)
//  cuja interface não é a que o seu sistema espera, e que você NÃO pode mudar.
//
//  O Adapter é a única camada do programa que conhece as duas.
// =============================================================================
#include "adaptador.hpp"
#include "interface_moderna.hpp"
#include "legado.hpp"

#include <fmt/core.h>

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  O CLIENTE — escrito contra a interface moderna, sem saber do legado
// ─────────────────────────────────────────────────────────────────────────────
void processar_pedido(adapter::Log& log, int id) {
    log.info(fmt::format("processando pedido {}", id));
    if (id % 2 == 0) {
        log.aviso(fmt::format("pedido {} tem itens em falta", id));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  A versão moderna: adaptador por LAMBDA, sem hierarquia
//
//  Se a interface a adaptar tem UM método, uma classe inteira é exagero.
// ─────────────────────────────────────────────────────────────────────────────
namespace moderno {

using Escritor = std::function<void(adapter::Nivel, std::string_view)>;

// O "adaptador" vira uma lambda que fecha sobre o handle legado
Escritor adaptar(adapter::legado::LegacyLogger* h) {
    return [h](adapter::Nivel n, std::string_view msg) {
        adapter::legado::legacy_logger_write(h, static_cast<int>(n),
                                             msg.data(), msg.size());
    };
}

void usar(const Escritor& e) {
    e(adapter::Nivel::Info, "via adaptador-lambda");
    e(adapter::Nivel::Erro, "sem escrever uma classe");
}

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
    fmt::print("│              cpp_academy — Adapter (GoF)                 │\n");
    fmt::print("│   \"Converta a interface de uma classe em outra que os    │\n");
    fmt::print("│    clientes esperam.\"                          — GoF     │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. As duas interfaces");

    fmt::print("  O LEGADO (que não podemos mudar):\n");
    fmt::print("    LegacyLogger* legacy_logger_create(const char*);\n");
    fmt::print("    void          legacy_logger_destroy(LegacyLogger*);\n");
    fmt::print("    LegacyResult  legacy_logger_write(LegacyLogger*, int level,\n");
    fmt::print("                                      const char*, size_t);\n\n");
    fmt::print("  O que o SISTEMA espera:\n");
    fmt::print("    std::expected<void, ErroDeLog> escrever(Nivel, std::string_view);\n\n");
    fmt::print("  Quatro incompatibilidades, e o adaptador resolve as quatro:\n");
    fmt::print("    • código de erro int   → std::expected\n");
    fmt::print("    • int mágico de nível  → enum class Nivel\n");
    fmt::print("    • (char*, size_t)      → std::string_view\n");
    fmt::print("    • create/destroy manual→ RAII\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. O adaptador em ação");

    adapter::LogAdaptado log{"pedidos"};

    processar_pedido(log, 101);
    processar_pedido(log, 102);

    fmt::print("    {} mensagens pendentes; descarregando:\n", log.pendentes());
    (void)log.descarregar();

    fmt::print("\n  `processar_pedido` recebe `adapter::Log&`. Ela não sabe que\n");
    fmt::print("  existe uma biblioteca C por baixo — e nunca vai saber.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. A tradução de erros");

    // Forçar o buffer a encher, para ver o erro traduzido
    adapter::LogAdaptado pequeno{"teste"};
    fmt::print("  o logger legado tem buffer de 5. Escrevendo 7 mensagens:\n");

    for (int i = 1; i <= 7; ++i) {
        const auto r = pequeno.info(fmt::format("mensagem {}", i));
        if (r) {
            fmt::print("    {} → ok\n", i);
        } else {
            fmt::print("    {} → recusado: {}\n", i, adapter::descrever(r.error()));
        }
    }

    fmt::print("\n  O `LEGACY_BUFFER_FULL` (um `int` 2) virou\n");
    fmt::print("  `std::unexpected{{ErroDeLog::BufferCheio}}`, e o cliente\n");
    fmt::print("  tratou com `if (r)` sem nunca ver o código numérico.\n");
    fmt::print("  Ver src/errors/demo_expected.hpp.\n");

    (void)pequeno.descarregar();

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. Adapter de OBJETO vs de CLASSE");

    fmt::print("  O GoF descreve duas variantes:\n\n");
    fmt::print("  (a) de OBJETO — o adaptador CONTÉM o adaptado:\n");
    fmt::print("        class LogAdaptado : public Log {{\n");
    fmt::print("            LegacyLogger* handle_;      // ← composição\n");
    fmt::print("        }};\n\n");
    fmt::print("  (b) de CLASSE — o adaptador HERDA do adaptado:\n");
    fmt::print("        class LogAdaptado : public Log, private LegacyImpl {{ }};\n");
    fmt::print("      (em C++, herança PRIVADA — \"é implementado em termos de\",\n");
    fmt::print("       não \"é um\"; ver src/oop/demo_inheritance.hpp, 1.1)\n\n");

    fmt::print("  | critério                       | objeto | classe |\n");
    fmt::print("  |--------------------------------|--------|--------|\n");
    fmt::print("  | funciona com API C (sem classe)| SIM    | não    |\n");
    fmt::print("  | adapta uma HIERARQUIA inteira  | sim    | não*   |\n");
    fmt::print("  | pode trocar o adaptado em run  | SIM    | não    |\n");
    fmt::print("  | acessa membros protected       | não    | sim    |\n");
    fmt::print("  | sobrescreve o comportamento    | não    | sim    |\n");
    fmt::print("  * a de classe adapta só o tipo exato que ela herda\n\n");

    fmt::print("  A recomendação: quase sempre a de OBJETO. A de classe só\n");
    fmt::print("  quando você precisa de `protected` ou de sobrescrever um\n");
    fmt::print("  virtual do adaptado — e aí vale conferir se não é um\n");
    fmt::print("  Decorator que você quer (design-of-pattern/decorator/).\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. A versão moderna: adaptar com lambda");

    auto* handle = adapter::legado::legacy_logger_create("lambda");
    moderno::usar(moderno::adaptar(handle));
    (void)adapter::legado::legacy_logger_flush(handle);
    adapter::legado::legacy_logger_destroy(handle);

    fmt::print("\n    using Escritor = std::function<void(Nivel, string_view)>;\n\n");
    fmt::print("    Escritor adaptar(LegacyLogger* h) {{\n");
    fmt::print("        return [h](Nivel n, string_view m) {{\n");
    fmt::print("            legacy_logger_write(h, (int)n, m.data(), m.size());\n");
    fmt::print("        }};\n");
    fmt::print("    }}\n\n");

    fmt::print("  Quando a lambda basta:\n");
    fmt::print("    ✓ a interface a adaptar tem UM método\n");
    fmt::print("    ✓ não há estado nem ciclo de vida a gerenciar\n");
    fmt::print("    ✓ o adaptador é usado num lugar só\n\n");
    fmt::print("  Quando a classe se paga:\n");
    fmt::print("    ✓ a interface tem VÁRIOS métodos relacionados\n");
    fmt::print("    ✓ há RAII a fazer (foi o caso aqui: o `destroy`)\n");
    fmt::print("    ✓ há tradução de erro/tipo em várias direções\n");
    fmt::print("    ✓ o adaptador precisa de estado próprio\n");

    fmt::print("\n  ⚠  repare que a versão-lambda acima PERDEU o RAII: o\n");
    fmt::print("     `destroy` voltou a ser responsabilidade de quem chama.\n");
    fmt::print("     Foi por isso que este demo precisou das duas últimas linhas\n");
    fmt::print("     manuais. É o trade-off, não um detalhe.\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║               Resumo do Adapter                          ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : traduzir uma interface na que o cliente quer║\n");
    fmt::print("║ Cenário    : biblioteca de terceiros ou legada           ║\n");
    fmt::print("║ Variante   : de OBJETO (composição) — prefira esta       ║\n");
    fmt::print("║ Variante   : de CLASSE (herança privada) — casos raros   ║\n");
    fmt::print("║ Bônus      : é o lugar natural para pôr RAII numa API C  ║\n");
    fmt::print("║ Bônus      : e para traduzir erro int → expected         ║\n");
    fmt::print("║ Alternativa: uma lambda, se for UM método e sem estado   ║\n");
    fmt::print("║ Confusão   : Adapter muda a INTERFACE; Decorator, o      ║\n");
    fmt::print("║              COMPORTAMENTO, mantendo a interface         ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ adapter — todos os demos concluídos\n\n");

    return 0;
}
