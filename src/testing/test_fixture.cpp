// =============================================================================
//  src/testing/test_fixture.cpp
//
//  Fixtures: SetUp, TearDown e o escopo de cada um
//  ───────────────────────────────────────────────
//  Os seis testes de test_basico.cpp repetem `PrecosFixos precos;` e
//  `Carrinho c{precos};`. Uma fixture elimina essa repetição — e, mais
//  importante, GARANTE que cada teste comece do zero.
//
//  ⚠ A regra que mais surpreende: o GoogleTest constrói uma instância NOVA da
//    fixture para CADA caso de teste. Não há como um teste contaminar o outro
//    por acidente — e também não há como compartilhar estado de propósito
//    (para isso existe SetUpTestSuite).
//
//  Rode com:   meson test -C build testing_fixture -v
// =============================================================================
#include "carrinho.hpp"
#include <gtest/gtest.h>
#include <memory>

namespace {

class PrecosFixos : public loja::RepositorioDePrecos {
public:
    std::optional<double> preco_de(const std::string& sku) const override {
        if (sku == "caneta")  return 2.50;
        if (sku == "caderno") return 12.00;
        return std::nullopt;
    }
};

// ── A fixture ───────────────────────────────────────────────────────────────
class CarrinhoTest : public ::testing::Test {
protected:
    // Duas formas de inicializar. Prefira o CONSTRUTOR quando puder:
    //   • SetUp()      → necessário se a inicialização puder falhar com
    //                    ASSERT_* (não se pode usar ASSERT num construtor,
    //                    porque ele não pode "retornar")
    //   • construtor   → mais simples, e permite membros `const` e sem
    //                    construtor padrão
    CarrinhoTest() : carrinho{precos} {}

    void SetUp() override {
        // Roda ANTES de cada teste, depois do construtor.
        ++setups_executados;
    }

    void TearDown() override {
        // Roda DEPOIS de cada teste, antes do destrutor. Prefira o DESTRUTOR,
        // exceto quando a limpeza puder falhar com ASSERT_* ou puder lançar.
        ++teardowns_executados;
    }

    // Escopo de SUÍTE: roda UMA vez, antes/depois de todos os casos.
    // Útil para recursos caros e imutáveis (abrir um arquivo grande, subir um
    // servidor de teste). ⚠ Estado compartilhado entre testes é uma fonte
    // clássica de testes que só falham numa ordem específica.
    static void SetUpTestSuite()    { ++suites_iniciadas; }
    static void TearDownTestSuite() { /* liberação do que a suíte alocou */ }

    static inline int suites_iniciadas     = 0;
    static inline int setups_executados    = 0;
    static inline int teardowns_executados = 0;

    PrecosFixos    precos;
    loja::Carrinho carrinho;      // ← novo a cada caso
};

// `TEST_F` (de Fixture) em vez de `TEST`. O primeiro argumento é o NOME DA
// CLASSE da fixture, e ele também vira o nome da suíte.
TEST_F(CarrinhoTest, ComecaVazio) {
    EXPECT_TRUE(carrinho.vazio());
}

TEST_F(CarrinhoTest, PrimeiroTesteAdiciona) {
    carrinho.adicionar("caneta", 5);
    EXPECT_EQ(carrinho.tamanho(), 1u);
}

// Este teste PROVA o isolamento: se a fixture fosse compartilhada, o carrinho
// já teria a caneta do teste anterior.
TEST_F(CarrinhoTest, SegundoTesteVeCarrinhoLimpo) {
    EXPECT_TRUE(carrinho.vazio())
        << "se este teste falhar, a fixture está sendo compartilhada";
    EXPECT_EQ(carrinho.tamanho(), 0u);
}

TEST_F(CarrinhoTest, SetUpRodouParaCadaCaso) {
    // Este é o 4º caso desta suíte na ordem de declaração; a contagem prova
    // que SetUp() rodou uma vez por caso, e SetUpTestSuite() só uma vez.
    EXPECT_GE(setups_executados, 1);
    EXPECT_EQ(suites_iniciadas, 1) << "SetUpTestSuite deve rodar UMA vez";
    EXPECT_EQ(teardowns_executados, setups_executados - 1)
        << "o TearDown do caso atual ainda não rodou";
}

// ── Desconto por faixa, com a fixture ───────────────────────────────────────
TEST_F(CarrinhoTest, SemDescontoAbaixoDeDezPecas) {
    carrinho.adicionar("caneta", 9);
    EXPECT_DOUBLE_EQ(carrinho.percentual_de_desconto(), 0.0);
    EXPECT_DOUBLE_EQ(carrinho.total(), 22.5);
}

TEST_F(CarrinhoTest, DescontoSomaPecasDeItensDiferentes) {
    carrinho.adicionar("caneta",  6);
    carrinho.adicionar("caderno", 4);      // 10 peças no total → 5%
    EXPECT_DOUBLE_EQ(carrinho.percentual_de_desconto(), 0.05);
}

}  // namespace
