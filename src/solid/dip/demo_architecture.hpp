#pragma once
// =============================================================================
//  demo_architecture.hpp — DIP em escala: a regra da dependência
// =============================================================================
//
//  Os demos anteriores inverteram UMA dependência. Aplicado a um sistema
//  inteiro, o DIP produz uma arquitetura — é a base da Clean Architecture,
//  da Hexagonal (Ports & Adapters) e da Onion.
//
//  Todas dizem a mesma coisa, com nomes diferentes:
//
//    A REGRA DA DEPENDÊNCIA
//    O código-fonte só pode apontar para DENTRO, na direção das políticas
//    de mais alto nível. Nada num círculo interno sabe o nome de nada num
//    círculo externo.
//
//        ┌──────────────────────────────────────┐
//        │  Frameworks & Drivers  (SQL, HTTP)   │  ← mais volátil
//        │  ┌────────────────────────────────┐  │
//        │  │  Adaptadores (repos, gateways) │  │
//        │  │  ┌──────────────────────────┐  │  │
//        │  │  │  Casos de uso            │  │  │
//        │  │  │  ┌────────────────────┐  │  │  │
//        │  │  │  │  Entidades         │  │  │  │  ← mais estável
//        │  │  │  └────────────────────┘  │  │  │
//        │  │  └──────────────────────────┘  │  │
//        │  └────────────────────────────────┘  │
//        └──────────────────────────────────────┘
//                    dependências ──▶ para dentro
//
//  E o mecanismo que torna isso possível é exatamente o DIP: quando o fluxo
//  de CONTROLE precisa ir de dentro para fora (o caso de uso precisa gravar
//  no banco), inverte-se a dependência de CÓDIGO com uma interface declarada
//  no círculo interno.
//
// =============================================================================

#include <fmt/core.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace demo_architecture {

// ─────────────────────────────────────────────────────────────────────────────
//  Círculo 1 — ENTIDADES
//  Regras de negócio que valeriam mesmo se o software não existisse.
//  Não incluem NADA. Nem framework, nem banco, nem log.
// ─────────────────────────────────────────────────────────────────────────────
namespace entidades {

class Assinatura {
public:
    Assinatura(int id, double mensalidade)
        : id_(id), mensalidade_(mensalidade) {}

    // Regra do negócio, não do software: desconto anual é de 2 meses.
    double valor_anual() const { return mensalidade_ * 10; }

    bool pode_cancelar(int meses_ativos) const { return meses_ativos >= 3; }

    int    id()          const { return id_; }
    double mensalidade() const { return mensalidade_; }

private:
    int    id_;
    double mensalidade_;
};

} // namespace entidades

// ─────────────────────────────────────────────────────────────────────────────
//  Círculo 2 — CASOS DE USO
//  Orquestram as entidades. Declaram as PORTAS de que precisam.
//  Incluem `entidades`, e mais nada de fora.
// ─────────────────────────────────────────────────────────────────────────────
namespace casos_de_uso {

// ── PORTAS (declaradas AQUI, pela política — é o DIP em ação) ───────────────
class RepositorioDeAssinaturas {
public:
    virtual ~RepositorioDeAssinaturas() = default;
    virtual void                             salvar(const entidades::Assinatura&) = 0;
    virtual std::optional<entidades::Assinatura> por_id(int id) const = 0;
};

class NotificadorDeCliente {
public:
    virtual ~NotificadorDeCliente() = default;
    virtual void avisar(int assinatura_id, const std::string& mensagem) = 0;
};

// ── O caso de uso ───────────────────────────────────────────────────────────
class ContratarAnual {
public:
    ContratarAnual(RepositorioDeAssinaturas& repo, NotificadorDeCliente& notif)
        : repo_(repo), notif_(notif) {}

    struct Resultado { bool sucesso; double cobrado; std::string motivo; };

    Resultado executar(int id, double mensalidade) {
        if (mensalidade <= 0) return {false, 0, "mensalidade inválida"};

        entidades::Assinatura a(id, mensalidade);
        const double total = a.valor_anual();          // regra da ENTIDADE

        repo_.salvar(a);                               // sai pela PORTA
        notif_.avisar(id, fmt::format("plano anual contratado: {:.2f}", total));

        return {true, total, ""};
    }

private:
    RepositorioDeAssinaturas& repo_;
    NotificadorDeCliente&     notif_;
};

} // namespace casos_de_uso

