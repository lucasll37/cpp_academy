// =============================================================================
//  src/testing/demo_porque.hpp
//
//  Por que testar, e o que É um teste bom
//  ──────────────────────────────────────
//  A pergunta útil não é "devo testar?". É "que teste vale a pena escrever?".
//  Uma suíte grande de testes ruins é PIOR que nenhuma: ela custa manutenção,
//  falha por motivos errados, e ensina a equipe a ignorar a luz vermelha.
//
//  Tópicos:
//  • A pirâmide, e por que ela é uma pirâmide
//  • F.I.R.S.T. — as cinco propriedades
//  • O que testar primeiro
//  • ⚠ Os testes que dão prejuízo
// =============================================================================
#pragma once
#include <fmt/core.h>

namespace demo_porque {

void demo_piramide() {
    fmt::print("\n── 1.1 A pirâmide ──\n");

    fmt::print("                    ▲\n");
    fmt::print("                   ╱ ╲     E2E / sistema     poucos, lentos,\n");
    fmt::print("                  ╱   ╲    (o app inteiro)    frágeis, caros\n");
    fmt::print("                 ╱─────╲\n");
    fmt::print("                ╱       ╲  integração        alguns\n");
    fmt::print("               ╱         ╲ (2+ componentes)  (banco, rede, IO)\n");
    fmt::print("              ╱───────────╲\n");
    fmt::print("             ╱             ╲ UNITÁRIOS       MUITOS, rápidos,\n");
    fmt::print("            ╱_______________╲ (uma unidade)  estáveis, baratos\n\n");

    fmt::print("      A forma é consequência do CUSTO POR TESTE, em três eixos:\n");
    fmt::print("        tempo de execução  — um unitário roda em µs; um E2E, em\n");
    fmt::print("                             segundos ou minutos;\n");
    fmt::print("        estabilidade       — quanto mais peças, mais motivos de\n");
    fmt::print("                             falhar sem que haja bug (flakiness);\n");
    fmt::print("        diagnóstico        — um unitário que falha aponta a linha;\n");
    fmt::print("                             um E2E que falha aponta \"algo, em\n");
    fmt::print("                             algum lugar\".\n\n");

    fmt::print("      ⚠  A inversão (\"casquinha de sorvete\") acontece quando a\n");
    fmt::print("         equipe testa só pela interface. O sintoma é a suíte que\n");
    fmt::print("         demora 40 minutos e falha 1 vez em 5 sem motivo.\n\n");

    fmt::print("      Este subprojeto trata do nível de baixo. Os arquivos\n");
    fmt::print("      test_*.cpp são testes UNITÁRIOS de verdade, rodados por\n");
    fmt::print("      `meson test -C build`.\n");
}

void demo_first() {
    fmt::print("\n── 1.2 F.I.R.S.T. ──\n");

    fmt::print("      F ast        rodar a suíte inteira precisa ser trivial. Se\n");
    fmt::print("                   demora, ninguém roda antes de commitar, e o\n");
    fmt::print("                   feedback chega tarde demais.\n\n");
    fmt::print("      I solated    cada teste independe dos outros e da ORDEM. É\n");
    fmt::print("                   por isso que o GoogleTest constrói uma fixture\n");
    fmt::print("                   NOVA para cada caso (test_fixture.cpp).\n\n");
    fmt::print("      R epeatable  mesmo resultado sempre, em qualquer máquina.\n");
    fmt::print("                   Inimigos: relógio, rede, aleatoriedade sem\n");
    fmt::print("                   semente, ordem de iteração de unordered_map,\n");
    fmt::print("                   caminhos absolutos, fuso horário.\n\n");
    fmt::print("      S elf-validating  passa ou falha, sem ninguém ler a saída.\n");
    fmt::print("                   Um teste que imprime e pede conferência humana\n");
    fmt::print("                   não é um teste.\n\n");
    fmt::print("      T imely      escrito junto com o código, não seis meses\n");
    fmt::print("                   depois. Código difícil de testar quase sempre\n");
    fmt::print("                   é código mal desenhado — e o teste escrito\n");
    fmt::print("                   cedo devolve esse recado a tempo.\n");
}

void demo_o_que_testar() {
    fmt::print("\n── 1.3 O que testar primeiro ──\n");

    fmt::print("      Por ordem de retorno:\n\n");
    fmt::print("        1. FRONTEIRAS. A regra de desconto tem faixas em 10, 50\n");
    fmt::print("           e 100. Os casos que importam são 9/10, 49/50, 99/100 —\n");
    fmt::print("           é ali que mora o `>` que deveria ser `>=`.\n");
    fmt::print("           test_parametrizado.cpp testa exatamente esses seis.\n\n");
    fmt::print("        2. O CAMINHO DE ERRO. Produto inexistente, quantidade\n");
    fmt::print("           inválida, carrinho vazio. É o código menos exercitado\n");
    fmt::print("           manualmente, e por isso o mais quebrado.\n\n");
    fmt::print("        3. O BUG QUE VOCÊ ACABOU DE CONSERTAR. Um teste por bug\n");
    fmt::print("           corrigido é a regra de melhor custo-benefício que\n");
    fmt::print("           existe: ele nunca volta.\n\n");
    fmt::print("        4. A REGRA DE NEGÓCIO. Aquilo que, se estiver errado,\n");
    fmt::print("           cobra do cliente o valor errado.\n\n");
    fmt::print("        5. O caminho feliz. Por último — é o que a primeira\n");
    fmt::print("           execução manual já cobre.\n\n");

    fmt::print("      E o que NÃO testar:\n");
    fmt::print("        • getters e setters triviais;\n");
    fmt::print("        • a biblioteca padrão (`std::vector` já é testada);\n");
    fmt::print("        • detalhes privados de implementação — teste pela\n");
    fmt::print("          interface pública, ou a refatoração vira suplício;\n");
    fmt::print("        • código gerado.\n");
}

void demo_testes_ruins() {
    fmt::print("\n── 1.4 ⚠ Os testes que dão prejuízo ──\n");

    fmt::print("      (a) O TESTE FRÁGIL — falha em toda refatoração correta.\n");
    fmt::print("          Sintoma: cheio de EXPECT_CALL verificando chamadas\n");
    fmt::print("          internas. Ele testa COMO, não O QUÊ.\n");
    fmt::print("          Ver a discussão em test_mock.cpp, último caso.\n\n");

    fmt::print("      (b) O TESTE INSTÁVEL (flaky) — passa às vezes. É o pior de\n");
    fmt::print("          todos, porque destrói a confiança na suíte inteira:\n");
    fmt::print("          depois de dois falsos alarmes, todo mundo reroda em vez\n");
    fmt::print("          de investigar. Causas: `sleep` em vez de sincronização,\n");
    fmt::print("          dependência de ordem, relógio, porta de rede fixa.\n");
    fmt::print("          Um teste instável deve ser CONSERTADO ou REMOVIDO — nunca\n");
    fmt::print("          deixado piscando.\n\n");

    fmt::print("      (c) O TESTE TAUTOLÓGICO — repete a implementação.\n");
    fmt::print("            EXPECT_EQ(c.total(), c.subtotal() * (1 - c.desconto()));\n");
    fmt::print("          Isso passa mesmo com a fórmula errada. O teste precisa\n");
    fmt::print("          trazer o valor esperado DE FORA: EXPECT_EQ(total, 77.9).\n\n");

    fmt::print("      (d) O TESTE SEM ASSERÇÃO — só verifica que não explodiu.\n");
    fmt::print("          Tem algum valor (pega crash e UB sob sanitizer), mas\n");
    fmt::print("          quase sempre é um teste pela metade.\n\n");

    fmt::print("      (e) O TESTE GIGANTE — 200 linhas, 30 asserções. Quando falha\n");
    fmt::print("          a de número 4, você não descobre se as outras 26 também\n");
    fmt::print("          falhariam. Um comportamento por teste.\n");
}

inline void run() {
    demo_piramide();
    demo_first();
    demo_o_que_testar();
    demo_testes_ruins();
}

}  // namespace demo_porque
