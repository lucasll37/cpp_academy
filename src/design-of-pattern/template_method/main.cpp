// =============================================================================
//  src/design-of-pattern/template_method/main.cpp
//
//  TEMPLATE METHOD — o esqueleto na base, os passos no derivado
//  ────────────────────────────────────────────────────────────
//  A base define a ORDEM; os derivados definem o CONTEÚDO dos passos.
//
//  A inversão que caracteriza o padrão:
//    Strategy        → o cliente CHAMA a estratégia
//    Template Method → a BASE chama o derivado
//                      ("don't call us, we'll call you")
// =============================================================================
#include "pipeline.hpp"
#include "pipelines_concretos.hpp"

#include <fmt/core.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  A versão moderna: o esqueleto como uma FUNÇÃO com callbacks
// ─────────────────────────────────────────────────────────────────────────────
namespace moderno {

using Registro = template_method::Registro;

struct Passos {
    std::function<std::vector<Registro>(const std::string&)> extrair;
    std::function<void(Registro&)>                           transformar;
    std::function<bool(const Registro&)> validar = [](const Registro& r) {
        return r.valido;
    };
};

// O esqueleto vira uma função livre. A ordem continua fixa, mas não há
// hierarquia nenhuma.
std::vector<Registro> executar(const std::string& fonte, const Passos& p) {
    auto registros = p.extrair(fonte);
    std::vector<Registro> saida;
    for (auto& r : registros) {
        if (!p.validar(r)) continue;
        p.transformar(r);
        saida.push_back(std::move(r));
    }
    return saida;
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
    fmt::print("│          cpp_academy — Template Method (GoF)             │\n");
    fmt::print("│   \"Defina o esqueleto de um algoritmo, deixando alguns   │\n");
    fmt::print("│    passos para as subclasses.\"                  — GoF    │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. O mesmo esqueleto, três conteúdos");

    template_method::PipelineCsv csv;
    const auto r_csv = csv.executar("alfa,10.0\nbeta,20.0\ngama,30.0");
    fmt::print("      resultado: ");
    for (const auto& r : r_csv) fmt::print("{}={:.1f} ", r.chave, r.valor);
    fmt::print("\n\n");

    template_method::PipelineJson json;
    const auto r_json = json.executar("x:1.4;y:2.6;z:-3.0");
    fmt::print("      resultado: ");
    for (const auto& r : r_json) fmt::print("{}={:.0f} ", r.chave, r.valor);
    fmt::print("\n\n");

    template_method::PipelineBanco banco;
    const auto r_banco = banco.executar("");
    fmt::print("      resultado: ");
    for (const auto& r : r_banco) fmt::print("{}={:.1f} ", r.chave, r.valor);
    fmt::print("\n");

    fmt::print("\n  Os três seguiram a MESMA sequência:\n");
    fmt::print("    antes_de_tudo → extrair → validar → transformar → ordenar\n");
    fmt::print("      → depois_de_tudo\n");
    fmt::print("  Essa ordem está escrita UMA vez, em `PipelineDeDados::executar`.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. Os três tipos de passo");

    fmt::print("  A distinção que faz o padrão funcionar:\n\n");
    fmt::print("  | tipo           | declaração              | o derivado...   |\n");
    fmt::print("  |----------------|-------------------------|-----------------|\n");
    fmt::print("  | OBRIGATÓRIO    | virtual ... = 0         | TEM que impl.   |\n");
    fmt::print("  | HOOK           | virtual ... {{ default }} | PODE sobrescr.  |\n");
    fmt::print("  | FIXO           | não-virtual, private    | não pode mudar  |\n\n");

    fmt::print("  Neste pipeline:\n");
    fmt::print("    obrigatórios: extrair(), transformar()\n");
    fmt::print("    hooks       : validar(), antes_de_tudo(), depois_de_tudo(),\n");
    fmt::print("                  deve_ordenar()\n");
    fmt::print("    fixo        : ordenar()  ← private, ninguém alcança\n\n");

    fmt::print("  Quanto cada pipeline escreveu:\n");
    fmt::print("    PipelineCsv   → 2 métodos (só os obrigatórios)\n");
    fmt::print("    PipelineJson  → 3 métodos (+ o hook `validar`)\n");
    fmt::print("    PipelineBanco → 6 métodos (usa todos os hooks)\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. Por que `executar()` é NÃO-virtual");

    fmt::print("  A ordem dos passos é EXATAMENTE o que o padrão protege.\n");
    fmt::print("  Se `executar()` fosse virtual, um derivado poderia\n");
    fmt::print("  sobrescrevê-lo e quebrar a garantia — e aí o padrão não\n");
    fmt::print("  garante mais nada.\n\n");
    fmt::print("  É o idioma NVI (Non-Virtual Interface):\n");
    fmt::print("    público  + NÃO-virtual → a fronteira e o contrato\n");
    fmt::print("    protegido + virtual    → os pontos de customização\n\n");
    fmt::print("  O mesmo idioma aparece em:\n");
    fmt::print("    src/solid/lsp/demo_testability.hpp, 3.2 — para pré/pós-condição\n");
    fmt::print("    src/oop/demo_virtual.hpp, 2.6           — para arg. padrão\n");
    fmt::print("    design-of-pattern/composite/            — encontrado na prática\n");

    fmt::print("\n  Repare também que os passos são `protected`, não `public`:\n");
    fmt::print("  o cliente não deve chamar `extrair()` direto — só `executar()`.\n");
    fmt::print("  É o ISP: a interface pública promete só o que o cliente usa\n");
    fmt::print("  (src/solid/isp/).\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. Estender um hook em vez de substituí-lo");

    fmt::print("  O `PipelineJson::validar` faz:\n\n");
    fmt::print("    bool validar(const Registro& r) const override {{\n");
    fmt::print("        return PipelineDeDados::validar(r) && r.valor >= 0;\n");
    fmt::print("               ^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    fmt::print("               chama o DEFAULT da base, e ACRESCENTA\n");
    fmt::print("    }}\n\n");
    fmt::print("  Chamar a versão da base é a diferença entre ESTENDER e\n");
    fmt::print("  SUBSTITUIR. Substituir descarta silenciosamente a regra que a\n");
    fmt::print("  base garantia — e isso costuma violar o LSP\n");
    fmt::print("  (src/solid/lsp/demo_contracts.hpp, regra 2).\n\n");

    fmt::print("  ⚠  a linguagem não OBRIGA a chamar a base. É uma convenção\n");
    fmt::print("     que precisa estar documentada no hook. Se a regra da base\n");
    fmt::print("     for inegociável, ela não deve ser um hook — deve estar no\n");
    fmt::print("     template method, antes de chamar o hook.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. Template Method vs Strategy");

    fmt::print("  Os dois permitem variar um algoritmo. A diferença:\n\n");
    fmt::print("  | critério            | Template Method  | Strategy        |\n");
    fmt::print("  |---------------------|------------------|-----------------|\n");
    fmt::print("  | mecanismo           | HERANÇA          | COMPOSIÇÃO      |\n");
    fmt::print("  | quem chama quem     | a BASE chama     | o cliente chama |\n");
    fmt::print("  | varia               | PASSOS de um algo| o algoritmo TODO|\n");
    fmt::print("  | trocar em runtime   | não              | SIM             |\n");
    fmt::print("  | quantos pontos      | vários (hooks)   | tipicamente 1   |\n");
    fmt::print("  | acoplamento         | forte (herança)  | fraco           |\n\n");

    fmt::print("  A pergunta que separa:\n");
    fmt::print("    \"a ORDEM dos passos é o que eu quero garantir?\"\n");
    fmt::print("      sim → Template Method\n");
    fmt::print("      não, quero trocar o algoritmo inteiro → Strategy\n\n");

    fmt::print("  ⚠  o Template Method usa HERANÇA, e isso é uma escolha cara:\n");
    fmt::print("     • acoplamento em tempo de COMPILAÇÃO\n");
    fmt::print("     • um derivado só pode ter UM pipeline-base\n");
    fmt::print("     • não dá para trocar de comportamento em runtime\n");
    fmt::print("     Se qualquer um desses incomodar, o Strategy é melhor.\n");
    fmt::print("     Ver design-of-pattern/strategy/ e src/solid/ocp/, 2.\n");

    // ── 6 ────────────────────────────────────────────────────────────────────
    secao("6. A versão moderna: callbacks num struct");

    const auto r_moderno = moderno::executar("", {
        .extrair = [](const std::string&) {
            return std::vector<moderno::Registro>{
                {"a", 10.0, true}, {"b", -5.0, true}, {"c", 20.0, true}};
        },
        .transformar = [](moderno::Registro& r) { r.valor *= 3; },
        .validar     = [](const moderno::Registro& r) { return r.valor > 0; },
    });

    fmt::print("  resultado: ");
    for (const auto& r : r_moderno) fmt::print("{}={:.0f} ", r.chave, r.valor);
    fmt::print("\n\n");

    fmt::print("    struct Passos {{\n");
    fmt::print("        std::function<vector<Registro>(const string&)> extrair;\n");
    fmt::print("        std::function<void(Registro&)>                transformar;\n");
    fmt::print("        std::function<bool(const Registro&)> validar = default;\n");
    fmt::print("    }};\n");
    fmt::print("    vector<Registro> executar(const string& fonte, const Passos&);\n\n");

    fmt::print("  Ganha:\n");
    fmt::print("    ✓ nenhuma hierarquia, nenhuma classe por variação\n");
    fmt::print("    ✓ os passos podem vir de LUGARES diferentes\n");
    fmt::print("    ✓ trocar um passo em runtime\n");
    fmt::print("    ✓ o default do hook é um membro com inicializador\n\n");
    fmt::print("  Perde:\n");
    fmt::print("    ✗ nada obriga a preencher `extrair` — um `std::function`\n");
    fmt::print("      vazio compila e lança `bad_function_call` em runtime.\n");
    fmt::print("      O `virtual ... = 0` garantia isso em COMPILAÇÃO.\n");
    fmt::print("    ✗ estado compartilhado entre os passos (os `lidos_`,\n");
    fmt::print("      `rejeitados_` da base) precisa virar captura\n");
    fmt::print("    ✗ o custo do type erasure por passo\n\n");

    fmt::print("  | situação                          | use              |\n");
    fmt::print("  |-----------------------------------|------------------|\n");
    fmt::print("  | passos obrigatórios verificados   | Template Method  |\n");
    fmt::print("  | os passos compartilham estado     | Template Method  |\n");
    fmt::print("  | montar a variação em runtime      | struct de function|\n");
    fmt::print("  | variação conhecida na compilação  | policy/template  |\n");
    fmt::print("  | trocar o algoritmo INTEIRO        | Strategy         |\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║            Resumo do Template Method                     ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : esqueleto na base, passos no derivado       ║\n");
    fmt::print("║ Inversão   : a BASE chama o derivado, não o contrário    ║\n");
    fmt::print("║ Chave      : `executar()` NÃO-virtual protege a ORDEM    ║\n");
    fmt::print("║ Três passos: obrigatório (=0), hook (default), fixo      ║\n");
    fmt::print("║ Convenção  : hooks devem CHAMAR a base ao estender       ║\n");
    fmt::print("║ Custo      : herança — acoplamento e sem troca em runtime║\n");
    fmt::print("║ vs Strategy: a ordem é o que se protege, não o algoritmo ║\n");
    fmt::print("║ Alternativa: struct de std::function, se quiser runtime  ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ template_method — todos os demos concluídos\n\n");

    return 0;
}
