// =============================================================================
//  src/modules/geometria.cppm — uma interface de módulo
//
//  A extensão `.cppm` é convenção (Clang a reconhece; o GCC exige `-x c++`).
//  O que define um arquivo como interface de módulo é a declaração
//  `export module NOME;` na primeira linha significativa.
//
//  A diferença estrutural para um header:
//    header  → o texto é COPIADO em cada TU que o inclui, e recompilado
//    módulo  → é compilado UMA vez, e o consumidor lê a forma BINÁRIA (.gcm)
// =============================================================================
module;                        // ← início do "global module fragment"

// Aqui, ANTES do `export module`, vão os #include de headers antigos.
// É o único lugar onde eles podem aparecer numa interface de módulo.
#include <cmath>

export module geometria;       // ← a declaração que define o módulo

// ─────────────────────────────────────────────────────────────────────────────
//  Tudo que NÃO é exportado fica invisível para quem importa — inclusive os
//  nomes do `#include` acima. Isso é o oposto do header, em que tudo vaza.
// ─────────────────────────────────────────────────────────────────────────────
namespace detalhe {
// Nem `export`ada nem visível: um consumidor não consegue chamá-la, e o nome
// não colide com nada dele.
double quadrado(double x) { return x * x; }
}  // namespace detalhe

// ─────────────────────────────────────────────────────────────────────────────
//  A interface pública
// ─────────────────────────────────────────────────────────────────────────────
export namespace geometria {

struct Ponto {
    double x{};
    double y{};
};

double distancia(const Ponto& a, const Ponto& b) {
    return std::sqrt(detalhe::quadrado(b.x - a.x) + detalhe::quadrado(b.y - a.y));
}

double area_circulo(double raio) {
    return 3.14159265358979323846 * detalhe::quadrado(raio);
}

// Templates FUNCIONAM em módulos — o compilador guarda a árvore sintática
// no .gcm, não texto.
template <typename T>
T maior(T a, T b) { return a > b ? a : b; }

// Constante exportada. Repare em DOIS detalhes que não são estilo:
//   (a) ela está DENTRO do namespace exportado;
//   (b) ela é `inline`.
//
// ⚠ O `inline` é OBRIGATÓRIO, e a razão é uma regra do C++ que módulos apenas
//   tornam visível: uma variável `constexpr` em escopo de namespace tem
//   LINKAGE INTERNA por padrão (como um `const`). Linkage interna significa
//   "este nome só existe dentro da própria unidade de tradução" — e `export`
//   não muda isso. O resultado é uma variável que PARECE exportada e não está:
//
//     export namespace geometria {
//         constexpr double PI = 3.14;   // ✗ consumidor: 'PI' is not a member
//                                       //   of 'geometria'
//     }
//
//   O `inline` dá linkage externa E autoriza a definição repetida em várias
//   unidades sem violar a ODR. É a mesma regra que vale para headers desde o
//   C++17 — só que num header o sintoma é silencioso (uma cópia por TU) e num
//   módulo é erro de compilação. O erro é a boa notícia.
//
// ⚠ CORREÇÃO DE UM DIAGNÓSTICO ERRADO QUE ESTAVA NESTE ARQUIVO:
//   um comentário anterior atribuía esse comportamento a um BUG DO GCC 13
//   ligado ao global module fragment, afirmando que "sem o global module
//   fragment a mesma declaração funciona" e que "dentro de um namespace
//   exportado funciona nos dois casos". As duas afirmações são FALSAS.
//   Rodando os dez casos no GCC 13.3.0 (a matriz está no README, seção
//   "Armadilhas"), as colunas com e sem GMF são IDÊNTICAS:
//
//     export constexpr double PI = 3.14;                    → ✗ com e sem GMF
//     export inline constexpr double PI = 3.14;             → ✓ com e sem GMF
//     export namespace n { constexpr double PI = 3.14; }    → ✗ com e sem GMF
//     export namespace n { inline constexpr double PI; }    → ✓ com e sem GMF
//     export int contador = 7;   // linkage externa         → ✓ com e sem GMF
//
//   Não há bug do GCC aqui: há a regra de linkage interna, e só ela.
inline constexpr double PI = 3.14159265358979323846;

}  // namespace geometria
