// =============================================================================
//  src/design-of-pattern/abstract_factory/main.cpp
//
//  ABSTRACT FACTORY — criar FAMÍLIAS compatíveis
//  ─────────────────────────────────────────────
//  A Factory simples (design-of-pattern/factory/) cria UM produto. Este cria
//  uma FAMÍLIA, e garante que os membros combinem entre si.
//
//  A diferença aparece no que cada um IMPEDE:
//
//    Factory  : criar_botao("escuro"), criar_campo("claro")   ← compila!
//    Abstract : a família é escolhida UMA vez; misturar é impossível
// =============================================================================
#include "tema_claro.hpp"
#include "tema_escuro.hpp"
#include "widgets.hpp"

#include <fmt/core.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  O CLIENTE — escrito UMA vez, contra as interfaces abstratas.
//
//  Repare que ele não menciona `claro::` nem `escuro::`. Acrescentar um tema
//  não muda uma linha aqui.
// ─────────────────────────────────────────────────────────────────────────────
void renderizar_formulario(const abstract_factory::FabricaDeWidgets& f) {
    const auto titulo = f.criar_botao("Salvar");
    const auto campo  = f.criar_campo();
    const auto caixa  = f.criar_caixa();

    fmt::print("    tema '{}':\n", f.nome_do_tema());
    fmt::print("      nome    {}\n", campo->desenhar("Ada Lovelace"));
    fmt::print("      email   {}\n", campo->desenhar("ada@exemplo.com"));
    fmt::print("      aceito  {} termos de uso\n", caixa->desenhar(true));
    fmt::print("      opcional{} newsletter\n", caixa->desenhar(false));
    fmt::print("      {}  → ao clicar: {}\n", titulo->desenhar(), titulo->ao_clicar());
}

