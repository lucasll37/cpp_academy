// =============================================================================
//  src/smart-pointers/demo_patterns.hpp
//
//  Padrões Modernos com Smart Pointers
//  ────────────────────────────────────
//  • Regras de ouro para funções (passagem e retorno)
//  • Factory com polimorfismo
//  • Pimpl idiom (handle/body)
//  • Ownership de recursos externos (RAII genérico)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace demo_patterns {

// =============================================================================
//  1. REGRAS DE OURO — como passar smart pointers para funções
// =============================================================================
//
//  ┌──────────────────────────┬────────────────────────────────────────────┐
//  │ Assinatura               │ Significado                                │
//  ├──────────────────────────┼────────────────────────────────────────────┤
//  │ f(Widget&)               │ Apenas usa — não se importa com ownership  │
//  │ f(const Widget&)         │ Apenas lê — não se importa com ownership   │
//  │ f(Widget*)               │ Apenas usa (pode ser nullptr)              │
//  │ f(unique_ptr<Widget>)    │ Toma ownership exclusivo                   │
//  │ f(unique_ptr<Widget>&)   │ Pode reatribuir o ponteiro do caller       │
//  │ f(shared_ptr<Widget>)    │ Compartilha ownership (copia o ptr)        │
//  │ f(const shared_ptr<W>&)  │ Compartilha ownership opcional (pode não   │
//  │                          │ copiar se não for necessário)              │
//  └──────────────────────────┴────────────────────────────────────────────┘

struct Widget {
    int val;
    explicit Widget(int v) : val(v) {
        fmt::print("  [Widget({})] construído\n", val);
    }
    ~Widget() { fmt::print("  [Widget({})] destruído\n", val); }
    void processar() const { fmt::print("  Widget({}) processando...\n", val); }
};

// Função que apenas usa — aceita referência, sem envolver ownership
void apenas_usa(const Widget& w) {
    fmt::print("  apenas_usa(Widget({}))\n", w.val);
}

// Função que toma ownership exclusivo — o caller "doa" o objeto
void toma_ownership(std::unique_ptr<Widget> w) {
    fmt::print("  toma_ownership: tenho Widget({}), vou destruí-lo\n", w->val);
}

// Função que compartilha ownership — incrementa ref-count
void compartilha(std::shared_ptr<Widget> w) {
    fmt::print("  compartilha: use_count dentro da função = {}\n",
               w.use_count());
}

void demo_passagem_correta() {
    fmt::print("\n── 4.1 Regras de passagem ──\n");

    auto u = std::make_unique<Widget>(1);
    apenas_usa(*u);                     // derreferência — passa por ref
    toma_ownership(std::move(u));       // move — u fica nulo
    fmt::print("  u válido após doação? {}\n", u != nullptr);

    auto s = std::make_shared<Widget>(2);
    fmt::print("  s.use_count() antes: {}\n", s.use_count());
    compartilha(s);                     // cópia do shared_ptr
    fmt::print("  s.use_count() depois: {}\n", s.use_count());
}

// =============================================================================
//  2. FACTORY com polimorfismo — retorno por unique_ptr
// =============================================================================
struct Forma {
    virtual ~Forma() = default;
    virtual std::string descricao() const = 0;
    virtual double area() const = 0;
};

struct Circulo final : Forma {
    double raio;
    explicit Circulo(double r) : raio(r) {
        fmt::print("  [Circulo(r={:.1f})] construído\n", r);
    }
    ~Circulo() override { fmt::print("  [Circulo] destruído\n"); }
    std::string descricao() const override { return "Círculo"; }
    double area() const override { return 3.14159 * raio * raio; }
};

struct Retangulo final : Forma {
    double w, h;
    Retangulo(double ww, double hh) : w(ww), h(hh) {
        fmt::print("  [Retangulo({:.1f}x{:.1f})] construído\n", w, h);
    }
    ~Retangulo() override { fmt::print("  [Retangulo] destruído\n"); }
    std::string descricao() const override { return "Retângulo"; }
    double area() const override { return w * h; }
};

// Factory retorna unique_ptr — caller toma ownership
std::unique_ptr<Forma> criar_forma(const std::string& tipo) {
    if (tipo == "circulo")    return std::make_unique<Circulo>(5.0);
    if (tipo == "retangulo")  return std::make_unique<Retangulo>(4.0, 3.0);
    return nullptr;
}

void demo_factory() {
    fmt::print("\n── 4.2 Factory com unique_ptr ──\n");

    std::vector<std::unique_ptr<Forma>> formas;
    formas.push_back(criar_forma("circulo"));
    formas.push_back(criar_forma("retangulo"));

    for (const auto& f : formas) {
        fmt::print("  {} → área = {:.2f}\n", f->descricao(), f->area());
    }
    // formas destruído aqui → todos os objetos liberados via destruidores virtuais
}

