// =============================================================================
//  src/design-of-pattern/visitor/main.cpp
//
//  VISITOR — acrescentar operações sem tocar na hierarquia
//  ───────────────────────────────────────────────────────
//  É o padrão mais complexo do catálogo, e o que o C++ moderno mais mudou.
//  Este demo mostra o mecanismo clássico E a alternativa com `std::variant`,
//  que resolve o mesmo problema com metade do código.
// =============================================================================
#include "ast.hpp"
#include "visitantes.hpp"

#include <fmt/core.h>

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  A versão moderna: variant + visit
// ─────────────────────────────────────────────────────────────────────────────
namespace moderno {

struct Numero   { double valor; };
struct Variavel { std::string nome; };
struct Soma;
struct Mult;

using Expr = std::variant<Numero, Variavel,
                          std::unique_ptr<Soma>, std::unique_ptr<Mult>>;

struct Soma { Expr esq, dir; };
struct Mult { Expr esq, dir; };

template <typename... Ts> struct Overload : Ts... { using Ts::operator()...; };
template <typename... Ts> Overload(Ts...) -> Overload<Ts...>;

// Uma operação = uma FUNÇÃO LIVRE. Nenhuma classe, nenhum `aceitar`.
double avaliar(const Expr& e, const std::map<std::string, double>& vars) {
    return std::visit(Overload{
        [](const Numero& n)   { return n.valor; },
        [&](const Variavel& v) {
            const auto it = vars.find(v.nome);
            return it != vars.end() ? it->second : 0.0;
        },
        [&](const std::unique_ptr<Soma>& s) {
            return avaliar(s->esq, vars) + avaliar(s->dir, vars);
        },
        [&](const std::unique_ptr<Mult>& m) {
            return avaliar(m->esq, vars) * avaliar(m->dir, vars);
        },
    }, e);
}

std::string imprimir(const Expr& e) {
    return std::visit(Overload{
        [](const Numero& n)    { return fmt::format("{:g}", n.valor); },
        [](const Variavel& v)  { return v.nome; },
        [](const std::unique_ptr<Soma>& s) {
            return fmt::format("({} + {})", imprimir(s->esq), imprimir(s->dir));
        },
        [](const std::unique_ptr<Mult>& m) {
            return fmt::format("({} * {})", imprimir(m->esq), imprimir(m->dir));
        },
    }, e);
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
    fmt::print("│               cpp_academy — Visitor (GoF)                │\n");
    fmt::print("│   \"Represente uma operação sobre os elementos de uma     │\n");
    fmt::print("│    estrutura, sem mudar as classes deles.\"      — GoF    │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // Monta: (2 + x) * -(3 * y)
    auto expr = visitor::mult(
        visitor::soma(visitor::num(2), visitor::var("x")),
        visitor::neg(visitor::mult(visitor::num(3), visitor::var("y"))));

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. O problema");

    fmt::print("  Uma AST com 5 tipos de nó. Você precisa de 4 operações:\n");
    fmt::print("    avaliar · imprimir · otimizar · derivar\n\n");
    fmt::print("  SEM Visitor, cada operação é um método virtual em CADA nó:\n");
    fmt::print("    5 nós × 4 operações = 20 métodos espalhados\n");
    fmt::print("    e acrescentar `derivar` toca as 5 classes e recompila tudo\n\n");
    fmt::print("  COM Visitor:\n");
    fmt::print("    1 método (`aceitar`) por nó, escrito UMA vez\n");
    fmt::print("    4 classes de visitante, cada uma num arquivo próprio\n");
    fmt::print("    acrescentar `derivar` = um arquivo NOVO, `ast.hpp` intacto\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. Três operações sobre a MESMA árvore");

    visitor::Impressor imp;
    expr->aceitar(imp);
    fmt::print("  imprimir  → {}\n", imp.texto());

    visitor::Avaliador aval{{{"x", 4.0}, {"y", 5.0}}};
    expr->aceitar(aval);
    fmt::print("  avaliar   → {:g}   (com x=4, y=5)\n", aval.resultado());

    visitor::Contador cont;
    expr->aceitar(cont);
    fmt::print("  contar    → {} números, {} variáveis, {} operações, profundidade {}\n",
               cont.numeros(), cont.variaveis(), cont.operacoes(), cont.profundidade());

    fmt::print("\n  As três operações vivem em `visitantes.hpp`. O `ast.hpp` não\n");
    fmt::print("  sabe que elas existem — ele só tem `aceitar`.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. O despacho duplo, passo a passo");

    fmt::print("  A parte que confunde. Rastreando `expr->aceitar(imp)`:\n\n");
    fmt::print("    1. `expr` é `Expressao*`, tipo dinâmico `Multiplicacao`\n");
    fmt::print("       → despacho VIRTUAL entra em `Multiplicacao::aceitar`\n\n");
    fmt::print("    2. lá dentro:  `v.visitar(*this)`\n");
    fmt::print("       agora `*this` é `Multiplicacao&` — um tipo CONCRETO\n");
    fmt::print("       → resolução de OVERLOAD escolhe `visitar(const Multiplicacao&)`\n\n");
    fmt::print("    3. o `visitar` do Impressor roda, e chama `aceitar` nos filhos\n");
    fmt::print("       → recursão\n\n");
    fmt::print("  DUAS resoluções em sequência:\n");
    fmt::print("    virtual (descobre o NÓ) + overload (descobre a OPERAÇÃO)\n");
    fmt::print("  É daí que vem o nome \"despacho duplo\".\n\n");

    fmt::print("  E é por isso que `aceitar` NÃO pode ser genérico: ele precisa\n");
    fmt::print("  do `*this` com o tipo concreto. Cada nó escreve a MESMA linha,\n");
    fmt::print("  e ela é diferente em cada um por causa do tipo de `*this`.\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. Os três defeitos do Visitor clássico");

    fmt::print("  ⚠  1. A hierarquia fica FECHADA para tipos novos.\n");
    fmt::print("        Acrescentar `Divisao` obriga a:\n");
    fmt::print("          • um `visitar(const Divisao&)` na interface `Visitante`\n");
    fmt::print("          • implementá-lo em TODOS os visitantes existentes\n");
    fmt::print("        É o inverso exato do que o padrão dá de bom.\n\n");

    fmt::print("  ⚠  2. `visitar` devolve `void`.\n");
    fmt::print("        O resultado tem que virar ESTADO do visitante:\n");
    fmt::print("          double resultado_;   // ← o `Avaliador` acima\n");
    fmt::print("        E aí a recursão precisa salvar antes de sobrescrever:\n");
    fmt::print("          s.esquerda().aceitar(*this);\n");
    fmt::print("          const double e = resultado_;   // ← salva!\n");
    fmt::print("          s.direita().aceitar(*this);\n");
    fmt::print("          resultado_ = e + resultado_;\n");
    fmt::print("        Esquecer o `salva` é um bug silencioso e clássico.\n\n");

    fmt::print("  ⚠  3. Acoplamento circular.\n");
    fmt::print("        `ast.hpp` precisa declarar `Visitante`, e `Visitante`\n");
    fmt::print("        precisa conhecer todos os nós. Os dois arquivos se\n");
    fmt::print("        conhecem, e isso exige declarações antecipadas.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. A versão moderna: variant + visit");

    moderno::Expr m_expr = std::make_unique<moderno::Mult>(moderno::Mult{
        std::make_unique<moderno::Soma>(moderno::Soma{
            moderno::Numero{2}, moderno::Variavel{"x"}}),
        moderno::Numero{7}});

    fmt::print("  imprimir → {}\n", moderno::imprimir(m_expr));
    fmt::print("  avaliar  → {:g}   (com x=4)\n",
               moderno::avaliar(m_expr, {{"x", 4.0}}));

    fmt::print("\n  A operação é uma FUNÇÃO LIVRE:\n\n");
    fmt::print("    double avaliar(const Expr& e, const auto& vars) {{\n");
    fmt::print("        return std::visit(Overload{{\n");
    fmt::print("            [](const Numero& n) {{ return n.valor; }},\n");
    fmt::print("            [&](const auto& s) {{ /* recursão */ }},\n");
    fmt::print("        }}, e);\n");
    fmt::print("    }}\n\n");

    fmt::print("  Os três defeitos, resolvidos:\n");
    fmt::print("    ✓ devolve VALOR — nenhum estado acumulado, nenhum `salva`\n");
    fmt::print("    ✓ nenhum `aceitar` nos nós — os nós são structs simples\n");
    fmt::print("    ✓ nenhum acoplamento circular\n");
    fmt::print("    ✓ EXAUSTIVIDADE verificada: esquecer um caso não compila\n\n");

    fmt::print("  E o defeito que ele TROCA, não resolve:\n");
    fmt::print("    ✗ a lista de tipos é FECHADA no `using Expr = variant<...>`\n");
    fmt::print("      — mas o Visitor clássico também é, na prática (defeito 1)\n");

    // ── 6 ────────────────────────────────────────────────────────────────────
    secao("6. Escolhendo entre os dois");

    fmt::print("  | critério                    | Visitor  | variant+visit |\n");
    fmt::print("  |-----------------------------|----------|---------------|\n");
    fmt::print("  | operações novas             | fácil    | fácil         |\n");
    fmt::print("  | TIPOS novos                 | difícil  | difícil       |\n");
    fmt::print("  | exaustividade verificada    | NÃO      | SIM           |\n");
    fmt::print("  | retorno com valor           | não      | SIM           |\n");
    fmt::print("  | linhas de infraestrutura    | ~40      | ~3            |\n");
    fmt::print("  | alocação por nó             | sempre   | opcional      |\n");
    fmt::print("  | tipos de bibliotecas alheias| sim      | não*          |\n");
    fmt::print("  * exceto se você os listar no variant\n\n");

    fmt::print("  A recomendação para C++ moderno: comece com `variant`.\n");
    fmt::print("  Use o Visitor clássico quando:\n");
    fmt::print("    • a hierarquia JÁ existe com herança e você não pode mudá-la\n");
    fmt::print("    • os tipos vêm de bibliotecas que você não controla\n");
    fmt::print("    • a hierarquia é REALMENTE aberta (plugins acrescentam nós)\n");
    fmt::print("    • há dezenas de tipos e o variant ficaria impraticável\n\n");

    fmt::print("  E note que o `visit` também faz despacho MÚLTIPLO, com N\n");
    fmt::print("  variants de uma vez — o Visitor clássico só faz duplo, e\n");
    fmt::print("  estender para triplo seria uma explosão de métodos.\n");
    fmt::print("  Ver src/vocabulary_types/demo_variant.hpp, 2.4.\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║                Resumo do Visitor                         ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : operações novas sem tocar na hierarquia     ║\n");
    fmt::print("║ Mecanismo  : despacho duplo (virtual + overload)         ║\n");
    fmt::print("║ Barato     : acrescentar uma OPERAÇÃO                    ║\n");
    fmt::print("║ Caro       : acrescentar um TIPO de nó                   ║\n");
    fmt::print("║ Defeito 1  : `visitar` devolve void → estado acumulado   ║\n");
    fmt::print("║ Defeito 2  : sem verificação de exaustividade            ║\n");
    fmt::print("║ Moderno    : std::variant + std::visit resolve os dois   ║\n");
    fmt::print("║ Use o GoF  : hierarquia existente, aberta ou de terceiros║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ visitor — todos os demos concluídos\n\n");

    return 0;
}
