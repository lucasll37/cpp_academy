// =============================================================================
//  src/templates/demo_variadic.hpp
//
//  Variadic Templates & Parameter Packs
//  ──────────────────────────────────────
//  Permitem templates com número ARBITRÁRIO de parâmetros de tipo.
//  São a base de: std::tuple, std::variant, std::make_unique,
//  std::bind, printf type-safe, e muitos outros.
//
//  Tópicos:
//  • sizeof...(Args) — conta parâmetros
//  • Recursão clássica (C++11/14)
//  • Fold expressions (C++17) — muito mais simples
//  • Perfect forwarding com variadic
//  • std::tuple por dentro (simplificado)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <tuple>
#include <utility>
#include <sstream>

namespace demo_variadic {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Sintaxe básica e sizeof...
// ─────────────────────────────────────────────────────────────────────────────

template <typename... Args>           // Args é um "parameter pack" de tipos
void contar_args(Args... args) {      // args é um "parameter pack" de valores
    fmt::print("  {} argumentos recebidos\n", sizeof...(args));
    // sizeof...(Args) e sizeof...(args) são equivalentes aqui
}

void demo_sizeof() {
    fmt::print("\n── 3.1 sizeof... ──\n");

    contar_args();                       // 0
    contar_args(1);                      // 1
    contar_args(1, 2.0, "três", true);   // 4
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Recursão clássica (C++11) — antes dos fold expressions
//     Técnica: caso-base + caso-recursivo
// ─────────────────────────────────────────────────────────────────────────────

// Caso base — sem argumentos
void imprimir_recursivo() {
    fmt::print("(fim)\n");
}

// Caso recursivo — retira o primeiro argumento, recursa no restante
template <typename T, typename... Rest>
void imprimir_recursivo(T primeiro, Rest... resto) {
    fmt::print("{}", primeiro);
    if constexpr (sizeof...(resto) > 0)
        fmt::print(", ");
    imprimir_recursivo(resto...);      // "..." expande o pack
}

// Soma recursiva clássica
template <typename T>
T somar(T val) { return val; }   // caso base

template <typename T, typename... Rest>
T somar(T primeiro, Rest... resto) {
    return primeiro + somar(resto...);
}

void demo_recursao() {
    fmt::print("\n── 3.2 Recursão clássica ──\n");

    fmt::print("  imprimir: ");
    imprimir_recursivo(1, 2.5, "ola", true);

    fmt::print("  somar(1,2,3,4,5) = {}\n", somar(1, 2, 3, 4, 5));
    fmt::print("  somar(0.5, 1.5, 2.0) = {:.1f}\n", somar(0.5, 1.5, 2.0));
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Fold Expressions (C++17) — a forma moderna e elegante
//
//  Sintaxe: ( pack op ... )        — fold unário à direita
//           ( ... op pack )        — fold unário à esquerda
//           ( pack op ... op init) — fold binário à direita
//           ( init op ... op pack) — fold binário à esquerda
// ─────────────────────────────────────────────────────────────────────────────

// Soma com fold — substitui toda a recursão acima
template <typename... Args>
auto somar_fold(Args... args) {
    return (args + ...);              // fold unário à direita: a+(b+(c+d))
}

// AND/OR lógico
template <typename... Args>
bool todos_verdadeiros(Args... args) {
    return (... && args);             // fold unário à esquerda: ((a&&b)&&c)
}

template <typename... Args>
bool algum_verdadeiro(Args... args) {
    return (... || args);
}

// Imprimir com fold — sem recursão!
template <typename... Args>
void imprimir_fold(Args... args) {
    ((fmt::print("{} ", args)), ...); // fold com vírgula: imprime cada um
    fmt::print("\n");
}

// Imprimir com separador usando fold binário
template <typename... Args>
void imprimir_com_sep(const char* sep, Args... args) {
    bool primeiro = true;
    auto imprimir_um = [&](auto val) {
        if (!primeiro) fmt::print("{}", sep);
        fmt::print("{}", val);
        primeiro = false;
    };
    (imprimir_um(args), ...);   // fold com vírgula expande para cada arg
    fmt::print("\n");
}

void demo_fold() {
    fmt::print("\n── 3.3 Fold expressions (C++17) ──\n");

    fmt::print("  somar_fold(1,2,3,4,5)       = {}\n", somar_fold(1, 2, 3, 4, 5));
    fmt::print("  somar_fold(0.5, 1.5, 2.0)   = {:.1f}\n", somar_fold(0.5, 1.5, 2.0));

    fmt::print("  todos(true,true,true)  = {}\n", todos_verdadeiros(true, true, true));
    fmt::print("  todos(true,false,true) = {}\n", todos_verdadeiros(true, false, true));
    fmt::print("  algum(false,false,true)= {}\n", algum_verdadeiro(false, false, true));

    fmt::print("  imprimir_fold: ");
    imprimir_fold(10, 20.5, "ola", true);

    fmt::print("  com sep \" | \": ");
    imprimir_com_sep(" | ", "alpha", "beta", "gamma");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Perfect forwarding com variadic — como std::make_unique funciona
//     std::forward preserva a categoria de valor (lvalue/rvalue) de cada arg
// ─────────────────────────────────────────────────────────────────────────────

struct Motor {
    int cilindros;
    double potencia;
    std::string combustivel;

    Motor(int c, double p, std::string comb)
        : cilindros(c), potencia(p), combustivel(std::move(comb)) {
        fmt::print("  [Motor] construído: {}cil, {:.0f}cv, {}\n",
                   cilindros, potencia, combustivel);
    }
};

// Nossa versão de make_unique usando perfect forwarding variadic
template <typename T, typename... Args>
T* meu_make(Args&&... args) {
    // std::forward<Args>(args)... expande preservando lvalue/rvalue de cada arg
    return new T(std::forward<Args>(args)...);
}

void demo_perfect_forward() {
    fmt::print("\n── 3.4 Perfect forwarding variadic ──\n");

    std::string comb = "gasolina";
    // comb é lvalue → forward como lvalue (cópia)
    // 4 e 300.0 são rvalues → forward como rvalue (move/sem cópia)
    Motor* m = meu_make<Motor>(4, 300.0, std::move(comb));
    fmt::print("  comb após move: '{}' (string movida)\n", comb);
    delete m;
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Tuple simplificada — como std::tuple funciona por dentro
//     Implementada via herança recursiva (cada nível guarda um elemento)
// ─────────────────────────────────────────────────────────────────────────────

// Caso base vazio
template <typename... Ts>
struct MinhaTuple {};

// Caso recursivo: herda do restante, armazena o primeiro
template <typename T, typename... Rest>
struct MinhaTuple<T, Rest...> : MinhaTuple<Rest...> {
    MinhaTuple(T val, Rest... resto)
        : MinhaTuple<Rest...>(resto...), cabeca(std::move(val)) {}
    T cabeca;
};

// get<0> — retorna o primeiro elemento
template <typename T, typename... Rest>
T& get_primeiro(MinhaTuple<T, Rest...>& t) {
    return t.cabeca;
}

void demo_tuple_simples() {
    fmt::print("\n── 3.5 Tuple simplificada (herança recursiva) ──\n");

    MinhaTuple<int, double, std::string> t(42, 3.14, "ola");
    fmt::print("  MinhaTuple<int,double,string>\n");
    fmt::print("  primeiro elemento: {}\n", get_primeiro(t));

    // A std::tuple real com std::get<N>
    auto st = std::make_tuple(99, 2.71, std::string("mundo"));
    fmt::print("  std::tuple: get<0>={}, get<1>={:.2f}, get<2>='{}'\n",
               std::get<0>(st), std::get<1>(st), std::get<2>(st));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_sizeof();
    demo_recursao();
    demo_fold();
    demo_perfect_forward();
    demo_tuple_simples();
}

} // namespace demo_variadic
