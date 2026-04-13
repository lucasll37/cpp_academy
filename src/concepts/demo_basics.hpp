// =============================================================================
//  src/concepts/demo_basics.hpp
//
//  Concepts — Fundamentos
//  ───────────────────────
//  Concepts são predicados booleanos em compile-time que expressam
//  requisitos sobre tipos de forma legível e com mensagens de erro claras.
//
//  Antes (C++17): SFINAE com enable_if — funciona, mas ilegível.
//  Agora (C++20): concept — legível, composável, erros compreensíveis.
//
//  Tópicos:
//  • Definindo concepts com `concept` + expressão booleana
//  • Usando concepts como constraints em templates
//  • Concepts da stdlib: std::integral, std::floating_point, etc.
//  • A cláusula `requires` inline
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <concepts>
#include <string>
#include <vector>
#include <type_traits>

namespace demo_basics {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Definindo um concept simples
//     Um concept é um predicado booleano avaliado em compile-time.
//     Sintaxe: template <typename T> concept Nome = <expressão booleana>;
// ─────────────────────────────────────────────────────────────────────────────

// Concept que aceita apenas tipos numéricos (inteiros ou ponto-flutuante)
template <typename T>
concept Numerico = std::integral<T> || std::floating_point<T>;

// Concept que aceita apenas tipos que podem ser convertidos para std::string
template <typename T>
concept Stringificavel = std::convertible_to<T, std::string>;

// Concept composto — tipo que é numérico E de 32 bits
template <typename T>
concept Numerico32bits = Numerico<T> && (sizeof(T) == 4);

// ─────────────────────────────────────────────────────────────────────────────
//  2. Quatro formas de aplicar um concept a um template
//     Todas equivalentes — escolha pelo estilo e clareza.
// ─────────────────────────────────────────────────────────────────────────────

// Forma 1: concept no lugar de `typename` no template parameter
template <Numerico T>
T dobrar_v1(T x) { return x * 2; }

// Forma 2: cláusula `requires` após a assinatura
template <typename T>
T dobrar_v2(T x) requires Numerico<T> { return x * 2; }

// Forma 3: cláusula `requires` no template parameter (mais verboso, mais explícito)
template <typename T> requires Numerico<T>
T dobrar_v3(T x) { return x * 2; }

// Forma 4: `auto` restrito — sintaxe abreviada (abbreviated function template)
//          O compilador infere o template parameter
Numerico auto dobrar_v4(Numerico auto x) { return x * 2; }

void demo_formas_de_aplicar() {
    fmt::print("\n── 1.1 Quatro formas de aplicar um concept ──\n");

    fmt::print("  dobrar_v1(21)   = {}\n", dobrar_v1(21));
    fmt::print("  dobrar_v2(3.14) = {:.2f}\n", dobrar_v2(3.14));
    fmt::print("  dobrar_v3(10)   = {}\n", dobrar_v3(10));
    fmt::print("  dobrar_v4(5.5f) = {:.1f}\n", dobrar_v4(5.5f));

    // Erro de compilação claro se descomentar:
    // dobrar_v1("texto");  // error: 'const char*' não satisfaz 'Numerico'
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Concepts pré-definidos da stdlib (<concepts>)
//     C++20 fornece um conjunto rico de concepts prontos para uso.
// ─────────────────────────────────────────────────────────────────────────────

template <std::integral T>
void processar_inteiro(T val) {
    fmt::print("  inteiro ({}): {}\n", sizeof(T) * 8, val);
}

template <std::floating_point T>
void processar_float(T val) {
    fmt::print("  float ({} bytes): {:.4f}\n", sizeof(T), val);
}

template <std::same_as<std::string> T>
void processar_string(const T& val) {
    fmt::print("  string: '{}' (len={})\n", val, val.size());
}

void demo_concepts_stdlib() {
    fmt::print("\n── 1.2 Concepts pré-definidos da stdlib ──\n");

    fmt::print("  std::integral:\n");
    processar_inteiro(42);
    processar_inteiro(42L);
    processar_inteiro(static_cast<short>(10));

    fmt::print("  std::floating_point:\n");
    processar_float(3.14);
    processar_float(2.71f);
    processar_float(1.41L);

    fmt::print("  std::same_as<std::string>:\n");
    processar_string(std::string("olá, concepts!"));

    fmt::print("\n  Tabela de concepts da <concepts>:\n");
    fmt::print(
        "  ┌────────────────────────────┬──────────────────────────────────────┐\n"
        "  │ Concept                    │ Satisfeito por                       │\n"
        "  ├────────────────────────────┼──────────────────────────────────────┤\n"
        "  │ std::integral              │ int, long, short, char, bool...      │\n"
        "  │ std::floating_point        │ float, double, long double           │\n"
        "  │ std::arithmetic            │ integral || floating_point           │\n"
        "  │ std::signed_integral       │ int, long, short...                  │\n"
        "  │ std::unsigned_integral     │ unsigned int, size_t...              │\n"
        "  │ std::same_as<T>            │ exatamente T                         │\n"
        "  │ std::derived_from<Base>    │ subclasses de Base                   │\n"
        "  │ std::convertible_to<T>     │ tipos implicitamente conversíveis    │\n"
        "  │ std::equality_comparable   │ tipos com == e !=                    │\n"
        "  │ std::totally_ordered       │ tipos com <, >, <=, >=               │\n"
        "  │ std::copyable              │ tipos copiáveis                      │\n"
        "  │ std::movable               │ tipos movíveis                       │\n"
        "  │ std::regular               │ copyable + default_initializable     │\n"
        "  │ std::invocable<Args...>    │ callable com esses argumentos        │\n"
        "  └────────────────────────────┴──────────────────────────────────────┘\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Concepts vs SFINAE — comparação direta
//     O mesmo comportamento, legibilidade radicalmente diferente.
// ─────────────────────────────────────────────────────────────────────────────

// ── SFINAE (C++17): funciona, mas difícil de ler ─────────────────────────────
template <typename T,
          std::enable_if_t<std::is_integral_v<T>, int> = 0>
std::string descricao_sfinae(T) { return "inteiro (SFINAE)"; }

template <typename T,
          std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
std::string descricao_sfinae(T) { return "float (SFINAE)"; }

// ── Concepts (C++20): mesma semântica, muito mais claro ──────────────────────
template <std::integral T>
std::string descricao_concept(T) { return "inteiro (concept)"; }

template <std::floating_point T>
std::string descricao_concept(T) { return "float (concept)"; }

void demo_sfinae_vs_concepts() {
    fmt::print("\n── 1.3 SFINAE vs Concepts ──\n");

    fmt::print("  descricao_sfinae(42)    = {}\n", descricao_sfinae(42));
    fmt::print("  descricao_sfinae(3.14)  = {}\n", descricao_sfinae(3.14));
    fmt::print("  descricao_concept(42)   = {}\n", descricao_concept(42));
    fmt::print("  descricao_concept(3.14) = {}\n", descricao_concept(3.14));

    fmt::print("\n  Vantagens dos concepts sobre SFINAE:\n");
    fmt::print("    • Erros de compilação apontam o concept, não a expansão interna\n");
    fmt::print("    • Sintaxe legível — intenção clara no código\n");
    fmt::print("    • Composáveis com &&, ||, !\n");
    fmt::print("    • Suporte a overload resolution por subsumption\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Concepts compostos — combinando com &&, ||, !
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
concept Inteiro32 = std::integral<T> && (sizeof(T) == 4);

template <typename T>
concept NaoConst = !std::is_const_v<T>;

template <typename T>
concept NumericoOuString =
    Numerico<T> || std::same_as<T, std::string>;

template <NumericoOuString T>
void imprimir_valor(const T& val) {
    if constexpr (std::same_as<T, std::string>) {
        fmt::print("  string: '{}'\n", val);
    } else {
        fmt::print("  numerico: {}\n", val);
    }
}

void demo_composicao() {
    fmt::print("\n── 1.4 Composição de concepts ──\n");

    fmt::print("  Inteiro32<int>:    {}\n", Inteiro32<int>);
    fmt::print("  Inteiro32<short>:  {}\n", Inteiro32<short>);
    fmt::print("  Inteiro32<long>:   {}\n", Inteiro32<long>);   // depende da plataforma
    fmt::print("  NaoConst<int>:     {}\n", NaoConst<int>);
    fmt::print("  NaoConst<const int>: {}\n", NaoConst<const int>);

    fmt::print("\n  NumericoOuString aceita:\n");
    imprimir_valor(42);
    imprimir_valor(3.14);
    imprimir_valor(std::string("academy"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_formas_de_aplicar();
    demo_concepts_stdlib();
    demo_sfinae_vs_concepts();
    demo_composicao();
}

} // namespace demo_basics