// ─────────────────────────────────────────────────────────────────────────────
//  A versão moderna: a fábrica como um struct de std::function
//
//  Se a "família" é só um conjunto de funções de criação, uma interface com N
//  métodos virtuais pode ser substituída por um struct de callables. Ganha-se
//  a possibilidade de montar famílias em RUNTIME, misturando implementações.
// ─────────────────────────────────────────────────────────────────────────────
namespace moderno {

struct Tema {
    std::string nome;
    std::function<std::string(std::string_view)> botao;
    std::function<std::string(std::string_view)> campo;
    std::function<std::string(bool)>             caixa;
};

Tema tema_minimalista() {
    return {
        .nome  = "minimalista",
        .botao = [](std::string_view r) { return fmt::format("<{}>", r); },
        .campo = [](std::string_view c) { return fmt::format("{}", c); },
        .caixa = [](bool m) { return std::string{m ? "+" : "-"}; },
    };
}

void renderizar(const Tema& t) {
    fmt::print("    tema '{}':\n", t.nome);
    fmt::print("      nome    {}\n", t.campo("Ada Lovelace"));
    fmt::print("      aceito  {} termos\n", t.caixa(true));
    fmt::print("      {}\n", t.botao("Salvar"));
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
    fmt::print("│          cpp_academy — Abstract Factory (GoF)            │\n");
    fmt::print("│   \"Forneça uma interface para criar FAMÍLIAS de objetos  │\n");
    fmt::print("│    relacionados, sem especificar suas classes.\"  — GoF   │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. O que ele impede que a Factory não impede");

    fmt::print("  Com uma Factory simples, uma por produto:\n\n");
    fmt::print("    auto b = criar_botao(\"escuro\");\n");
    fmt::print("    auto c = criar_campo(\"claro\");    // ← COMPILA, e a UI\n");
    fmt::print("                                      //   fica inconsistente\n\n");
    fmt::print("  Com Abstract Factory:\n\n");
    fmt::print("    const FabricaDeWidgets& f = escolher_tema();\n");
    fmt::print("    auto b = f.criar_botao(\"Salvar\");\n");
    fmt::print("    auto c = f.criar_campo();         // ← MESMA família,\n");
    fmt::print("                                      //   garantido pelo tipo\n\n");
    fmt::print("  A família é escolhida UMA vez. Depois disso, misturar não é\n");
    fmt::print("  \"desencorajado\" — é INEXPRIMÍVEL.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. O mesmo cliente, duas famílias");

    const abstract_factory::claro::Fabrica  f_claro;
    const abstract_factory::escuro::Fabrica f_escuro;

    renderizar_formulario(f_claro);
    fmt::print("\n");
    renderizar_formulario(f_escuro);

    fmt::print("\n  `renderizar_formulario` foi escrita UMA vez, contra\n");
    fmt::print("  `FabricaDeWidgets`. Ela não menciona `claro::` nem `escuro::`.\n");
    fmt::print("  Acrescentar um terceiro tema não muda uma linha dela.\n");
    fmt::print("  É o Open/Closed Principle — ver src/solid/ocp/.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. Escolhendo a família em runtime");

    const std::map<std::string, std::function<std::unique_ptr<
        abstract_factory::FabricaDeWidgets>()>> temas{
        {"claro",  [] { return std::make_unique<abstract_factory::claro::Fabrica>(); }},
        {"escuro", [] { return std::make_unique<abstract_factory::escuro::Fabrica>(); }},
    };

    for (const std::string escolhido : {"escuro", "claro", "inexistente"}) {
        const auto it = temas.find(escolhido);
        if (it == temas.end()) {
            fmt::print("    tema '{}' não existe — caindo no padrão\n", escolhido);
            continue;
        }
        const auto fabrica = it->second();
        const auto b = fabrica->criar_botao("OK");
        fmt::print("    '{}' → {}\n", escolhido, b->desenhar());
    }

    fmt::print("\n  O mapa de fábricas é a ponte entre uma configuração de\n");
    fmt::print("  runtime (uma string vinda de um arquivo) e o sistema de tipos.\n");
    fmt::print("  Ele é o ÚNICO lugar do programa que conhece os nomes concretos.\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. O custo real do padrão");

    fmt::print("  Acrescentar um TEMA (uma família nova):\n");
    fmt::print("    ✓ um arquivo novo, nada mais muda        ← barato\n\n");
    fmt::print("  Acrescentar um WIDGET (um produto novo na família):\n");
    fmt::print("    ✗ um método novo em `FabricaDeWidgets`\n");
    fmt::print("    ✗ uma classe nova em CADA tema existente\n");
    fmt::print("    ✗ TODAS as fábricas concretas param de compilar até\n");
    fmt::print("      implementarem o método novo\n\n");

    fmt::print("  Este é o trade-off central, e ele tem nome: o padrão é\n");
    fmt::print("  aberto no eixo das FAMÍLIAS e fechado no eixo dos PRODUTOS.\n");
    fmt::print("  É o \"expression problem\" outra vez, e a pergunta que decide é:\n\n");
    fmt::print("    \"o que vai variar mais — o número de famílias ou o de\n");
    fmt::print("     produtos por família?\"\n\n");
    fmt::print("  Se forem os PRODUTOS, o Abstract Factory é o padrão errado.\n");
    fmt::print("  (o mesmo eixo aparece em src/vocabulary_types/, 2.7)\n");

    fmt::print("\n  ⚠  E há um sintoma de uso errado: uma fábrica abstrata com\n");
    fmt::print("     UM produto só. Isso é uma Factory, não um Abstract Factory —\n");
    fmt::print("     e a interface a mais é peso morto.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. A versão moderna: struct de std::function");

    moderno::renderizar(moderno::tema_minimalista());

    fmt::print("\n    struct Tema {{\n");
    fmt::print("        std::string nome;\n");
    fmt::print("        std::function<std::string(std::string_view)> botao;\n");
    fmt::print("        std::function<std::string(std::string_view)> campo;\n");
    fmt::print("        std::function<std::string(bool)>             caixa;\n");
    fmt::print("    }};\n\n");

    fmt::print("  O que essa forma GANHA:\n");
    fmt::print("    ✓ montar uma família em RUNTIME, misturando implementações\n");
    fmt::print("      (pegar o botão de um tema e o campo de outro)\n");
    fmt::print("    ✓ nenhuma hierarquia, nenhum arquivo por tema\n");
    fmt::print("    ✓ criar uma variação é preencher um struct\n\n");
    fmt::print("  O que ela PERDE:\n");
    fmt::print("    ✗ a garantia de consistência — misturar volta a ser possível\n");
    fmt::print("      (e às vezes é exatamente o que se quer)\n");
    fmt::print("    ✗ estado compartilhado entre os produtos da família\n");
    fmt::print("    ✗ o custo do type erasure (src/lambdas/demo_function.hpp)\n\n");

    fmt::print("  | quando                              | use                  |\n");
    fmt::print("  |-------------------------------------|----------------------|\n");
    fmt::print("  | famílias fixas, consistência é regra| Abstract Factory     |\n");
    fmt::print("  | os produtos compartilham estado     | Abstract Factory     |\n");
    fmt::print("  | famílias montadas em runtime        | struct de function   |\n");
    fmt::print("  | um produto só                       | Factory simples      |\n");
    fmt::print("  | a variação é conhecida na compilação| policy/template      |\n");
    fmt::print("  |                                     | (src/solid/ocp/, 2)  |\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║           Resumo do Abstract Factory                     ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : criar FAMÍLIAS de produtos compatíveis      ║\n");
    fmt::print("║ Garantia   : misturar famílias fica INEXPRIMÍVEL         ║\n");
    fmt::print("║ Barato     : acrescentar uma FAMÍLIA (um arquivo)        ║\n");
    fmt::print("║ Caro       : acrescentar um PRODUTO (toca todas as fábr.)║\n");
    fmt::print("║ Pergunta   : varia mais o nº de famílias ou de produtos? ║\n");
    fmt::print("║ Sintoma ruim: fábrica abstrata com UM produto            ║\n");
    fmt::print("║ Alternativa: struct de std::function, se quiser misturar ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ abstract_factory — todos os demos concluídos\n\n");

    return 0;
}
