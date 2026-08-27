// =============================================================================
//  src/value_semantics/demo_value_categories.hpp
//
//  Categorias de Valor — lvalue, prvalue, xvalue
//  ──────────────────────────────────────────────
//  Toda expressão em C++ tem um tipo E uma categoria de valor. A categoria
//  responde duas perguntas independentes:
//    • tem identidade?  (dá para pegar o endereço / referenciar de novo?)
//    • pode ser movida? (o compilador pode roubar seus recursos?)
//
//  Tópicos:
//  • As 3 categorias primárias: lvalue, prvalue, xvalue
//  • As 2 categorias compostas: glvalue, rvalue
//  • Ligação: T&, const T&, T&&
//  • std::move não move nada — é só um cast
//  • decltype vs decltype(auto)
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <type_traits>
#include <utility>

namespace demo_value_categories {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O mapa das categorias
//
//                      expressão
//                      ┌───┴───┐
//                  glvalue    rvalue
//                  ┌──┴──┐    ┌──┴──┐
//              lvalue   xvalue    prvalue
//              (xvalue pertence aos dois ramos)
//
//     lvalue  : tem identidade, NÃO pode ser movido   → `x`, `*p`, `arr[0]`
//     xvalue  : tem identidade, PODE ser movido       → `std::move(x)`
//     prvalue : sem identidade, PODE ser movido       → `42`, `f()`, `T{}`
// ─────────────────────────────────────────────────────────────────────────────

// Três overloads que revelam a categoria do argumento em tempo de compilação.
// A resolução de overload prefere, nesta ordem: T& → T&& → const T&
void inspecionar(std::string&)        { fmt::print("  → lvalue (T&)\n"); }
void inspecionar(std::string&&)       { fmt::print("  → rvalue (T&&)\n"); }
void inspecionar(const std::string&)  { fmt::print("  → const lvalue (const T&)\n"); }

std::string cria_string() { return "vinda de uma função"; }

void demo_categorias() {
    fmt::print("\n── 1.1 Identificando categorias por overload ──\n");

    std::string nomeado = "sou nomeado";
    const std::string constante = "sou const";

    fmt::print("  inspecionar(nomeado)        ");
    inspecionar(nomeado);                 // lvalue: tem nome, tem endereço

    fmt::print("  inspecionar(constante)      ");
    inspecionar(constante);               // const lvalue

    fmt::print("  inspecionar(std::string{{}}) ");
    inspecionar(std::string("temporário")); // prvalue: temporário sem nome

    fmt::print("  inspecionar(cria_string())  ");
    inspecionar(cria_string());           // prvalue: retorno por valor

    fmt::print("  inspecionar(std::move(nom)) ");
    inspecionar(std::move(nomeado));      // xvalue: tem nome MAS foi marcado
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. A regra que mais confunde:
//     "um parâmetro do tipo T&& é, ele mesmo, um lvalue"
//
//     Se a expressão TEM NOME, ela é lvalue — não importa o tipo declarado.
// ─────────────────────────────────────────────────────────────────────────────
void recebe_rvalue(std::string&& s) {
    // `s` foi declarado como rvalue reference, mas AQUI DENTRO `s` tem nome.
    // Logo, a expressão `s` é um LVALUE.
    fmt::print("  dentro de recebe_rvalue(std::string&&), a expressão `s` é: ");
    inspecionar(s);                   // → lvalue!

    fmt::print("  já `std::move(s)` é:                                      ");
    inspecionar(std::move(s));        // → rvalue
}

void demo_nome_e_lvalue() {
    fmt::print("\n── 1.2 Se tem nome, é lvalue ──\n");
    recebe_rvalue("um temporário qualquer");
    fmt::print("  Consequência: repassar um T&& adiante SEM std::move faz cópia.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. std::move não move nada
//
//     A implementação real da libstdc++ é essencialmente esta:
// ─────────────────────────────────────────────────────────────────────────────
template <typename T>
constexpr std::remove_reference_t<T>&& meu_move(T&& t) noexcept {
    // Um static_cast. Só isso. Nenhum byte é copiado, nenhum recurso é roubado.
    return static_cast<std::remove_reference_t<T>&&>(t);
}

void demo_move_e_cast() {
    fmt::print("\n── 1.3 std::move é um cast, não uma operação ──\n");

    std::string s = "conteúdo intacto";

    // Fazer o cast NÃO esvazia a string. Nada acontece até que alguém
    // construa/atribua a partir do rvalue resultante.
    auto&& r = meu_move(s);
    (void)r;

    fmt::print("  após meu_move(s), s ainda é: '{}'\n", s);
    fmt::print("  o move só ocorre quando um construtor/atribuição de move é chamado\n");

    // AGORA sim: o construtor de move de std::string entra em ação.
    std::string destino = std::move(s);
    fmt::print("  após 'std::string destino = std::move(s)':\n");
    fmt::print("    destino = '{}'\n", destino);
    fmt::print("    s       = '{}'  ← válido porém não especificado\n", s);
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. decltype vs decltype(auto)
//
//     decltype(expr) preserva a categoria de valor na forma do tipo:
//       • lvalue  → T&
//       • xvalue  → T&&
//       • prvalue → T
// ─────────────────────────────────────────────────────────────────────────────
void demo_decltype() {
    fmt::print("\n── 1.4 decltype preserva a categoria ──\n");

    int x = 10;
    int& rx = x;

    static_assert(std::is_same_v<decltype(x),    int>);   // nome puro → o tipo declarado
    static_assert(std::is_same_v<decltype((x)),  int&>);  // (x) é expressão lvalue → int&
    static_assert(std::is_same_v<decltype(rx),   int&>);
    static_assert(std::is_same_v<decltype(std::move(x)), int&&>);  // xvalue → int&&
    static_assert(std::is_same_v<decltype(10),   int>);   // prvalue → int

    fmt::print("  decltype(x)            = int    (nome, usa o tipo declarado)\n");
    fmt::print("  decltype((x))          = int&   (parênteses → expressão lvalue!)\n");
    fmt::print("  decltype(std::move(x)) = int&&  (xvalue)\n");
    fmt::print("  decltype(10)           = int    (prvalue)\n");
    fmt::print("  ⚠  o par de parênteses extra muda o tipo — pegadinha clássica\n");
}

// `auto` DESCARTA referência e const; `decltype(auto)` PRESERVA tudo.
// Isto importa em wrappers genéricos que precisam repassar o retorno fielmente.
int  global = 7;
int& obtem_ref() { return global; }

void demo_decltype_auto() {
    fmt::print("\n── 1.5 auto vs decltype(auto) no retorno ──\n");

    auto           copia = obtem_ref();   // int   — a referência foi descartada
    decltype(auto) ref   = obtem_ref();   // int&  — a referência foi preservada

    copia = 999;   // mexe só na cópia local
    fmt::print("  após 'copia = 999':  global = {}\n", global);

    ref = 42;      // mexe no global de verdade
    fmt::print("  após 'ref = 42':     global = {}\n", global);

    static_assert(std::is_same_v<decltype(copia), int>);
    static_assert(std::is_same_v<decltype(ref),   int&>);
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Estender a vida de um temporário com referência
// ─────────────────────────────────────────────────────────────────────────────
struct Ruidoso {
    std::string tag;
    explicit Ruidoso(std::string t) : tag(std::move(t)) {
        fmt::print("    Ruidoso('{}') construído\n", tag);
    }
    ~Ruidoso() { fmt::print("    Ruidoso('{}') destruído\n", tag); }
};

Ruidoso faz_ruidoso() { return Ruidoso("temporário"); }

void demo_lifetime_extension() {
    fmt::print("\n── 1.6 Extensão de tempo de vida por referência ──\n");

    {
        fmt::print("  const Ruidoso& r = faz_ruidoso();\n");
        // Ligar um temporário a uma referência const (ou rvalue) estende a vida
        // dele até o fim do escopo da referência.
        const Ruidoso& r = faz_ruidoso();
        fmt::print("    ...ainda vivo aqui: tag = '{}'\n", r.tag);
        fmt::print("  saindo do escopo:\n");
    }

    fmt::print("  ⚠  a extensão NÃO é transitiva: ligar a um membro de um\n");
    fmt::print("     temporário retornado por função devolve referência pendurada.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_categorias();
    demo_nome_e_lvalue();
    demo_move_e_cast();
    demo_decltype();
    demo_decltype_auto();
    demo_lifetime_extension();
}

} // namespace demo_value_categories
