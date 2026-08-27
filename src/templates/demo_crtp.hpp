// =============================================================================
//  src/templates/demo_crtp.hpp
//
//  CRTP — Curiously Recurring Template Pattern
//  ─────────────────────────────────────────────
//  Padrão onde uma classe Derivada passa a si mesma como parâmetro
//  de template para sua Base:
//
//      class Derivada : public Base<Derivada> { ... };
//
//  Isso permite que a Base chame métodos da Derivada em compile-time,
//  sem virtual dispatch — polimorfismo estático (zero custo em runtime).
//
//  Usos reais:
//  • Eigen: MatrixBase<Derived> — todas as operações matriciais
//  • SFML: sf::Transformable<Derived>
//  • Boost.Operators: gera operadores a partir de poucos primitivos
//  • Mixin pattern: injetar funcionalidade sem herança profunda
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <chrono>
#include <cmath>

namespace demo_crtp {

// ─────────────────────────────────────────────────────────────────────────────
//  1. CRTP básico — polimorfismo estático sem virtual
//     Virtual dispatch: ponteiro para vtable consultado em runtime.
//     CRTP: tipo completo conhecido em compile-time → chamada direta inlined.
// ─────────────────────────────────────────────────────────────────────────────

// Base CRTP — Derived é o tipo concreto
template <typename Derived>
struct FormaBase {
    // A base chama métodos da derivada via static_cast — zero overhead
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }

    double perimetro() const {
        return static_cast<const Derived*>(this)->perimetro_impl();
    }

    void descrever() const {
        fmt::print("  {} → área={:.2f}, perímetro={:.2f}\n",
                   static_cast<const Derived*>(this)->nome(),
                   area(), perimetro());
    }
};

struct Circulo : FormaBase<Circulo> {
    double raio;
    explicit Circulo(double r) : raio(r) {}

    double area_impl()      const { return 3.14159 * raio * raio; }
    double perimetro_impl() const { return 2.0 * 3.14159 * raio; }
    const char* nome()      const { return "Círculo"; }
};

struct Retangulo : FormaBase<Retangulo> {
    double w, h;
    Retangulo(double ww, double hh) : w(ww), h(hh) {}

    double area_impl()      const { return w * h; }
    double perimetro_impl() const { return 2.0 * (w + h); }
    const char* nome()      const { return "Retângulo"; }
};

// Função genérica — sem virtual, sem ponteiro para base abstrata
template <typename T>
void imprimir_forma(const FormaBase<T>& forma) {
    forma.descrever();
}

