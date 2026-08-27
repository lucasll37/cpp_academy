// =============================================================================
//  src/testing/demo_organizacao.hpp
//
//  Fixtures, parametrização e como o binário de teste é organizado
//  ───────────────────────────────────────────────────────────────
//  Tópicos:
//  • O ciclo de vida completo de uma fixture
//  • TEST vs TEST_F vs TEST_P vs TYPED_TEST
//  • As flags de linha de comando que salvam o dia
//  • main() próprio vs gtest_main
// =============================================================================
#pragma once
#include <fmt/core.h>

namespace demo_organizacao {

void demo_ciclo() {
    fmt::print("\n── 3.1 O ciclo de vida ──\n");

    fmt::print("      Para uma suíte com 3 casos, na ordem exata:\n\n");
    fmt::print("        SetUpTestSuite()          ← UMA vez\n");
    fmt::print("          ┌ construtor da fixture ┐\n");
    fmt::print("          │ SetUp()               │\n");
    fmt::print("          │   corpo do caso 1     │  ← instância NOVA\n");
    fmt::print("          │ TearDown()            │\n");
    fmt::print("          └ destrutor             ┘\n");
    fmt::print("          ┌ construtor ... caso 2 ┘  ← outra instância\n");
    fmt::print("          ┌ construtor ... caso 3 ┘  ← outra ainda\n");
    fmt::print("        TearDownTestSuite()       ← UMA vez\n\n");

    fmt::print("      A fixture é reconstruída para CADA caso. É o que garante\n");
    fmt::print("      o \"I\" de isolado, do F.I.R.S.T. — test_fixture.cpp tem um\n");
    fmt::print("      caso que PROVA isso, e falharia se o estado vazasse.\n\n");

    fmt::print("      Construtor ou SetUp()?\n");
    fmt::print("        construtor  → padrão. Permite membros `const` e sem\n");
    fmt::print("                      construtor padrão.\n");
    fmt::print("        SetUp()     → necessário quando a preparação pode falhar\n");
    fmt::print("                      com ASSERT_* (não se pode \"retornar\" de um\n");
    fmt::print("                      construtor) ou quando a fixture é herdada e\n");
    fmt::print("                      a derivada precisa estender a preparação.\n\n");
    fmt::print("      Destrutor ou TearDown()?\n");
    fmt::print("        destrutor   → padrão, e à prova de exceções.\n");
    fmt::print("        TearDown()  → quando a limpeza usa ASSERT_* ou pode lançar\n");
    fmt::print("                      (lançar de um destrutor chama terminate —\n");
    fmt::print("                       ver src/errors/demo_noexcept.hpp).\n\n");

    fmt::print("      ⚠  `SetUpTestSuite` compartilha estado ENTRE casos. É a\n");
    fmt::print("         porta de entrada para o teste que só falha numa ordem\n");
    fmt::print("         específica. Use apenas para recursos caros e IMUTÁVEIS.\n");
}

void demo_variantes() {
    fmt::print("\n── 3.2 As quatro variantes ──\n");

    fmt::print("      | macro       | varia   | quando                        |\n");
    fmt::print("      |-------------|---------|-------------------------------|\n");
    fmt::print("      | TEST        | nada    | caso isolado, sem preparação  |\n");
    fmt::print("      | TEST_F      | fixture | preparação repetida           |\n");
    fmt::print("      | TEST_P      | DADOS   | mesma lógica, muitas entradas |\n");
    fmt::print("      | TYPED_TEST  | TIPOS   | template, ou N implementações |\n\n");

    fmt::print("      TEST_P é o que mais rende. Compare:\n\n");
    fmt::print("        for (auto caso : casos) {{        // ❌ dentro de um TEST_F\n");
    fmt::print("            EXPECT_EQ(f(caso.e), caso.s);\n");
    fmt::print("        }}\n");
    fmt::print("        → 1 teste. Falha diz \"algo em algum caso\". Um ASSERT\n");
    fmt::print("          interromperia os casos restantes.\n\n");
    fmt::print("        INSTANTIATE_TEST_SUITE_P(Faixas, T, Values(...), nome);\n");
    fmt::print("        → N testes independentes. A falha diz\n");
    fmt::print("          `Faixas/DescontoTest.AplicaAFaixaCorreta/exatamente_50`.\n\n");

    fmt::print("      ⚠  O último argumento (o gerador de nomes) é opcional e\n");
    fmt::print("         quase obrigatório na prática. Sem ele os casos se\n");
    fmt::print("         chamam .../0, .../1, ... e \"/3 falhou\" não diz nada.\n\n");

    fmt::print("      ⚠  Em TYPED_TEST, os membros da fixture precisam de\n");
    fmt::print("         `this->`: a fixture é template e o nome é dependente.\n");
    fmt::print("         Sem o `this->`, o compilador nem procura na classe base.\n");
    fmt::print("         (é a mesma regra de src/templates/, sobre nomes\n");
    fmt::print("          dependentes e `typename`)\n");
}

void demo_flags() {
    fmt::print("\n── 3.3 As flags que salvam o dia ──\n");

    fmt::print("        --gtest_filter=Carrinho.*            rodar um subconjunto\n");
    fmt::print("        --gtest_filter=*Desconto*:-*Lento*   incluir e excluir (`-`)\n");
    fmt::print("        --gtest_list_tests                   listar sem rodar\n");
    fmt::print("        --gtest_repeat=100                   caçar flakiness\n");
    fmt::print("        --gtest_shuffle                      ordem aleatória\n");
    fmt::print("        --gtest_random_seed=N                reproduzir um shuffle\n");
    fmt::print("        --gtest_break_on_failure             parar no debugger\n");
    fmt::print("        --gtest_output=xml:res.xml           para o CI\n");
    fmt::print("        --gtest_also_run_disabled_tests      inclui os DISABLED_\n\n");

    fmt::print("      A combinação que caça teste instável:\n");
    fmt::print("        ./teste --gtest_repeat=200 --gtest_shuffle \\\n");
    fmt::print("                --gtest_break_on_failure\n");
    fmt::print("      Repetição com ordem embaralhada encontra tanto o teste\n");
    fmt::print("      aleatoriamente instável quanto o que depende de ordem.\n\n");

    fmt::print("      Pelo Meson:\n");
    fmt::print("        meson test -C build                 tudo\n");
    fmt::print("        meson test -C build testing_mock -v ver a saída\n");
    fmt::print("        meson test -C build --repeat 50\n");
    fmt::print("        meson test -C build --gdb            sob o depurador\n");
    fmt::print("        meson test -C build --wrapper=valgrind\n\n");

    fmt::print("      Um teste temporariamente quebrado se marca renomeando:\n");
    fmt::print("        TEST(Carrinho, DISABLED_AindaNaoImplementado) {{ ... }}\n");
    fmt::print("      O GoogleTest o pula e AVISA quantos estão desabilitados —\n");
    fmt::print("      o que é melhor que comentar o teste e esquecê-lo.\n");
}

void demo_main() {
    fmt::print("\n── 3.4 gtest_main ou main() próprio ──\n");

    fmt::print("      Linkar com `-lgtest_main` dá este main de graça:\n\n");
    fmt::print("        int main(int argc, char** argv) {{\n");
    fmt::print("            ::testing::InitGoogleTest(&argc, argv);\n");
    fmt::print("            return RUN_ALL_TESTS();\n");
    fmt::print("        }}\n\n");
    fmt::print("      (para gmock, `-lgmock_main`, que chama InitGoogleMock)\n\n");

    fmt::print("      Escreva o seu quando precisar de inicialização GLOBAL:\n");
    fmt::print("      abrir um log, subir um servidor, registrar um\n");
    fmt::print("      `::testing::Environment`. Para tudo mais, use o pronto.\n\n");

    fmt::print("      ⚠  `RUN_ALL_TESTS()` precisa ser CHAMADO uma vez e o seu\n");
    fmt::print("         retorno RETORNADO. Ignorá-lo faz o binário sair com 0\n");
    fmt::print("         mesmo com testes falhando — e o CI fica verde com a\n");
    fmt::print("         suíte quebrada. O GoogleTest marca a função como\n");
    fmt::print("         `[[nodiscard]]` justamente por isso.\n");
}

inline void run() {
    demo_ciclo();
    demo_variantes();
    demo_flags();
    demo_main();
}

}  // namespace demo_organizacao
