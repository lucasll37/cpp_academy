// =============================================================================
//  src/testing/test_mock.cpp
//
//  gmock — dublês de teste que verificam a INTERAÇÃO
//  ─────────────────────────────────────────────────
//  Os testes anteriores usam um `PrecosFixos` escrito à mão: ele DEVOLVE
//  valores, e o teste verifica o ESTADO resultante. Isso cobre a maioria dos
//  casos, e é o que se deve preferir.
//
//  Há uma classe de perguntas que o estado não responde:
//    • o carrinho consultou o repositório UMA vez, ou uma por item?
//    • ele consultou com o sku CERTO?
//    • ele consultou de novo depois de a resposta ter sido cacheada?
//
//  Essas são perguntas sobre INTERAÇÃO, e é para elas que gmock existe.
//
//  Os cinco tipos de dublê (a taxonomia de Meszaros):
//    dummy  — só preenche um parâmetro; nunca é usado
//    stub   — devolve respostas prontas       ← PrecosFixos é um stub
//    spy    — um stub que anota o que recebeu
//    mock   — tem EXPECTATIVAS, e falha se elas não forem satisfeitas
//    fake   — implementação real, mas simplificada (um "banco" em memória)
//
//  Rode com:   meson test -C build testing_mock -v
// =============================================================================
#include "carrinho.hpp"
#include <gmock/gmock.h>
#include <map>
#include <gtest/gtest.h>

namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

// ── O mock: uma linha por método virtual ────────────────────────────────────
class MockPrecos : public loja::RepositorioDePrecos {
public:
    // MOCK_METHOD(retorno, nome, (parâmetros), (qualificadores));
    // ⚠ os parênteses em volta dos parâmetros são obrigatórios, e um tipo com
    //   vírgula (std::map<int,int>) precisa dos seus próprios parênteses ou de
    //   um alias — a macro se confunde com a vírgula.
    MOCK_METHOD(std::optional<double>, preco_de, (const std::string& sku),
                (const, override));
};

// ── 1. Definir o comportamento: ON_CALL / WillByDefault ─────────────────────
TEST(MockPrecos, StubBasico) {
    NiceMock<MockPrecos> precos;    // Nice: não reclama de chamadas inesperadas

    // ⚠ A ORDEM AQUI IMPORTA, e ao contrário da intuição: o gmock tenta as
    //   regras da MAIS RECENTE para a mais antiga. A regra genérica precisa
    //   vir PRIMEIRO, para que as específicas, declaradas depois, ganhem.
    //
    //   A primeira versão deste teste tinha a ordem invertida (específica
    //   antes, `_` depois) e falhava com:
    //       C++ exception with description "produto desconhecido: caneta"
    //   — porque o `_`, sendo o mais recente, capturava também a caneta.
    ON_CALL(precos, preco_de(_))    // `_` casa com qualquer argumento
        .WillByDefault(Return(std::nullopt));
    ON_CALL(precos, preco_de(Eq("caneta")))
        .WillByDefault(Return(std::optional<double>{2.50}));

    loja::Carrinho c{precos};
    c.adicionar("caneta", 2);

    EXPECT_DOUBLE_EQ(c.subtotal(), 5.0);
    EXPECT_THROW(c.adicionar("dragao", 1), loja::ProdutoDesconhecido);
}

// ── 2. Verificar a INTERAÇÃO: EXPECT_CALL ───────────────────────────────────
TEST(MockPrecos, ConsultaUmaVezPorAdicionar) {
    MockPrecos precos;

    // Esta é a asserção do teste. Ela é verificada no DESTRUTOR do mock —
    // não há EXPECT_ nenhum no corpo, e ainda assim o teste pode falhar.
    EXPECT_CALL(precos, preco_de(Eq("caneta")))
        .Times(2)                                   // exatamente duas vezes
        .WillRepeatedly(Return(std::optional<double>{2.50}));

    loja::Carrinho c{precos};
    c.adicionar("caneta", 1);
    c.adicionar("caneta", 1);      // mesmo sku: o carrinho consulta DE NOVO
}

