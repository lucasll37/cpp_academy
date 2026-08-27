// =============================================================================
//  src/design-of-pattern/facade/main.cpp
//
//  FACADE — uma porta simples para um subsistema complexo
//  ──────────────────────────────────────────────────────
//  A Facade é o padrão mais simples do catálogo, e o mais fácil de aplicar
//  errado. Ela NÃO é uma camada de abstração, nem um wrapper genérico: é uma
//  interface CONVENIENTE para o caso de uso comum, deixando o subsistema
//  acessível para quem precisa do controle fino.
// =============================================================================
#include "pipeline.hpp"
#include "subsistemas.hpp"

#include <fmt/core.h>

#include <expected>
#include <string>
#include <vector>

static void secao(const char* titulo) {
    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║  {:^56}  ║\n", titulo);
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
}

int main() {
    fmt::print("\n");
    fmt::print("┌─────────────────────────────────────────────────────────┐\n");
    fmt::print("│               cpp_academy — Facade (GoF)                 │\n");
    fmt::print("│   \"Forneça uma interface unificada para um conjunto de   │\n");
    fmt::print("│    interfaces de um subsistema.\"               — GoF     │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    const std::vector<std::string> fontes{"main.cpp", "util.cpp", "rede.cpp"};

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. Sem a Facade");

    fmt::print("  o cliente precisa conhecer SEIS subsistemas e a ORDEM:\n\n");
    {
        facade::ResolvedorDeDependencias deps;
        facade::Cache                    cache;
        facade::Compilador               cc;
        facade::Linker                   ld;
        facade::ExecutorDeTestes         testes;
        facade::Empacotador              pack;

        deps.adicionar_repositorio("https://conan.io/center");
        const auto bibliotecas = deps.resolver({"fmt", "asio"});

        cc.definir_padrao("c++20");
        cc.definir_otimizacao(2);
        const auto objetos = cc.compilar(fontes);

        for (const auto& o : objetos) cache.guardar(o, 1024);

        const auto binario = ld.linkar(objetos, bibliotecas, "servidor");
        const auto r = testes.executar(binario);
        fmt::print("      [test] {} passaram, {} falharam\n", r.passou, r.falhou);

        const auto pacote = pack.empacotar(binario, "tar.gz");
        fmt::print("\n  resultado: {}\n", pacote);
    }

    fmt::print("\n  São ~15 linhas, e o cliente precisa saber:\n");
    fmt::print("    • que `resolver` vem ANTES de `compilar`\n");
    fmt::print("    • que a saída de `compilar` alimenta `linkar`\n");
    fmt::print("    • que `linkar` precisa das bibliotecas do `resolver`\n");
    fmt::print("    • quais são os defaults razoáveis de cada subsistema\n");
    fmt::print("  Esse conhecimento fica DUPLICADO em cada cliente.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. Com a Facade");

    facade::PipelineDeBuild pipeline;

    const auto r = pipeline.construir("servidor", fontes, {
        .padrao         = "c++20",
        .otimizacao     = 2,
        .rodar_testes   = true,
        .formato_pacote = "tar.gz",
        .dependencias   = {"fmt", "asio"},
    });

    if (r) {
        fmt::print("\n  resultado: {}\n", *r);
    } else {
        fmt::print("\n  falhou: {}\n", facade::descrever(r.error()));
    }

    fmt::print("\n  UMA chamada. O conhecimento sobre ordem e fluxo de dados\n");
    fmt::print("  mora num lugar só — dentro da Facade.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. O default que quase sempre serve");

    const auto simples = pipeline.construir("cli", {"cli.cpp"});
    if (simples) fmt::print("\n  build mínimo: {}\n", *simples);

    fmt::print("\n    pipeline.construir(\"cli\", {{\"cli.cpp\"}});\n\n");
    fmt::print("  As `Opcoes` têm defaults sensatos, então o caso comum é uma\n");
    fmt::print("  linha. É esse o ponto da Facade: otimizar o CASO COMUM.\n");
    fmt::print("  (os defaults usam designated initializers — ver builder/, 5)\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. O que a Facade NÃO faz");

    fmt::print("  ⚠  Ela NÃO esconde os subsistemas.\n\n");
    fmt::print("     A seção 1 deste demo usou os seis diretamente, e continua\n");
    fmt::print("     compilando. Isso é DE PROPÓSITO: a Facade é uma\n");
    fmt::print("     CONVENIÊNCIA, não uma prisão.\n\n");
    fmt::print("     Quem precisa de controle fino — compilar sem linkar, usar\n");
    fmt::print("     um cache externo, rodar só um subconjunto de testes —\n");
    fmt::print("     desce um nível e usa os subsistemas.\n\n");

    fmt::print("  ⚠  Ela NÃO acrescenta funcionalidade.\n");
    fmt::print("     Se você se pegar escrevendo regra de negócio dentro da\n");
    fmt::print("     Facade, ela virou outra coisa — provavelmente um Mediator\n");
    fmt::print("     (que COORDENA e tem lógica própria) ou um caso de uso.\n\n");

    fmt::print("  ⚠  Ela NÃO é uma camada de abstração.\n");
    fmt::print("     Uma abstração permite TROCAR a implementação; a Facade\n");
    fmt::print("     simplifica UMA implementação específica. Se você quer\n");
    fmt::print("     trocar o compilador inteiro, precisa de uma interface\n");
    fmt::print("     abstrata — ver src/solid/dip/.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. Facade vs os padrões vizinhos");

    fmt::print("  | padrão    | quantas classes | muda a interface? | acrescenta? |\n");
    fmt::print("  |-----------|-----------------|-------------------|-------------|\n");
    fmt::print("  | Facade    | MUITAS          | simplifica        | não         |\n");
    fmt::print("  | Adapter   | uma             | TRADUZ            | não         |\n");
    fmt::print("  | Decorator | uma             | MANTÉM            | SIM         |\n");
    fmt::print("  | Proxy     | uma             | MANTÉM            | controla    |\n");
    fmt::print("  | Mediator  | muitas          | —                 | SIM, coordena|\n\n");

    fmt::print("  As perguntas que separam:\n");
    fmt::print("    quantas classes estão do outro lado?\n");
    fmt::print("      uma → Adapter/Decorator/Proxy | muitas → Facade/Mediator\n");
    fmt::print("    a interface do outro lado é INCOMPATÍVEL?\n");
    fmt::print("      sim → Adapter | não, só complexa → Facade\n");
    fmt::print("    há lógica NOVA?\n");
    fmt::print("      não → Facade | sim → Mediator\n");

    fmt::print("\n  ⚠  o anti-padrão da Facade tem nome: \"God Object\". Uma\n");
    fmt::print("     Facade que cresce até virar a ÚNICA porta de entrada, com\n");
    fmt::print("     40 métodos e estado próprio, deixou de simplificar e passou\n");
    fmt::print("     a acoplar. O sinal de alarme é ela acumular estado que os\n");
    fmt::print("     subsistemas não têm. Ver src/solid/srp/.\n");

    fmt::print("\n  Onde a Facade aparece na biblioteca padrão:\n");
    fmt::print("    std::filesystem  → esconde as syscalls do SO\n");
    fmt::print("    std::regex       → esconde o autômato\n");
    fmt::print("    std::async       → esconde thread + promise + future\n");
    fmt::print("    std::format      → esconde o parser e os formatters\n");

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║                Resumo da Facade                          ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : interface simples p/ subsistema complexo    ║\n");
    fmt::print("║ Ela codifica: a ORDEM e o fluxo entre os subsistemas     ║\n");
    fmt::print("║ Ela NÃO    : esconde, abstrai, nem acrescenta lógica     ║\n");
    fmt::print("║ Chave      : o subsistema continua ACESSÍVEL             ║\n");
    fmt::print("║ Objetivo   : otimizar o CASO COMUM, não todos os casos   ║\n");
    fmt::print("║ Anti-padrão: virar God Object com estado próprio         ║\n");
    fmt::print("║ Na STL     : filesystem, regex, async, format            ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ facade — todos os demos concluídos\n\n");

    return 0;
}
