// =============================================================================
//  src/oop/demo_rtti.hpp
//
//  RTTI — Run-Time Type Information
//  ────────────────────────────────
//  Objetos polimórficos carregam, junto da vtable, a identidade do próprio
//  tipo. Isso habilita duas operações que precisam saber em EXECUÇÃO o que o
//  objeto realmente é: `typeid` e `dynamic_cast`.
//
//  RTTI é frequentemente sinal de modelagem fraca — se você está perguntando
//  o tipo, talvez faltasse um método virtual. Mas há usos legítimos, e vale
//  conhecer o custo antes de decidir.
//
//  Tópicos:
//  • typeid e std::type_info
//  • Desmangling do nome (o que `1N4demo3FooE` quer dizer)
//  • std::type_index como chave de mapa
//  • dynamic_cast: ponteiro vs referência
//  • O custo do dynamic_cast, medido
//  • Alternativas: virtual, visitor, std::variant
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#if defined(__GNUG__)
#  include <cxxabi.h>
#endif

namespace demo_rtti {

// ─────────────────────────────────────────────────────────────────────────────
//  Utilitário: converter o nome mangled do type_info em algo legível
//
//     O padrão só garante que `type_info::name()` devolve uma string
//     "definida pela implementação". No GCC/Clang ela vem no formato do
//     Itanium ABI (`N4demo3FooE`) e precisa passar pelo demangler.
// ─────────────────────────────────────────────────────────────────────────────
inline std::string legivel(const std::type_info& ti) {
#if defined(__GNUG__)
    int status = 0;
    // __cxa_demangle aloca com malloc — o unique_ptr com deleter free() cuida disso
    std::unique_ptr<char, void(*)(void*)> bruto(
        abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status), std::free);
    return (status == 0 && bruto) ? bruto.get() : ti.name();
#else
    return ti.name();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hierarquia de exemplo
// ─────────────────────────────────────────────────────────────────────────────
struct Forma {
    virtual double area() const = 0;
    virtual ~Forma() = default;
};

struct Circulo : Forma {
    double raio = 1.0;
    explicit Circulo(double r) : raio(r) {}
    double area() const override { return 3.14159265 * raio * raio; }
};

struct Retangulo : Forma {
    double largura = 1.0, altura = 1.0;
    Retangulo(double l, double a) : largura(l), altura(a) {}
    double area() const override { return largura * altura; }
    double diagonal() const { return std::sqrt(largura*largura + altura*altura); }
};

struct Quadrado : Retangulo {
    explicit Quadrado(double lado) : Retangulo(lado, lado) {}
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. typeid — perguntando o tipo
//
//     Em objeto POLIMÓRFICO, typeid é avaliado em runtime e devolve o tipo
//     DINÂMICO. Em tipo não-polimórfico, é resolvido em compilação e devolve
//     o tipo ESTÁTICO.
// ─────────────────────────────────────────────────────────────────────────────
struct NaoPolimorfica { int x; };
struct DerivadaNaoPolimorfica : NaoPolimorfica { int y; };

void demo_typeid() {
    fmt::print("\n── 5.1 typeid ──\n");

    Circulo c(2.0);
    Forma&  f = c;

    fmt::print("      typeid(c).name()  cru      = {}\n", typeid(c).name());
    fmt::print("      demanglado                 = {}\n", legivel(typeid(c)));
    fmt::print("      typeid(f) via Forma&       = {}  ← tipo DINÂMICO\n",
               legivel(typeid(f)));
    fmt::print("      typeid(Forma)              = {}\n", legivel(typeid(Forma)));

    // Sem virtual não há RTTI: o resultado é o tipo estático.
    DerivadaNaoPolimorfica d{};
    NaoPolimorfica& np = d;
    fmt::print("      não-polimórfica via base&  = {}  ← tipo ESTÁTICO\n",
               legivel(typeid(np)));

    // Comparar type_info é a forma de teste de tipo EXATO (não aceita derivados)
    Quadrado q(3.0);
    Forma& fq = q;
    fmt::print("\n      typeid(fq) == typeid(Quadrado)  ? {}\n",
               typeid(fq) == typeid(Quadrado));
    fmt::print("      typeid(fq) == typeid(Retangulo) ? {}  ← exato, não aceita base\n",
               typeid(fq) == typeid(Retangulo));
    fmt::print("      Para \"é um Retangulo ou derivado\", use dynamic_cast.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. std::type_index — type_info utilizável como chave
//
//     `std::type_info` não é copiável nem ordenável, então não serve de chave.
//     `std::type_index` é um wrapper copiável, comparável e hasheável.
//     É a base de qualquer registro dirigido por tipo — inclusive do ECS
//     deste repositório (ver src/ecs/component_store.hpp).
// ─────────────────────────────────────────────────────────────────────────────
void demo_type_index() {
    fmt::print("\n── 5.2 std::type_index como chave ──\n");

    std::unordered_map<std::type_index, std::string> descricoes{
        {typeid(Circulo),   "definida por 1 raio"},
        {typeid(Retangulo), "definida por 2 lados"},
        {typeid(Quadrado),  "retângulo com lados iguais"},
    };

    std::vector<std::unique_ptr<Forma>> formas;
    formas.push_back(std::make_unique<Circulo>(1.5));
    formas.push_back(std::make_unique<Retangulo>(2.0, 3.0));
    formas.push_back(std::make_unique<Quadrado>(2.0));

    for (const auto& f : formas) {
        auto it = descricoes.find(typeid(*f));
        fmt::print("      {:<12} area={:6.2f}  {}\n",
                   legivel(typeid(*f)).substr(legivel(typeid(*f)).rfind(':') + 1),
                   f->area(),
                   it != descricoes.end() ? it->second : "(sem descrição)");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. dynamic_cast — conversão verificada em runtime
//
//     Duas formas, com tratamentos de falha diferentes:
//       • ponteiro   → devolve nullptr        (teste, sem exceção)
//       • referência → lança std::bad_cast    (falha é excepcional)
// ─────────────────────────────────────────────────────────────────────────────
void demo_dynamic_cast() {
    fmt::print("\n── 5.3 dynamic_cast ──\n");

    std::vector<std::unique_ptr<Forma>> formas;
    formas.push_back(std::make_unique<Circulo>(1.0));
    formas.push_back(std::make_unique<Retangulo>(3.0, 4.0));
    formas.push_back(std::make_unique<Quadrado>(5.0));

    for (const auto& f : formas) {
        // Downcast por ponteiro: nullptr em caso de falha.
        // Repare que Quadrado PASSA no teste de Retangulo — dynamic_cast
        // aceita derivados, diferente de comparar typeid.
        if (auto* r = dynamic_cast<const Retangulo*>(f.get())) {
            fmt::print("      {:<28} é Retangulo → diagonal = {:.2f}\n",
                       legivel(typeid(*f)), r->diagonal());
        } else {
            fmt::print("      {:<28} não é Retangulo\n", legivel(typeid(*f)));
        }
    }

    // Downcast por referência: lança se falhar.
    Circulo c(1.0);
    Forma&  f = c;
    try {
        auto& r = dynamic_cast<Retangulo&>(f);
        fmt::print("      inalcançável: {}\n", r.area());
    } catch (const std::bad_cast& e) {
        fmt::print("      dynamic_cast<Retangulo&> num Circulo → std::bad_cast: {}\n",
                   e.what());
    }

    // Upcast (derivado → base) NÃO precisa de dynamic_cast: é implícito.
    // Sidecast (entre bases irmãs) precisa, e o dynamic_cast dá conta.
    fmt::print("      upcast é implícito; downcast e sidecast exigem dynamic_cast\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. O custo do dynamic_cast, medido
//
//     dynamic_cast percorre a árvore de herança em runtime. Não é O(1):
//     depende da profundidade e da forma da hierarquia.
// ─────────────────────────────────────────────────────────────────────────────
void demo_custo() {
    fmt::print("\n── 5.4 O custo, medido ──\n");

    constexpr int N = 200'000;
    Quadrado q(2.0);
    Forma&   f = q;

    volatile double soma = 0;       // volatile impede o compilador de eliminar tudo

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) soma = soma + f.area();          // virtual puro
    auto t1 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        if (auto* r = dynamic_cast<Retangulo*>(&f)) soma = soma + r->diagonal();
    }
    auto t2 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        if (typeid(f) == typeid(Quadrado)) soma = soma + 1;
    }
    auto t3 = std::chrono::steady_clock::now();

    using us = std::chrono::microseconds;
    auto virt = std::chrono::duration_cast<us>(t1 - t0).count();
    auto dyn  = std::chrono::duration_cast<us>(t2 - t1).count();
    auto tid  = std::chrono::duration_cast<us>(t3 - t2).count();

    // ns por operação e o fator em relação à chamada virtual — é a comparação
    // que interessa; o total em µs sozinho não diz nada.
    const double ns_virt = static_cast<double>(virt) * 1000.0 / N;
    const double ns_dyn  = static_cast<double>(dyn)  * 1000.0 / N;
    const double ns_tid  = static_cast<double>(tid)  * 1000.0 / N;

    fmt::print("      {} iterações de cada:\n", N);
    fmt::print("        {:<18} {:>6} µs   {:>5.1f} ns/op   {:>4.1f}×\n",
               "chamada virtual", virt, ns_virt, 1.0);
    fmt::print("        {:<18} {:>6} µs   {:>5.1f} ns/op   {:>4.1f}×\n",
               "dynamic_cast", dyn, ns_dyn, ns_virt > 0 ? ns_dyn / ns_virt : 0.0);
    fmt::print("        {:<18} {:>6} µs   {:>5.1f} ns/op   {:>4.1f}×\n",
               "typeid ==", tid, ns_tid, ns_virt > 0 ? ns_tid / ns_virt : 0.0);
    fmt::print("      (build de debug, -O0 — os números mudam bastante com -O2)\n");
    fmt::print("      A ordem de grandeza é o que importa: dynamic_cast é a\n");
    fmt::print("      operação mais cara das três, e por uma boa margem.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. As alternativas ao RTTI
//
//     Quase toda cadeia de `if (dynamic_cast<X>)` é um método virtual que
//     não foi escrito. Três saídas, em ordem de preferência:
// ─────────────────────────────────────────────────────────────────────────────
// (a) Método virtual — a solução padrão: o objeto responde por si
struct FormaVirtual {
    virtual double area() const = 0;
    virtual std::string descrever() const = 0;   // ← em vez de perguntar o tipo
    virtual ~FormaVirtual() = default;
};

struct CirculoVirtual : FormaVirtual {
    double raio;
    explicit CirculoVirtual(double r) : raio(r) {}
    double area() const override { return 3.14159265 * raio * raio; }
    std::string descrever() const override { return fmt::format("círculo r={}", raio); }
};

void demo_alternativas() {
    fmt::print("\n── 5.5 Alternativas ao RTTI ──\n");

    std::unique_ptr<FormaVirtual> f = std::make_unique<CirculoVirtual>(2.0);
    fmt::print("      (a) método virtual : {}\n", f->descrever());

    fmt::print("      (b) padrão Visitor : despacho duplo sem dynamic_cast\n");
    fmt::print("                           (ver src/design-of-pattern/)\n");
    fmt::print("      (c) std::variant   : hierarquia FECHADA, sem herança nenhuma —\n");
    fmt::print("                           std::visit faz o despacho, e o compilador\n");
    fmt::print("                           garante que todos os casos foram tratados\n");

    fmt::print("\n      Use RTTI quando:\n");
    fmt::print("        • você não controla a hierarquia (biblioteca de terceiros)\n");
    fmt::print("        • está escrevendo serialização/reflexão genérica\n");
    fmt::print("        • precisa de um registro dirigido por tipo (ver src/ecs/)\n");
    fmt::print("      Evite quando um método virtual resolveria.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_typeid();
    demo_type_index();
    demo_dynamic_cast();
    demo_custo();
    demo_alternativas();
}

} // namespace demo_rtti
