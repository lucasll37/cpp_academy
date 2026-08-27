// =============================================================================
//  src/testing/test_basico.cpp
//
//  TEST, EXPECT vs ASSERT, e a comparação de ponto flutuante
//  ─────────────────────────────────────────────────────────
//  Rode com:   meson test -C build testing_basico -v
// =============================================================================
#include "carrinho.hpp"
#include <gtest/gtest.h>

namespace {

// Um dublê mínimo, escrito à mão. A seção de gmock mostra a alternativa.
class PrecosFixos : public loja::RepositorioDePrecos {
public:
    std::optional<double> preco_de(const std::string& sku) const override {
        if (sku == "caneta")  return 2.50;
        if (sku == "caderno") return 12.00;
        return std::nullopt;
    }
};

// ── A anatomia de um teste ──────────────────────────────────────────────────
//   TEST(SuiteDeTestes, NomeDoCaso) { ... }
//   O par (suite, caso) precisa ser único no binário INTEIRO. Os nomes viram
//   identificadores C++, então nada de espaço, acento ou hífen.
TEST(Carrinho, ComecaVazio) {
    PrecosFixos   precos;
    loja::Carrinho c{precos};

    EXPECT_TRUE(c.vazio());
    EXPECT_EQ(c.tamanho(), 0u);
    EXPECT_DOUBLE_EQ(c.subtotal(), 0.0);
}

TEST(Carrinho, AdicionarUmItem) {
    PrecosFixos   precos;
    loja::Carrinho c{precos};

    c.adicionar("caneta", 3);

    EXPECT_FALSE(c.vazio());
    ASSERT_EQ(c.tamanho(), 1u);          // ← ASSERT: sem isto, a linha seguinte
                                         //   acessaria itens()[0] num vetor vazio
    EXPECT_EQ(c.itens()[0].sku, "caneta");
    EXPECT_EQ(c.itens()[0].quantidade, 3);
}

// ── EXPECT vs ASSERT ────────────────────────────────────────────────────────
//   EXPECT_* falha e CONTINUA  → mais informação por execução
//   ASSERT_* falha e RETORNA   → use quando o resto do teste não faz sentido
//
//   ⚠ ASSERT_* faz `return` — logo NÃO funciona em função que devolva valor,
//     nem propaga de uma função auxiliar para o teste. Para isso existe
//     ASSERT_NO_FATAL_FAILURE(chamada()).
TEST(Carrinho, MesmoSkuDuasVezesSomaQuantidade) {
    PrecosFixos   precos;
    loja::Carrinho c{precos};

    c.adicionar("caneta", 2);
    c.adicionar("caneta", 5);

    ASSERT_EQ(c.tamanho(), 1u) << "deveria continuar sendo UM item";
    EXPECT_EQ(c.itens()[0].quantidade, 7);
}

// ── ⚠ Ponto flutuante NÃO se compara com EXPECT_EQ ─────────────────────────
TEST(Carrinho, SubtotalUsaComparacaoDeFloat) {
    PrecosFixos   precos;
    loja::Carrinho c{precos};

    c.adicionar("caneta", 3);            // 3 × 2.50 = 7.50

    // EXPECT_EQ(c.subtotal(), 7.5) até funcionaria aqui por sorte, mas
    // 0.1 + 0.2 != 0.3 em binário. Use sempre a forma própria:
    EXPECT_DOUBLE_EQ(c.subtotal(), 7.5);          // tolerância de 4 ULPs
    EXPECT_NEAR(c.subtotal(), 7.5, 1e-9);         // tolerância explícita

    // A demonstração do problema:
    EXPECT_NE(0.1 + 0.2, 0.3);                    // ← verdadeiro!
    EXPECT_DOUBLE_EQ(0.1 + 0.2, 0.3);             // ← e este passa
}

// ── Exceções ────────────────────────────────────────────────────────────────
TEST(Carrinho, ProdutoDesconhecidoLanca) {
    PrecosFixos   precos;
    loja::Carrinho c{precos};

    EXPECT_THROW(c.adicionar("dragao", 1), loja::ProdutoDesconhecido);
    EXPECT_NO_THROW(c.adicionar("caneta", 1));
    EXPECT_ANY_THROW(c.adicionar("dragao", 1));

    // Para inspecionar a mensagem, capture:
    try {
        c.adicionar("dragao", 1);
        FAIL() << "deveria ter lançado";     // ← FAIL() é um ASSERT que sempre falha
    } catch (const loja::ProdutoDesconhecido& e) {
        EXPECT_STREQ(e.what(), "produto desconhecido: dragao");
    }

    // O carrinho ficou intacto depois das falhas — garantia forte.
    // Ver src/errors/demo_garantias.hpp.
    EXPECT_EQ(c.tamanho(), 1u);
}

// ── A mensagem de falha é parte do teste ────────────────────────────────────
TEST(Carrinho, MensagensUteis) {
    PrecosFixos   precos;
    loja::Carrinho c{precos};
    c.adicionar("caderno", 4);

    // O `<<` acrescenta contexto à mensagem SE o teste falhar.
    EXPECT_EQ(c.tamanho(), 1u)
        << "itens no carrinho: " << c.itens().size()
        << ", subtotal: " << c.subtotal();

    // EXPECT_EQ(a, b) já imprime os DOIS valores na falha — é por isso que
    // ele é melhor que EXPECT_TRUE(a == b), que só imprimiria "false".
    EXPECT_DOUBLE_EQ(c.subtotal(), 48.0);
}

}  // namespace
