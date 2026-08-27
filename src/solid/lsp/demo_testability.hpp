#pragma once
// =============================================================================
//  demo_testability.hpp — LSP: como DETECTAR a violação
// =============================================================================
//
//  O problema central do LSP é que ele não é verificável pelo compilador.
//  Mas é verificável por VOCÊ — e há três ferramentas, da mais barata para a
//  mais cara:
//
//    1. testes de CONTRATO — uma suíte escrita contra a BASE, rodada contra
//       CADA derivado. É a tradução direta do princípio em código.
//    2. `final` e composição — impedir a herança que você não vai verificar
//    3. concepts / NVI — mover parte do contrato para o compilador
//
//  A ferramenta 1 é a que realmente resolve, e é surpreendentemente barata.
//
// =============================================================================

#include <fmt/core.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace demo_testability {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Testes de contrato — a suíte que roda contra TODO derivado
// ─────────────────────────────────────────────────────────────────────────────
class Pilha {
public:
    virtual ~Pilha() = default;
    virtual void empilhar(int v)  = 0;
    virtual int  desempilhar()    = 0;   // lança se vazia
    virtual bool vazia()    const = 0;
    virtual std::size_t tamanho() const = 0;
};

class PilhaVetor final : public Pilha {
public:
    void empilhar(int v) override { dados_.push_back(v); }
    int  desempilhar() override {
        if (dados_.empty()) throw std::runtime_error("pilha vazia");
        int v = dados_.back(); dados_.pop_back(); return v;
    }
    bool vazia() const override { return dados_.empty(); }
    std::size_t tamanho() const override { return dados_.size(); }
private:
    std::vector<int> dados_;
};

// Uma pilha com capacidade máxima. Parece inofensiva — e VIOLA o LSP:
// `empilhar` da base promete "sempre aceita"; esta versão descarta em silêncio.
class PilhaLimitada final : public Pilha {
public:
    explicit PilhaLimitada(std::size_t max) : max_(max) {}
    void empilhar(int v) override {
        if (dados_.size() >= max_) return;          // ← descarta calado
        dados_.push_back(v);
    }
    int  desempilhar() override {
        if (dados_.empty()) throw std::runtime_error("pilha vazia");
        int v = dados_.back(); dados_.pop_back(); return v;
    }
    bool vazia() const override { return dados_.empty(); }
    std::size_t tamanho() const override { return dados_.size(); }
private:
    std::vector<int> dados_;
    std::size_t      max_;
};

// ── A suíte de contrato ─────────────────────────────────────────────────────
// Escrita UMA vez, contra a interface. Rodada contra cada implementação.
// É literalmente o LSP transformado em código executável.
struct Resultado { int passou = 0, falhou = 0; };

Resultado testar_contrato_de_pilha(const std::function<std::unique_ptr<Pilha>()>& criar,
                                   const char* nome) {
    Resultado r;
    // As falhas são ACUMULADAS, não impressas na hora: senão elas apareceriam
    // acima do cabeçalho da implementação e pareceriam ser da anterior.
    std::vector<std::string> falhas;
    auto checar = [&](bool ok, const char* descricao) {
        (ok ? r.passou : r.falhou)++;
        if (!ok) falhas.emplace_back(descricao);
    };

    // Contrato 1: uma pilha nova está vazia
    checar(criar()->vazia(), "pilha nova deve estar vazia");

    // Contrato 2: empilhar N elementos resulta em tamanho N
    {
        auto p = criar();
        for (int i = 0; i < 10; ++i) p->empilhar(i);
        checar(p->tamanho() == 10, "empilhar 10 → tamanho 10");
    }

    // Contrato 3: LIFO — o último a entrar é o primeiro a sair
    {
        auto p = criar();
        p->empilhar(1); p->empilhar(2); p->empilhar(3);
        checar(p->desempilhar() == 3, "LIFO: desempilhar devolve o último");
    }

    // Contrato 4: desempilhar de pilha vazia lança
    {
        auto p = criar();
        bool lancou = false;
        try { p->desempilhar(); } catch (const std::exception&) { lancou = true; }
        checar(lancou, "desempilhar vazia deve lançar");
    }

    // Contrato 5: round-trip — tudo que entra, sai
    {
        auto p = criar();
        for (int i = 0; i < 5; ++i) p->empilhar(i);
        int saiu = 0;
        while (!p->vazia()) { p->desempilhar(); ++saiu; }
        checar(saiu == 5, "round-trip: 5 entraram, 5 devem sair");
    }

    fmt::print("     {:<18} {} passou, {} falhou  {}\n",
               nome, r.passou, r.falhou, r.falhou == 0 ? "✓" : "✗ LSP VIOLADO");
    for (const auto& f : falhas) fmt::print("       ✗ {}\n", f);
    return r;
}

