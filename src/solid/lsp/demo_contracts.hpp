#pragma once
// =============================================================================
//  demo_contracts.hpp — LSP: as quatro regras do contrato
// =============================================================================
//
//  LSP não é intuição — tem regras precisas. Um subtipo pode variar o
//  contrato, mas só numa direção:
//
//    1. PRÉ-CONDIÇÕES  não podem ser FORTALECIDAS
//       (o derivado não pode exigir MAIS do chamador)
//    2. PÓS-CONDIÇÕES  não podem ser ENFRAQUECIDAS
//       (o derivado não pode entregar MENOS)
//    3. INVARIANTES da base têm que ser preservados
//    4. HISTORY RULE — o derivado não pode permitir mudanças de estado que
//       a base proibia
//
//  Regra mnemônica: o derivado pode ser mais GENEROSO, nunca mais EXIGENTE.
//  (é a mesma covariância/contravariância dos tipos de função)
//
// =============================================================================

#include <fmt/core.h>
#include <algorithm>
#include <fmt/ranges.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace demo_contracts {

// ─────────────────────────────────────────────────────────────────────────────
//  Regra 1 — pré-condições não podem ser fortalecidas
// ─────────────────────────────────────────────────────────────────────────────
namespace precondicao {

class Deposito {
public:
    virtual ~Deposito() = default;
    // CONTRATO: aceita qualquer valor positivo
    virtual void depositar(double valor) {
        if (valor <= 0) throw std::invalid_argument("valor deve ser positivo");
        saldo_ += valor;
    }
    double saldo() const { return saldo_; }
protected:
    double saldo_ = 0;
};

// ❌ Fortalece a pré-condição: agora exige valor >= 100
class DepositoVip : public Deposito {
public:
    void depositar(double valor) override {
        if (valor < 100) throw std::invalid_argument("VIP exige mínimo de 100");
        Deposito::depositar(valor);
    }
};

// ✅ Enfraquece (é mais generoso): aceita até valores negativos, como estorno
class DepositoComEstorno : public Deposito {
public:
    void depositar(double valor) override {
        if (valor < 0) { saldo_ += valor; return; }    // aceita mais casos
        Deposito::depositar(valor);
    }
};

void demo() {
    fmt::print("  Regra 1 — pré-condições não podem ser FORTALECIDAS\n");

    // O cliente foi escrito contra Deposito: "qualquer valor positivo serve"
    auto cliente = [](Deposito& d, const char* rotulo) {
        try {
            d.depositar(50);                            // válido pelo contrato da base
            fmt::print("     {:<20} depositar(50) → saldo {:.2f}  ✓\n", rotulo, d.saldo());
        } catch (const std::exception& e) {
            fmt::print("     {:<20} depositar(50) → EXCEÇÃO: {}  ✗ LSP\n", rotulo, e.what());
        }
    };

    Deposito           base;   cliente(base,    "Deposito");
    DepositoComEstorno bom;    cliente(bom,     "DepositoComEstorno");
    DepositoVip        ruim;   cliente(ruim,    "DepositoVip");

    fmt::print("     O DepositoVip exige MAIS do chamador que a base prometia.\n");
    fmt::print("     Código que funcionava com Deposito quebra com ele.\n");
}

} // namespace precondicao

// ─────────────────────────────────────────────────────────────────────────────
//  Regra 2 — pós-condições não podem ser enfraquecidas
// ─────────────────────────────────────────────────────────────────────────────
namespace poscondicao {

class Ordenador {
public:
    virtual ~Ordenador() = default;
    // CONTRATO: devolve TODOS os elementos, em ordem crescente
    virtual std::vector<int> ordenar(std::vector<int> v) const {
        std::sort(v.begin(), v.end());
        return v;
    }
};

// ❌ Enfraquece a pós-condição: devolve menos elementos que recebeu
class OrdenadorQueDeduplica : public Ordenador {
public:
    std::vector<int> ordenar(std::vector<int> v) const override {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());   // ← perdeu elementos
        return v;
    }
};

// ✅ Fortalece a pós-condição: além de ordenado, garante estabilidade.
//    Entregar MAIS do que prometeu é sempre permitido.
class OrdenadorEstavel : public Ordenador {
public:
    std::vector<int> ordenar(std::vector<int> v) const override {
        std::stable_sort(v.begin(), v.end());
        return v;
    }
};

void demo() {
    fmt::print("\n  Regra 2 — pós-condições não podem ser ENFRAQUECIDAS\n");

    const std::vector<int> entrada{3, 1, 3, 2, 1};

    auto cliente = [&entrada](const Ordenador& o, const char* rotulo) {
        auto r = o.ordenar(entrada);
        const bool manteve_tamanho = r.size() == entrada.size();
        fmt::print("     {:<24} {} elementos → {}  {}\n", rotulo, entrada.size(),
                   fmt::join(r, " "), manteve_tamanho ? "✓" : "✗ LSP (perdeu elementos)");
    };

    Ordenador             base;    cliente(base,   "Ordenador");
    OrdenadorEstavel      bom;     cliente(bom,    "OrdenadorEstavel");
    OrdenadorQueDeduplica ruim;    cliente(ruim,   "OrdenadorQueDeduplica");

    fmt::print("     Quem contava com `resultado.size() == entrada.size()`\n");
    fmt::print("     quebra em silêncio — sem exceção, sem crash, dado sumido.\n");
}

} // namespace poscondicao

