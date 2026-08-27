// =============================================================================
//  src/testing/test_death.cpp
//
//  Death tests — testar que o programa MORRE
//  ─────────────────────────────────────────
//  Nem todo erro deve virar exceção. `Carrinho::adicionar` tem uma
//  PRÉ-CONDIÇÃO: quantidade > 0. Violá-la não é entrada inválida do usuário —
//  é bug do chamador. A resposta certa a um bug é abortar, e não recuperar:
//  continuar rodando com uma invariante quebrada é pior que parar.
//  (Ver src/errors/ sobre erro vs bug.)
//
//  Como testar que uma chamada ABORTA, se abortar mataria o processo de
//  teste? O GoogleTest resolve com `fork()`: a expressão roda num processo
//  FILHO, e o pai verifica o código de saída e a saída em stderr.
//
//  Rode com:   meson test -C build testing_death -v
// =============================================================================
#include "carrinho.hpp"
#include <gtest/gtest.h>
#include <optional>
#include <string>

namespace {

class PrecosFixos : public loja::RepositorioDePrecos {
public:
    std::optional<double> preco_de(const std::string& sku) const override {
        if (sku == "caneta") return 2.50;
        return std::nullopt;
    }
};

// ⚠ A convenção de NOME: uma suíte cujo nome termina em `DeathTest` é
//   executada ANTES de todas as outras. Isso não é estética — é para reduzir
//   a chance de o fork acontecer com threads já criadas, o que é a principal
//   fonte de death tests instáveis.
class CarrinhoDeathTest : public ::testing::Test {
protected:
    PrecosFixos precos;
};

// O `assert` vira no-op quando NDEBUG está definido (buildtype=release).
// Testar o abort seria testar o compilador, não o código — então o teste
// só existe onde a pré-condição existe.
#ifndef NDEBUG

TEST_F(CarrinhoDeathTest, QuantidadeZeroAborta) {
    loja::Carrinho c{precos};

    // EXPECT_DEATH(instrução, regex_da_stderr)
    // O segundo argumento casa contra o que o processo filho escreve em
    // stderr — é o que distingue "morreu pelo motivo certo" de
    // "morreu por qualquer motivo".
    EXPECT_DEATH(c.adicionar("caneta", 0), "quantidade precisa ser positiva");
}

TEST_F(CarrinhoDeathTest, QuantidadeNegativaAborta) {
    loja::Carrinho c{precos};
    EXPECT_DEATH(c.adicionar("caneta", -5), "quantidade precisa ser positiva");
}

// EXPECT_DEBUG_DEATH espera a morte em debug e apenas EXECUTA a instrução em
// release — é o par natural do `assert`, e dispensa o #ifndef manual.
TEST_F(CarrinhoDeathTest, FormaPortavelComDebugDeath) {
    loja::Carrinho c{precos};
    EXPECT_DEBUG_DEATH(c.adicionar("caneta", 0), "quantidade");
}

#else

TEST_F(CarrinhoDeathTest, SemAssercoesEmRelease) {
    GTEST_SKIP() << "compilado com NDEBUG: a pré-condição não está ativa";
}

#endif  // NDEBUG

// ── ⚠ Os quatro perigos dos death tests ─────────────────────────────────────
TEST(DeathTestNotas, ArmadilhasConhecidas) {
    // (1) THREADS. `fork()` num processo com threads clona só a thread que
    //     chamou. Se outra thread segurava um mutex, o filho trava para
    //     sempre. O GoogleTest AVISA ("Death tests use fork(), which is
    //     unsafe particularly in a threaded context"). Por isso a convenção
    //     do sufixo DeathTest, que os roda primeiro.
    //
    // (2) EFEITOS COLATERAIS. O que a instrução faz acontece no FILHO e é
    //     descartado. Isto NUNCA passa:
    //         int x = 0;
    //         EXPECT_DEATH({ x = 1; morre(); }, "");
    //         EXPECT_EQ(x, 1);     // ← x ainda é 0 no pai
    //
    // (3) O REGEX. É POSIX estendido no Linux e um subconjunto simples no
    //     Windows. Um `.` casa qualquer caractere, e parênteses agrupam —
    //     escreva `\\(` para casar um parêntese literal. Um regex vazio ""
    //     casa qualquer coisa, e aí o teste só verifica que morreu.
    //
    // (4) VELOCIDADE. Cada death test é um fork + execução. Num teste de
    //     unidade que roda em microssegundos, isso é caro. Use com parcimônia.
    //
    // O estilo pode ser trocado com:
    //     ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    // que reexecuta o BINÁRIO em vez de dar fork — mais lento e mais seguro.
    SUCCEED() << "esta é uma nota, não uma verificação";
}

// ── O que NÃO é death test ──────────────────────────────────────────────────
TEST(DeathTestNotas, ExcecaoNaoEhMorte) {
    PrecosFixos    precos;
    loja::Carrinho c{precos};

    // Uma exceção é comportamento NORMAL e previsto: o programa continua.
    // Use EXPECT_THROW, não EXPECT_DEATH.
    EXPECT_THROW(c.adicionar("inexistente", 1), loja::ProdutoDesconhecido);
    EXPECT_TRUE(c.vazio()) << "o carrinho sobreviveu à exceção";
}

}  // namespace