void testes_de_contrato() {
    fmt::print("  1. Testes de contrato — o LSP virando código executável\n");

    testar_contrato_de_pilha([] { return std::unique_ptr<Pilha>(new PilhaVetor); },
                             "PilhaVetor");
    testar_contrato_de_pilha([] { return std::unique_ptr<Pilha>(new PilhaLimitada(3)); },
                             "PilhaLimitada(3)");

    fmt::print("\n     A mesma suíte, os mesmos casos. A PilhaLimitada falha nos\n");
    fmt::print("     contratos que dependem de \"empilhar sempre aceita\".\n");
    fmt::print("     Isso é o LSP sendo VERIFICADO — não discutido.\n");
    fmt::print("     Em GoogleTest: TYPED_TEST_SUITE faz exatamente isto.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. NVI — mover o contrato para a base, onde ninguém sobrescreve
//
//  Non-Virtual Interface: o método PÚBLICO não é virtual e guarda as
//  checagens de pré/pós-condição. O virtual é PRIVADO e só implementa o miolo.
//  O derivado não consegue afrouxar o contrato porque não controla a fronteira.
// ─────────────────────────────────────────────────────────────────────────────
class Repositorio {
public:
    virtual ~Repositorio() = default;

    // Público e NÃO virtual: aqui mora o contrato, e ele é inegociável.
    std::string buscar(int id) {
        if (id <= 0) throw std::invalid_argument("id deve ser positivo");  // pré

        std::string r = buscar_impl(id);                                    // miolo

        if (r.empty()) throw std::runtime_error("implementação devolveu vazio"); // pós
        return r;
    }

private:
    // Privado e virtual: o derivado customiza SÓ o miolo.
    virtual std::string buscar_impl(int id) = 0;
};

class RepoMemoria final : public Repositorio {
private:
    std::string buscar_impl(int id) override { return fmt::format("registro-{}", id); }
};

// Tenta violar o contrato devolvendo vazio — e é a BASE que barra.
class RepoQuebrado final : public Repositorio {
private:
    std::string buscar_impl(int) override { return ""; }
};

void nvi() {
    fmt::print("\n  2. NVI — o contrato mora na base, não no derivado\n");

    auto usar = [](Repositorio& r, const char* nome) {
        try {
            fmt::print("     {:<14} buscar(7) → '{}'\n", nome, r.buscar(7));
        } catch (const std::exception& e) {
            fmt::print("     {:<14} buscar(7) → barrado pela BASE: {}\n", nome, e.what());
        }
    };

    RepoMemoria  bom;    usar(bom,  "RepoMemoria");
    RepoQuebrado ruim;   usar(ruim, "RepoQuebrado");

    // A pré-condição também é da base, e vale para todos:
    try { bom.buscar(-1); }
    catch (const std::exception& e) {
        fmt::print("     pré-condição id>0 aplicada a TODOS: {}\n", e.what());
    }

    fmt::print("     O derivado não consegue afrouxar o contrato — ele não\n");
    fmt::print("     controla a fronteira pública. É LSP por construção.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. As saídas quando a herança simplesmente não cabe
// ─────────────────────────────────────────────────────────────────────────────
void alternativas() {
    fmt::print("\n  3. Quando a herança não cabe\n");

    fmt::print("     O caso clássico: Pinguim é uma Ave, mas não voa.\n");
    fmt::print("     Cinco saídas, da melhor para a pior:\n\n");

    fmt::print("     ✓ SEGREGAR a interface     — Ave e AveQueVoa separadas (→ ISP)\n");
    fmt::print("     ✓ COMPOR                   — Pinguim TEM um modo de locomoção\n");
    fmt::print("     ✓ std::variant             — hierarquia fechada, sem herança\n");
    fmt::print("     ~ marcar `final`           — não resolve, mas contém o estrago\n");
    fmt::print("     ✗ voar() lançando exceção  — LSP violado; o cliente quebra\n");
    fmt::print("     ✗ voar() sem fazer nada    — pior: quebra em SILÊNCIO\n");

    fmt::print("\n     Repare que a saída boa (segregar) é o princípio SEGUINTE.\n");
    fmt::print("     LSP e ISP são o mesmo problema visto de dois lados:\n");
    fmt::print("       LSP: \"o derivado consegue cumprir tudo que a base promete?\"\n");
    fmt::print("       ISP: \"a base está prometendo coisas demais?\"\n");
}

// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    fmt::print("┌─ Demo 3: Detectando e prevenindo violações ─────────────┐\n");
    testes_de_contrato();
    nvi();
    alternativas();
    fmt::print("└─────────────────────────────────────────────────────────┘\n");
}

} // namespace demo_testability
