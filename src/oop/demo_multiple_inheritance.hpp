// =============================================================================
//  src/oop/demo_multiple_inheritance.hpp
//
//  Herança múltipla, o diamante e a herança virtual
//  ────────────────────────────────────────────────
//  C++ é uma das poucas linguagens de grande porte que permite herdar de mais
//  de uma classe. Isso resolve casos reais (misturar interfaces, mixins), mas
//  cria um problema que nenhuma linguagem de herança simples tem: o DIAMANTE.
//
//      A          Se B e C herdam de A, e D herda de B e C,
//     / \         quantas cópias de A existem dentro de um D?
//    B   C        Sem `virtual`: DUAS. Com `virtual`: uma.
//     \ /
//      D
//
//  Tópicos:
//  • Herança múltipla de interfaces (o caso seguro)
//  • O diamante e a ambiguidade
//  • `virtual` na herança — uma única subobjeto-base
//  • O custo: layout, ponteiros ajustados, quem chama o ctor da base virtual
//  • Mixins com CRTP como alternativa
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <utility>

namespace demo_multiple_inheritance {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O caso seguro: herdar várias INTERFACES
//
//     Interfaces puras (só virtuais puros, sem estado) não têm o problema do
//     diamante, porque não há dados a duplicar. É assim que Java/C# fazem, e
//     é 90% dos usos legítimos de herança múltipla em C++.
// ─────────────────────────────────────────────────────────────────────────────
struct Desenhavel {
    virtual void desenhar() const = 0;
    virtual ~Desenhavel() = default;
};

struct Salvavel {
    virtual std::string salvar() const = 0;
    virtual ~Salvavel() = default;
};

struct Botao : Desenhavel, Salvavel {
    std::string rotulo = "OK";

