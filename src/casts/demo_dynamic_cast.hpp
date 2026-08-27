// =============================================================================
//  src/casts/demo_dynamic_cast.hpp
//
//  dynamic_cast<T> — downcast seguro verificado em RUNTIME
//  ─────────────────────────────────────────────────────────
//  • Requer que a classe base tenha ao menos um método virtual (RTTI).
//  • Para ponteiros: retorna nullptr se o cast falhar.
//  • Para referências: lança std::bad_cast se o cast falhar.
//  • Custo de runtime real (consulta tabela de tipos — typeinfo).
//  • Regra: se você tem certeza do tipo → static_cast (mais rápido).
//           se pode falhar → dynamic_cast (mais seguro).
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <memory>
#include <vector>
#include <stdexcept>

namespace demo_dynamic_cast {

// ─────────────────────────────────────────────────────────────────────────────
//  Hierarquia de exemplo
// ─────────────────────────────────────────────────────────────────────────────
struct Veiculo {
    virtual ~Veiculo() = default;
    virtual const char* nome() const { return "Veiculo"; }
};

struct Carro : Veiculo {
    const char* nome() const override { return "Carro"; }
    void buzinar() const { fmt::print("  Bip bip!\n"); }
    int num_portas = 4;
};

struct Moto : Veiculo {
    const char* nome() const override { return "Moto"; }
    void empinar() const { fmt::print("  *empina*\n"); }
};

struct CarroEletrico : Carro {
    const char* nome() const override { return "CarroEletrico"; }
    void carregar() const { fmt::print("  *carregando bateria*\n"); }
    int autonomia_km = 400;
};

// ─────────────────────────────────────────────────────────────────────────────
//  1. Downcast bem-sucedido vs. falho — ponteiro
// ─────────────────────────────────────────────────────────────────────────────
void demo_ponteiro() {
    fmt::print("\n── 2.1 dynamic_cast com ponteiro ──\n");

    Veiculo* v1 = new Carro();
    Veiculo* v2 = new Moto();

    // Cast correto: v1 é realmente um Carro
    Carro* c = dynamic_cast<Carro*>(v1);
    if (c) {
        fmt::print("  v1 é Carro ✓ — portas: {}\n", c->num_portas);
        c->buzinar();
    }

    // Cast errado: v2 é Moto, não Carro → retorna nullptr
    Carro* nao_carro = dynamic_cast<Carro*>(v2);
    if (!nao_carro) {
        fmt::print("  v2 NÃO é Carro ✓ — dynamic_cast retornou nullptr\n");
    }

    // Tenta cast para subclasse mais profunda
    CarroEletrico* ce = dynamic_cast<CarroEletrico*>(v1); // v1 é Carro, não CarroEletrico
    fmt::print("  v1 é CarroEletrico? {} (é só Carro)\n", ce != nullptr);

    delete v1;
    delete v2;
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Downcast com referência — lança std::bad_cast se falhar
// ─────────────────────────────────────────────────────────────────────────────
void demo_referencia() {
    fmt::print("\n── 2.2 dynamic_cast com referência (lança bad_cast) ──\n");

    Carro carro;
    Veiculo& v = carro;

    // Cast correto — sem exceção
    try {
        Carro& c = dynamic_cast<Carro&>(v);
        fmt::print("  Cast para Carro& bem-sucedido, portas: {}\n", c.num_portas);
    } catch (const std::bad_cast& e) {
        fmt::print("  bad_cast: {}\n", e.what());
    }

    // Cast errado — lança std::bad_cast
    Moto moto;
    Veiculo& vm = moto;
    try {
        [[maybe_unused]] Carro& c = dynamic_cast<Carro&>(vm);
        fmt::print("  ERRO: não deveria chegar aqui\n");
    } catch (const std::bad_cast& e) {
        fmt::print("  bad_cast capturado ✓ — Moto não pode ser Carro\n");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Padrão visitante — dynamic_cast para despacho de tipo
//     (alternativa menos elegante ao visitor pattern, mas didática)
// ─────────────────────────────────────────────────────────────────────────────
void processar_veiculo(Veiculo* v) {
    if (auto* ce = dynamic_cast<CarroEletrico*>(v)) {
        fmt::print("  CarroEletrico: autonomia {}km, ", ce->autonomia_km);
        ce->carregar();
    } else if (auto* c = dynamic_cast<Carro*>(v)) {
        fmt::print("  Carro: {} portas, ", c->num_portas);
        c->buzinar();
    } else if (auto* m = dynamic_cast<Moto*>(v)) {
        fmt::print("  Moto: ");
        m->empinar();
    } else {
        fmt::print("  Veiculo desconhecido: {}\n", v->nome());
    }
}

void demo_despacho() {
    fmt::print("\n── 2.3 Despacho de tipo com dynamic_cast ──\n");

    std::vector<Veiculo*> frota = {
        new Carro(),
        new Moto(),
        new CarroEletrico(),
        new Carro()
    };

    for (auto* v : frota) {
        processar_veiculo(v);
        delete v;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Crosscast — cast entre ramos diferentes da hierarquia (com herança múltipla)
// ─────────────────────────────────────────────────────────────────────────────
struct Voador {
    virtual ~Voador() = default;
    virtual void voar() const { fmt::print("  *voando*\n"); }
};

struct CarroVoador : Carro, Voador {
    const char* nome() const override { return "CarroVoador"; }
};

void demo_crosscast() {
    fmt::print("\n── 2.4 Crosscast (herança múltipla) ──\n");

    CarroVoador cv;
    Veiculo* v = &cv;

    // De Veiculo* para Voador* — só dynamic_cast consegue!
    // static_cast falharia aqui (não há relação direta entre os ramos)
    Voador* voador = dynamic_cast<Voador*>(v);
    if (voador) {
        fmt::print("  Crosscast Veiculo* → Voador* bem-sucedido ✓\n");
        voador->voar();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Performance — quando NÃO usar dynamic_cast
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando_nao_usar() {
    fmt::print("\n── 2.5 Quando NÃO usar dynamic_cast ──\n");
    fmt::print(
        "  ✗ Loop crítico de performance  → use virtual dispatch ou static_cast\n"
        "  ✗ Você tem certeza do tipo     → use static_cast (sem custo RTTI)\n"
        "  ✗ Compilado com -fno-rtti      → dynamic_cast não funciona\n"
        "  ✓ Downcast onde tipo pode variar em runtime\n"
        "  ✓ Crosscast em herança múltipla\n"
        "  ✓ Código de diagnóstico / serialização\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_ponteiro();
    demo_referencia();
    demo_despacho();
    demo_crosscast();
    demo_quando_nao_usar();
}

} // namespace demo_dynamic_cast