// ─────────────────────────────────────────────────────────────────────────────
//  Círculo 3 — ADAPTADORES
//  Implementam as portas do círculo interno usando a tecnologia do externo.
//  A seta aponta PARA DENTRO: é o adaptador que inclui o caso de uso.
// ─────────────────────────────────────────────────────────────────────────────
namespace adaptadores {

class RepositorioSql final : public casos_de_uso::RepositorioDeAssinaturas {
public:
    void salvar(const entidades::Assinatura& a) override {
        fmt::print("       [SQL] INSERT assinaturas(id={}, mensal={:.2f})\n",
                   a.id(), a.mensalidade());
        dados_.emplace(a.id(), a);
    }
    std::optional<entidades::Assinatura> por_id(int id) const override {
        auto it = dados_.find(id);
        return it != dados_.end() ? std::optional{it->second} : std::nullopt;
    }
private:
    std::map<int, entidades::Assinatura> dados_;
};

class NotificadorPorEmail final : public casos_de_uso::NotificadorDeCliente {
public:
    void avisar(int id, const std::string& msg) override {
        fmt::print("       [SMTP] para assinante {}: {}\n", id, msg);
    }
};

// ── Dublês: mais dois adaptadores, e nada de especial neles ────────────────
class RepositorioFalso final : public casos_de_uso::RepositorioDeAssinaturas {
public:
    void salvar(const entidades::Assinatura& a) override { salvos.push_back(a); }
    std::optional<entidades::Assinatura> por_id(int) const override { return std::nullopt; }
    std::vector<entidades::Assinatura> salvos;
};

class NotificadorFalso final : public casos_de_uso::NotificadorDeCliente {
public:
    void avisar(int, const std::string& msg) override { mensagens.push_back(msg); }
    std::vector<std::string> mensagens;
};

} // namespace adaptadores

// ─────────────────────────────────────────────────────────────────────────────
//  Círculo 4 — o composition root, que pode conhecer TODOS os círculos
// ─────────────────────────────────────────────────────────────────────────────
void demo() {
    fmt::print("  Produção — o caso de uso com SQL e SMTP:\n");
    adaptadores::RepositorioSql        repo;
    adaptadores::NotificadorPorEmail   notif;
    casos_de_uso::ContratarAnual       uc(repo, notif);

    auto r = uc.executar(42, 99.90);
    fmt::print("       resultado: sucesso={} cobrado={:.2f}\n", r.sucesso, r.cobrado);

    fmt::print("\n  Teste — o MESMO caso de uso, sem banco e sem rede:\n");
    adaptadores::RepositorioFalso   repo_falso;
    adaptadores::NotificadorFalso   notif_falso;
    casos_de_uso::ContratarAnual    sut(repo_falso, notif_falso);

    auto ok  = sut.executar(1, 100.0);
    auto bad = sut.executar(2, -5.0);

    fmt::print("       anual de 100/mês cobrou {:.2f} (esperado 1000.00)  {}\n",
               ok.cobrado, ok.cobrado == 1000.0 ? "PASSOU" : "FALHOU");
    fmt::print("       mensalidade negativa recusada: {}  {}\n",
               !bad.sucesso, !bad.sucesso ? "PASSOU" : "FALHOU");
    fmt::print("       gravou {} assinatura(s) (esperado 1)  {}\n",
               repo_falso.salvos.size(), repo_falso.salvos.size() == 1 ? "PASSOU" : "FALHOU");
    fmt::print("       notificou {} vez(es) (esperado 1)  {}\n",
               notif_falso.mensagens.size(),
               notif_falso.mensagens.size() == 1 ? "PASSOU" : "FALHOU");
}

void a_regra() {
    fmt::print("\n  A regra da dependência, verificável no `#include`:\n\n");
    fmt::print("     | círculo         | pode incluir              | volatilidade |\n");
    fmt::print("     |-----------------|---------------------------|--------------|\n");
    fmt::print("     | entidades       | nada                      | mínima       |\n");
    fmt::print("     | casos de uso    | entidades                 | baixa        |\n");
    fmt::print("     | adaptadores     | casos de uso + entidades  | média        |\n");
    fmt::print("     | frameworks      | tudo                      | máxima       |\n");

    fmt::print("\n     O teste é mecânico: `grep -r '#include' src/entidades/`.\n");
    fmt::print("     Se aparecer um header de SQL, a arquitetura foi violada —\n");
    fmt::print("     por mais interfaces que existam no meio do caminho.\n");

    fmt::print("\n  E a inversão de CONTROLE vs. de DEPENDÊNCIA:\n");
    fmt::print("     fluxo de CONTROLE : caso de uso ──▶ banco    (de dentro p/ fora)\n");
    fmt::print("     fluxo de CÓDIGO   : banco ──▶ caso de uso    (de fora p/ dentro)\n");
    fmt::print("     São OPOSTOS, e é essa oposição que dá nome ao princípio.\n");
}

void quando_nao() {
    fmt::print("\n  ⚠ Quando NÃO inverter\n");
    fmt::print("     DIP paga-se com uma interface, uma indireção e um arquivo\n");
    fmt::print("     a mais por dependência. Não vale a pena quando:\n");
    fmt::print("       • a dependência é ESTÁVEL — `std::string`, `std::vector`,\n");
    fmt::print("         `fmt`. Ninguém abstrai a biblioteca padrão.\n");
    fmt::print("       • o programa é pequeno e não tem testes automatizados\n");
    fmt::print("       • existe UM implementador e não haverá outro\n");
    fmt::print("\n     A pergunta que decide: \"esta dependência é VOLÁTIL?\"\n");
    fmt::print("     Inverta o que muda (banco, rede, relógio, sistema de\n");
    fmt::print("     arquivos, hora atual, aleatoriedade). Não inverta o que\n");
    fmt::print("     é estável há 30 anos.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 3: DIP em escala — a regra da dependência ────────┐\n");
    demo();
    a_regra();
    quando_nao();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_architecture
