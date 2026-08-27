// =============================================================================
//  src/concepts/demo_requires.hpp
//
//  Requires Expressions — Verificando Capacidades de Tipos
//  ─────────────────────────────────────────────────────────
//  A `requires expression` é o mecanismo de baixo nível que permite
//  checar se um conjunto de operações é válido para um tipo.
//  É o que concepts usam internamente, mas você pode usá-la diretamente.
//
//  Quatro tipos de requisitos dentro de requires { ... }:
//  • Simple requirement:    a expressão precisa ser válida (compilar)
//  • Type requirement:      typename T::value_type precisa existir
//  • Compound requirement:  a expressão tem tipo específico
//  • Nested requirement:    sub-constraint booleana dentro do requires
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <concepts>
#include <string>
#include <vector>
#include <iterator>

namespace demo_requires {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Simple requirement — a expressão precisa compilar
//     Não importa o tipo de retorno; só verifica se a operação existe.
// ─────────────────────────────────────────────────────────────────────────────

// Concept: tipo que pode ser somado com si mesmo
template <typename T>
concept Somavel = requires(T a, T b) {
    a + b;        // simple requirement: a + b precisa compilar
    a - b;        // a - b também precisa compilar
    a += b;       // a += b também
};

// Concept: tipo que tem método .size() e .empty()
template <typename T>
concept TemTamanho = requires(T c) {
    c.size();     // .size() precisa existir
    c.empty();    // .empty() precisa existir
};

void demo_simple_requirement() {
    fmt::print("\n── 2.1 Simple requirement ──\n");

    fmt::print("  Somavel<int>:         {}\n", Somavel<int>);
    fmt::print("  Somavel<double>:      {}\n", Somavel<double>);
    fmt::print("  Somavel<std::string>: {}\n", Somavel<std::string>); // string tem + mas não +=int
    fmt::print("  TemTamanho<std::vector<int>>: {}\n", TemTamanho<std::vector<int>>);
    fmt::print("  TemTamanho<std::string>:      {}\n", TemTamanho<std::string>);
    fmt::print("  TemTamanho<int>:              {}\n", TemTamanho<int>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Type requirement — typename T::X precisa existir
//     Verifica a existência de tipos aninhados (nested types).
// ─────────────────────────────────────────────────────────────────────────────

// Concept: container STL-like — tem value_type, iterator, begin(), end()
template <typename T>
concept Container = requires {
    typename T::value_type;        // type requirement: value_type existe
    typename T::iterator;          // type requirement: iterator existe
    typename T::const_iterator;    // type requirement: const_iterator existe
} && requires(T c) {
    c.begin();                     // begin() existe
    c.end();                       // end() existe
    c.size();                      // size() existe
};

template <Container C>
void imprimir_container(const C& c) {
    fmt::print("  container [size={}]: ", c.size());
    for (const auto& elem : c) {
        fmt::print("{} ", elem);
    }
    fmt::print("\n");
}

void demo_type_requirement() {
    fmt::print("\n── 2.2 Type requirement ──\n");

    fmt::print("  Container<std::vector<int>>:  {}\n", Container<std::vector<int>>);
    fmt::print("  Container<std::string>:        {}\n", Container<std::string>);
    fmt::print("  Container<int>:                {}\n", Container<int>);

    imprimir_container(std::vector<int>{1, 2, 3, 4, 5});
    imprimir_container(std::string("academy"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Compound requirement — { expr } -> TipoConcept
//     Verifica tanto a validade quanto o TIPO de retorno de uma expressão.
//     Sintaxe: { expressao } -> Concept;
//              { expressao } noexcept;            (não lança exceções)
//              { expressao } noexcept -> Concept; (ambos)
// ─────────────────────────────────────────────────────────────────────────────

// Concept: tipo com .size() que retorna algo conversível para size_t
template <typename T>
concept TemSizeCorreto = requires(T c) {
    { c.size() } -> std::convertible_to<std::size_t>;  // compound: tipo verificado
};

// Concept: tipo com operação swap() sem lançar exceção
template <typename T>
concept SwapSemExcecao = requires(T a, T b) {
    { swap(a, b) } noexcept;   // compound: verifica noexcept
};

// Concept: serializable — tem to_string() retornando std::string
template <typename T>
concept Serializavel = requires(const T& obj) {
    { obj.to_string() } -> std::same_as<std::string>;  // retorno exato
};

struct Ponto {
    double x, y;
    std::string to_string() const {
        return fmt::format("({:.1f}, {:.1f})", x, y);
    }
};

struct NaoSerializavel {
    int valor;
};

template <Serializavel T>
void serializar(const T& obj) {
    fmt::print("  serializado: {}\n", obj.to_string());
}

void demo_compound_requirement() {
    fmt::print("\n── 2.3 Compound requirement ──\n");

    fmt::print("  TemSizeCorreto<std::vector<int>>: {}\n",
               TemSizeCorreto<std::vector<int>>);
    fmt::print("  TemSizeCorreto<int>:              {}\n",
               TemSizeCorreto<int>);

    fmt::print("  Serializavel<Ponto>:           {}\n", Serializavel<Ponto>);
    fmt::print("  Serializavel<NaoSerializavel>: {}\n", Serializavel<NaoSerializavel>);

    serializar(Ponto{3.0, 4.0});
    serializar(Ponto{1.5, 2.5});
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Nested requirement — requires dentro de requires
//     Sub-constraint booleana que pode referenciar os parâmetros do requires.
// ─────────────────────────────────────────────────────────────────────────────

// Concept: tipo numérico de precisão adequada (≥ 4 bytes)
template <typename T>
concept NumericoPreciso = requires {
    requires std::floating_point<T>;          // nested: sub-concept aplicado
    requires (sizeof(T) >= sizeof(float));    // nested: constraint booleana
};

// Concept: iterador válido com tipo de valor acessível
template <typename It>
concept IteradorValido = requires(It it) {
    *it;                                          // simple: dereferenceable
    ++it;                                         // simple: incrementable
    requires std::equality_comparable<It>;        // nested: concept separado
    typename std::iterator_traits<It>::value_type; // type: value_type existe
};

void demo_nested_requirement() {
    fmt::print("\n── 2.4 Nested requirement ──\n");

    fmt::print("  NumericoPreciso<float>:       {}\n", NumericoPreciso<float>);
    fmt::print("  NumericoPreciso<double>:      {}\n", NumericoPreciso<double>);
    fmt::print("  NumericoPreciso<int>:         {}\n", NumericoPreciso<int>);

    fmt::print("  IteradorValido<int*>:                        {}\n",
               IteradorValido<int*>);
    fmt::print("  IteradorValido<std::vector<int>::iterator>:  {}\n",
               IteradorValido<std::vector<int>::iterator>);
    fmt::print("  IteradorValido<int>:                         {}\n",
               IteradorValido<int>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Requires expression como valor booleano inline
//     Pode ser usada diretamente em static_assert ou if constexpr
//     sem definir um concept explicitamente.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
void inspecionar_tipo(const T&) {
    constexpr bool tem_size  = requires(T x) { x.size(); };
    constexpr bool tem_begin = requires(T x) { x.begin(); };
    constexpr bool eh_num    = std::is_arithmetic_v<T>;

    fmt::print("  {:<20} | tem .size()={} | tem .begin()={} | aritmetico={}\n",
               typeid(T).name(), tem_size, tem_begin, eh_num);
}

void demo_requires_inline() {
    fmt::print("\n── 2.5 Requires expression como valor booleano inline ──\n");
    fmt::print("  {:20} | tem .size() | tem .begin() | aritmético\n", "tipo");
    fmt::print("  {:-<20}   {:-<12}   {:-<13}   {:-<10}\n", "", "", "", "");

    inspecionar_tipo(42);
    inspecionar_tipo(3.14);
    inspecionar_tipo(std::string("x"));
    inspecionar_tipo(std::vector<int>{});
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_simple_requirement();
    demo_type_requirement();
    demo_compound_requirement();
    demo_nested_requirement();
    demo_requires_inline();
}

} // namespace demo_requires
