#pragma once
// =============================================================================
//  demo_client_side.hpp — ISP: a interface pertence ao CLIENTE
// =============================================================================
//
//  A leitura ingênua do ISP é "interfaces pequenas". A leitura correta é mais
//  forte: a interface é definida pelo que o CLIENTE precisa, não pelo que o
//  fornecedor tem a oferecer.
//
//  Consequência prática, e é ela que muda o desenho: a interface deve ser
//  DECLARADA junto do cliente, não junto da implementação. Um serviço
//  gigantesco pode ser consumido por dez clientes através de dez interfaces
//  minúsculas — cada uma nomeada segundo o que AQUELE cliente faz.
//
//  Uncle Bob chama isso de "role interfaces": a interface descreve um PAPEL
//  na interação, não um objeto do domínio.
//
// =============================================================================

#include <fmt/core.h>
#include <memory>
#include <string>
#include <vector>

namespace demo_client_side {

// ─────────────────────────────────────────────────────────────────────────────
//  O problema: um serviço legado, grande, que já existe e não vai mudar
// ─────────────────────────────────────────────────────────────────────────────
namespace legado {

// 12 métodos, escritos ao longo de anos por equipes diferentes.
class ServicoDeUsuarios {
public:
    std::string nome_de(int id) const      { return fmt::format("usuário-{}", id); }
    std::string email_de(int id) const     { return fmt::format("u{}@exemplo.com", id); }
    bool        ativo(int id) const        { return id % 2 == 0; }
    double      saldo_de(int id) const     { return id * 100.0; }
    void        debitar(int id, double v)  { fmt::print("        débito de {:.2f} em {}\n", v, id); }
    void        creditar(int id, double v) { fmt::print("        crédito de {:.2f} em {}\n", v, id); }
    void        registrar_acesso(int id)   { fmt::print("        acesso de {} registrado\n", id); }
    void        enviar_email(int id, const std::string& assunto) {
                    fmt::print("        e-mail '{}' para {}\n", assunto, email_de(id)); }
    void        bloquear(int id)           { fmt::print("        {} bloqueado\n", id); }
    void        desbloquear(int id)        { fmt::print("        {} desbloqueado\n", id); }
    int         total_de_usuarios() const  { return 1000; }
    void        purgar_inativos()          { fmt::print("        purgando inativos\n"); }
};

} // namespace legado

// ─────────────────────────────────────────────────────────────────────────────
//  ❌ O jeito ingênuo: cada cliente recebe o serviço inteiro
//
//  `EnviadorDeBoasVindas` só precisa de nome e e-mail. Mas recebendo o serviço
//  todo, ele PODE chamar `purgar_inativos()`. E um dia alguém chama.
//  Além disso, testá-lo exige construir (ou mockar) as 12 operações.
// ─────────────────────────────────────────────────────────────────────────────
namespace acoplado {

class EnviadorDeBoasVindas {
public:
    explicit EnviadorDeBoasVindas(legado::ServicoDeUsuarios& s) : servico_(s) {}

    void enviar(int id) {
        // usa 2 dos 12 métodos... e tem acesso aos outros 10
        fmt::print("        Olá, {}! ({})\n", servico_.nome_de(id), servico_.email_de(id));
    }

private:
    legado::ServicoDeUsuarios& servico_;
};

} // namespace acoplado

// ─────────────────────────────────────────────────────────────────────────────
//  ✅ Interfaces definidas pelo CLIENTE — role interfaces
//
//  Cada cliente declara, ao lado de si mesmo, o mínimo de que precisa.
//  O serviço legado é adaptado a cada papel por um adaptador fininho.
// ─────────────────────────────────────────────────────────────────────────────
namespace por_papel {

// ── Papel 1: "de onde eu tiro o nome e o e-mail de alguém" ──────────────────
class FonteDeContato {
public:
    virtual ~FonteDeContato() = default;
    virtual std::string nome(int id) const  = 0;
    virtual std::string email(int id) const = 0;
};

class EnviadorDeBoasVindas {
public:
    explicit EnviadorDeBoasVindas(const FonteDeContato& fonte) : fonte_(fonte) {}
    void enviar(int id) {
        fmt::print("        Olá, {}! ({})\n", fonte_.nome(id), fonte_.email(id));
    }
private:
    const FonteDeContato& fonte_;
};

// ── Papel 2: "com o que eu movimento dinheiro" ──────────────────────────────
class Carteira {
public:
    virtual ~Carteira() = default;
    virtual double saldo(int id) const = 0;
    virtual void   debitar(int id, double valor) = 0;
};

class ProcessadorDePagamento {
public:
    explicit ProcessadorDePagamento(Carteira& c) : carteira_(c) {}
    bool cobrar(int id, double valor) {
        if (carteira_.saldo(id) < valor) {
            fmt::print("        saldo insuficiente para cobrar {:.2f}\n", valor);
            return false;
        }
        carteira_.debitar(id, valor);
        return true;
    }
private:
    Carteira& carteira_;
};

// ── Os adaptadores: ligam o serviço gordo a cada papel ──────────────────────
class ContatoViaServico final : public FonteDeContato {
public:
    explicit ContatoViaServico(const legado::ServicoDeUsuarios& s) : s_(s) {}
    std::string nome(int id) const override  { return s_.nome_de(id); }
    std::string email(int id) const override { return s_.email_de(id); }
private:
    const legado::ServicoDeUsuarios& s_;
};

class CarteiraViaServico final : public Carteira {
public:
    explicit CarteiraViaServico(legado::ServicoDeUsuarios& s) : s_(s) {}
    double saldo(int id) const override { return s_.saldo_de(id); }
    void debitar(int id, double v) override { s_.debitar(id, v); }
private:
    legado::ServicoDeUsuarios& s_;
};

} // namespace por_papel

// ─────────────────────────────────────────────────────────────────────────────
//  E o que isso compra: dublês de teste triviais
// ─────────────────────────────────────────────────────────────────────────────
namespace testes {

// Para testar o EnviadorDeBoasVindas, o dublê tem DOIS métodos.
// Na versão acoplada, precisaríamos de doze.
class ContatoFalso final : public por_papel::FonteDeContato {
public:
    std::string nome(int)  const override { return "Ada Lovelace"; }
    std::string email(int) const override { return "ada@teste.local"; }
};

class CarteiraFalsa final : public por_papel::Carteira {
public:
    double saldo(int) const override { return saldo_configurado; }
    void debitar(int, double v) override { debitos.push_back(v); }