TEST(MockPrecos, NaoConsultaAoCalcularTotal) {
    NiceMock<MockPrecos> precos;
    ON_CALL(precos, preco_de(_))
        .WillByDefault(Return(std::optional<double>{10.0}));

    loja::Carrinho c{precos};
    c.adicionar("x", 3);

    // A partir daqui, NENHUMA consulta a mais deve acontecer: o preço já foi
    // copiado para o Item. Se alguém "otimizar" o Carrinho e passar a
    // reconsultar, este teste acusa.
    ::testing::Mock::VerifyAndClearExpectations(&precos);
    EXPECT_CALL(precos, preco_de(_)).Times(0);

    (void)c.subtotal();
    (void)c.total();
    (void)c.percentual_de_desconto();
}

// ── 3. Sequência de chamadas ────────────────────────────────────────────────
TEST(MockPrecos, OrdemDasConsultas) {
    MockPrecos precos;
    {
        ::testing::InSequence seq;      // dentro deste escopo, a ORDEM importa
        EXPECT_CALL(precos, preco_de(Eq("primeiro")))
            .WillOnce(Return(std::optional<double>{1.0}));
        EXPECT_CALL(precos, preco_de(Eq("segundo")))
            .WillOnce(Return(std::optional<double>{2.0}));
    }

    loja::Carrinho c{precos};
    c.adicionar("primeiro", 1);
    c.adicionar("segundo",  1);
}

// ── 4. Nice, Naggy e Strict ─────────────────────────────────────────────────
TEST(MockPrecos, OsTresNiveisDeRigor) {
    // NaggyMock (o PADRÃO): chamada inesperada gera um AVISO no log,
    //                       mas o teste PASSA.
    // NiceMock:             chamada inesperada é ignorada em silêncio.
    // StrictMock:           chamada inesperada FALHA o teste.
    StrictMock<MockPrecos> precos;

    EXPECT_CALL(precos, preco_de(Eq("caneta")))
        .WillOnce(Return(std::optional<double>{2.50}));

    loja::Carrinho c{precos};
    c.adicionar("caneta", 1);
    // Qualquer outra chamada a preco_de() faria este teste falhar.

    // Recomendação prática: use NiceMock por padrão. Um mock naggy enche o
    // log de ruído; um strict quebra o teste toda vez que a implementação
    // ganha uma chamada legítima a mais, e vira manutenção pura.
    // Use StrictMock só quando "não chamar mais nada" É o requisito.
}

// ── 5. ⚠ Quando NÃO usar mock ───────────────────────────────────────────────
TEST(MockPrecos, PrefiraOFakeQuandoDerCerto) {
    // Este "fake" é uma implementação real e simples. Comparado ao mock:
    //   • o teste fala de COMPORTAMENTO, não de chamadas;
    //   • não quebra quando a implementação muda de forma legítima;
    //   • é reutilizável em dezenas de testes sem repetir ON_CALL.
    struct FakeRepositorio : loja::RepositorioDePrecos {
        std::map<std::string, double> tabela;
        std::optional<double> preco_de(const std::string& sku) const override {
            auto it = tabela.find(sku);
            return it == tabela.end() ? std::nullopt
                                      : std::optional<double>{it->second};
        }
    };

    FakeRepositorio precos;
    precos.tabela = {{"caneta", 2.50}, {"caderno", 12.00}};

    loja::Carrinho c{precos};
    c.adicionar("caneta",  4);
    c.adicionar("caderno", 6);          // 10 peças → 5%

    EXPECT_DOUBLE_EQ(c.subtotal(), 82.0);
    EXPECT_DOUBLE_EQ(c.total(), 82.0 * 0.95);

    // ⚠ Um teste cheio de EXPECT_CALL testa a IMPLEMENTAÇÃO, não o
    //   comportamento. Ele passa a falhar em toda refatoração correta —
    //   e um teste que falha quando nada quebrou treina a equipe a ignorá-lo.
    //   Regra: mock para verificar EFEITOS COLATERAIS que não dá para
    //   observar de outro jeito (enviou o e-mail? gravou no log?).
    //   Para tudo mais, fake ou stub.
}

}  // namespace
