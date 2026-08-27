// =============================================================================
//  src/oop/demo_virtual.hpp
//
//  Funções virtuais — vtable, vptr e o custo real do polimorfismo
//  ──────────────────────────────────────────────────────────────
//  `virtual` troca uma decisão de compilação por uma de execução: em vez de o
//  compilador saber qual função chamar, o próprio OBJETO carrega a resposta.
//
//  O mecanismo (não está no padrão, mas é o que todo compilador faz):
//    • cada classe polimórfica ganha uma vtable — um array de ponteiros de função
//    • cada OBJETO ganha um vptr — um ponteiro para a vtable da sua classe real
//    • `p->f()` vira `(*p->vptr[índice_de_f])(p)`
//
//  Tópicos:
//  • Despacho estático vs dinâmico
//  • O vptr, medido com sizeof
//  • `override` e `final` — e o bug que `override` previne
//  • Destrutor virtual: o vazamento que ele evita
//  • Chamar virtual dentro do construtor
//  • Argumentos padrão em funções virtuais
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <string>
#include <vector>

namespace demo_virtual {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Despacho estático vs dinâmico
// ─────────────────────────────────────────────────────────────────────────────
struct Forma {
    void          nome_estatico() const { fmt::print("      Forma (estático)\n"); }
    virtual void  nome_dinamico() const { fmt::print("      Forma (dinâmico)\n"); }
    virtual ~Forma() = default;
};

struct Circulo : Forma {
    void nome_estatico() const { fmt::print("      Círculo (estático)\n"); }
    void nome_dinamico() const override { fmt::print("      Círculo (dinâmico)\n"); }
};

void demo_despacho() {
    fmt::print("\n── 2.1 Despacho estático vs dinâmico ──\n");

    Circulo c;
    Forma&  ref = c;                // referência de base para objeto derivado

    fmt::print("    via Forma& (o objeto REAL é um Círculo):\n");
    ref.nome_estatico();            // decidido pelo TIPO DA REFERÊNCIA → Forma
    ref.nome_dinamico();            // decidido pelo TIPO DO OBJETO     → Círculo

    fmt::print("      não-virtual → o compilador resolve pelo tipo estático\n");
    fmt::print("      virtual     → o objeto resolve em runtime, pelo vptr\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O vptr é visível no sizeof
// ─────────────────────────────────────────────────────────────────────────────
struct SemVirtual { int a; int b; };
struct ComVirtual { int a; int b; virtual ~ComVirtual() = default; };

void demo_vptr() {
    fmt::print("\n── 2.2 O custo em memória: o vptr ──\n");

    fmt::print("      sizeof(int)        = {} bytes\n", sizeof(int));
    fmt::print("      sizeof(SemVirtual) = {} bytes (2 ints)\n", sizeof(SemVirtual));
    fmt::print("      sizeof(ComVirtual) = {} bytes (vptr 8 + int 4 + int 4 — sem padding)\n",
               sizeof(ComVirtual));
    fmt::print("      diferença          = {} bytes = 1 ponteiro\n",
               sizeof(ComVirtual) - sizeof(SemVirtual));

    // Dois objetos da MESMA classe compartilham a vtable — o vptr deles é igual.
    Circulo c1, c2;
    Forma   f;
    const void* vptr_c1 = *reinterpret_cast<void* const*>(&c1);
    const void* vptr_c2 = *reinterpret_cast<void* const*>(&c2);
    const void* vptr_f  = *reinterpret_cast<void* const*>(&f);

    fmt::print("      vptr de dois Círculos são iguais? {}   ← uma vtable POR CLASSE\n",
               vptr_c1 == vptr_c2);
    fmt::print("      vptr de Círculo == vptr de Forma?  {}  ← o vptr de cada objeto\n",
               vptr_c1 == vptr_f);
    fmt::print("                                                aponta para a vtable da classe REAL\n");
    fmt::print("      → a vtable é POR CLASSE; o vptr é POR OBJETO\n");
    fmt::print("      ⚠ ler o vptr com *reinterpret_cast<void* const*>(&obj) é UB formal:\n");
    fmt::print("        acessa a representação do objeto por um tipo incompatível\n");
    fmt::print("        (strict aliasing — ver src/undefined_behavior/). Só funciona\n");
    fmt::print("        porque o Itanium ABI põe o vptr no offset 0; não é portável.\n");

    fmt::print("      Custo total: 1 ponteiro por objeto + 1 indireção por chamada\n");
    fmt::print("      (e a chamada não pode ser inlined — costuma ser o custo maior)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. `override` — a palavra que previne o bug mais chato da herança
//
//     Sem `override`, uma assinatura ligeiramente diferente cria uma função
//     NOVA em vez de sobrescrever. Compila, roda, e a chamada polimórfica
//     continua indo para a base. Silenciosamente.
// ─────────────────────────────────────────────────────────────────────────────
// O GCC 13 detecta o caso do SensorQuebrado com -Woverloaded-virtual (que vem
// em -Wall) e aponta para a declaração da BASE — por isso o `push` precisa vir
// antes de Sensor. Silenciamos porque o erro é o objeto da demonstração.
// Note que o compilador só consegue avisar porque o nome COLIDE; se o método
// derivado se chamasse `ler_canal`, nada seria dito.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"

struct Sensor {
    virtual double ler(int /*canal*/) const { fmt::print("      Sensor::ler(int)\n"); return 0.0; }
    virtual ~Sensor() = default;
};

struct SensorQuebrado : Sensor {
    // Repare no `double` em vez de `int`: assinatura diferente.
    // Isto NÃO sobrescreve nada — cria um novo overload.
    double ler(double canal) const { fmt::print("      SensorQuebrado::ler(double)\n");
                                     return canal; }
};
#pragma GCC diagnostic pop

struct SensorCerto : Sensor {
    // `override` faz o compilador VERIFICAR que existe um virtual com esta
    // assinatura na base. Se não existir, é erro de compilação.
    double ler(int canal) const override { fmt::print("      SensorCerto::ler(int)\n");
                                           return canal; }
    // double ler(double) const override; // ✗ erro: nada para sobrescrever
};

void demo_override() {
    fmt::print("\n── 2.3 `override` — o bug silencioso ──\n");

    SensorQuebrado q;
    SensorCerto    ok;

    fmt::print("    chamando via Sensor& em SensorQuebrado:\n");
    static_cast<Sensor&>(q).ler(1);      // vai para Sensor::ler — o bug
    fmt::print("      ↑ foi para a BASE: a assinatura não bateu\n");

    fmt::print("    chamando via Sensor& em SensorCerto:\n");
    static_cast<Sensor&>(ok).ler(1);     // vai para SensorCerto::ler
    fmt::print("      ↑ sobrescreveu de verdade\n");

    fmt::print("      Marque SEMPRE `override`. Custo zero, pega o erro na compilação.\n");
}

// `final` impede que uma classe seja herdada ou um virtual seja sobrescrito.
// Além da intenção documentada, permite ao compilador devirtualizar chamadas.
struct Imutavel final : Sensor {
    double ler(int) const override { return 42.0; }
};
// struct Tenta : Imutavel {};          // ✗ erro: Imutavel é final

// ─────────────────────────────────────────────────────────────────────────────
//  4. Destrutor virtual — o vazamento que ele evita
//
//     `delete ptr_base` com destrutor NÃO-virtual é undefined behavior:
//     o destrutor do derivado nunca roda, e os recursos dele vazam.
// ─────────────────────────────────────────────────────────────────────────────
struct BaseSemDtorVirtual {
    ~BaseSemDtorVirtual() { fmt::print("      - BaseSemDtorVirtual::~\n"); }
};

struct DerivadaComRecurso : BaseSemDtorVirtual {
    std::vector<int> recurso = std::vector<int>(1000);
    ~DerivadaComRecurso() { fmt::print("      - DerivadaComRecurso::~ (libera 1000 ints)\n"); }
};

struct BaseComDtorVirtual {
    virtual ~BaseComDtorVirtual() { fmt::print("      - BaseComDtorVirtual::~\n"); }
};

struct DerivadaSegura : BaseComDtorVirtual {
    std::vector<int> recurso = std::vector<int>(1000);
    ~DerivadaSegura() override { fmt::print("      - DerivadaSegura::~ (libera 1000 ints)\n"); }
};

void demo_dtor_virtual() {
    fmt::print("\n── 2.4 Destrutor virtual ──\n");

    fmt::print("    COM destrutor virtual, deletando pela base:\n");
    {
        BaseComDtorVirtual* p = new DerivadaSegura;
        delete p;                    // ✓ chama os DOIS destrutores
    }

    fmt::print("\n    SEM destrutor virtual, deletando pelo tipo correto:\n");
    {
        DerivadaComRecurso* p = new DerivadaComRecurso;
        delete p;                    // ✓ ok: o tipo estático já é o certo
    }

    fmt::print("\n    SEM destrutor virtual, deletando pela BASE:\n");
    fmt::print("      BaseSemDtorVirtual* p = new DerivadaComRecurso;\n");
    fmt::print("      delete p;   // ← undefined behavior\n");
    fmt::print("      Esta linha está COMENTADA no código de propósito: é UB,\n");
    fmt::print("      o GCC recusa com -Wdelete-non-virtual-dtor (vem em -Wall)\n");
    fmt::print("      e o AddressSanitizer aborta o programa.\n");
    // BaseSemDtorVirtual* p = new DerivadaComRecurso;
    // delete p;                     // ← NUNCA

    fmt::print("\n      Regra: se a classe tem QUALQUER método virtual, o destrutor\n");
    fmt::print("      também precisa ser virtual. Se ela não é feita para herança,\n");
    fmt::print("      marque a classe `final`.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Virtual dentro do construtor / destrutor
//
//     Durante o construtor da Base, o objeto AINDA É uma Base — o vptr aponta
//     para a vtable da Base. A chamada virtual não desce para o derivado.
//     Faz sentido: os membros do derivado ainda não foram inicializados.
// ─────────────────────────────────────────────────────────────────────────────
struct BaseCtor {
    BaseCtor() {
        fmt::print("      BaseCtor::BaseCtor() chamando quem_sou():\n        ");
        quem_sou();                 // NÃO chega em DerivadaCtor
    }
    virtual void quem_sou() const { fmt::print("BaseCtor\n"); }
    virtual ~BaseCtor() = default;
};

struct DerivadaCtor : BaseCtor {
    std::string nome = "derivada-inicializada";
    void quem_sou() const override { fmt::print("DerivadaCtor (nome='{}')\n", nome); }
};

void demo_virtual_no_ctor() {
    fmt::print("\n── 2.5 Chamada virtual dentro do construtor ──\n");

    DerivadaCtor d;
    fmt::print("      objeto pronto, chamando quem_sou() de novo:\n        ");
    d.quem_sou();

    fmt::print("      Durante o ctor da base, o vptr ainda é o da base.\n");
    fmt::print("      Se despachasse para o derivado, `nome` seria lido antes\n");
    fmt::print("      de existir. A linguagem escolheu a opção segura.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Argumentos padrão em virtuais — nunca faça isto
//
//     O corpo é escolhido DINAMICAMENTE (pelo objeto), mas o argumento padrão
//     é escolhido ESTATICAMENTE (pelo tipo do ponteiro). Os dois discordam.
// ─────────────────────────────────────────────────────────────────────────────
struct BaseArg {
    virtual void f(int x = 10) const { fmt::print("      BaseArg::f(x={})\n", x); }
    virtual ~BaseArg() = default;
};

struct DerivadaArg : BaseArg {
    void f(int x = 20) const override { fmt::print("      DerivadaArg::f(x={})\n", x); }
};

void demo_arg_padrao() {
    fmt::print("\n── 2.6 ✗ Argumento padrão em função virtual ──\n");

    DerivadaArg d;
    BaseArg&    b = d;

    fmt::print("    d.f();   // tipo estático = DerivadaArg\n");
    d.f();
    fmt::print("    b.f();   // tipo estático = BaseArg, objeto = DerivadaArg\n");
    b.f();
    fmt::print("      ↑ corpo do DERIVADO com o default da BASE — ninguém espera isso\n");
    fmt::print("      Solução: não use argumento padrão em virtual. Se precisar,\n");
    fmt::print("      exponha um método NÃO-virtual público que aplica o default e\n");
    fmt::print("      delega para um virtual privado (idioma NVI).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Classe abstrata — interface pura
// ─────────────────────────────────────────────────────────────────────────────
struct Serializavel {
    virtual std::string serializar() const = 0;   // = 0 → puramente virtual
    virtual ~Serializavel() = default;
};

struct Ponto : Serializavel {
    int x = 1, y = 2;
    std::string serializar() const override { return fmt::format("{{x:{},y:{}}}", x, y); }
};

void demo_abstrata() {
    fmt::print("\n── 2.7 Classe abstrata ──\n");

    // Serializavel s;              // ✗ erro: não se instancia classe abstrata

    std::vector<std::unique_ptr<Serializavel>> itens;
    itens.push_back(std::make_unique<Ponto>());

    for (const auto& i : itens) fmt::print("      {}\n", i->serializar());

    fmt::print("      `= 0` obriga o derivado a implementar.\n");
    fmt::print("      Uma classe abstrata AINDA pode ter dados e implementação —\n");
    fmt::print("      até um virtual puro pode ter corpo, chamável via Base::f().\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_despacho();
    demo_vptr();
    demo_override();
    demo_dtor_virtual();
    demo_virtual_no_ctor();
    demo_arg_padrao();
    demo_abstrata();
}

} // namespace demo_virtual
