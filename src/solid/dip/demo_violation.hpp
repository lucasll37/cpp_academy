#pragma once
// =============================================================================
//  demo_violation.hpp — DIP: violação vs. conformidade
// =============================================================================
//
//  Dependency Inversion Principle, de Robert C. Martin:
//
//    a) "High-level modules should not depend on low-level modules.
//        Both should depend on abstractions."
//    b) "Abstractions should not depend on details.
//        Details should depend on abstractions."
//
//  O nome engana. "Inversão" não é sobre injetar dependência — é sobre a
//  direção da SETA de dependência no diagrama de módulos.
//
//    ANTES (fluxo natural, acoplado):
//      [Política de negócio]  ──depende──▶  [MySQL]
//
//    DEPOIS (dependência invertida):
//      [Política de negócio] ──▶ [IRepositorio] ◀──implementa── [MySQL]
//
//    A seta do MySQL foi INVERTIDA: agora ele aponta para cima, para a
//    abstração que a política definiu. E a interface pertence à POLÍTICA,
//    não ao banco — é o ISP do demo anterior levado às últimas consequências.
//
//  O teste decisivo: em que direção o CÓDIGO-FONTE aponta em tempo de
//  compilação? Se o módulo de negócio dá `#include "mysql.hpp"`, não houve
//  inversão nenhuma, por mais interfaces que existam.
//
// =============================================================================

#include <fmt/core.h>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace demo_violation {

struct Pedido {
    int         id;
    std::string cliente;
    double      total;
};

// ─────────────────────────────────────────────────────────────────────────────
//  ❌ VIOLAÇÃO — a política de negócio conhece o banco pelo nome
// ─────────────────────────────────────────────────────────────────────────────
namespace bad {

// Módulo de BAIXO nível: detalhe de infraestrutura
class BancoMySql {
public:
    void conectar() { fmt::print("       [MySQL] conectando em localhost:3306…\n"); }
    void inserir(const Pedido& p) {
        fmt::print("       [MySQL] INSERT INTO pedidos VALUES({}, '{}', {:.2f})\n",
                   p.id, p.cliente, p.total);
    }
    std::optional<Pedido> selecionar(int id) {
        fmt::print("       [MySQL] SELECT * FROM pedidos WHERE id={}\n", id);
        return Pedido{id, "cliente-do-banco", 250.0};
    }
};

// Módulo de ALTO nível — a regra de negócio, que é o que o produto realmente é.
// E ele depende, em tempo de COMPILAÇÃO, de uma classe concreta de banco.
class ServicoDePedidos {
public:
    ServicoDePedidos() { banco_.conectar(); }   // ← acoplado no construtor

    void registrar(const Pedido& p) {
        if (p.total <= 0) { fmt::print("       recusado: total inválido\n"); return; }
        banco_.inserir(p);                       // ← acoplado à API do MySQL
    }

private:
    BancoMySql banco_;                           // ← acoplado ao TIPO concreto
};

void demo() {
    fmt::print("  ❌ VIOLAÇÃO — a regra de negócio depende do MySQL\n");

    ServicoDePedidos servico;
    servico.registrar({1, "Ada", 199.90});

    fmt::print("\n     Consequências, todas caras:\n");
    fmt::print("       • testar a REGRA exige um MySQL rodando\n");
    fmt::print("       • trocar para PostgreSQL edita a regra de negócio\n");
    fmt::print("       • a regra recompila quando o header do MySQL muda\n");
    fmt::print("       • não dá para reusar a regra num contexto sem banco\n");
    fmt::print("     E a seta aponta para o lado errado:\n");
    fmt::print("       [ServicoDePedidos] ──depende──▶ [BancoMySql]\n");
}

} // namespace bad

// ─────────────────────────────────────────────────────────────────────────────
//  ✅ CONFORMIDADE — a seta invertida
//
//  A interface é declarada AO LADO da política (é ela quem define o que
//  precisa), e a infraestrutura a implementa. O `#include` do módulo de
//  negócio nunca menciona MySQL.
// ─────────────────────────────────────────────────────────────────────────────
namespace good {

// ── Camada de POLÍTICA: a abstração pertence a ela ──────────────────────────
// Repare no nome: "RepositorioDePedidos", não "IBancoDeDados". O nome vem do
// que a POLÍTICA precisa, não do que a infraestrutura é.
class RepositorioDePedidos {
public:
    virtual ~RepositorioDePedidos() = default;
    virtual void                  salvar(const Pedido& p) = 0;
    virtual std::optional<Pedido> por_id(int id) const    = 0;
};

// E este módulo não conhece MySQL, arquivo, memória — nada.
class ServicoDePedidos {
public:
    explicit ServicoDePedidos(RepositorioDePedidos& repo) : repo_(repo) {}

