// =============================================================================
//  src/testing/test_parametrizado.cpp
//
//  TEST_P e testes tipados — a mesma lógica, muitos dados
//  ──────────────────────────────────────────────────────
//  A regra de desconto tem quatro faixas e três fronteiras. Escrever oito
//  TEST_F quase idênticos é convite a erro por copiar-e-colar: o dia em que
//  alguém acrescenta a faixa de 200 peças, esquece um dos oito.
//
//  `TEST_P` separa A LÓGICA do teste dos DADOS do teste. Um corpo, N casos.
//
//  ⚠ E, mais importante que a economia de linhas: quando um caso falha, o
//    nome dele diz QUAL entrada quebrou. Um laço `for` dentro de um TEST_F
//    para na primeira falha e não diz em qual iteração.
//
//  Rode com:   meson test -C build testing_parametrizado -v
// =============================================================================
#include "carrinho.hpp"
#include <gtest/gtest.h>
#include <deque>
#include <string>
#include <tuple>
#include <vector>

namespace {

class PrecosFixos : public loja::RepositorioDePrecos {
public:
    std::optional<double> preco_de(const std::string& sku) const override {
        if (sku == "caneta") return 1.00;      // preço 1.00 simplifica a conta
        return std::nullopt;
    }
};

// ── O caso: entrada e resultado esperado, num struct com NOMES ──────────────
//   Um `std::tuple<int,double,const char*>` também funcionaria, mas
//   `caso.pecas` se lê melhor que `std::get<0>(caso)` — e a mensagem de
//   falha fica compreensível.
struct CasoDesconto {
    int         pecas;
    double      desconto_esperado;
    const char* rotulo;      // vira parte do NOME do teste
};

class DescontoTest : public ::testing::TestWithParam<CasoDesconto> {
protected:
    PrecosFixos precos;
};

TEST_P(DescontoTest, AplicaAFaixaCorreta) {
    const auto& caso = GetParam();

    loja::Carrinho c{precos};
    c.adicionar("caneta", caso.pecas);

    EXPECT_DOUBLE_EQ(c.percentual_de_desconto(), caso.desconto_esperado)
        << "com " << caso.pecas << " peças";
    EXPECT_DOUBLE_EQ(c.total(), caso.pecas * (1.0 - caso.desconto_esperado));
}

// As FRONTEIRAS são o que importa: 9/10, 49/50, 99/100. Um teste que só
// verifica 5, 30 e 200 passaria com um `>` no lugar de um `>=`.
INSTANTIATE_TEST_SUITE_P(
    Faixas,                              // prefixo do nome gerado
    DescontoTest,
    ::testing::Values(
        CasoDesconto{  1, 0.00, "uma_peca"},
        CasoDesconto{  9, 0.00, "logo_abaixo_de_10"},
        CasoDesconto{ 10, 0.05, "exatamente_10"},
        CasoDesconto{ 49, 0.05, "logo_abaixo_de_50"},
        CasoDesconto{ 50, 0.10, "exatamente_50"},
        CasoDesconto{ 99, 0.10, "logo_abaixo_de_100"},
        CasoDesconto{100, 0.15, "exatamente_100"},
        CasoDesconto{999, 0.15, "muito_acima"}
    ),
    // Sem este lambda, os testes se chamariam Faixas/DescontoTest.
    // AplicaAFaixaCorreta/0..7 — e "/3 falhou" não diz nada.
    [](const ::testing::TestParamInfo<CasoDesconto>& info) {
        return std::string{info.param.rotulo};
    });

// ── Geradores prontos ───────────────────────────────────────────────────────
//   Values(...)          lista explícita
//   Range(0, 10)         0,1,...,9      (e Range(0,10,2) → 0,2,4,6,8)
//   Bool()               false, true
//   ValuesIn(container)  a partir de um vetor/array em runtime
//   Combine(a, b)        PRODUTO CARTESIANO — cuidado, explode rápido
class QuantidadeTest : public ::testing::TestWithParam<int> {};

TEST_P(QuantidadeTest, SubtotalEhLinear) {
    PrecosFixos    precos;
    loja::Carrinho c{precos};
    const int      n = GetParam();

    c.adicionar("caneta", n);
    EXPECT_DOUBLE_EQ(c.subtotal(), n * 1.00);
}
INSTANTIATE_TEST_SUITE_P(DeUmAOito, QuantidadeTest, ::testing::Range(1, 9));

// ── Testes TIPADOS: a mesma lógica para vários TIPOS ────────────────────────
//   TEST_P varia os DADOS. TYPED_TEST varia o TIPO — é como se testa um
//   template, ou várias implementações de uma mesma interface.
template <typename T>
class ContainerTest : public ::testing::Test {
protected:
    T container;
};

using TiposTestados = ::testing::Types<std::vector<int>, std::deque<int>>;
TYPED_TEST_SUITE(ContainerTest, TiposTestados);

TYPED_TEST(ContainerTest, ComecaVazioEDepoisNao) {
    // ⚠ Dentro de TYPED_TEST, os membros da fixture precisam de `this->`:
    //   a fixture é um template, e o nome é dependente. Sem o `this->`, o
    //   compilador nem procura em `ContainerTest<T>`.
    EXPECT_TRUE(this->container.empty());
    this->container.push_back(42);
    EXPECT_EQ(this->container.size(), 1u);
    EXPECT_EQ(this->container.front(), 42);
}

}  // namespace
