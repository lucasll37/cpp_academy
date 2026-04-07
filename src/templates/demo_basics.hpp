// =============================================================================
//  src/templates/demo_basics.hpp
//
//  Fundamentos de Templates
//  ─────────────────────────
//  Templates são o mecanismo de geração de código em compile-time do C++.
//  O compilador cria versões especializadas do código para cada combinação
//  de tipos usada — sem custo de runtime.
//
//  Tópicos:
//  • Function templates e dedução de tipo
//  • Class templates e membros
//  • Non-type template parameters (NTTP)
//  • Template template parameters
//  • Default template arguments
//  • Two-phase lookup e typename
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <array>
#include <string>
#include <vector>

namespace demo_basics {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Function template — o compilador deduz T a partir dos argumentos
// ─────────────────────────────────────────────────────────────────────────────

// Definição: T é um "parâmetro de tipo" — placeholder para qualquer tipo
template <typename T>
T maximo(T a, T b) {
    return (a > b) ? a : b;
}

// Template com múltiplos parâmetros de tipo
template <typename T, typename U>
auto soma(T a, U b) -> decltype(a + b) {  // trailing return type deduz o tipo resultante
    return a + b;
}

void demo_function_template() {
    fmt::print("\n── 1.1 Function templates ──\n");

    // O compilador instancia maximo<int>, maximo<double>, maximo<std::string>
    fmt::print("  maximo(3, 7)         = {}\n", maximo(3, 7));
    fmt::print("  maximo(3.14, 2.71)   = {}\n", maximo(3.14, 2.71));
    fmt::print("  maximo(\"abc\",\"xyz\") = {}\n", maximo(std::string("abc"), std::string("xyz")));

    // Especificação explícita quando a dedução é ambígua
    // maximo(3, 3.14);              // ✗ erro: T seria int ou double?
    fmt::print("  maximo<double>(3, 3.14) = {}\n", maximo<double>(3, 3.14));  // força T=double

    // Múltiplos tipos
    fmt::print("  soma(10, 3.14)       = {}\n", soma(10, 3.14));   // int+double = double
    fmt::print("  soma(1, 2)           = {}\n", soma(1, 2));        // int+int    = int
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Class template — estrutura de dados genérica
// ─────────────────────────────────────────────────────────────────────────────

// Pilha (stack) genérica — funciona para qualquer tipo T
template <typename T>
class Pilha {
public:
    void empurrar(const T& val) {
        dados_.push_back(val);
    }

    void empurrar(T&& val) {            // versão move — evita cópia desnecessária
        dados_.push_back(std::move(val));
    }

    T retirar() {
        T topo = std::move(dados_.back());
        dados_.pop_back();
        return topo;
    }

    const T& topo() const { return dados_.back(); }
    bool     vazia() const { return dados_.empty(); }
    std::size_t tamanho() const { return dados_.size(); }

private:
    std::vector<T> dados_;
};

void demo_class_template() {
    fmt::print("\n── 1.2 Class templates ──\n");

    Pilha<int> pi;
    pi.empurrar(1);
    pi.empurrar(2);
    pi.empurrar(3);
    fmt::print("  Pilha<int>: topo={}, tamanho={}\n", pi.topo(), pi.tamanho());
    fmt::print("  retirar: {}, {}, {}\n", pi.retirar(), pi.retirar(), pi.retirar());

    Pilha<std::string> ps;
    ps.empurrar("alpha");
    ps.empurrar("beta");
    fmt::print("  Pilha<string>: topo='{}'\n", ps.topo());

    // C++17: Class Template Argument Deduction (CTAD)
    // O compilador deduz T a partir do construtor — não precisa de <int>
    // (precisaria de um deduction guide ou construtor que informe o tipo)
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Non-Type Template Parameters (NTTP)
//     Template parametrizado por VALOR, não por tipo
//     Muito usado em: std::array<T,N>, matrizes em Eigen, buffers fixos
// ─────────────────────────────────────────────────────────────────────────────

// Buffer de tamanho fixo em compile-time — sem alocação dinâmica
template <typename T, std::size_t N>
class BufferFixo {
public:
    BufferFixo() : tamanho_(0) {}

    bool adicionar(const T& val) {
        if (tamanho_ >= N) return false;
        dados_[tamanho_++] = val;
        return true;
    }

    std::size_t tamanho()   const { return tamanho_; }
    std::size_t capacidade() const { return N; }      // N é conhecido em compile-time!
    const T& operator[](std::size_t i) const { return dados_[i]; }

private:
    std::array<T, N> dados_;   // alocado na stack — tamanho N em compile-time
    std::size_t tamanho_;
};

void demo_nttp() {
    fmt::print("\n── 1.3 Non-Type Template Parameters (NTTP) ──\n");

    BufferFixo<float, 4> buf;
    buf.adicionar(1.1f);
    buf.adicionar(2.2f);
    buf.adicionar(3.3f);
    fmt::print("  BufferFixo<float,4>: tamanho={}, cap={}\n",
               buf.tamanho(), buf.capacidade());
    fmt::print("  valores: ");
    for (std::size_t i = 0; i < buf.tamanho(); ++i)
        fmt::print("{:.1f} ", buf[i]);
    fmt::print("\n");

    // Tipos diferentes → instâncias DIFERENTES geradas pelo compilador
    BufferFixo<int, 8>  buf8;   // código separado de BufferFixo<float,4>
    BufferFixo<int, 16> buf16;  // código separado de BufferFixo<int,8>
    fmt::print("  sizeof(BufferFixo<int,8>)  = {} bytes\n", sizeof(buf8));
    fmt::print("  sizeof(BufferFixo<int,16>) = {} bytes\n", sizeof(buf16));
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Default template arguments — como std::vector<T, Allocator>
// ─────────────────────────────────────────────────────────────────────────────

// Segundo parâmetro tem valor default — o usuário pode sobrescrever
template <typename T, typename Container = std::vector<T>>
class FilaGenerica {
public:
    void enfileirar(const T& val) { dados_.push_back(val); }
    T    desenfileirar() {
        T frente = dados_.front();
        dados_.erase(dados_.begin());
        return frente;
    }
    bool vazia() const { return dados_.empty(); }
    std::size_t tamanho() const { return dados_.size(); }

private:
    Container dados_;
};

void demo_default_args() {
    fmt::print("\n── 1.4 Default template arguments ──\n");

    FilaGenerica<int> fila;              // Container = std::vector<int> (default)
    fila.enfileirar(10);
    fila.enfileirar(20);
    fila.enfileirar(30);
    fmt::print("  FilaGenerica<int>: tamanho={}\n", fila.tamanho());
    fmt::print("  desenfileirar: {}, {}\n", fila.desenfileirar(), fila.desenfileirar());
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. typename vs class — e quando typename é obrigatório
// ─────────────────────────────────────────────────────────────────────────────

// typename e class são equivalentes nos parâmetros de template.
// MAS: dentro do corpo do template, typename é OBRIGATÓRIO para
// indicar que um nome dependente é um tipo (não um valor estático).

template <typename Container>
void imprimir_primeiro(const Container& c) {
    // Container::value_type é um "nome dependente" — o compilador não sabe
    // se é um tipo ou um membro estático sem o typename explícito
    typename Container::value_type primeiro = c.front();
    fmt::print("  primeiro elemento: {}\n", primeiro);
}

void demo_typename() {
    fmt::print("\n── 1.5 typename em nomes dependentes ──\n");

    std::vector<int>         vi = {42, 1, 2};
    std::vector<std::string> vs = {"hello", "world"};

    imprimir_primeiro(vi);
    imprimir_primeiro(vs);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_function_template();
    demo_class_template();
    demo_nttp();
    demo_default_args();
    demo_typename();
}

} // namespace demo_basics