    bool registrar(const Pedido& p) {
        if (p.total <= 0) {
            fmt::print("       recusado: total inválido\n");
            return false;
        }
        repo_.salvar(p);
        return true;
    }

    double total_com_frete(int id, double frete) const {
        auto p = repo_.por_id(id);
        return p ? p->total + frete : 0.0;
    }

private:
    RepositorioDePedidos& repo_;
};

// ── Camada de INFRAESTRUTURA: implementa a abstração da política ────────────
// A seta agora aponta PARA CIMA: é o MySQL que inclui a interface da política.
class RepositorioMySql final : public RepositorioDePedidos {
public:
    RepositorioMySql() { fmt::print("       [MySQL] conectando…\n"); }
    void salvar(const Pedido& p) override {
        fmt::print("       [MySQL] INSERT id={} total={:.2f}\n", p.id, p.total);
        dados_[p.id] = p;
    }
    std::optional<Pedido> por_id(int id) const override {
        auto it = dados_.find(id);
        return it != dados_.end() ? std::optional{it->second} : std::nullopt;
    }
private:
    std::map<int, Pedido> dados_;
};

class RepositorioEmArquivo final : public RepositorioDePedidos {
public:
    void salvar(const Pedido& p) override {
        fmt::print("       [arquivo] gravando pedido {} em pedidos.csv\n", p.id);
        dados_[p.id] = p;
    }
    std::optional<Pedido> por_id(int id) const override {
        auto it = dados_.find(id);
        return it != dados_.end() ? std::optional{it->second} : std::nullopt;
    }
private:
    std::map<int, Pedido> dados_;
};

// Um repositório em memória: nasceu para os testes, e não custou nada.
class RepositorioEmMemoria final : public RepositorioDePedidos {
public:
    void salvar(const Pedido& p) override { dados_[p.id] = p; ++gravacoes; }
    std::optional<Pedido> por_id(int id) const override {
        auto it = dados_.find(id);
        return it != dados_.end() ? std::optional{it->second} : std::nullopt;
    }
    std::map<int, Pedido> dados_;
    int                   gravacoes = 0;
};

void demo() {
    fmt::print("  ✅ CONFORMIDADE — a seta invertida\n");

    fmt::print("     produção com MySQL:\n");
    RepositorioMySql mysql;
    ServicoDePedidos servico_prod(mysql);
    servico_prod.registrar({1, "Ada", 199.90});
    fmt::print("       total com frete: {:.2f}\n", servico_prod.total_com_frete(1, 20));

    fmt::print("\n     o MESMO serviço, agora gravando em arquivo:\n");
    RepositorioEmArquivo arquivo;
    ServicoDePedidos servico_arq(arquivo);
    servico_arq.registrar({2, "Grace", 350.00});

    fmt::print("\n     e o MESMO serviço, num teste — sem banco, sem I/O:\n");
    RepositorioEmMemoria memoria;
    ServicoDePedidos sut(memoria);

    const bool aceitou = sut.registrar({3, "Linus", 99.90});
    const bool recusou = !sut.registrar({4, "Dennis", -10.0});

    fmt::print("       total válido aceito?   {}  {}\n", aceitou, aceitou ? "PASSOU" : "FALHOU");
    fmt::print("       total negativo recusado? {}  {}\n", recusou, recusou ? "PASSOU" : "FALHOU");
    fmt::print("       gravações no repositório: {} (esperado 1)  {}\n",
               memoria.gravacoes, memoria.gravacoes == 1 ? "PASSOU" : "FALHOU");

    fmt::print("\n     `ServicoDePedidos` é o MESMO código nos três casos.\n");
    fmt::print("     Ele nunca soube que MySQL existe.\n");
    fmt::print("       [ServicoDePedidos] ──▶ [RepositorioDePedidos] ◀── [MySQL]\n");
    fmt::print("                                                     ◀── [Arquivo]\n");
    fmt::print("                                                     ◀── [Memória]\n");
}

} // namespace good

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 1: Violação vs. Conformidade ─────────────────────┐\n");
    bad::demo();
    fmt::print("\n");
    good::demo();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_violation
