// =============================================================================
//  src/linkage/comum.hpp
//
//  O cabeçalho compartilhado — o palco do experimento
//  ──────────────────────────────────────────────────
//  Este arquivo é incluído por unidade_a.cpp E por unidade_b.cpp. Cada
//  declaração abaixo se comporta de um jeito diferente quando isso acontece,
//  e é exatamente essa diferença que o subprojeto mede.
//
//  Não há truque nenhum aqui: é um cabeçalho comum, incluído duas vezes em
//  unidades de tradução diferentes. Os endereços que os demos imprimem vêm
//  do linker, não de simulação.
// =============================================================================
#pragma once
#include <string>

// ⚠ As entidades de linkage INTERNA abaixo (static, namespace anônimo) são
//   usadas por unidade_a.cpp e unidade_b.cpp, mas NÃO por main.cpp — que
//   também inclui este cabeçalho. O GCC avisa, com razão, "defined but not
//   used": naquela TU existe mesmo uma cópia inútil.
//
//   Esse aviso É a lição desta pasta: cada TU que inclui o cabeçalho ganha a
//   sua própria cópia, use-a ou não. Suprimimos aqui porque a duplicação é o
//   objeto de estudo; em código de produção, o aviso deve ser corrigido.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

namespace comum {

// ── (1) `inline` → UMA definição no programa inteiro ────────────────────────
//    O linker funde todas as cópias. Endereço IGUAL nas duas TUs.
inline int funcao_inline() { return 1; }

// ── (2) `static` → linkage INTERNA: uma cópia POR unidade de tradução ──────
//    Endereço DIFERENTE em cada TU. Não é bug — é a definição de `static`.
static int funcao_static() { return 2; }

// ── (3) namespace anônimo → o mesmo efeito de `static`, e a forma moderna ──
namespace {
int funcao_anonima() { return 3; }
}

// ── (4) `inline` variável (C++17) → UM objeto no programa inteiro ──────────
inline int contador_inline = 0;

// ── (5) `static` variável em cabeçalho → uma cópia POR TU ⚠ ────────────────
//    Este é o bug clássico: parece uma global, e são N globais.
static int contador_static = 0;

// ── (6) `const` em escopo de namespace → linkage INTERNA por padrão em C++ ─
//    (em C seria externa — uma das divergências entre as duas linguagens)
const int kConstante = 42;

// ── (7) `constexpr` em escopo de namespace → implica `const`, logo interna ─
//    Para compartilhar UM objeto entre TUs, precisa de `inline constexpr`.
constexpr int  kConstexpr        = 43;
inline constexpr int kConstexprInline = 44;

// ── (8) template → instanciado em cada TU, e o linker funde ────────────────
template <typename T>
T identidade(T v) { return v; }

// ── Coletores: cada TU responde com os SEUS endereços ──────────────────────
struct Enderecos {
    std::string tu;
    const void* funcao_inline;
    const void* funcao_static;
    const void* funcao_anonima;
    const void* contador_inline;
    const void* contador_static;
    const void* constante;
    const void* constexpr_inline;
    const void* template_int;
};

Enderecos enderecos_de_a();
Enderecos enderecos_de_b();

// Cada TU incrementa os DOIS contadores o mesmo número de vezes.
void incrementa_em_a(int n);
void incrementa_em_b(int n);

int  contador_inline_visto_de_a();
int  contador_static_visto_de_a();
int  contador_inline_visto_de_b();
int  contador_static_visto_de_b();

}  // namespace comum

#pragma GCC diagnostic pop
