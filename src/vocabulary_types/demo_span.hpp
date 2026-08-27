// =============================================================================
//  src/vocabulary_types/demo_span.hpp
//
//  std::span<T> — a visão sobre uma sequência contígua
//  ───────────────────────────────────────────────────
//  A assinatura mais comum do C legado é o par `(ponteiro, tamanho)`:
//
//      void processar(int* dados, size_t n);
//
//  Ela funciona e é rápida, mas separa duas informações que só fazem sentido
//  juntas. Nada impede passar o tamanho errado, e a função não consegue ser
//  usada em `for (auto x : ...)` nem nos algoritmos da STL.
//
//  `span` é exatamente esse par, unificado num tipo que é um RANGE. Não possui
//  os dados, custa dois ponteiros (ou um ponteiro e nada, na versão estática) e
//  aceita array C, `std::array`, `std::vector` e qualquer coisa contígua.
//
//  Tópicos:
//  • O problema (ponteiro, tamanho) e como span resolve
//  • Extent dinâmico vs estático — e o sizeof de cada um
//  • subspan, first, last — janelas sem cópia
//  • Por que span é borrowed_range (e string_view também)
//  • const-correctness: span<const T> vs const span<T>
//  • ⚠ as mesmas armadilhas de tempo de vida do string_view
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

