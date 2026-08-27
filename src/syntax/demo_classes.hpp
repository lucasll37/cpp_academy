// =============================================================================
//  src/syntax/demo_classes.hpp
//
//  Classes — a anatomia completa
//  ─────────────────────────────
//  Uma classe reúne DADOS e as OPERAÇÕES que mantêm esses dados válidos. Tudo
//  o que ela pode conter cabe num esqueleto:
//
//      class Nome : public Base {          // herança
//      public:                             // rótulo de acesso
//          Nome();                         // construtor
//          Nome(int x);                    // outro construtor (sobrecarga)
//          ~Nome();                        // destrutor
//          int  metodo() const;            // método const → não modifica
//          static int contador();          // membro estático → da CLASSE
//          friend void espiar(const Nome&);// amigo → vê o privado
//      private:
//          int membro_ = 0;                // inicializador dentro da classe
//          inline static int total_ = 0;   // estático, definido aqui (C++17)
//      };
//
//  Tópicos:
//  • Rótulos de acesso: public / protected / private
//  • Construtores: padrão, parametrizado, delegante, `explicit`, conversor
//  • Lista de inicialização — e por que ela não é açúcar sintático
//  • Destrutor e RAII
//  • `= default` e `= delete`
//  • Membros `static`, `const`, `mutable`, e o ponteiro `this`
//  • Métodos const, sobrecarga por const, e ref-qualifiers
//  • `friend`
//  • Herança, `virtual`, `override`, `final`, classe abstrata
//  • A Regra do 0/3/5
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace demo_classes {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Anatomia — uma classe com tudo no lugar
// ─────────────────────────────────────────────────────────────────────────────
class ContaBancaria {
public:
    // ── Construtores ──────────────────────────────────────────────────────
    ContaBancaria() : ContaBancaria("sem-titular", 0.0) {}   // DELEGANTE (C++11)

    // `explicit` impede a conversão implícita `ContaBancaria c = "ana";`
    explicit ContaBancaria(std::string titular, double saldo_inicial = 0.0)
        : titular_(std::move(titular)),        // ← LISTA DE INICIALIZAÇÃO:
          saldo_(saldo_inicial),               //   constrói na ordem de DECLARAÇÃO
          numero_(++proximo_numero_)           //   dos membros, não desta lista
    {
        ++abertas_;
        // Aqui, no CORPO, os membros já existem: isto seria ATRIBUIÇÃO, não init.
    }

    // ── Destrutor ─────────────────────────────────────────────────────────
    ~ContaBancaria() { --abertas_; }

    // ── Métodos ───────────────────────────────────────────────────────────
    void depositar(double v) {
        if (v <= 0) return;                    // a invariante fica AQUI dentro
        saldo_ += v;
        ++operacoes_;                          // permitido: `operacoes_` é mutable
    }

    bool sacar(double v) {
        if (v <= 0 || v > saldo_) return false;
        saldo_ -= v;
        return true;
    }

    // `const` no fim = "este método não modifica o objeto". Só métodos const
    // podem ser chamados através de uma `const ContaBancaria&`.
    double      saldo()   const { return saldo_; }
    const auto& titular() const { return titular_; }
    int         numero()  const { return numero_; }

    // `mutable` deixa um membro mudar mesmo dentro de método const —
    // legítimo para caches, contadores e mutexes; nunca para estado lógico.
    int consultas() const { ++operacoes_; return operacoes_; }

    // ── Membros estáticos: pertencem à CLASSE, não ao objeto ──────────────
    static int  abertas()          { return abertas_; }
    static void reiniciar_numeros() { proximo_numero_ = 0; }

private:
    std::string titular_;
    double      saldo_  = 0.0;                 // inicializador dentro da classe
    int         numero_ = 0;
    mutable int operacoes_ = 0;                // mutável mesmo em método const

    // `inline static` (C++17): estático definido aqui, sem linha na .cpp
    inline static int proximo_numero_ = 0;
    inline static int abertas_        = 0;

    // ── friend: acesso ao privado, concedido explicitamente ───────────────
    friend void auditar(const ContaBancaria& c);
};

void auditar(const ContaBancaria& c) {
    // `auditar` não é membro, mas vê `saldo_` e `titular_` por ser friend
    fmt::print("        [auditoria] conta #{} de {} → saldo {:.2f}\n",
               c.numero_, c.titular_, c.saldo_);
}

void demo_anatomia() {
    fmt::print("\n── 5.1 Anatomia de uma classe ──\n");

    ContaBancaria::reiniciar_numeros();
    {
        ContaBancaria a{"ana", 100.0};
        ContaBancaria b{"bruno"};
        ContaBancaria padrao;                  // usa o construtor delegante

        a.depositar(50.0);
        bool ok = a.sacar(30.0);

        fmt::print("      construtor explícito: ContaBancaria a{{\"ana\", 100.0}}\n");
        fmt::print("        após depositar(50) e sacar(30) (ok={}) → saldo {:.2f}\n",
                   ok, a.saldo());
        fmt::print("      argumento padrão:     ContaBancaria b{{\"bruno\"}} → saldo {:.2f}\n",
                   b.saldo());
        fmt::print("      delegante:            ContaBancaria padrao;  → titular \"{}\"\n",
                   padrao.titular());

        fmt::print("\n      Membros estáticos — um só para todos os objetos:\n");
        fmt::print("        contas abertas agora: {}\n", ContaBancaria::abertas());
        fmt::print("        números atribuídos: a=#{} b=#{} padrao=#{}\n",
                   a.numero(), b.numero(), padrao.numero());

        fmt::print("\n      friend:\n");
        auditar(a);

        fmt::print("\n      mutable — muda dentro de método const:\n");
        const ContaBancaria& somente_leitura = a;
        const int primeira = somente_leitura.consultas();
        const int segunda  = somente_leitura.consultas();
        fmt::print("        consultas(): {} → {} (o objeto é const!)\n", primeira, segunda);
    }
    fmt::print("\n      Ao sair do bloco os destrutores rodaram: abertas = {}\n",
               ContaBancaria::abertas());
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. A lista de inicialização não é açúcar sintático
// ─────────────────────────────────────────────────────────────────────────────
struct Rastreado {
    std::string nome;
    explicit Rastreado(std::string n) : nome(std::move(n)) {
        fmt::print("          ctor({})\n", nome);
    }
    Rastreado(const Rastreado& o) : nome(o.nome) {
        fmt::print("          CÓPIA de {}\n", nome);
    }
    Rastreado& operator=(const Rastreado& o) {
        nome = o.nome;
        fmt::print("          ATRIBUIÇÃO de {}\n", nome);
        return *this;
    }
    ~Rastreado() = default;
};

struct UsaLista {
    Rastreado membro;
    explicit UsaLista(const Rastreado& r) : membro(r) {}     // constrói UMA vez
};

struct UsaCorpo {
    Rastreado membro;
    explicit UsaCorpo(const Rastreado& r) : membro("vazio") { membro = r; }  // 2 passos
};

void demo_lista_init() {
    fmt::print("\n── 5.2 Lista de inicialização ──\n");

    Rastreado fonte{"fonte"};

    fmt::print("\n      Com lista `: membro(r)`:\n");
    { UsaLista u{fonte}; (void)u; }

    fmt::print("\n      No corpo `{{ membro = r; }}`:\n");
    { UsaCorpo u{fonte}; (void)u; }

    fmt::print("\n      → no corpo, o membro JÁ FOI construído: você paga\n");
    fmt::print("        construção + atribuição. E membros const ou referência\n");
    fmt::print("        nem sequer PODEM ser atribuídos — só inicializados.\n");

    fmt::print("\n      ⚠ A ordem de construção é a de DECLARAÇÃO dos membros,\n");
    fmt::print("        não a ordem em que você escreveu a lista. Escrever\n");
    fmt::print("        `: b_(1), a_(b_)` lê `b_` antes de existir → UB.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. `= default` e `= delete`
// ─────────────────────────────────────────────────────────────────────────────
class NaoCopiavel {
public:
    NaoCopiavel() = default;                                 // "gere o padrão"
    NaoCopiavel(const NaoCopiavel&)            = delete;     // "proíba isto"
    NaoCopiavel& operator=(const NaoCopiavel&) = delete;
    NaoCopiavel(NaoCopiavel&&)                 = default;    // mas mover, sim
    NaoCopiavel& operator=(NaoCopiavel&&)      = default;
    int valor = 7;
};

// `= delete` também bloqueia uma SOBRECARGA específica
void so_aceita_int(int x) { fmt::print("        so_aceita_int({})\n", x); }
void so_aceita_int(double) = delete;

void demo_default_delete() {
    fmt::print("\n── 5.3 = default e = delete ──\n");

    NaoCopiavel a;
    // NaoCopiavel b = a;             // ✗ erro: construtor de cópia deletado
    NaoCopiavel b = std::move(a);     // ✓ mover continua permitido

    fmt::print("      `= default` → gere a implementação padrão (e continue trivial)\n");
    fmt::print("      `= delete`  → torne este uso um ERRO DE COMPILAÇÃO\n");
    fmt::print("        NaoCopiavel b = a;            ✗ não compila\n");
    fmt::print("        NaoCopiavel b = std::move(a); ✓ valor = {}\n", b.valor);

    fmt::print("\n      Deletar sobrecargas para barrar conversões indesejadas:\n");
    fmt::print("        void so_aceita_int(int);\n");
    fmt::print("        void so_aceita_int(double) = delete;\n");
    so_aceita_int(42);
    fmt::print("        so_aceita_int(3.14) → não compila (antes, converteria em silêncio)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Métodos const, sobrecarga por const e ref-qualifiers
// ─────────────────────────────────────────────────────────────────────────────
class Buffer {
public:
    explicit Buffer(std::size_t n) : dados_(n, 0) {}

    // Sobrecarga por const: o compilador escolhe pela constância do objeto
    int&       operator[](std::size_t i)       { return dados_[i]; }
    const int& operator[](std::size_t i) const { return dados_[i]; }

    // Ref-qualifiers: `&` só em lvalue, `&&` só em rvalue (temporário)
    const std::vector<int>& vetor() const&  { return dados_; }             // objeto nomeado
    std::vector<int>        vetor() &&      { return std::move(dados_); }  // temporário: rouba

    std::size_t tamanho() const { return dados_.size(); }

private:
    std::vector<int> dados_;
};

void demo_const_metodos() {
    fmt::print("\n── 5.4 Métodos const e ref-qualifiers ──\n");

    Buffer b{4};
    b[0] = 10;                                   // usa a versão NÃO-const
    b[1] = 20;

    const Buffer& cb = b;
    fmt::print("      Buffer b{{4}}; b[0]=10;\n");
    fmt::print("        b[0]  (não-const) → devolve int&,       valor {}\n", b[0]);
    fmt::print("        cb[0] (const)     → devolve const int&, valor {}\n", cb[0]);
    fmt::print("        → o MESMO nome, duas implementações, escolhidas pela constância\n");

    fmt::print("\n      Ref-qualifiers:\n");
    fmt::print("        b.vetor()          → `const&`: devolve referência (sem cópia), size={}\n",
               b.vetor().size());
    auto roubado = Buffer{8}.vetor();          // temporário → chama a versão &&
    fmt::print("        Buffer{{8}}.vetor()  → `&&`: MOVE o interior, size={}\n",
               roubado.size());
    fmt::print("        → o temporário vai morrer de qualquer forma; roubar é seguro\n");

    fmt::print("\n      `const` no método é uma promessa ao COMPILADOR e ao leitor:\n");
    fmt::print("        sem ele, nem dá para chamar através de `const Buffer&`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Herança, virtual, override, final, abstrata
// ─────────────────────────────────────────────────────────────────────────────

// Classe ABSTRATA: tem ao menos um método virtual PURO (`= 0`) → não instanciável
class Forma {
public:
    virtual ~Forma() = default;                  // destrutor virtual: OBRIGATÓRIO
    virtual double area() const = 0;             // puro: derivadas TÊM de implementar
    virtual std::string nome() const { return "forma"; }   // virtual com padrão

    void descrever() const {                     // NÃO virtual: comum a todas
        fmt::print("        {} → área {:.2f}\n", nome(), area());
    }
};

class Circulo : public Forma {
public:
    explicit Circulo(double r) : raio_(r) {}
    double      area() const override { return 3.14159265 * raio_ * raio_; }
    std::string nome() const override { return "círculo"; }
private:
    double raio_;
};

class Quadrado final : public Forma {            // `final`: ninguém herda de Quadrado
public:
    explicit Quadrado(double l) : lado_(l) {}
    double      area() const override final { return lado_ * lado_; }
    std::string nome() const override { return "quadrado"; }
private:
    double lado_;
};

void demo_heranca() {
    fmt::print("\n── 5.5 Herança e polimorfismo ──\n");

    std::vector<std::unique_ptr<Forma>> formas;
    formas.push_back(std::make_unique<Circulo>(2.0));
    formas.push_back(std::make_unique<Quadrado>(3.0));

    fmt::print("      Despacho dinâmico através de Forma*:\n");
    for (const auto& f : formas) f->descrever();

    fmt::print("\n      | palavra   | significado                                     |\n");
    fmt::print("      |-----------|-------------------------------------------------|\n");
    fmt::print("      | virtual   | pode ser substituída; despacho pelo tipo REAL    |\n");
    fmt::print("      | = 0       | pura: a classe vira abstrata, não instanciável   |\n");
    fmt::print("      | override  | \"estou substituindo\" — o compilador CONFERE      |\n");
    fmt::print("      | final     | ninguém mais substitui (método) / herda (classe) |\n");

    fmt::print("\n      Os três tipos de herança:\n");
    fmt::print("        class D : public B     → \"D É-UM B\"    (o normal)\n");
    fmt::print("        class D : protected B  → só derivadas veem a base\n");
    fmt::print("        class D : private B    → \"D É-IMPLEMENTADO-EM-TERMOS-DE B\"\n");
    fmt::print("        ⚠ omitir a palavra em `class` dá PRIVATE; em `struct`, PUBLIC\n");

    fmt::print("\n      ⚠ Base polimórfica SEM destrutor virtual + `delete` por ponteiro\n");
    fmt::print("        da base = UB. Regra: virtual público, ou não-virtual protegido.\n");
    fmt::print("      vtable, slicing, herança múltipla e RTTI → src/oop.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. A Regra do 0 / 3 / 5
// ─────────────────────────────────────────────────────────────────────────────
class RegraDoZero {                 // sem recurso cru → NÃO escreva nada
public:
    explicit RegraDoZero(std::string n) : nome_(std::move(n)) {}
private:
    std::string              nome_;      // cada membro já sabe copiar/mover/liberar
    std::vector<int>         dados_;
    std::shared_ptr<int>     recurso_;
};

void demo_regra_dos_cinco() {
    fmt::print("\n── 5.6 A Regra do 0 / 3 / 5 ──\n");

    fmt::print("      As cinco operações especiais que o compilador pode gerar:\n");
    fmt::print("        1. destrutor              ~T()\n");
    fmt::print("        2. construtor de cópia     T(const T&)\n");
    fmt::print("        3. atribuição de cópia     T& operator=(const T&)\n");
    fmt::print("        4. construtor de move      T(T&&)\n");
    fmt::print("        5. atribuição de move      T& operator=(T&&)\n");

    fmt::print("\n      Regra dos 3 (C++98) → precisou de uma, precisa das três\n");
    fmt::print("      Regra dos 5 (C++11) → some o move; declarar UMA suprime as outras\n");
    fmt::print("      Regra do 0  (a boa) → não declare NENHUMA: componha com tipos\n");
    fmt::print("                            que já se gerenciam (string, vector, unique_ptr)\n");

    RegraDoZero r{"exemplo"};
    fmt::print("\n        class RegraDoZero {{ std::string; std::vector<int>; shared_ptr<int>; }};\n");
    fmt::print("        → copiável, movível e sem vazamentos, com ZERO linhas escritas\n");
    fmt::print("        (troque por unique_ptr e ela vira move-only — também de graça)\n");
    fmt::print("        sizeof = {} bytes\n", sizeof(r));
    fmt::print("      Detalhes e medições → src/value_semantics.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_anatomia();
    demo_lista_init();
    demo_default_delete();
    demo_const_metodos();
    demo_heranca();
    demo_regra_dos_cinco();
}

} // namespace demo_classes
