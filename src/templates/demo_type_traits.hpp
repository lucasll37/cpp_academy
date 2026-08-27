// =============================================================================
//  src/templates/demo_type_traits.hpp
//
//  <type_traits> e Computação em Compile-Time
//  ────────────────────────────────────────────
//  type_traits são templates que respondem PERGUNTAS sobre tipos em
//  compile-time: "T é inteiro?", "T é copiável?", "qual o tipo sem const?".
//
//  Tópicos:
//  • Categorias primárias de tipo (is_integral, is_pointer, ...)
//  • Categorias compostas (is_arithmetic, is_object, ...)
//  • Propriedades de tipo (is_const, is_trivial, sizeof via alignment)
//  • Transformações de tipo (remove_const, add_pointer, decay, ...)
//  • constexpr — funções e variáveis calculadas em compile-time
//  • Construindo seu próprio type trait
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <type_traits>
#include <string>
#include <vector>
#include <array>

namespace demo_type_traits {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Categorias primárias de tipo — "o que é T?"
// ─────────────────────────────────────────────────────────────────────────────
void demo_categorias_primarias() {
    fmt::print("\n── 5.1 Categorias primárias de tipo ──\n");

    // Helpers: _v é atalho para ::value (C++17)
    auto check = [](const char* nome, bool val) {
        fmt::print("  {:<35} {}\n", nome, val);
    };

    check("is_void<void>",                   std::is_void_v<void>);
    check("is_integral<int>",                std::is_integral_v<int>);
    check("is_integral<bool>",               std::is_integral_v<bool>);
    check("is_integral<char>",               std::is_integral_v<char>);
    check("is_integral<double>",             std::is_integral_v<double>);
    check("is_floating_point<float>",        std::is_floating_point_v<float>);
    check("is_floating_point<int>",          std::is_floating_point_v<int>);
    check("is_pointer<int*>",                std::is_pointer_v<int*>);
    check("is_pointer<int>",                 std::is_pointer_v<int>);
    check("is_array<int[5]>",                std::is_array_v<int[5]>);
    check("is_array<std::array<int,5>>",     std::is_array_v<std::array<int,5>>);
    check("is_function<int(double)>",        std::is_function_v<int(double)>);
    check("is_enum<std::byte>",              std::is_enum_v<std::byte>);
    check("is_class<std::string>",           std::is_class_v<std::string>);
    check("is_class<int>",                   std::is_class_v<int>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Propriedades de tipos — "T tem essa característica?"
// ─────────────────────────────────────────────────────────────────────────────
struct Trivial    { int x; double y; };                // POD-like
struct NaoTrivial { NaoTrivial() { /* lógica */ } int x; };
struct Final      final { };
struct Base       { virtual ~Base() = default; };
struct Derivada   : Base { };

void demo_propriedades() {
    fmt::print("\n── 5.2 Propriedades de tipos ──\n");

    auto check = [](const char* nome, bool val) {
        fmt::print("  {:<45} {}\n", nome, val);
    };

    check("is_const<const int>",             std::is_const_v<const int>);
    check("is_const<int>",                   std::is_const_v<int>);
    check("is_reference<int&>",              std::is_reference_v<int&>);
    check("is_lvalue_reference<int&>",       std::is_lvalue_reference_v<int&>);
    check("is_rvalue_reference<int&&>",      std::is_rvalue_reference_v<int&&>);

    fmt::print("\n");
    check("is_trivial<Trivial>",             std::is_trivial_v<Trivial>);
    check("is_trivial<NaoTrivial>",          std::is_trivial_v<NaoTrivial>);
    check("is_trivially_copyable<Trivial>",  std::is_trivially_copyable_v<Trivial>);
    check("is_standard_layout<Trivial>",     std::is_standard_layout_v<Trivial>);

    fmt::print("\n");
    check("is_abstract<Base>",              std::is_abstract_v<Base>);
    check("is_polymorphic<Base>",           std::is_polymorphic_v<Base>);
    check("is_polymorphic<Trivial>",        std::is_polymorphic_v<Trivial>);
    check("is_final<Final>",                std::is_final_v<Final>);
    check("is_base_of<Base,Derivada>",      std::is_base_of_v<Base, Derivada>);
    check("is_base_of<Derivada,Base>",      std::is_base_of_v<Derivada, Base>);

    fmt::print("\n");
    check("is_copy_constructible<std::string>",   std::is_copy_constructible_v<std::string>);
    check("is_move_constructible<std::string>",   std::is_move_constructible_v<std::string>);
    check("is_default_constructible<std::string>",std::is_default_constructible_v<std::string>);
    check("is_destructible<int>",                 std::is_destructible_v<int>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Transformações de tipo — "dê-me T sem const, com pointer, decayed..."
// ─────────────────────────────────────────────────────────────────────────────
void demo_transformacoes() {
    fmt::print("\n── 5.3 Transformações de tipo ──\n");

    // remove_const / add_const
    using T1 = std::remove_const_t<const int>;          // → int
    using T2 = std::add_const_t<int>;                   // → const int
    fmt::print("  remove_const<const int> == int:       {}\n",
               std::is_same_v<T1, int>);
    fmt::print("  add_const<int> == const int:          {}\n",
               std::is_same_v<T2, const int>);

    // remove_reference
    using T3 = std::remove_reference_t<int&>;           // → int
    using T4 = std::remove_reference_t<int&&>;          // → int
    fmt::print("  remove_reference<int&>  == int:       {}\n", std::is_same_v<T3, int>);
    fmt::print("  remove_reference<int&&> == int:       {}\n", std::is_same_v<T4, int>);

    // add_pointer / remove_pointer
    using T5 = std::add_pointer_t<int>;                 // → int*
    using T6 = std::remove_pointer_t<int*>;             // → int
    fmt::print("  add_pointer<int>    == int*:          {}\n", std::is_same_v<T5, int*>);
    fmt::print("  remove_pointer<int*> == int:          {}\n", std::is_same_v<T6, int>);

    // decay — o que acontece quando você passa T para uma função por valor
    // remove ref, remove const/volatile de escalares, array→pointer, func→pointer
    using T7 = std::decay_t<const int&>;                // → int
    using T8 = std::decay_t<int[5]>;                    // → int*  (array decay!)
    using T9 = std::decay_t<int(double)>;               // → int(*)(double)
    fmt::print("  decay<const int&> == int:             {}\n", std::is_same_v<T7, int>);
    fmt::print("  decay<int[5]>     == int*:            {}\n", std::is_same_v<T8, int*>);
    fmt::print("  decay<int(double)> == int(*)(double): {}\n",
               std::is_same_v<T9, int(*)(double)>);

    // conditional — escolhe tipo em compile-time (ternário de tipos)
    using T10 = std::conditional_t<true,  int, double>; // → int
    using T11 = std::conditional_t<false, int, double>; // → double
    fmt::print("  conditional<true,  int,double> == int:    {}\n", std::is_same_v<T10, int>);
    fmt::print("  conditional<false, int,double> == double: {}\n", std::is_same_v<T11, double>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. constexpr — cálculo em compile-time
//     Uma função constexpr pode ser avaliada em compile-time quando
//     seus argumentos também são conhecidos em compile-time.
// ─────────────────────────────────────────────────────────────────────────────

constexpr int fatorial(int n) {
    return (n <= 1) ? 1 : n * fatorial(n - 1);
}

constexpr int fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

// constexpr pode retornar tipos diferentes dependendo de template params
template <int N>
constexpr std::array<int, N> gerar_quadrados() {
    std::array<int, N> result{};
    for (int i = 0; i < N; ++i)
        result[i] = i * i;
    return result;
}

void demo_constexpr() {
    fmt::print("\n── 5.4 constexpr — cálculo em compile-time ──\n");

    // Tudo calculado em compile-time — zero custo em runtime
    constexpr int fat5  = fatorial(5);   // 120, calculado pelo compilador
    constexpr int fat10 = fatorial(10);  // 3628800
    constexpr int fib10 = fibonacci(10); // 55

    fmt::print("  fatorial(5)   = {} (em compile-time)\n", fat5);
    fmt::print("  fatorial(10)  = {} (em compile-time)\n", fat10);
    fmt::print("  fibonacci(10) = {} (em compile-time)\n", fib10);

    // Array inteiro calculado em compile-time
    constexpr auto quad = gerar_quadrados<6>();
    fmt::print("  quadrados[0..5]: ");
    for (auto v : quad) fmt::print("{} ", v);
    fmt::print("\n");

    // static_assert: falha de compilação se condição for falsa
    static_assert(fatorial(5) == 120,   "fatorial(5) deve ser 120");
    static_assert(fibonacci(10) == 55,  "fibonacci(10) deve ser 55");
    fmt::print("  static_assert OK — verificado em compile-time\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Construindo seu próprio type trait
//     Template + specialization + herança de true_type/false_type
// ─────────────────────────────────────────────────────────────────────────────

// Trait: é um container STL? (tem begin/end/size e value_type)
template <typename T, typename = void>
struct eh_container : std::false_type {};

template <typename T>
struct eh_container<T,
    std::void_t<
        typename T::value_type,
        decltype(std::declval<T>().begin()),
        decltype(std::declval<T>().end()),
        decltype(std::declval<T>().size())
    >
> : std::true_type {};

// Trait: é uma string ou string_view?
template <typename T>
struct eh_string : std::false_type {};
template <> struct eh_string<std::string>      : std::true_type {};
template <> struct eh_string<std::string_view> : std::true_type {};
template <> struct eh_string<const char*>      : std::true_type {};

// Função que usa os traits para comportamento diferente
template <typename T>
void inspecionar(const T& val) {
    fmt::print("  tipo: {} bytes | container={} | string={} | arithmetic={}\n",
               sizeof(T),
               eh_container<T>::value,
               eh_string<T>::value,
               std::is_arithmetic_v<T>);
    (void)val;
}

void demo_custom_trait() {
    fmt::print("\n── 5.5 Construindo type traits customizados ──\n");

    fmt::print("  int:               "); inspecionar(42);
    fmt::print("  std::string:       "); inspecionar(std::string("ola"));
    fmt::print("  std::vector<int>:  "); inspecionar(std::vector<int>{1,2,3});
    fmt::print("  std::array<int,3>: "); inspecionar(std::array<int,3>{});
    fmt::print("  const char*:       "); inspecionar("literal");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_categorias_primarias();
    demo_propriedades();
    demo_transformacoes();
    demo_constexpr();
    demo_custom_trait();
}

} // namespace demo_type_traits