namespace demo_span {

// ─────────────────────────────────────────────────────────────────────────────
//  1. O problema que span resolve
// ─────────────────────────────────────────────────────────────────────────────

// ✗ A forma antiga: dois parâmetros que precisam concordar, e ninguém verifica
int soma_antiga(const int* dados, std::size_t n) {
    int total = 0;
    for (std::size_t i = 0; i < n; ++i) total += dados[i];
    return total;
}

// ✓ Um parâmetro, e ele é um range
int soma_span(std::span<const int> dados) {
    return std::accumulate(dados.begin(), dados.end(), 0);
}

void demo_o_problema() {
    fmt::print("\n── 5.1 O problema (ponteiro, tamanho) ──\n");

    int          bruto[5]{1, 2, 3, 4, 5};
    std::array   arr{10, 20, 30};
    std::vector  vec{100, 200};

    fmt::print("      soma_antiga precisa do tamanho à mão:\n");
    fmt::print("        array C   : {}\n", soma_antiga(bruto, 5));
    fmt::print("        std::array: {}\n", soma_antiga(arr.data(), arr.size()));
    fmt::print("        vector    : {}\n", soma_antiga(vec.data(), vec.size()));
    fmt::print("        ⚠  `soma_antiga(bruto, 500)` compila e lê fora do array\n");

    fmt::print("      soma_span aceita os três direto, sem tamanho:\n");
    fmt::print("        array C   : {}\n", soma_span(bruto));
    fmt::print("        std::array: {}\n", soma_span(arr));
    fmt::print("        vector    : {}\n", soma_span(vec));
    fmt::print("      ↑ o tamanho vem do próprio objeto — impossível errar.\n");

    // E como span é um range, o corpo da função pode usar range-for e <ranges>
    fmt::print("      dentro da função, `dados` é um range de verdade:\n");
    fmt::print("        maior elemento de {} = {}\n", fmt::join(vec, ","),
               *std::ranges::max_element(std::span<const int>{vec}));
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Extent dinâmico vs estático
//
//     span<T>          → tamanho em RUNTIME: guarda ponteiro + tamanho
//     span<T, N>       → tamanho em COMPILAÇÃO: guarda só o ponteiro
//
//     A versão estática é mais barata e permite checagem em compilação, mas só
//     serve quando o tamanho é fixo e conhecido.
// ─────────────────────────────────────────────────────────────────────────────
void processar_qualquer(std::span<const double> v) {
    fmt::print("        dinâmico: {} elementos\n", v.size());
}

void processar_exatamente_3(std::span<const double, 3> v) {
    // Aqui `v.size()` é constante de compilação
    static_assert(v.extent == 3);
    fmt::print("        estático: {} elementos (extent é constexpr)\n", v.size());
}

void demo_extent() {
    fmt::print("\n── 5.2 Extent dinâmico vs estático ──\n");

    fmt::print("      sizeof(span<int>)     = {}  (ponteiro + tamanho)\n",
               sizeof(std::span<int>));
    fmt::print("      sizeof(span<int, 3>)  = {}  (só o ponteiro — o 3 é do tipo)\n",
               sizeof(std::span<int, 3>));
    fmt::print("      sizeof(vector<int>)   = {}  (ponteiro + size + capacity)\n",
               sizeof(std::vector<int>));

    std::array<double, 3> tres{1.0, 2.0, 3.0};
    std::vector<double>   muitos{1, 2, 3, 4, 5};

    fmt::print("      com std::array<double,3>:\n");
    processar_qualquer(tres);
    processar_exatamente_3(tres);          // ✓ o tamanho bate em compilação

    fmt::print("      com std::vector<double> de 5:\n");
    processar_qualquer(muitos);
    // processar_exatamente_3(muitos);     // ✗ não compila: extent desconhecido
    fmt::print("        processar_exatamente_3(vector) → NÃO COMPILA\n");
    fmt::print("        (o vector não carrega o tamanho no tipo)\n");

    // Converter estático → dinâmico é implícito; o contrário exige cast
    std::span<double, 3> est{tres};
    std::span<double>    din = est;              // ✓ implícito
    fmt::print("      span<T,3> → span<T> é implícito (size={})\n", din.size());

    auto de_volta = din.first<3>();               // ✓ explícito, e você garante
    static_assert(de_volta.extent == 3);
    fmt::print("      span<T> → span<T,3> via .first<3>() (você assume o risco)\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Janelas: subspan, first, last
//
//     Nenhuma delas copia nada — todas devolvem outro span apontando para
//     dentro do mesmo buffer.
// ─────────────────────────────────────────────────────────────────────────────
void demo_janelas() {
    fmt::print("\n── 5.3 Janelas sem cópia ──\n");

    std::vector<int> dados(12);
    std::iota(dados.begin(), dados.end(), 1);      // 1..12
    std::span<int> tudo{dados};

    fmt::print("      base            : {}\n", fmt::join(tudo, " "));
    fmt::print("      first(4)        : {}\n", fmt::join(tudo.first(4), " "));
    fmt::print("      last(4)         : {}\n", fmt::join(tudo.last(4), " "));
    fmt::print("      subspan(4, 4)   : {}\n", fmt::join(tudo.subspan(4, 4), " "));
    fmt::print("      subspan(8)      : {}\n", fmt::join(tudo.subspan(8), " "));

    // Uso real: processar um buffer em blocos, sem copiar nem alocar
    fmt::print("      processando em blocos de 5:\n");
    for (std::size_t i = 0; i < tudo.size(); i += 5) {
        auto bloco = tudo.subspan(i, std::min<std::size_t>(5, tudo.size() - i));
        fmt::print("        bloco de {} → soma {}\n", bloco.size(),
                   std::accumulate(bloco.begin(), bloco.end(), 0));
    }

    // O span não é const: escrever através dele muda o vetor original
    tudo.subspan(0, 3)[0] = 999;
    fmt::print("      após escrever pela janela: dados[0] = {}\n", dados[0]);
    fmt::print("      ↑ span é uma VISÃO mutável, não uma cópia.\n");

    fmt::print("      ⚠  subspan fora dos limites é UB, não exceção. As versões\n");
    fmt::print("         com template (`first<4>()`) checam em compilação quando\n");
    fmt::print("         o extent é estático — prefira essas quando puder.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. const-correctness — a distinção que confunde
//
//     span<const T>  → não posso MUDAR os elementos (o que você quase sempre quer)
//     const span<T>  → não posso mudar o SPAN (a janela), mas posso mudar os
//                      elementos. Quase nunca é o que se quer.
// ─────────────────────────────────────────────────────────────────────────────
void demo_const() {
    fmt::print("\n── 5.4 span<const T> vs const span<T> ──\n");

    std::vector<int> v{1, 2, 3};

    std::span<const int> elementos_const{v};
    const std::span<int> janela_const{v};

    // elementos_const[0] = 9;        // ✗ não compila: elementos são const
    janela_const[0] = 9;              // ✓ compila! o span é const, os dados não

    fmt::print("      span<const int> el{{v}};   el[0] = 9;  → NÃO COMPILA\n");
    fmt::print("      const span<int> ja{{v}};   ja[0] = 9;  → compila (v[0]={})\n", v[0]);

    fmt::print("\n      | quer proteger…    | escreva          |\n");
    fmt::print("      |-------------------|------------------|\n");
    fmt::print("      | os elementos      | span<const T>    |  ← 95% dos casos\n");
    fmt::print("      | a janela          | const span<T>    |\n");
    fmt::print("      | ambos             | const span<const T> |\n");

    fmt::print("\n      Numa assinatura de função, `std::span<const T>` é o default:\n");
    fmt::print("        void ler(std::span<const int> v);    // só lê\n");
    fmt::print("        void preencher(std::span<int> v);     // escreve\n");
    fmt::print("      A assinatura já documenta a intenção.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Por que span é borrowed_range
//
//     Um `borrowed_range` é um range cujos ITERADORES continuam válidos mesmo
//     depois de o objeto range morrer. Isso vale para span e string_view
//     porque eles não possuem nada — os iteradores apontam para o buffer alheio.
//
//     A consequência prática é que devolver um span de função é seguro (desde
//     que o BUFFER continue vivo), enquanto devolver uma view de um vector
//     local não é.
// ─────────────────────────────────────────────────────────────────────────────
void demo_borrowed() {
    fmt::print("\n── 5.5 span é borrowed_range ──\n");

    fmt::print("      std::ranges::borrowed_range<...>:\n");
    fmt::print("        span<int>          : {}\n", std::ranges::borrowed_range<std::span<int>>);
    fmt::print("        string_view        : {}\n", std::ranges::borrowed_range<std::string_view>);
    fmt::print("        vector<int>        : {}  ← o objeto POSSUI os dados\n",
               std::ranges::borrowed_range<std::vector<int>>);
    fmt::print("        vector<int>&       : {}  ← a referência não possui\n",
               std::ranges::borrowed_range<std::vector<int>&>);

    fmt::print("\n      É por isso que os algoritmos de <ranges> aceitam um span\n");
    fmt::print("      temporário e devolvem um iterador utilizável, mas recusam\n");
    fmt::print("      um vector temporário (devolvem std::ranges::dangling).\n");
    fmt::print("      Ver src/ranges/demo_pipelines.hpp para essa proteção.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. ⚠ As armadilhas de tempo de vida
// ─────────────────────────────────────────────────────────────────────────────
// ✗ Devolver um span para um LOCAL: o buffer morre, o span fica pendurado.
//   (a função está comentada de propósito — o GCC até avisa com -Wreturn-local-addr)
//
// std::span<int> perigoso() {
//     std::vector<int> local{1, 2, 3};
//     return local;                     // ← dangling
// }

// ✓ Devolver um span para um buffer que sobrevive à chamada
class Buffer {
public:
    Buffer() : dados_(16) { std::iota(dados_.begin(), dados_.end(), 0); }

    // Seguro: o span aponta para o membro, cuja vida é a do Buffer.
    std::span<const int> ver() const { return dados_; }

    // Ainda mais explícito sobre a dependência:
    std::span<const int> ver_janela(std::size_t inicio, std::size_t n) const {
        if (inicio + n > dados_.size()) return {};      // devolve span vazio
        return std::span<const int>{dados_}.subspan(inicio, n);
    }

private:
    std::vector<int> dados_;
};

void demo_lifetime() {
    fmt::print("\n── 5.6 ⚠ Tempo de vida ──\n");

    Buffer b;
    fmt::print("      Buffer::ver() → {} elementos: {}\n",
               b.ver().size(), fmt::join(b.ver().first(6), " "));
    fmt::print("      ver_janela(4,4) → {}\n", fmt::join(b.ver_janela(4, 4), " "));
    fmt::print("      ver_janela(14,8) → vazio (fora dos limites): size={}\n",
               b.ver_janela(14, 8).size());

    fmt::print("\n      As três regras, iguais às do string_view:\n");
    fmt::print("        1. span é PARÂMETRO — o uso dominante e sempre seguro\n");
    fmt::print("        2. span como RETORNO só se o buffer sobrevive à chamada\n");
    fmt::print("        3. span como MEMBRO exige documentar quem é o dono\n");

    fmt::print("\n      E a armadilha que o compilador NÃO pega:\n");
    fmt::print("        std::span<int> s{{v}};\n");
    fmt::print("        v.push_back(4);      // realocou → s está pendurado\n");
    fmt::print("        s[0] = 1;            // heap-use-after-free\n");
    fmt::print("      É a mesma invalidação de iteradores de src/stl/ — um span\n");
    fmt::print("      é tão frágil quanto um par de iteradores, porque É isso.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_o_problema();
    demo_extent();
    demo_janelas();
    demo_const();
    demo_borrowed();
    demo_lifetime();
}

} // namespace demo_span