// =============================================================================
//  3. PIMPL IDIOM — esconder implementação atrás de unique_ptr
//     Benefícios:
//       • Compilation firewall: mudar implementação sem recompilar dependentes
//       • ABI stability
//       • Encapsulamento real (membros privados não expostos no .hpp)
// =============================================================================

// Normalmente Impl ficaria em um .cpp — aqui colocamos junto para ser didático
class Conexao {
public:
    // Forward declaration da implementação (na prática, fica só no .hpp)
    struct Impl;

    explicit Conexao(std::string host, int porta);
    ~Conexao();  // precisa ser definido onde Impl é completo

    Conexao(const Conexao&)            = delete;
    Conexao& operator=(const Conexao&) = delete;
    Conexao(Conexao&&)                 = default;
    Conexao& operator=(Conexao&&)      = default;

    void conectar();
    void desconectar();
    bool esta_conectado() const;

private:
    std::unique_ptr<Impl> pimpl_;
};

// Definição de Impl — na prática estaria em conexao.cpp
struct Conexao::Impl {
    std::string host;
    int porta;
    bool conectado = false;

    Impl(std::string h, int p) : host(std::move(h)), porta(p) {
        fmt::print("  [Impl] criado para {}:{}\n", host, porta);
    }
    ~Impl() {
        fmt::print("  [Impl] destruído ({}:{})\n", host, porta);
    }
};

Conexao::Conexao(std::string host, int porta)
    : pimpl_(std::make_unique<Impl>(std::move(host), porta)) {}

Conexao::~Conexao() = default; // necessário aqui onde Impl é completo

void Conexao::conectar() {
    pimpl_->conectado = true;
    fmt::print("  Conectado a {}:{}\n", pimpl_->host, pimpl_->porta);
}

void Conexao::desconectar() {
    pimpl_->conectado = false;
    fmt::print("  Desconectado de {}:{}\n", pimpl_->host, pimpl_->porta);
}

bool Conexao::esta_conectado() const { return pimpl_->conectado; }

void demo_pimpl() {
    fmt::print("\n── 4.3 Pimpl idiom ──\n");

    Conexao conn("db.exemplo.com", 5432);
    conn.conectar();
    fmt::print("  Conectado? {}\n", conn.esta_conectado());
    conn.desconectar();
    fmt::print("  Conectado? {}\n", conn.esta_conectado());
    // Impl destruído automaticamente ao sair de escopo
}

// =============================================================================
//  4. RAII GENÉRICO com unique_ptr e deleter lambda
//     Útil para recursos de C APIs sem wrapper dedicado
// =============================================================================
void demo_raii_generico() {
    fmt::print("\n── 4.4 RAII genérico com unique_ptr + lambda ──\n");

    // Simula um handle opaco de uma biblioteca C
    struct HandleC {
        int id;
        explicit HandleC(int i) : id(i) {
            fmt::print("  [HandleC #{}] aberto\n", id);
        }
    };

    auto abrir_handle = [](int id) -> HandleC* { return new HandleC(id); };
    auto fechar_handle = [](HandleC* h) {
        fmt::print("  [HandleC #{}] fechado (via RAII)\n", h->id);
        delete h;
    };

    {
        std::unique_ptr<HandleC, decltype(fechar_handle)>
            h(abrir_handle(99), fechar_handle);
        fmt::print("  Usando handle #{}\n", h->id);
    } // fechar_handle chamado automaticamente
    fmt::print("  (bloco encerrado — handle liberado)\n");
}

// =============================================================================
//  5. Resumo visual das regras de escolha
// =============================================================================
void demo_resumo() {
    fmt::print("\n── 4.5 Guia de escolha ──\n");
    fmt::print(
        "  Pergunta                              → Use\n"
        "  ─────────────────────────────────────────────────────────────\n"
        "  Recurso tem um único dono?            → unique_ptr\n"
        "  Recurso é compartilhado?              → shared_ptr\n"
        "  Precisa observar sem influenciar vida → weak_ptr\n"
        "  Apenas usar sem ownership?            → referência (T& / T*)\n"
        "  Liberar recurso customizado (C API)?  → unique_ptr + deleter\n"
        "  Esconder implementação (pimpl)?       → unique_ptr<Impl>\n"
        "  Objeto precisa de shared_ptr de si?  → enable_shared_from_this\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada do demo
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_passagem_correta();
    demo_factory();
    demo_pimpl();
    demo_raii_generico();
    demo_resumo();
}

} // namespace demo_patterns
