// =============================================================================
//  src/design-of-pattern/proxy/main.cpp
//
//  PROXY — um substituto que controla o acesso
//  ───────────────────────────────────────────
//  Mesma interface do objeto real, e um controle no meio do caminho.
//
//  É o padrão mais usado do catálogo e o menos reconhecido: todo
//  `std::shared_ptr` é um smart proxy, e ninguém o chama assim.
// =============================================================================
#include "imagem.hpp"
#include "proxies.hpp"

#include <fmt/core.h>

#include <memory>
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
    fmt::print("│                cpp_academy — Proxy (GoF)                 │\n");
    fmt::print("│   \"Forneça um substituto para outro objeto, para         │\n");
    fmt::print("│    controlar o acesso a ele.\"                   — GoF    │\n");
    fmt::print("└─────────────────────────────────────────────────────────┘\n");

    // ── 1 ────────────────────────────────────────────────────────────────────
    secao("1. Proxy virtual — carregamento preguiçoso");

    proxy::ImagemReal::construcoes = 0;

    fmt::print("  criando uma galeria de 4 imagens:\n");
    std::vector<proxy::ImagemPreguicosa> galeria;
    galeria.reserve(4);
    for (const std::string n : {"foto1.png", "foto2.png", "foto3.png", "foto4.png"}) {
        galeria.emplace_back(n);
    }
    fmt::print("    {} objetos REAIS construídos até aqui\n",
               proxy::ImagemReal::construcoes);
    fmt::print("    (nenhum disco foi tocado — os proxies só guardaram o nome)\n");

    fmt::print("\n  o usuário rola a tela e vê só a segunda:\n");
    galeria[1].desenhar();
    fmt::print("    {} objetos reais construídos\n", proxy::ImagemReal::construcoes);

    fmt::print("\n  ele volta na segunda:\n");
    galeria[1].desenhar();
    fmt::print("    {} objetos reais construídos — o proxy REUSOU\n",
               proxy::ImagemReal::construcoes);

    fmt::print("\n  estado de cada proxy:\n");
    for (const auto& img : galeria) {
        fmt::print("    {:<12} carregada: {}\n", img.nome(),
                   img.ja_carregou() ? "sim" : "não");
    }

    fmt::print("\n  ⚠  repare no `mutable` em `real_`:\n");
    fmt::print("       mutable std::unique_ptr<ImagemReal> real_;\n");
    fmt::print("     `desenhar()` é `const` — logicamente ele não muda a imagem.\n");
    fmt::print("     Mas precisa MATERIALIZAR o objeto real. É exatamente o caso\n");
    fmt::print("     de uso de `mutable`: constância LÓGICA, não binária.\n");

    // ── 2 ────────────────────────────────────────────────────────────────────
    secao("2. Proxy de proteção");

    const auto real = std::make_shared<proxy::ImagemReal>("confidencial.png");
    proxy::ImagemProtegida protegida{real, proxy::Papel::Administrador};

    for (const auto& [papel, nome] : {
             std::pair{proxy::Papel::Anonimo,       "anônimo"},
             std::pair{proxy::Papel::Usuario,       "usuário"},
             std::pair{proxy::Papel::Administrador, "administrador"}}) {
        protegida.definir_papel_atual(papel);
        fmt::print("    como {}:\n", nome);
        protegida.desenhar();
    }

    fmt::print("\n  O cliente chama `desenhar()` na mesma interface. Quem decide\n");
    fmt::print("  se a chamada chega ao objeto real é o proxy — e o objeto real\n");
    fmt::print("  não sabe que existe controle de acesso.\n");
    fmt::print("  É separação de responsabilidades: ver src/solid/srp/.\n");

    // ── 3 ────────────────────────────────────────────────────────────────────
    secao("3. Smart proxy — instrumentação");

    proxy::ImagemInstrumentada instr{real};
    instr.desenhar();
    instr.desenhar();
    (void)instr.bytes();

    fmt::print("\n    {} acessos registrados\n", instr.acessos());
    fmt::print("  Um proxy é o lugar natural para métricas, log, cache e\n");
    fmt::print("  rate limiting — tudo que é ORTOGONAL à função do objeto.\n");

    // ── 4 ────────────────────────────────────────────────────────────────────
    secao("4. Os smart proxies que você já usa");

    fmt::print("  A biblioteca padrão é cheia deles:\n\n");
    fmt::print("  | tipo                    | o que ele controla              |\n");
    fmt::print("  |-------------------------|---------------------------------|\n");
    fmt::print("  | std::unique_ptr<T>      | posse exclusiva; libera no fim  |\n");
    fmt::print("  | std::shared_ptr<T>      | contagem de referência          |\n");
    fmt::print("  | std::weak_ptr<T>        | acesso sem posse; detecta morte |\n");
    fmt::print("  | std::vector<bool>::ref  | acesso a UM BIT como se fosse bool|\n");
    fmt::print("  | std::atomic_ref<T>      | acesso atômico a um objeto normal|\n");
    fmt::print("  | iteradores              | acesso a um elemento            |\n\n");

    fmt::print("  Todos têm a assinatura do padrão: `operator->` e `operator*`\n");
    fmt::print("  repassam para o objeto real, e o proxy faz algo no caminho.\n");
    fmt::print("  O mecanismo do `operator->` que torna isso possível — o\n");
    fmt::print("  drill-down — está em src/operators/demo_chamada.hpp, 4.4.\n");

    fmt::print("\n  E o `vector<bool>::reference` é o exemplo mais didático:\n");
    fmt::print("  ele existe porque um BIT não tem endereço, então não há como\n");
    fmt::print("  devolver `bool&`. O proxy finge ser um `bool&`.\n");
    fmt::print("  As consequências disso estão em src/stl/demo_pitfalls.hpp, 5.2.\n");

    // ── 5 ────────────────────────────────────────────────────────────────────
    secao("5. Proxy vs Decorator vs Adapter");

    fmt::print("  Os três embrulham UM objeto. A diferença é a INTENÇÃO:\n\n");
    fmt::print("  | padrão    | interface | acrescenta | empilha | quem decide |\n");
    fmt::print("  |-----------|-----------|------------|---------|-------------|\n");
    fmt::print("  | Adapter   | MUDA      | não        | não     | o cliente   |\n");
    fmt::print("  | Decorator | mantém    | SIM        | SIM     | o cliente   |\n");
    fmt::print("  | Proxy     | mantém    | controla   | não*    | o PROXY     |\n");
    fmt::print("  * dá para empilhar, mas normalmente não se quer\n\n");

    fmt::print("  A pergunta que separa Proxy de Decorator:\n");
    fmt::print("    \"o objeto real SEMPRE é chamado?\"\n");
    fmt::print("      sim → Decorator (ele acrescenta ANTES/DEPOIS)\n");
    fmt::print("      não → Proxy (ele pode NÃO repassar — negar, adiar, cachear)\n\n");
    fmt::print("  No demo da seção 2, o proxy NEGOU a chamada. Um Decorator\n");
    fmt::print("  nunca faria isso — ele decora, não bloqueia.\n");

    // Demonstração de que dá para empilhar, quando faz sentido
    fmt::print("\n  empilhando (proteção + instrumentação):\n");
    auto instrumentado = std::make_shared<proxy::ImagemInstrumentada>(real);
    proxy::ImagemProtegida dupla{instrumentado, proxy::Papel::Usuario};
    dupla.definir_papel_atual(proxy::Papel::Usuario);
    dupla.desenhar();
    dupla.definir_papel_atual(proxy::Papel::Anonimo);
    dupla.desenhar();
    fmt::print("    o instrumentado registrou {} acesso(s) — o negado nem chegou nele\n",
               instrumentado->acessos());

    fmt::print("\n");
    fmt::print("╔══════════════════════════════════════════════════════════╗\n");
    fmt::print("║                 Resumo do Proxy                          ║\n");
    fmt::print("╠══════════════════════════════════════════════════════════╣\n");
    fmt::print("║ Intenção   : controlar o acesso, mantendo a interface    ║\n");
    fmt::print("║ Virtual    : adia a criação cara (use `mutable`)         ║\n");
    fmt::print("║ Proteção   : verifica permissão antes de repassar        ║\n");
    fmt::print("║ Smart      : log, métricas, cache, contagem de ref       ║\n");
    fmt::print("║ Remoto     : o objeto real está noutro processo          ║\n");
    fmt::print("║ Na STL     : unique_ptr, shared_ptr, weak_ptr, iteradores║\n");
    fmt::print("║ vs Decorator: o Proxy pode NÃO repassar a chamada        ║\n");
    fmt::print("║ Mecanismo  : operator-> com drill-down                   ║\n");
    fmt::print("╚══════════════════════════════════════════════════════════╝\n");
    fmt::print("\n✓ proxy — todos os demos concluídos\n\n");

    return 0;
}
