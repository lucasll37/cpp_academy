// =============================================================================
//  src/templates/demo_sfinae.hpp
//
//  SFINAE — Substitution Failure Is Not An Error
//  ───────────────────────────────────────────────
//  Quando o compilador tenta substituir tipos nos parâmetros de um template
//  e a substituição produz código inválido, ele NÃO gera um erro — apenas
//  descarta silenciosamente aquela sobrecarga e tenta outras.
//
//  Isso permite selecionar implementações diferentes em compile-time
//  dependendo das propriedades dos tipos.
//
//  Tópicos:
//  • SFINAE clássico com std::enable_if
//  • enable_if no retorno, no parâmetro e no template parameter
//  • void_t e detection idiom (C++17)
//  • if constexpr — a alternativa moderna e legível ao SFINAE
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <type_traits>
#include <string>
#include <vector>
#include <iterator>

namespace demo_sfinae {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O problema que SFINAE resolve
//     Queremos uma função que se comporta diferente para inteiros e floats
// ─────────────────────────────────────────────────────────────────────────────

// ── Sem SFINAE: poderíamos usar overload manual, mas não escala ──────────────
void descrever_sem_sfinae(int)    { fmt::print("  inteiro\n"); }
void descrever_sem_sfinae(double) { fmt::print("  double\n"); }
// Problema: precisaríamos de uma versão para float, long, short, etc.

// ── Com SFINAE: uma implementação para todos os inteiros, outra para floats ──

// enable_if<condição, Tipo>::type só existe quando condição é true
// Se condição é false → "substituição falha" → sobrecarga descartada silenciosamente

// Versão para tipos inteiros (int, long, short, char, bool...)
template <typename T>
typename std::enable_if<std::is_integral<T>::value, void>::type
descrever(T val) {
    fmt::print("  integral: {} ({})\n", val, sizeof(T));
}

// Versão para tipos de ponto flutuante (float, double, long double)
template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, void>::type
descrever(T val) {
    fmt::print("  floating: {:.4f} ({})\n", static_cast<double>(val), sizeof(T));
}

void demo_enable_if_retorno() {
    fmt::print("\n── 4.1 enable_if no tipo de retorno ──\n");

    descrever(42);          // → integral
    descrever(true);        // → integral (bool é inteiro)
    descrever(3.14f);       // → floating
    descrever(2.71828);     // → floating
    descrever(100L);        // → integral
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. enable_if como parâmetro de template (sintaxe mais limpa)
// ─────────────────────────────────────────────────────────────────────────────

// Parâmetro de template com default — invisível para o chamador
template <
    typename T,
    typename = std::enable_if_t<std::is_arithmetic<T>::value>
>
T dobrar(T val) {
    return val * 2;
}

// Versão para strings — SEM overlap com dobrar acima (tipos não aritméticos)
template <
    typename T,
    typename = std::enable_if_t<std::is_same<T, std::string>::value>
>
std::string dobrar(T val) {
    return val + val;
}

void demo_enable_if_param() {
    fmt::print("\n── 4.2 enable_if como parâmetro de template ──\n");

    fmt::print("  dobrar(5)     = {}\n", dobrar(5));
    fmt::print("  dobrar(3.14)  = {:.2f}\n", dobrar(3.14));
    fmt::print("  dobrar(\"ab\") = '{}'\n", dobrar(std::string("ab")));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Detection idiom com void_t (C++17)
//     Detecta se um tipo possui determinado membro/método/typedef
//     Sem lançar erro — apenas retorna true/false em compile-time
// ─────────────────────────────────────────────────────────────────────────────

// void_t<...> se todos os tipos em ... forem válidos → void
// Se qualquer um for inválido → SFINAE descarta

// Detector: o tipo T tem .size() ?
template <typename T, typename = void>
struct tem_size : std::false_type {};

template <typename T>
struct tem_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

// Detector: o tipo T tem ::value_type ?
template <typename T, typename = void>
struct tem_value_type : std::false_type {};

template <typename T>
struct tem_value_type<T, std::void_t<typename T::value_type>>
    : std::true_type {};

// Detector: o tipo T tem operador + com ele mesmo?
template <typename T, typename = void>
struct tem_adicao : std::false_type {};

template <typename T>
struct tem_adicao<T, std::void_t<decltype(std::declval<T>() + std::declval<T>())>>
    : std::true_type {};

struct SemMembros { int x; };
struct ComSize    { std::size_t size() const { return 42; } };

void demo_detection() {
    fmt::print("\n── 4.3 Detection idiom com void_t ──\n");

    fmt::print("  tem_size<std::vector<int>>:  {}\n", tem_size<std::vector<int>>::value);
    fmt::print("  tem_size<std::string>:        {}\n", tem_size<std::string>::value);
    fmt::print("  tem_size<SemMembros>:         {}\n", tem_size<SemMembros>::value);
    fmt::print("  tem_size<ComSize>:            {}\n", tem_size<ComSize>::value);
    fmt::print("\n");
    fmt::print("  tem_value_type<std::vector<int>>: {}\n",
               tem_value_type<std::vector<int>>::value);
    fmt::print("  tem_value_type<int>:              {}\n",
               tem_value_type<int>::value);
    fmt::print("\n");
    fmt::print("  tem_adicao<int>:         {}\n", tem_adicao<int>::value);
    fmt::print("  tem_adicao<std::string>: {}\n", tem_adicao<std::string>::value);
    fmt::print("  tem_adicao<SemMembros>:  {}\n", tem_adicao<SemMembros>::value);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. if constexpr (C++17) — alternativa legível ao SFINAE para lógica interna
//     Ao contrário de if normal, o ramo NÃO compilado não é verificado.
//     Isso permite escrever código que seria inválido para alguns tipos.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
std::string para_string(const T& val) {
    if constexpr (std::is_same_v<T, std::string>) {
        // Este ramo só compila quando T = std::string
        return "\"" + val + "\"";
    } else if constexpr (std::is_arithmetic_v<T>) {
        // Este ramo só compila quando T é aritmético
        return std::to_string(val);
    } else if constexpr (std::is_pointer_v<T>) {
        // Ponteiro → endereço hexadecimal
        std::ostringstream oss;
        oss << static_cast<const void*>(val);
        return "ptr:" + oss.str();
    } else {
        // Para outros tipos, retorna info genérica
        return "[tipo desconhecido, " + std::to_string(sizeof(T)) + " bytes]";
    }
}

void demo_if_constexpr() {
    fmt::print("\n── 4.4 if constexpr (C++17) ──\n");

    fmt::print("  para_string(42)         = {}\n", para_string(42));
    fmt::print("  para_string(3.14)       = {}\n", para_string(3.14));
    fmt::print("  para_string(true)       = {}\n", para_string(true));
    fmt::print("  para_string(\"ola\")    = {}\n", para_string(std::string("ola")));

    int x = 7;
    fmt::print("  para_string(&x)         = {}\n", para_string(&x));
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. SFINAE vs if constexpr — quando usar cada um
// ─────────────────────────────────────────────────────────────────────────────
void demo_quando_usar() {
    fmt::print("\n── 4.5 SFINAE vs if constexpr — quando usar ──\n");
    fmt::print(
        "  SFINAE (enable_if / void_t):\n"
        "    ✓ Selecionar QUAL SOBRECARGA ou specialization usar\n"
        "    ✓ Detectar capacidades de tipos em traits reutilizáveis\n"
        "    ✓ APIs de biblioteca que precisam de overload resolution\n"
        "    ✗ Verboso e difícil de ler\n"
        "\n"
        "  if constexpr:\n"
        "    ✓ Lógica condicional DENTRO de uma função template\n"
        "    ✓ Muito mais legível que SFINAE para casos simples\n"
        "    ✓ O ramo descartado não precisa compilar\n"
        "    ✗ Não seleciona entre sobrecargas de funções diferentes\n"
        "\n"
        "  Regra prática: se a condição é sobre o COMPORTAMENTO interno\n"
        "  → if constexpr. Se é sobre QUAL função chamar → SFINAE/overload.\n"
    );
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_enable_if_retorno();
    demo_enable_if_param();
    demo_detection();
    demo_if_constexpr();
    demo_quando_usar();
}

} // namespace demo_sfinae
