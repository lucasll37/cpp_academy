// =============================================================================
//  src/design-of-pattern/builder/main.cpp
//
//  BUILDER — separar a construção da representação
//  ───────────────────────────────────────────────
//  O problema: um objeto com muitos campos, quase todos opcionais. As saídas
//  sem Builder são todas ruins:
//
//    construtor gigante   → Relatorio r{"V","csv",true,false,true,100,",",...}
//                           ilegível, e trocar dois `bool` compila
//    N sobrecargas        → explosão combinatória (2^n para n opcionais)
//    setters públicos     → o objeto existe em estado INVÁLIDO entre eles
//
//  O Builder resolve os três: cada passo tem nome, a ordem não importa, e o
//  produto só existe depois de validado.
// =============================================================================
#include "construtor.hpp"
#include "relatorio.hpp"

#include <fmt/color.h>
#include <fmt/core.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  A "versão moderna": designated initializers (C++20)
//
//  Para um agregado SEM invariantes, o C++20 dá quase tudo que o Builder dá,
//  sem escrever uma classe a mais.
// ─────────────────────────────────────────────────────────────────────────────
namespace moderno {

struct OpcoesDeRelatorio {
    std::string titulo;                    // obrigatório por convenção, não por tipo
    std::string formato        = "csv";
    bool        cabecalho      = true;
    bool        rodape         = false;
    int         limite         = 0;
    std::string separador      = ",";
    int         casas_decimais = 2;
};

std::string gerar(const OpcoesDeRelatorio& o) {
    return fmt::format("'{}' [{}] cab={} dec={}", o.titulo, o.formato,
                       o.cabecalho ? "sim" : "nao", o.casas_decimais);
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
    fmt::print("│              cpp_academy — Builder (GoF)                 │\n");
    fmt::print("│   \"Separe a construção de um objeto complexo da sua      │\n");
    fmt::print("│    representação.\"                            — GoF      │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. O problema que ele resolve");

    fmt::print("  Sem Builder, as três saídas e seus defeitos:\n\n");
    fmt::print("  (a) construtor gigante:\n");
    fmt::print("        Relatorio r{{\"Vendas\",\"csv\",true,false,true,100,\",\",\"utf-8\",2}};\n");
    fmt::print("      → trocar dois `bool` de lugar COMPILA e muda o comportamento\n\n");
    fmt::print("  (b) N sobrecargas:\n");
    fmt::print("        Relatorio(t, f);\n");
    fmt::print("        Relatorio(t, f, cabecalho);\n");
    fmt::print("        Relatorio(t, f, cabecalho, rodape);   ...\n");
    fmt::print("      → 2^n combinações para n campos opcionais\n\n");
    fmt::print("  (c) setters públicos:\n");
    fmt::print("        Relatorio r; r.set_titulo(...); r.set_formato(...);\n");
    fmt::print("      → entre as chamadas, o objeto está INVÁLIDO e é visível\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. O Builder em ação");

    const auto simples = builder::Relatorio::Construtor{"Vendas Q3", "csv"}.construir();
    fmt::print("  mínimo (só os obrigatórios):\n    {}\n", simples.descrever());

    const auto completo =
        builder::Relatorio::Construtor{"Vendas Anual", "json"}
            .colunas({"data", "produto", "valor"})
            .com_rodape()
            .limite(1000)
            .casas_decimais(4)
            .codificacao("latin1")
            .construir();
    fmt::print("\n  encadeado, na ordem que fizer sentido:\n    {}\n",
               completo.descrever());

    fmt::print("\n  Repare em três coisas:\n");
    fmt::print("    • os OBRIGATÓRIOS estão no construtor do builder — não há\n");
    fmt::print("      como esquecer, porque o builder não existe sem eles\n");
    fmt::print("    • a ORDEM dos opcionais é livre\n");
    fmt::print("    • cada passo tem NOME: `.com_rodape()` não é `true`\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. Validação no ponto certo");

    const auto tentar = [](const char* rotulo, auto&& fn) {
        try {
            const auto r = fn();
            fmt::print("  {:<28} → ok: {}\n", rotulo, r.descrever());
        } catch (const std::invalid_argument& e) {
            fmt::print("  {:<28} → recusado: {}\n", rotulo, e.what());
        }
    };

    tentar("formato válido", [] {
        return builder::Relatorio::Construtor{"X", "html"}.construir();
    });
    tentar("formato inválido", [] {
        return builder::Relatorio::Construtor{"X", "pdf"}.construir();
    });
    tentar("casas_decimais = 99", [] {
        return builder::Relatorio::Construtor{"X", "csv"}.casas_decimais(99).construir();
    });
    tentar("csv sem separador", [] {
        return builder::Relatorio::Construtor{"X", "csv"}.separador("").construir();
    });

    fmt::print("\n  A validação acontece UMA vez, no `construir()`, quando todos\n");
    fmt::print("  os campos já estão definidos. Um setter público não conseguiria\n");
    fmt::print("  validar regras que dependem de DOIS campos (como \"csv exige\n");
    fmt::print("  separador\") — ele não sabe se o outro já foi definido.\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. O `&&` no construir()");

    fmt::print("  TODOS os métodos do builder são `&&`-qualificados:\n");
    fmt::print("    Construtor&& limite(int n) &&  {{ ...; return std::move(*this); }}\n");
    fmt::print("    Relatorio    construir()   &&;\n\n");

    fmt::print("  Este demo TENTOU primeiro a versão \"óbvia\", e ela não compila:\n");
    fmt::print("    Construtor&  limite(int n)  {{ return *this; }}   // devolve LVALUE\n");
    fmt::print("    Relatorio    construir() &&;                    // exige RVALUE\n\n");
    fmt::print("    Construtor{{\"X\",\"csv\"}}.limite(10).construir();\n");
    fmt::print("      error: passing 'Construtor' as 'this' argument discards qualifiers\n\n");
    fmt::print("  O `limite()` devolveu um lvalue, e o `construir()` recusou.\n");
    fmt::print("  Duas saídas:\n");
    fmt::print("    (a) sobrecarregar cada setter em `&` e `&&`  → dobra o código\n");
    fmt::print("    (b) tornar TUDO `&&`                          → o que fizemos\n\n");

    fmt::print("  Com (b), o builder é rvalue do início ao fim:\n");
    fmt::print("    auto b = Construtor{{\"X\",\"csv\"}};\n");
    fmt::print("    b.limite(10);                        // ✗ NÃO COMPILA\n");
    fmt::print("    std::move(b).limite(10).construir(); // ✓ explícito\n");
    fmt::print("    Construtor{{\"X\",\"csv\"}}.construir();   // ✓ temporário\n\n");
    fmt::print("  E isso impede o bug de reusar o builder:\n");
    fmt::print("    auto a = b.construir();\n");
    fmt::print("    b.limite(10);\n");
    fmt::print("    auto c = b.construir();   // `a` e `c` compartilhariam estado?\n");
    fmt::print("  Com tudo `&&`, a pergunta nem se coloca.\n");
    fmt::print("  (ver src/value_semantics/demo_move_only.hpp, 5.2)\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. A versão moderna: designated initializers");

    const auto r = moderno::gerar({
        .titulo         = "Vendas Q3",
        .formato        = "json",
        .cabecalho      = false,
        .casas_decimais = 4,
    });
    fmt::print("  C++20, sem escrever um Builder:\n");
    fmt::print("    gerar({{.titulo = \"Vendas Q3\", .formato = \"json\",\n");
    fmt::print("           .cabecalho = false, .casas_decimais = 4}})\n");
    fmt::print("    → {}\n", r);

    fmt::print("\n  O que os designated initializers DÃO:\n");
    fmt::print("    ✓ cada campo tem nome no ponto de uso\n");
    fmt::print("    ✓ os omitidos usam o default do struct\n");
    fmt::print("    ✓ zero código extra\n");

    fmt::print("\n  O que eles NÃO dão, e por isso o Builder ainda existe:\n");
    fmt::print("    ✗ VALIDAÇÃO — o agregado aceita qualquer combinação\n");
    fmt::print("    ✗ campos obrigatórios — nada obriga a preencher `titulo`\n");
    fmt::print("    ✗ ORDEM LIVRE — em C++ os designadores precisam seguir a\n");
    fmt::print("      ordem de DECLARAÇÃO dos membros (em C, não)\n");
    fmt::print("    ✗ construção em ETAPAS, com lógica entre elas\n");
    fmt::print("    ✗ o produto pode ter invariantes/membros privados\n");

    fmt::print("\n  | situação                          | use                  |\n");
    fmt::print("  |-----------------------------------|----------------------|\n");
    fmt::print("  | agregado simples, sem invariantes | designated init      |\n");
    fmt::print("  | há campos obrigatórios            | Builder              |\n");
    fmt::print("  | validação cruzada entre campos    | Builder              |\n");
    fmt::print("  | construção em etapas com lógica   | Builder              |\n");
    fmt::print("  | o produto tem membros privados    | Builder              |\n");
    fmt::print("  | poucos campos (≤ 3)               | construtor normal    |\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║              Resumo do Builder                           ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : separar construção de representação         ║\n");
    fmt::print("║ Sintoma    : construtor com 5+ params, muitos opcionais  ║\n");
    fmt::print("║ Chave      : obrigatórios no ctor do builder             ║\n");
    fmt::print("║ Chave      : validar UMA vez, no construir()             ║\n");
    fmt::print("║ Idioma C++ : `construir() &&` impede reuso acidental     ║\n");
    fmt::print("║ Alternativa: designated initializers, se não há invariante║\n");
    fmt::print("║ Cuidado    : não use para 3 campos — é peso morto        ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ builder — todos os demos concluídos\n\n");

    return 0;
}