    double              saldo_configurado = 0;
    std::vector<double> debitos;
};

} // namespace testes

// ─────────────────────────────────────────────────────────────────────────────
void demo() {
    legado::ServicoDeUsuarios servico;

    fmt::print("  ❌ Acoplado ao serviço inteiro (12 métodos disponíveis):\n");
    acoplado::EnviadorDeBoasVindas ruim(servico);
    ruim.enviar(7);
    fmt::print("     ↑ usou 2 métodos, mas PODE chamar `purgar_inativos()`.\n");
    fmt::print("       Para testar, é preciso um dublê com 12 métodos.\n");

    fmt::print("\n  ✅ Cada cliente com a SUA interface:\n");
    por_papel::ContatoViaServico  contato(servico);
    por_papel::CarteiraViaServico carteira(servico);

    por_papel::EnviadorDeBoasVindas   bom(contato);
    por_papel::ProcessadorDePagamento pagamento(carteira);

    bom.enviar(7);
    pagamento.cobrar(7, 500.0);
    fmt::print("     ↑ o enviador NÃO CONSEGUE tocar em dinheiro. O processador\n");
    fmt::print("       NÃO CONSEGUE mandar e-mail. Garantido pelo tipo.\n");

    fmt::print("\n  E o teste fica trivial:\n");
    testes::ContatoFalso falso;
    por_papel::EnviadorDeBoasVindas sut(falso);
    sut.enviar(999);

    testes::CarteiraFalsa carteira_falsa;
    carteira_falsa.saldo_configurado = 1000;
    por_papel::ProcessadorDePagamento sut2(carteira_falsa);

    const bool ok      = sut2.cobrar(1, 300);
    carteira_falsa.saldo_configurado = 10;
    const bool recusou = !sut2.cobrar(1, 300);

    fmt::print("        cobrança com saldo   → {}  {}\n", ok, ok ? "PASSOU" : "FALHOU");
    fmt::print("        cobrança sem saldo   → recusada: {}  {}\n",
               recusou, recusou ? "PASSOU" : "FALHOU");
    fmt::print("        débitos registrados  → {}\n", carteira_falsa.debitos.size());

    fmt::print("\n     Os dublês têm 2 métodos cada. Nenhum framework de mock,\n");
    fmt::print("     nenhum banco de dados, nenhuma rede.\n");
}

void principios() {
    fmt::print("\n  A regra que resume tudo:\n");
    fmt::print("     A interface pertence ao CLIENTE, não ao fornecedor.\n");
    fmt::print("     Por isso ela deve ser DECLARADA junto do cliente —\n");
    fmt::print("     e nomeada segundo o PAPEL, não segundo a classe concreta.\n\n");

    fmt::print("     | nome ruim (do fornecedor) | nome bom (do papel)   |\n");
    fmt::print("     |---------------------------|-----------------------|\n");
    fmt::print("     | IServicoDeUsuarios        | FonteDeContato        |\n");
    fmt::print("     | IRepositorioMySql         | LeitorDePedidos       |\n");
    fmt::print("     | IGerenciadorDeArquivos    | Persistidor           |\n");
    fmt::print("     | IHelper / IManager / IUtil| (qualquer coisa menos)|\n");

    fmt::print("\n     Ver o próximo princípio (DIP): é ele que explica POR QUE\n");
    fmt::print("     a interface declarada no cliente inverte a dependência.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 2: A interface pertence ao cliente ───────────────┐\n");
    demo();
    principios();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_client_side