// ─────────────────────────────────────────────────────────────────────────────
//  Regra 3 — invariantes têm que ser preservados
// ─────────────────────────────────────────────────────────────────────────────
namespace invariante {

class Conta {
public:
    virtual ~Conta() = default;
    // INVARIANTE DA CLASSE: saldo_ nunca é negativo
    virtual void sacar(double valor) {
        if (valor > saldo_) throw std::runtime_error("saldo insuficiente");
        saldo_ -= valor;
    }
    void depositar(double v) { saldo_ += v; }
    double saldo() const { return saldo_; }

    bool invariante_ok() const { return saldo_ >= 0; }

protected:
    double saldo_ = 0;
};

// ❌ Quebra o invariante da base: permite saldo negativo
class ContaComChequeEspecial : public Conta {
public:
    void sacar(double valor) override { saldo_ -= valor; }   // sem checagem!
};

// ✅ Modela o cheque especial SEM violar o invariante: o limite é um campo
//    próprio, e `saldo()` continua nunca sendo negativo.
class ContaComLimite : public Conta {
public:
    explicit ContaComLimite(double limite) : limite_(limite) {}
    void sacar(double valor) override {
        if (valor > saldo_ + limite_) throw std::runtime_error("excede o limite");
        const double do_saldo = std::min(valor, saldo_);
        saldo_  -= do_saldo;
        usado_  += valor - do_saldo;
    }
    double limite_usado() const { return usado_; }
private:
    double limite_, usado_ = 0;
};

void demo() {
    fmt::print("\n  Regra 3 — invariantes da base têm que valer\n");

    auto cliente = [](Conta& c, const char* rotulo) {
        c.depositar(100);
        try { c.sacar(150); } catch (const std::exception&) { /* esperado */ }
        fmt::print("     {:<24} saldo={:7.2f}  invariante(saldo>=0): {}\n",
                   rotulo, c.saldo(), c.invariante_ok() ? "✓" : "✗ LSP VIOLADO");
    };

    Conta                   base;  cliente(base,  "Conta");
    ContaComLimite          bom(500); cliente(bom, "ContaComLimite");
    ContaComChequeEspecial  ruim; cliente(ruim,  "ContaComChequeEspecial");

    fmt::print("     usado do limite (ContaComLimite): {:.2f}\n", bom.limite_usado());
    fmt::print("     O cheque especial é um conceito legítimo — mas modelá-lo\n");
    fmt::print("     quebrando `saldo >= 0` sabota todo código que confia nisso.\n");
}

} // namespace invariante

// ─────────────────────────────────────────────────────────────────────────────
//  Regra 4 — History Rule
// ─────────────────────────────────────────────────────────────────────────────
namespace historia {

// Um ponto IMUTÁVEL: a base promete que, uma vez construído, nunca muda.
class PontoImutavel {
public:
    PontoImutavel(int x, int y) : x_(x), y_(y) {}
    virtual ~PontoImutavel() = default;
    int x() const { return x_; }
    int y() const { return y_; }
protected:
    int x_, y_;
};

// ❌ O derivado acrescenta mutabilidade que a base PROIBIA.
//    A assinatura da base continua intacta, mas o histórico possível
//    de estados mudou — e quem cacheou x()/y() foi traído.
class PontoMutavel : public PontoImutavel {
public:
    using PontoImutavel::PontoImutavel;
    void mover(int dx, int dy) { x_ += dx; y_ += dy; }
};

void demo() {
    fmt::print("\n  Regra 4 — History Rule\n");

    // Cliente escrito contra PontoImutavel: cacheia porque "não muda mesmo"
    auto cliente_que_cacheia = [](const PontoImutavel& p) {
        const int cache_x = p.x();
        return [&p, cache_x] {
            return cache_x == p.x() ? "cache válido ✓" : "cache INVÁLIDO ✗ LSP";
        };
    };

    PontoImutavel fixo(10, 20);
    auto verificar_fixo = cliente_que_cacheia(fixo);
    fmt::print("     PontoImutavel  → {}\n", verificar_fixo());

    PontoMutavel movel(10, 20);
    auto verificar_movel = cliente_que_cacheia(movel);
    movel.mover(5, 5);                       // a base jurava que isto não existia
    fmt::print("     PontoMutavel   → {}\n", verificar_movel());

    fmt::print("     Nenhum método foi sobrescrito. Nenhuma assinatura mudou.\n");
    fmt::print("     O que mudou foi o conjunto de HISTÓRIAS possíveis do objeto.\n");
    fmt::print("     Por isso `final` e a imutabilidade são ferramentas de LSP:\n");
    fmt::print("     elas impedem que um derivado invente histórias novas.\n");
}

} // namespace historia

// ─────────────────────────────────────────────────────────────────────────────
void resumo() {
    fmt::print("\n  As quatro regras, em uma tabela:\n");
    fmt::print("     | o subtipo pode... | pré-cond. | pós-cond. | invariante |\n");
    fmt::print("     |-------------------|-----------|-----------|------------|\n");
    fmt::print("     | exigir MAIS       |    ✗      |    —      |     —      |\n");
    fmt::print("     | exigir MENOS      |    ✓      |    —      |     —      |\n");
    fmt::print("     | entregar MAIS     |    —      |    ✓      |     ✓      |\n");
    fmt::print("     | entregar MENOS    |    —      |    ✗      |     ✗      |\n");
    fmt::print("\n     Mnemônico: o derivado pode ser mais GENEROSO, nunca mais\n");
    fmt::print("     EXIGENTE. É a mesma contravariância dos tipos de função.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 2: As quatro regras do contrato ──────────────────┐\n");
    precondicao::demo();
    poscondicao::demo();
    invariante::demo();
    historia::demo();
    resumo();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_contracts
