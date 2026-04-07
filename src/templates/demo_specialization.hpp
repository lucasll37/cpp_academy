// =============================================================================
//  src/templates/demo_specialization.hpp
//
//  Template Specialization — full & partial
//  ─────────────────────────────────────────
//  O template primário define o comportamento geral.
//  Specializations definem comportamento alternativo para tipos específicos.
//
//  • Full specialization:    fixa TODOS os parâmetros    → template <>
//  • Partial specialization: fixa ALGUNS parâmetros      → template <...>
//    (só para class templates — function templates usam overload)
//
//  Casos de uso reais:
//  • std::vector<bool> — specialization famosa (controversa)
//  • std::hash<T>      — cada tipo provê sua própria hash
//  • type_traits       — is_pointer<T*>, is_array<T[]>, etc.
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <string>
#include <cstring>
#include <type_traits>

namespace demo_specialization {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Full specialization de function template
//     Problema: comparar strings com == compara ponteiros, não conteúdo
// ─────────────────────────────────────────────────────────────────────────────

// Template primário — funciona para int, double, etc.
template <typename T>
bool sao_iguais(const T& a, const T& b) {
    fmt::print("  [template primário] ");
    return a == b;
}

// Full specialization para const char* — compara conteúdo, não ponteiro
template <>
bool sao_iguais<const char*>(const char* const& a, const char* const& b) {
    fmt::print("  [specialization const char*] ");
    return std::strcmp(a, b) == 0;
}

void demo_full_func() {
    fmt::print("\n── 2.1 Full specialization de function template ──\n");

    fmt::print("  sao_iguais(1, 1):      {}\n", sao_iguais(1, 1));
    fmt::print("  sao_iguais(1.0, 2.0):  {}\n", sao_iguais(1.0, 2.0));

    const char* s1 = "hello";
    const char* s2 = "hello";
    // s1 e s2 podem ter endereços diferentes mesmo com o mesmo conteúdo
    fmt::print("  sao_iguais(\"hello\",\"hello\"): {}\n", sao_iguais(s1, s2));
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Full specialization de class template
//     Exemplo: serializador com comportamento radicalmente diferente por tipo
// ─────────────────────────────────────────────────────────────────────────────

// Template primário
template <typename T>
struct Serializador {
    static std::string serializar(const T& val) {
        return "[objeto: " + std::to_string(sizeof(T)) + " bytes]";
    }
    static std::string nome_tipo() { return "generico"; }
};

// Full specialization para int
template <>
struct Serializador<int> {
    static std::string serializar(const int& val) {
        return "int:" + std::to_string(val);
    }
    static std::string nome_tipo() { return "int"; }
};

// Full specialization para bool
template <>
struct Serializador<bool> {
    static std::string serializar(const bool& val) {
        return std::string("bool:") + (val ? "true" : "false");
    }
    static std::string nome_tipo() { return "bool"; }
};

// Full specialization para std::string
template <>
struct Serializador<std::string> {
    static std::string serializar(const std::string& val) {
        return "\"" + val + "\"";
    }
    static std::string nome_tipo() { return "string"; }
};

void demo_full_class() {
    fmt::print("\n── 2.2 Full specialization de class template ──\n");

    fmt::print("  int 42:      {}\n", Serializador<int>::serializar(42));
    fmt::print("  bool true:   {}\n", Serializador<bool>::serializar(true));
    fmt::print("  string:      {}\n", Serializador<std::string>::serializar("ola"));
    fmt::print("  double 3.14: {}\n", Serializador<double>::serializar(3.14)); // primário
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Partial specialization de class template
//     Fixa PARTE dos parâmetros — o restante continua genérico
// ─────────────────────────────────────────────────────────────────────────────

// Template primário — par genérico
template <typename T, typename U>
struct Par {
    T primeiro;
    U segundo;
    void imprimir() const {
        fmt::print("  Par<T,U>: ({}, {})\n", primeiro, segundo);
    }
    const char* descricao() const { return "Par genérico"; }
};

// Partial specialization: ambos os tipos são iguais
template <typename T>
struct Par<T, T> {
    T primeiro;
    T segundo;
    void imprimir() const {
        fmt::print("  Par<T,T> (mesmo tipo): ({}, {})\n", primeiro, segundo);
    }
    const char* descricao() const { return "Par homogêneo"; }

    // Método extra disponível só quando tipos são iguais
    T media() const { return (primeiro + segundo) / T(2); }
};

// Partial specialization: segundo tipo é ponteiro para o primeiro
template <typename T>
struct Par<T, T*> {
    T  valor;
    T* ponteiro;
    void imprimir() const {
        fmt::print("  Par<T,T*>: valor={}, *ptr={}\n", valor, *ponteiro);
    }
    const char* descricao() const { return "Par valor+ponteiro"; }
};

void demo_partial() {
    fmt::print("\n── 2.3 Partial specialization ──\n");

    Par<int, double> p1 { 1, 3.14 };
    p1.imprimir();
    fmt::print("  descricao: {}\n", p1.descricao());

    Par<int, int> p2 { 4, 6 };
    p2.imprimir();
    fmt::print("  media: {}\n", p2.media());   // só disponível na specialization T,T
    fmt::print("  descricao: {}\n", p2.descricao());

    int x = 99;
    Par<int, int*> p3 { 42, &x };
    p3.imprimir();
    fmt::print("  descricao: {}\n", p3.descricao());
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Partial specialization para ponteiros e arrays
//     Como a STL implementa is_pointer, is_array, etc.
// ─────────────────────────────────────────────────────────────────────────────

// Nossa versão simplificada de type trait
template <typename T>
struct eh_ponteiro {
    static constexpr bool valor = false;
    static const char*    nome() { return "não-ponteiro"; }
};

template <typename T>
struct eh_ponteiro<T*> {           // partial spec: qualquer tipo T*
    static constexpr bool valor = true;
    static const char*    nome() { return "ponteiro"; }
};

template <typename T>
struct eh_ponteiro<T* const> {     // partial spec: ponteiro const
    static constexpr bool valor = true;
    static const char*    nome() { return "ponteiro const"; }
};

void demo_partial_ponteiro() {
    fmt::print("\n── 2.4 Partial spec para ponteiros (como type_traits) ──\n");

    fmt::print("  eh_ponteiro<int>:         {} ({})\n",
               eh_ponteiro<int>::valor, eh_ponteiro<int>::nome());
    fmt::print("  eh_ponteiro<int*>:        {} ({})\n",
               eh_ponteiro<int*>::valor, eh_ponteiro<int*>::nome());
    fmt::print("  eh_ponteiro<int* const>:  {} ({})\n",
               eh_ponteiro<int* const>::valor, eh_ponteiro<int* const>::nome());
    fmt::print("  eh_ponteiro<double*>:     {} ({})\n",
               eh_ponteiro<double*>::valor, eh_ponteiro<double*>::nome());

    // Compare com a versão da STL
    fmt::print("  std::is_pointer<int*>::value:    {}\n", std::is_pointer<int*>::value);
    fmt::print("  std::is_pointer<double>::value:  {}\n", std::is_pointer<double>::value);
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Especialização e prioridade — qual versão o compilador escolhe?
//     Regra: mais especializada ganha.
//     Empate entre parciais → ambiguidade → erro de compilação.
// ─────────────────────────────────────────────────────────────────────────────

template <typename T, typename U> struct Prioridade {
    static const char* qual() { return "primário <T,U>"; }
};
template <typename T>             struct Prioridade<T, T> {
    static const char* qual() { return "parcial <T,T>"; }
};
template <typename T>             struct Prioridade<T, int> {
    static const char* qual() { return "parcial <T,int>"; }
};
template <>                       struct Prioridade<int, int> {
    static const char* qual() { return "full <int,int>  ← ganha (mais específica)"; }
};

void demo_prioridade() {
    fmt::print("\n── 2.5 Prioridade de specialization ──\n");

    fmt::print("  <double,float>: {}\n", Prioridade<double, float>::qual());
    fmt::print("  <double,double>: {}\n", Prioridade<double, double>::qual());
    fmt::print("  <double,int>:    {}\n", Prioridade<double, int>::qual());
    fmt::print("  <int,int>:       {}\n", Prioridade<int, int>::qual());
    // <int,int> poderia match <T,T> ou <T,int> — ambas seriam ambíguas,
    // mas a full specialization tem prioridade sobre qualquer parcial.
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_full_func();
    demo_full_class();
    demo_partial();
    demo_partial_ponteiro();
    demo_prioridade();
}

} // namespace demo_specialization
