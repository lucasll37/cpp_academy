// =============================================================================
//  src/design-of-pattern/composite/main.cpp
//
//  COMPOSITE — tratar objeto e composição do mesmo jeito
//  ─────────────────────────────────────────────────────
//  O cliente pergunta "qual o seu tamanho?" e recebe resposta, sem saber se
//  está falando com um arquivo ou com uma árvore de 500 pastas.
//
//  A recursão fica ENCAPSULADA no composto: `Pasta::tamanho()` soma os filhos,
//  e se um filho for outra pasta, ele soma os dele. Ninguém escreve um laço
//  aninhado.
// =============================================================================
#include "arquivo.hpp"
#include "no.hpp"
#include "pasta.hpp"

#include <fmt/core.h>

#include <memory>
#include <numeric>
#include <string>
#include <variant>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  A versão moderna: variant recursivo
//
//  A mesma árvore, sem hierarquia — e com exaustividade verificada.
//  Ver src/vocabulary_types/demo_variant.hpp, 2.7.
// ─────────────────────────────────────────────────────────────────────────────
namespace moderno {

struct Arquivo { std::string nome; std::size_t bytes; };
struct Pasta;

using No = std::variant<Arquivo, std::unique_ptr<Pasta>>;

struct Pasta {
    std::string     nome;
    std::vector<No> filhos;
};

template <typename... Ts> struct Overload : Ts... { using Ts::operator()...; };
template <typename... Ts> Overload(Ts...) -> Overload<Ts...>;

std::size_t tamanho(const No& n) {
    return std::visit(Overload{
        [](const Arquivo& a) { return a.bytes; },
        [](const std::unique_ptr<Pasta>& p) {
            std::size_t t = 0;
            for (const auto& f : p->filhos) t += tamanho(f);
            return t;
        },
    }, n);
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
    fmt::print("│             cpp_academy — Composite (GoF)                │\n");
    fmt::print("│   \"Componha objetos em árvores e trate objetos           │\n");
    fmt::print("│    individuais e composições uniformemente.\"     — GoF    │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. A árvore");

    auto raiz = std::make_unique<composite::Pasta>("projeto");

    auto src = std::make_unique<composite::Pasta>("src");
    src->adicionar(std::make_unique<composite::Arquivo>("main.cpp", 2048))
        .adicionar(std::make_unique<composite::Arquivo>("util.cpp", 1024));

    auto testes = std::make_unique<composite::Pasta>("testes");
    testes->adicionar(std::make_unique<composite::Arquivo>("test_util.cpp", 512));

    auto docs = std::make_unique<composite::Pasta>("docs");
    auto img  = std::make_unique<composite::Pasta>("imagens");
    img->adicionar(std::make_unique<composite::Arquivo>("diagrama.png", 40960));
    docs->adicionar(std::make_unique<composite::Arquivo>("README.md", 4096))
         .adicionar(std::move(img));

    raiz->adicionar(std::move(src))
         .adicionar(std::move(testes))
         .adicionar(std::move(docs))
         .adicionar(std::make_unique<composite::Arquivo>("LICENSE", 1100));

    raiz->imprimir();

    fmt::print("\n  ⚠  UM BUG ENCONTRADO ESCREVENDO ESTE DEMO:\n");
    fmt::print("     a primeira versão declarava\n");
    fmt::print("       virtual void imprimir(int nivel = 0) const = 0;\n");
    fmt::print("     e `raiz->imprimir()` NÃO COMPILOU — o override em `Pasta`\n");
    fmt::print("     não repetiu o argumento padrão, e o default é escolhido\n");
    fmt::print("     pelo tipo ESTÁTICO.\n");
    fmt::print("     A correção é a que src/oop/demo_virtual.hpp, 2.6 recomenda:\n");
    fmt::print("       void imprimir() const {{ imprimir_em(0); }}   // NÃO-virtual\n");
    fmt::print("       virtual void imprimir_em(int) const = 0;    // virtual\n");
    fmt::print("     É o idioma NVI, e aqui ele não é estilo — é o que compila.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. A uniformidade");

    // Uma função que aceita QUALQUER nó — folha ou composto
    const auto relatar = [](const composite::No& n) {
        fmt::print("    {:<14} {:>7} B  {:>2} arquivo(s)\n",
                   n.nome(), n.tamanho(), n.contar_arquivos());
    };

    const composite::Arquivo solto{"config.ini", 256};

    fmt::print("  a MESMA função, com uma folha e com a árvore inteira:\n");
    relatar(solto);
    relatar(*raiz);

    fmt::print("\n  `relatar` recebe `const No&`. Ela não pergunta se é arquivo\n");
    fmt::print("  ou pasta — e é essa a definição do padrão.\n");
    fmt::print("  A recursão está ENCAPSULADA em `Pasta::tamanho()`:\n");
    fmt::print("    for (const auto& f : filhos_) total += f->tamanho();\n");
    fmt::print("  Se o filho for outra pasta, ele soma os dele. Nenhum laço\n");
    fmt::print("  aninhado no código do cliente.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. Operações que atravessam a árvore");

    fmt::print("  busca recursiva:\n");
    for (const std::string alvo : {"diagrama.png", "util.cpp", "inexistente.txt"}) {
        const auto* achado = raiz->encontrar(alvo);
        if (achado) {
            fmt::print("    '{}' → achado, {} B\n", alvo, achado->tamanho());
        } else {
            fmt::print("    '{}' → não encontrado\n", alvo);
        }
    }

    fmt::print("\n  remoção no nível raiz:\n");
    fmt::print("    antes  : {} B em {} arquivos\n",
               raiz->tamanho(), raiz->contar_arquivos());
    const bool removeu = raiz->remover("docs");
    fmt::print("    remover(\"docs\") → {}\n", removeu);
    fmt::print("    depois : {} B em {} arquivos\n",
               raiz->tamanho(), raiz->contar_arquivos());
    fmt::print("  ↑ removeu a subárvore inteira, com os 2 arquivos dela.\n");
    fmt::print("    O `unique_ptr` destruiu tudo em cascata — nenhum `delete`.\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. A decisão de design que o padrão força");

    fmt::print("  Onde ficam `adicionar()` e `remover()`?\n\n");
    fmt::print("  (a) NA INTERFACE `No`, para todos:\n");
    fmt::print("        class No {{ virtual void adicionar(unique_ptr<No>) = 0; }};\n");
    fmt::print("      → uniformidade MÁXIMA: o cliente compõe sem saber o tipo\n");
    fmt::print("      → mas `Arquivo::adicionar()` teria que:\n");
    fmt::print("          lançar exceção   → viola o LSP (src/solid/lsp/, 5.3)\n");
    fmt::print("          não fazer nada   → pior: falha em SILÊNCIO\n\n");
    fmt::print("  (b) SÓ NO COMPOSTO `Pasta` — o que fizemos:\n");
    fmt::print("        class Pasta : public No {{ Pasta& adicionar(...); }};\n");
    fmt::print("      → a interface fica HONESTA: `No` só promete o que toda\n");
    fmt::print("        folha consegue cumprir\n");
    fmt::print("      → o cliente precisa do tipo `Pasta` para compor\n\n");

    fmt::print("  O próprio GoF apresenta as duas e admite o trade-off. A\n");
    fmt::print("  escolha (b) é a que respeita LSP e ISP:\n");
    fmt::print("    LSP: \"o derivado cumpre TUDO que a base promete?\"\n");
    fmt::print("    ISP: \"a base está prometendo coisas demais?\"\n");
    fmt::print("  Na versão (a), a resposta é não e sim, respectivamente.\n");

    fmt::print("\n  ⚠  na prática, quase todo código de composição acontece na\n");
    fmt::print("     CONSTRUÇÃO da árvore, onde os tipos concretos já são\n");
    fmt::print("     conhecidos. A uniformidade que importa é a das operações\n");
    fmt::print("     de LEITURA, e essa a opção (b) preserva inteira.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. A versão moderna: variant recursivo");

    auto p_img = std::make_unique<moderno::Pasta>();
    p_img->nome = "imagens";
    p_img->filhos.push_back(moderno::Arquivo{"diagrama.png", 40960});

    auto p_docs = std::make_unique<moderno::Pasta>();
    p_docs->nome = "docs";
    p_docs->filhos.push_back(moderno::Arquivo{"README.md", 4096});
    p_docs->filhos.push_back(std::move(p_img));

    const moderno::No arvore = std::move(p_docs);
    fmt::print("  tamanho da árvore (variant) = {} B\n", moderno::tamanho(arvore));

    fmt::print("\n    using No = std::variant<Arquivo, std::unique_ptr<Pasta>>;\n\n");
    fmt::print("    std::size_t tamanho(const No& n) {{\n");
    fmt::print("        return std::visit(Overload{{\n");
    fmt::print("            [](const Arquivo& a) {{ return a.bytes; }},\n");
    fmt::print("            [](const auto& p) {{ /* soma os filhos */ }},\n");
    fmt::print("        }}, n);\n");
    fmt::print("    }}\n\n");

    fmt::print("  | critério                  | Composite | variant       |\n");
    fmt::print("  |---------------------------|-----------|---------------|\n");
    fmt::print("  | tipos de nó NOVOS de fora | SIM       | não           |\n");
    fmt::print("  | operações novas           | recompila | SIM           |\n");
    fmt::print("  | exaustividade verificada  | não       | SIM           |\n");
    fmt::print("  | folha sem alocação        | não       | SIM           |\n");
    fmt::print("  | uniformidade de chamada   | `n.op()`  | `op(n)`       |\n\n");

    fmt::print("  A diferença de FORMA importa: no Composite a operação é um\n");
    fmt::print("  MÉTODO (fechado — acrescentá-la recompila a hierarquia); com\n");
    fmt::print("  variant é uma FUNÇÃO LIVRE (aberta — escreva outra e pronto).\n");
    fmt::print("  É o \"expression problem\" de novo:\n");
    fmt::print("    muitos tipos de nó, poucas operações → Composite\n");
    fmt::print("    poucos tipos de nó, muitas operações → variant\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║              Resumo do Composite                         ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : tratar folha e composição uniformemente     ║\n");
    fmt::print("║ Chave      : o composto DELEGA aos filhos e agrega       ║\n");
    fmt::print("║ Decisão    : adicionar/remover só no COMPOSTO            ║\n");
    fmt::print("║ Motivo     : pôr na interface violaria LSP e ISP         ║\n");
    fmt::print("║ Posse      : unique_ptr destrói a subárvore em cascata   ║\n");
    fmt::print("║ Alternativa: variant recursivo, se a hierarquia é fechada║\n");
    fmt::print("║ Aparece em : sistemas de arquivo, GUI, AST, cenas 3D     ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ composite — todos os demos concluídos\n\n");

    return 0;
}
