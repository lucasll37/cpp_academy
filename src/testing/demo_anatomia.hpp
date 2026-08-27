// =============================================================================
//  src/testing/demo_anatomia.hpp
//
//  A anatomia de um teste e o catálogo de asserções
//  ────────────────────────────────────────────────
//  Tópicos:
//  • Arrange / Act / Assert
//  • O nome do teste como documentação
//  • EXPECT vs ASSERT — a regra
//  • O catálogo de macros
//  • ⚠ Ponto flutuante
// =============================================================================
#pragma once
#include <fmt/core.h>

namespace demo_anatomia {

void demo_aaa() {
    fmt::print("\n── 2.1 Arrange / Act / Assert ──\n");

    fmt::print("        TEST(Carrinho, MesmoSkuDuasVezesSomaQuantidade) {{\n");
    fmt::print("            PrecosFixos    precos;              // ARRANGE\n");
    fmt::print("            loja::Carrinho c{{precos}};           //   preparar\n");
    fmt::print("\n");
    fmt::print("            c.adicionar(\"caneta\", 2);           // ACT\n");
    fmt::print("            c.adicionar(\"caneta\", 5);           //   a ação testada\n");
    fmt::print("\n");
    fmt::print("            ASSERT_EQ(c.tamanho(), 1u);         // ASSERT\n");
    fmt::print("            EXPECT_EQ(c.itens()[0].quantidade, 7);\n");
    fmt::print("        }}\n\n");

    fmt::print("      Três blocos, nessa ordem, com uma linha em branco entre\n");
    fmt::print("      eles. Se o ACT tem mais de duas ou três linhas, ou se há\n");
    fmt::print("      DOIS blocos de act-assert, o teste está testando duas\n");
    fmt::print("      coisas — separe.\n\n");

    fmt::print("      Um teste é o único código em que a repetição é preferível\n");
    fmt::print("      à abstração. `EXPECT_DOUBLE_EQ(total, 77.9)` com o número\n");
    fmt::print("      literal é MELHOR que uma função `calcula_esperado()` — que\n");
    fmt::print("      repetiria a implementação e passaria a esconder o bug.\n");
}

void demo_nomes() {
    fmt::print("\n── 2.2 O nome como documentação ──\n");

    fmt::print("      Um relatório de falha é lido por alguém que não escreveu o\n");
    fmt::print("      teste, às vezes anos depois. O nome precisa dizer o que era\n");
    fmt::print("      esperado, sem abrir o arquivo:\n\n");

    fmt::print("        ruim  : TEST(CarrinhoTest, Test1)\n");
    fmt::print("        ruim  : TEST(CarrinhoTest, Adicionar)\n");
    fmt::print("        ruim  : TEST(CarrinhoTest, FuncionaCorretamente)\n");
    fmt::print("        bom   : TEST(Carrinho, MesmoSkuDuasVezesSomaQuantidade)\n");
    fmt::print("        bom   : TEST(Carrinho, ProdutoDesconhecidoLanca)\n");
    fmt::print("        bom   : TEST(Desconto, ExatamenteDezPecasDaCincoPorCento)\n\n");

    fmt::print("      A fórmula que funciona: <situação><ação><resultado esperado>.\n\n");

    fmt::print("      ⚠  Restrições técnicas: os nomes viram IDENTIFICADORES C++.\n");
    fmt::print("         Nada de espaço, acento, hífen — e nada de sublinhado, que\n");
    fmt::print("         o próprio GoogleTest desaconselha (ele usa `_` para colar\n");
    fmt::print("         suite e caso, e nomes com `_` podem colidir).\n");
}

void demo_expect_assert() {
    fmt::print("\n── 2.3 EXPECT vs ASSERT ──\n");

    fmt::print("      EXPECT_*  registra a falha e CONTINUA o teste\n");
    fmt::print("      ASSERT_*  registra a falha e faz `return`\n\n");

    fmt::print("      A regra: use EXPECT por padrão — mais informação por\n");
    fmt::print("      execução. Use ASSERT quando o resto do teste não faz\n");
    fmt::print("      sentido, ou CRASHARIA:\n\n");
    fmt::print("        ASSERT_EQ(c.tamanho(), 1u);       // sem isto, a linha\n");
    fmt::print("        EXPECT_EQ(c.itens()[0].sku, \"x\"); // seguinte é UB\n\n");

    fmt::print("      ⚠  ASSERT_* faz `return` — logo:\n");
    fmt::print("         • não funciona em função que devolva valor;\n");
    fmt::print("         • dentro de uma função AUXILIAR, ele retorna da\n");
    fmt::print("           auxiliar, e o teste CONTINUA. Para propagar:\n");
    fmt::print("             ASSERT_NO_FATAL_FAILURE(minha_verificacao());\n");
    fmt::print("         • não use dentro de construtor nem de destrutor — para\n");
    fmt::print("           isso existem SetUp() e TearDown().\n");
}

void demo_catalogo() {
    fmt::print("\n── 2.4 O catálogo ──\n");

    fmt::print("      Booleanas:\n");
    fmt::print("        EXPECT_TRUE(x)          EXPECT_FALSE(x)\n\n");
    fmt::print("      Comparação (imprimem OS DOIS valores na falha):\n");
    fmt::print("        EXPECT_EQ  NE  LT  LE  GT  GE\n\n");
    fmt::print("      Strings C (char*), comparando CONTEÚDO e não ponteiro:\n");
    fmt::print("        EXPECT_STREQ  STRNE  STRCASEEQ  STRCASENE\n");
    fmt::print("        ⚠  para std::string, use EXPECT_EQ normal.\n\n");
    fmt::print("      Ponto flutuante:\n");
    fmt::print("        EXPECT_FLOAT_EQ   EXPECT_DOUBLE_EQ   (4 ULPs)\n");
    fmt::print("        EXPECT_NEAR(a, b, tolerancia)\n\n");
    fmt::print("      Exceções:\n");
    fmt::print("        EXPECT_THROW(expr, Tipo)   EXPECT_NO_THROW  EXPECT_ANY_THROW\n\n");
    fmt::print("      Morte (test_death.cpp):\n");
    fmt::print("        EXPECT_DEATH(expr, regex)  EXPECT_DEBUG_DEATH\n\n");
    fmt::print("      Controle:\n");
    fmt::print("        FAIL()  ADD_FAILURE()  SUCCEED()  GTEST_SKIP()\n\n");
    fmt::print("      Matchers do gmock — usáveis em testes comuns via EXPECT_THAT:\n");
    fmt::print("        EXPECT_THAT(v, ElementsAre(1, 2, 3));\n");
    fmt::print("        EXPECT_THAT(v, UnorderedElementsAre(3, 1, 2));\n");
    fmt::print("        EXPECT_THAT(v, Contains(7));\n");
    fmt::print("        EXPECT_THAT(v, SizeIs(Ge(3)));\n");
    fmt::print("        EXPECT_THAT(s, HasSubstr(\"erro\"));\n");
    fmt::print("        EXPECT_THAT(s, MatchesRegex(\"[0-9]+\"));\n");
    fmt::print("        EXPECT_THAT(x, AllOf(Gt(0), Lt(100)));\n");
    fmt::print("        EXPECT_THAT(p, Pointee(Eq(42)));\n");
    fmt::print("        EXPECT_THAT(o, Optional(Eq(42)));\n\n");
    fmt::print("      Os matchers valem muito pela MENSAGEM DE FALHA: um\n");
    fmt::print("      `ElementsAre` que falha imprime os dois vetores inteiros e\n");
    fmt::print("      marca o índice divergente. Um `EXPECT_TRUE(v == esperado)`\n");
    fmt::print("      imprimiria apenas \"false\".\n");
}

void demo_float() {
    fmt::print("\n── 2.5 ⚠ Ponto flutuante ──\n");

    fmt::print("      0.1 + 0.2 == 0.3 é FALSO em binário:\n");
    fmt::print("        0.1 + 0.2 = {:.20f}\n", 0.1 + 0.2);
    fmt::print("        0.3       = {:.20f}\n", 0.3);
    fmt::print("        diferença = {:.3e}\n", (0.1 + 0.2) - 0.3);

    fmt::print("\n      EXPECT_EQ sobre double é quase sempre um bug de teste.\n");
    fmt::print("      As três alternativas:\n");
    fmt::print("        EXPECT_DOUBLE_EQ(a, b)      tolerância de 4 ULPs —\n");
    fmt::print("                                    escala com a magnitude\n");
    fmt::print("        EXPECT_NEAR(a, b, 1e-9)     tolerância ABSOLUTA — cuidado\n");
    fmt::print("                                    com números muito grandes\n");
    fmt::print("        EXPECT_THAT(a, DoubleNear(b, eps))\n\n");

    fmt::print("      ⚠  Nenhuma delas funciona bem em torno de ZERO: 4 ULPs de\n");
    fmt::print("         zero é um número absurdamente pequeno. Perto de zero,\n");
    fmt::print("         use EXPECT_NEAR com uma tolerância que faça sentido para\n");
    fmt::print("         o seu domínio.\n\n");
    fmt::print("      ⚠  E NaN não é igual a nada, nem a si mesmo. Verifique com\n");
    fmt::print("         EXPECT_TRUE(std::isnan(x)).\n");
}

inline void run() {
    demo_aaa();
    demo_nomes();
    demo_expect_assert();
    demo_catalogo();
    demo_float();
}

}  // namespace demo_anatomia
