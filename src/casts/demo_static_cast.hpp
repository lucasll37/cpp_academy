// =============================================================================
//  src/casts/demo_static_cast.hpp
//
//  static_cast<T> — conversão verificada em COMPILE-TIME
//  ──────────────────────────────────────────────────────
//  • O compilador verifica se a conversão é "razoável" entre os tipos.
//  • Zero custo em runtime — nenhuma verificação extra é emitida.
//  • Abrange: numérico, upcast, downcast sem verificação, void*, enum↔int.
//  • É o cast "padrão" — use quando os outros não se aplicam.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <cstdint>

namespace demo_static_cast {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Conversões numéricas — o caso mais comum
// ─────────────────────────────────────────────────────────────────────────────
void demo_numerico() {
    fmt::print("\n── 1.1 Conversões numéricas ──\n");

    double pi = 3.14159;

    // Sem cast: compilador emite warning "narrowing conversion"
    // int truncado = pi;          // ⚠ warning

    // Com static_cast: sinaliza intenção explícita → sem warning
    int truncado = static_cast<int>(pi);
    fmt::print("  double {:.5f} → int {}\n", pi, truncado);

    // int → double: promoção segura (sem perda)
    int amostras = 7;
    double media = static_cast<double>(amostras) / 2;   // sem cast: divisão inteira!
    fmt::print("  7 / 2 sem cast    = {}\n", amostras / 2);       // 3
    fmt::print("  7 / 2 com cast    = {:.1f}\n", media);          // 3.5

    // char ↔ int
    char letra = 'A';
    int codigo = static_cast<int>(letra);
    fmt::print("  'A' como int      = {}\n", codigo);             // 65
    char volta = static_cast<char>(66);
    fmt::print("  66 como char      = {}\n", volta);              // B

    // Truncamento perigoso — static_cast permite, mas você assume a responsabilidade
    int grande = 300;
    uint8_t pequeno = static_cast<uint8_t>(grande); // 300 % 256 = 44
    fmt::print("  int 300 → uint8_t = {} (truncamento: 300 mod 256)\n", +pequeno);
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Hierarquia de classes — upcast (sempre seguro)
// ─────────────────────────────────────────────────────────────────────────────
struct Animal {
    virtual ~Animal() = default;
    virtual const char* tipo() const { return "Animal"; }
};

struct Cachorro : Animal {
    const char* tipo() const override { return "Cachorro"; }
    void latir() const { fmt::print("  Au au!\n"); }
};

void demo_upcast() {
    fmt::print("\n── 1.2 Upcast (Derivado → Base) — sempre seguro ──\n");

    Cachorro dog;

    // Upcast: static_cast é opcional aqui (implícito), mas deixa explícito
    Animal* animal = static_cast<Animal*>(&dog);
    fmt::print("  animal->tipo() = {}\n", animal->tipo()); // "Cachorro" (virtual)

    // Upcast por referência
    Animal& ref = static_cast<Animal&>(dog);
    fmt::print("  ref.tipo()     = {}\n", ref.tipo());
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Downcast sem verificação — você assume que sabe o tipo real
//     Use dynamic_cast se não tiver certeza!
// ─────────────────────────────────────────────────────────────────────────────
void demo_downcast() {
    fmt::print("\n── 1.3 Downcast (Base → Derivado) — sem verificação ──\n");

    Animal* animal = new Cachorro(); // objeto real é Cachorro

    // static_cast: rápido, sem verificação — comportamento correto aqui
    // porque sabemos com certeza que o objeto é um Cachorro
    Cachorro* dog = static_cast<Cachorro*>(animal);
    dog->latir();
    fmt::print("  downcast correto: {}\n", dog->tipo());

    // ⚠  Se o objeto NÃO fosse Cachorro, o comportamento seria undefined!
    //    Nesse caso, prefira dynamic_cast (veja demo_dynamic_cast.hpp)
    delete animal;
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. void* — o "ponteiro universal" de C
//     static_cast é a forma correta de converter para/de void* em C++
// ─────────────────────────────────────────────────────────────────────────────
void demo_void_ptr() {
    fmt::print("\n── 1.4 Conversão para void* ──\n");

    int valor = 42;
    int* ptr = &valor;

    // Qualquer ponteiro → void* (sem cast explícito em C++, mas com é mais claro)
    void* generico = static_cast<void*>(ptr);
    fmt::print("  endereço como void* = {}\n", generico);

    // void* → tipo concreto: static_cast necessário
    int* recuperado = static_cast<int*>(generico);
    fmt::print("  valor recuperado    = {}\n", *recuperado);
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. enum ↔ int
// ─────────────────────────────────────────────────────────────────────────────
enum class Cor { Vermelho = 0, Verde = 1, Azul = 2 };

void demo_enum() {
    fmt::print("\n── 1.5 enum class ↔ int ──\n");

    // enum class NÃO converte implicitamente (ao contrário de enum simples)
    // int c = Cor::Verde;               // ✗ erro de compilação

    int c = static_cast<int>(Cor::Verde);
    fmt::print("  Cor::Verde como int = {}\n", c);

    Cor cor = static_cast<Cor>(2);
    fmt::print("  int 2 como Cor::    = Azul? {}\n", cor == Cor::Azul);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_numerico();
    demo_upcast();
    demo_downcast();
    demo_void_ptr();
    demo_enum();
}

} // namespace demo_static_cast