void demo_polimorfismo_estatico() {
    fmt::print("\n── 6.1 Polimorfismo estático (sem virtual) ──\n");

    Circulo    c(5.0);
    Retangulo  r(4.0, 3.0);

    imprimir_forma(c);
    imprimir_forma(r);

    fmt::print("  (nenhuma vtable, nenhum virtual dispatch — tudo inlined)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Mixin com CRTP — injetar operadores a partir de primitivos
//     Boost.Operators faz exatamente isso para ==, !=, <, >, <=, >=
// ─────────────────────────────────────────────────────────────────────────────

// Mixin que gera !=, >, <=, >= a partir de == e <
template <typename Derived>
struct Comparavel {
    bool operator!=(const Derived& outro) const {
        return !(static_cast<const Derived&>(*this) == outro);
    }
    bool operator>(const Derived& outro) const {
        return outro < static_cast<const Derived&>(*this);
    }
    bool operator<=(const Derived& outro) const {
        return !(static_cast<const Derived&>(*this) > outro);
    }
    bool operator>=(const Derived& outro) const {
        return !(static_cast<const Derived&>(*this) < outro);
    }
};

// O usuário só precisa implementar == e <; os outros 4 chegam de graça
struct Temperatura : Comparavel<Temperatura> {
    double celsius;
    explicit Temperatura(double c) : celsius(c) {}

    bool operator==(const Temperatura& o) const { return celsius == o.celsius; }
    bool operator< (const Temperatura& o) const { return celsius < o.celsius;  }
};

void demo_mixin_operadores() {
    fmt::print("\n── 6.2 Mixin CRTP — operadores gerados automaticamente ──\n");

    Temperatura t1(20.0), t2(30.0), t3(20.0);

    fmt::print("  t1(20) == t3(20): {}\n", t1 == t3);
    fmt::print("  t1(20) != t2(30): {}\n", t1 != t2);   // gerado pelo mixin!
    fmt::print("  t1(20) <  t2(30): {}\n", t1 <  t2);
    fmt::print("  t2(30) >  t1(20): {}\n", t2 >  t1);   // gerado pelo mixin!
    fmt::print("  t1(20) <= t3(20): {}\n", t1 <= t3);   // gerado pelo mixin!
    fmt::print("  t1(20) >= t3(20): {}\n", t1 >= t3);   // gerado pelo mixin!
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. CRTP para contagem de instâncias — estado estático por tipo
//     Cada Derivada tem seu próprio contador (não compartilhado com outras)
// ─────────────────────────────────────────────────────────────────────────────

template <typename Derived>
struct ContadorInstancias {
    // static por tipo — cada Derivada tem o seu próprio
    static inline int contagem = 0;

    ContadorInstancias()  { ++contagem; }
    ~ContadorInstancias() { --contagem; }

    static int instancias_vivas() { return contagem; }
};

struct Conexao  : ContadorInstancias<Conexao>  {
    std::string host;
    explicit Conexao(std::string h) : host(std::move(h)) {}
};

struct Sessao   : ContadorInstancias<Sessao>   {
    int id;
    explicit Sessao(int i) : id(i) {}
};

void demo_contador() {
    fmt::print("\n── 6.3 Contador de instâncias por tipo ──\n");

    fmt::print("  Conexoes: {}, Sessoes: {}\n",
               Conexao::instancias_vivas(), Sessao::instancias_vivas());

    Conexao c1("db.host"), c2("cache.host");
    Sessao  s1(1);

    fmt::print("  Conexoes: {}, Sessoes: {}\n",
               Conexao::instancias_vivas(), Sessao::instancias_vivas());
    {
        Conexao c3("temp.host");
        Sessao  s2(2);
        Sessao  s3(3);
        fmt::print("  Conexoes: {}, Sessoes: {} (dentro do bloco)\n",
                   Conexao::instancias_vivas(), Sessao::instancias_vivas());
    }
    fmt::print("  Conexoes: {}, Sessoes: {} (após bloco)\n",
               Conexao::instancias_vivas(), Sessao::instancias_vivas());
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. CRTP para clone() type-safe — o problema do "slicing" resolvido
//     virtual Base* clone() retorna Base* → caller precisa fazer cast
//     CRTP permite retornar o tipo exato
// ─────────────────────────────────────────────────────────────────────────────

template <typename Derived>
struct Clonavel {
    // Retorna Derived* — não Base* — sem cast no caller
    Derived* clonar() const {
        return new Derived(static_cast<const Derived&>(*this));
    }
};

struct Documento : Clonavel<Documento> {
    std::string titulo;
    int         versao;
    Documento(std::string t, int v) : titulo(std::move(t)), versao(v) {}
};

struct Imagem : Clonavel<Imagem> {
    int largura, altura;
    Imagem(int w, int h) : largura(w), altura(h) {}
};

void demo_clone() {
    fmt::print("\n── 6.4 Clone type-safe com CRTP ──\n");

    Documento doc("Relatorio", 3);
    Documento* copia = doc.clonar();   // retorna Documento*, não Base*!
    fmt::print("  Original:  '{}' v{}\n", doc.titulo, doc.versao);
    fmt::print("  Clone:     '{}' v{}\n", copia->titulo, copia->versao);
    delete copia;

    Imagem img(1920, 1080);
    Imagem* img2 = img.clonar();
    fmt::print("  Imagem clonada: {}x{}\n", img2->largura, img2->altura);
    delete img2;
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. CRTP vs virtual — comparação direta
// ─────────────────────────────────────────────────────────────────────────────
void demo_crtp_vs_virtual() {
    fmt::print("\n── 6.5 CRTP vs virtual ──\n");
    fmt::print(
        "  ┌──────────────────────┬──────────────────┬──────────────────────┐\n"
        "  │                      │ virtual          │ CRTP                 │\n"
        "  ├──────────────────────┼──────────────────┼──────────────────────┤\n"
        "  │ Custo de chamada     │ indireta (vtable)│ zero (inlined)       │\n"
        "  │ Polimorfismo         │ runtime          │ compile-time         │\n"
        "  │ Coleção heterogênea  │ ✓ (Base*)        │ ✗ (tipos diferentes) │\n"
        "  │ Tamanho do objeto    │ +pointer vtable  │ sem overhead         │\n"
        "  │ Extensível por lib   │ ✓ (sem recompile)│ ✗ (recompila tudo)   │\n"
        "  │ Legibilidade         │ clara            │ mais complexa        │\n"
        "  ├──────────────────────┼──────────────────┼──────────────────────┤\n"
        "  │ Quando usar          │ polimorfismo     │ performance crítica   │\n"
        "  │                      │ em runtime,      │ mixins, operadores,  │\n"
        "  │                      │ coleções mistas  │ traits estáticos     │\n"
        "  └──────────────────────┴──────────────────┴──────────────────────┘\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_polimorfismo_estatico();
    demo_mixin_operadores();
    demo_contador();
    demo_clone();
    demo_crtp_vs_virtual();
}

} // namespace demo_crtp