    void desenhar() const override { fmt::print("      [ {} ]\n", rotulo); }
    std::string salvar() const override { return fmt::format("botao:{}", rotulo); }
};

void demo_interfaces() {
    fmt::print("\n── 4.1 Herança múltipla de interfaces ──\n");

    Botao b;

    Desenhavel& d = b;              // vê só a face "desenhável"
    Salvavel&   s = b;              // vê só a face "salvável"

    d.desenhar();
    fmt::print("      salvo como: {}\n", s.salvar());

    // Os dois subobjetos ficam em OFFSETS diferentes dentro de Botao.
    // Converter o ponteiro AJUSTA o endereço — não é reinterpretação.
    fmt::print("      &b            = {}\n", static_cast<const void*>(&b));
    fmt::print("      (Desenhavel*) = {}\n", static_cast<const void*>(&d));
    fmt::print("      (Salvavel*)   = {}\n", static_cast<const void*>(&s));
    fmt::print("      ↑ endereços diferentes para o MESMO objeto\n");
    fmt::print("      É por isso que comparar ponteiros de bases diferentes\n");
    fmt::print("      não diz se são o mesmo objeto — use dynamic_cast<void*>.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. O diamante SEM herança virtual — duas cópias da base
// ─────────────────────────────────────────────────────────────────────────────
namespace sem_virtual {

struct Dispositivo {
    std::string id = "dev-0";
    int         consumo = 10;

    Dispositivo() { fmt::print("        + Dispositivo()\n"); }
    virtual ~Dispositivo() = default;
};

struct Scanner  : Dispositivo { };
struct Impressor: Dispositivo { };

// Multifuncional contém DOIS Dispositivos completos
struct Multifuncional : Scanner, Impressor { };

} // namespace sem_virtual

void demo_diamante_duplicado() {
    fmt::print("\n── 4.2 O diamante sem `virtual` — base duplicada ──\n");

    fmt::print("      construindo Multifuncional:\n");
    sem_virtual::Multifuncional mf;

    fmt::print("      ↑ o construtor de Dispositivo rodou DUAS vezes\n");

    // mf.id;                       // ✗ erro: ambíguo — qual dos dois `id`?
    // O acesso exige qualificar o caminho:
    mf.Scanner::id   = "via-scanner";
    mf.Impressor::id = "via-impressor";

    fmt::print("      mf.Scanner::id   = '{}'\n", mf.Scanner::id);
    fmt::print("      mf.Impressor::id = '{}'\n", mf.Impressor::id);
    fmt::print("      ↑ dois estados independentes para o que deveria ser um só\n");

    fmt::print("      consumo total contado em dobro: {} + {} = {}\n",
               mf.Scanner::consumo, mf.Impressor::consumo,
               mf.Scanner::consumo + mf.Impressor::consumo);

    // Dispositivo* p = &mf;        // ✗ erro: conversão ambígua
    fmt::print("      `Dispositivo* p = &mf;` nem compila — conversão ambígua\n");
    fmt::print("      sizeof(Multifuncional) = {} bytes\n",
               sizeof(sem_virtual::Multifuncional));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. O diamante COM herança virtual — uma única base
//
//     `virtual` na lista de herança diz: "compartilhe este subobjeto com
//     quem mais herdar virtualmente dele".
// ─────────────────────────────────────────────────────────────────────────────
namespace com_virtual {

struct Dispositivo {
    std::string id      = "dev-0";
    int         consumo = 10;

    Dispositivo() { fmt::print("        + Dispositivo()\n"); }
    explicit Dispositivo(std::string i) : id(std::move(i)) {
        fmt::print("        + Dispositivo('{}')\n", id);
    }
    virtual ~Dispositivo() = default;
};

struct Scanner : virtual Dispositivo {
    Scanner() : Dispositivo("via-scanner") {
        fmt::print("        + Scanner()\n");
    }
};

struct Impressor : virtual Dispositivo {
    Impressor() : Dispositivo("via-impressor") {
        fmt::print("        + Impressor()\n");
    }
};

// Regra que surpreende: quem inicializa a base VIRTUAL é a classe MAIS
// DERIVADA (aqui, Multifuncional) — não Scanner nem Impressor. As chamadas
// a Dispositivo(...) dentro de Scanner e Impressor são IGNORADAS.
struct Multifuncional : Scanner, Impressor {
    Multifuncional() : Dispositivo("mfp-central") {
        fmt::print("        + Multifuncional()\n");
    }
};

} // namespace com_virtual

void demo_diamante_virtual() {
    fmt::print("\n── 4.3 O diamante com `virtual` — base única ──\n");

    fmt::print("      construindo Multifuncional:\n");
    com_virtual::Multifuncional mf;

    fmt::print("      ↑ Dispositivo() rodou UMA vez só\n");

    mf.id = "mfp-01";               // ✓ sem ambiguidade: só existe um `id`
    fmt::print("      mf.id                = '{}'\n", mf.id);
    fmt::print("      mf.Scanner::id       = '{}'  ← o mesmo objeto\n", mf.Scanner::id);
    fmt::print("      mf.Impressor::id     = '{}'  ← o mesmo objeto\n", mf.Impressor::id);

    com_virtual::Dispositivo* p = &mf;   // ✓ conversão agora é inequívoca
    fmt::print("      consumo (único): {}\n", p->consumo);

    fmt::print("\n      Quem inicializou a base virtual? A classe MAIS DERIVADA.\n");
    fmt::print("      Os `Dispositivo(\"via-scanner\")` de Scanner e Impressor\n");
    fmt::print("      foram ignorados — repare que o id inicial foi 'mfp-central'.\n");

    fmt::print("      sizeof com virtual = {} bytes (vs {} sem)\n",
               sizeof(com_virtual::Multifuncional),
               sizeof(sem_virtual::Multifuncional));
    fmt::print("      A base virtual exige um ponteiro extra (vbptr) para localizar\n");
    fmt::print("      o subobjeto compartilhado em runtime.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Mixins com CRTP — herança múltipla sem custo de runtime
//
//     Se a variação é conhecida em COMPILAÇÃO, não é preciso vtable nenhuma.
//     Cada mixin recebe o tipo derivado como parâmetro de template.
// ─────────────────────────────────────────────────────────────────────────────
template <typename Derivada>
struct Contavel {
    void incrementar() { static_cast<Derivada*>(this)->contador += 1; }
};

template <typename Derivada>
struct Imprimivel {
    void imprimir() const {
        const auto& self = *static_cast<const Derivada*>(this);
        fmt::print("      {} (contador={})\n", self.nome, self.contador);
    }
};

// Herda de dois mixins. Zero vtable, zero vptr, tudo resolvido em compilação.
struct Evento : Contavel<Evento>, Imprimivel<Evento> {
    std::string nome    = "clique";
    int         contador = 0;
};

void demo_crtp_mixins() {
    fmt::print("\n── 4.4 Mixins com CRTP (herança múltipla estática) ──\n");

    Evento e;
    e.incrementar();
    e.incrementar();
    e.imprimir();

    fmt::print("      sizeof(Evento) = {} bytes — sem vptr\n", sizeof(Evento));
    fmt::print("      Os mixins são bases VAZIAS: o compilador aplica EBO\n");
    fmt::print("      (Empty Base Optimization) e elas não ocupam nada.\n");
    fmt::print("      Ver também: src/templates/demo_crtp.hpp\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_interfaces();
    demo_diamante_duplicado();
    demo_diamante_virtual();
    demo_crtp_mixins();
}

} // namespace demo_multiple_inheritance
