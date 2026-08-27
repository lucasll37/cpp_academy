#pragma once
// =============================================================================
//  demo_testability.hpp — OCP e o custo real de violá-lo
// =============================================================================
//
//  Os três demos anteriores mostraram COMO ficar aberto para extensão. Este
//  mostra POR QUE isso importa — em duas moedas concretas:
//
//    1. o número de arquivos já testados que precisam ser reabertos
//    2. o número de testes que precisam ser reescritos
//
//  E fecha com a armadilha: OCP mal aplicado é abstração especulativa, que
//  custa mais do que o problema que evita.
//
// =============================================================================

#include <fmt/core.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace demo_testability {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O custo de estender, contado
// ─────────────────────────────────────────────────────────────────────────────
void contabilidade() {
    fmt::print("  1. O custo de acrescentar UM caso novo\n\n");

    fmt::print("     | o que muda                | com switch | com OCP |\n");
    fmt::print("     |---------------------------|------------|---------|\n");
    fmt::print("     | arquivos criados          |     0      |    1    |\n");
    fmt::print("     | arquivos EDITADOS         |    3-5     |    0    |\n");
    fmt::print("     | testes existentes a rodar |   todos    |  todos  |\n");
    fmt::print("     | testes a REESCREVER       |    3-5     |    0    |\n");
    fmt::print("     | risco de regressão        |    alto    |  nenhum |\n");
    fmt::print("     | compilação incremental    | recompila  | só o novo|\n");

    fmt::print("\n     A linha que decide é \"arquivos EDITADOS\". Código que\n");
    fmt::print("     não é tocado não pode regredir.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Testar o que é fechado: um dublê basta
//
//  Como `Notificador` recebe o canal por injeção, o teste não precisa de rede,
//  de SMTP, nem de mock framework — um tipo local de 5 linhas resolve.
// ─────────────────────────────────────────────────────────────────────────────
class Canal {
public:
    virtual ~Canal() = default;
    virtual bool enviar(const std::string& destino, const std::string& msg) = 0;
    virtual std::string nome() const = 0;
};

class CanalEmail final : public Canal {
public:
    bool enviar(const std::string& destino, const std::string&) override {
        fmt::print("        [SMTP] conectando… enviando para {}\n", destino);
        return true;
    }
    std::string nome() const override { return "email"; }
};

class CanalSms final : public Canal {
public:
    bool enviar(const std::string& destino, const std::string&) override {
        fmt::print("        [SMS] gateway… enviando para {}\n", destino);
        return true;
    }
    std::string nome() const override { return "sms"; }
};

// O consumidor, fechado para modificação
class Notificador {
public:
    void adicionar_canal(std::unique_ptr<Canal> c) { canais_.push_back(std::move(c)); }

    int notificar(const std::string& destino, const std::string& msg) {
        int enviados = 0;
        for (auto& c : canais_) {
            if (c->enviar(destino, msg)) ++enviados;
        }
        return enviados;
    }

private:
    std::vector<std::unique_ptr<Canal>> canais_;
};

// ── O dublê de teste: mais uma extensão, como qualquer outra ────────────────
class CanalEspiao final : public Canal {
public:
    bool enviar(const std::string& destino, const std::string& msg) override {
        registro.push_back(destino + " :: " + msg);
        return responder_sucesso;
    }
    std::string nome() const override { return "espião"; }

    std::vector<std::string> registro;
    bool responder_sucesso = true;
};

void testabilidade() {
    fmt::print("\n  2. Testar o consumidor sem tocar em rede\n");

    fmt::print("     produção:\n");
    Notificador prod;
    prod.adicionar_canal(std::make_unique<CanalEmail>());
    prod.adicionar_canal(std::make_unique<CanalSms>());
    fmt::print("        {} canais entregaram\n", prod.notificar("ada@exemplo.com", "olá"));

    fmt::print("\n     teste (mesmo Notificador, canal diferente):\n");
    auto espiao = std::make_unique<CanalEspiao>();
    auto* ptr = espiao.get();                    // guardamos para inspecionar depois

    Notificador sut;
    sut.adicionar_canal(std::move(espiao));

    const int enviados = sut.notificar("teste@local", "mensagem de teste");

    fmt::print("        enviados = {} (esperado 1) → {}\n",
               enviados, enviados == 1 ? "PASSOU" : "FALHOU");
    fmt::print("        registro = '{}'\n", ptr->registro.front());

    // Simular falha é trivial: nenhum framework, nenhuma rede fora do ar
    ptr->responder_sucesso = false;
    const int apos_falha = sut.notificar("teste@local", "outra");   // UMA chamada só
    fmt::print("        com o canal falhando → enviados = {} (esperado 0) → {}\n",
               apos_falha, apos_falha == 0 ? "PASSOU" : "FALHOU");
    fmt::print("        o espião registrou {} mensagens no total\n", ptr->registro.size());

    fmt::print("\n     O CanalEspiao é só mais uma extensão. Nenhuma linha de\n");
    fmt::print("     `Notificador` mudou para torná-lo testável.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. A armadilha: abstração especulativa
// ─────────────────────────────────────────────────────────────────────────────
void armadilha() {
    fmt::print("\n  3. ⚠ A armadilha — abstração especulativa\n");

    fmt::print("     OCP aplicado ANTES de haver um segundo caso é custo puro:\n");
    fmt::print("       • uma interface com um único implementador\n");
    fmt::print("       • uma fábrica que sempre devolve o mesmo tipo\n");
    fmt::print("       • um ponto de extensão que ninguém nunca estendeu\n");

    fmt::print("\n     Cada um deles cobra: indireção para ler, vtable para rodar,\n");
    fmt::print("     e um arquivo a mais para manter. E a abstração escolhida\n");
    fmt::print("     antes do segundo caso quase sempre é a ERRADA — porque foi\n");
    fmt::print("     desenhada com uma amostra de tamanho 1.\n");

    fmt::print("\n     A heurística de Uncle Bob é a Rule of Three:\n");
    fmt::print("       1ª vez  → escreva direto\n");
    fmt::print("       2ª vez  → duplique, e anote o incômodo\n");
    fmt::print("       3ª vez  → AGORA abstraia; você já viu o eixo da variação\n");

    fmt::print("\n     OCP é resposta a uma mudança que JÁ ACONTECEU duas vezes —\n");
    fmt::print("     não profecia sobre a que talvez aconteça.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 4: Custo, testabilidade e excessos ───────────────┐\n");
    contabilidade();
    testabilidade();
    armadilha();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_testability
