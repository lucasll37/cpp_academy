// =============================================================================
//  src/stl/demo_iterators.hpp
//
//  Iteradores — a cola entre containers e algoritmos
//  ─────────────────────────────────────────────────
//  A STL não tem N containers × M algoritmos = N×M implementações. Ela tem
//  N containers + M algoritmos, e os iteradores no meio. Um algoritmo não sabe
//  em que container opera; ele só sabe o que o iterador PROMETE saber fazer.
//
//  Essa promessa é a CATEGORIA do iterador:
//
//    input        → ler, avançar 1×, passagem única     (istream_iterator)
//    forward      → ler/escrever, múltiplas passagens   (forward_list)
//    bidirectional→ tudo acima + retroceder             (list, map, set)
//    random access→ tudo acima + saltar em O(1), it+n   (vector, deque, array)
//    contiguous   → tudo acima + memória contígua (C++20) (vector, array, span)
//
//  Tópicos:
//  • As categorias e o que cada uma habilita
//  • iterator_traits e como um algoritmo consulta a categoria
//  • Escrever um iterador STL-compatível do zero
//  • Iteradores adaptadores: reverse, insert, stream
//  • Sentinelas (C++20) — end() que não é do mesmo tipo
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <list>
#include <numeric>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace demo_iterators {

// ─────────────────────────────────────────────────────────────────────────────
//  1. Consultando a categoria em tempo de compilação
// ─────────────────────────────────────────────────────────────────────────────
template <typename It>
constexpr const char* nome_da_categoria() {
    using cat = typename std::iterator_traits<It>::iterator_category;
    if constexpr (std::is_same_v<cat, std::random_access_iterator_tag>) return "random access";
    else if constexpr (std::is_same_v<cat, std::bidirectional_iterator_tag>) return "bidirectional";
    else if constexpr (std::is_same_v<cat, std::forward_iterator_tag>) return "forward";
    else if constexpr (std::is_same_v<cat, std::input_iterator_tag>) return "input";
    else return "outra";
}

void demo_categorias() {
    fmt::print("\n── 2.1 Categorias de iterador ──\n");

    fmt::print("      vector<int>::iterator  → {}\n",
               nome_da_categoria<std::vector<int>::iterator>());
    fmt::print("      list<int>::iterator    → {}\n",
               nome_da_categoria<std::list<int>::iterator>());
    fmt::print("      istream_iterator<int>  → {}\n",
               nome_da_categoria<std::istream_iterator<int>>());

    // A categoria decide o que COMPILA:
    std::vector<int> v{1, 2, 3, 4, 5};
    std::list<int>   l{1, 2, 3, 4, 5};

    auto itv = v.begin();
    fmt::print("      vector: it + 3 → {} (salto O(1), só random access permite)\n",
               *(itv + 3));

    auto itl = l.begin();
    // *(itl + 3);                    // ✗ erro: list não tem operator+
    std::advance(itl, 3);             // ✓ genérico: usa ++ n vezes se preciso
    fmt::print("      list:   std::advance(it,3) → {} (percorre 3 nós)\n", *itl);

    fmt::print("      `std::advance`/`std::distance` funcionam para TODAS as\n");
    fmt::print("      categorias — em O(1) se random access, O(n) caso contrário.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Como um algoritmo se especializa pela categoria
//
//     `std::distance` é o exemplo canônico: subtração direta se o iterador
//     permitir, contagem elemento a elemento se não.
// ─────────────────────────────────────────────────────────────────────────────
template <typename It>
typename std::iterator_traits<It>::difference_type minha_distancia(It primeiro, It ultimo) {
    using cat = typename std::iterator_traits<It>::iterator_category;

    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, cat>) {
        fmt::print("        [caminho O(1): subtração direta]\n");
        return ultimo - primeiro;
    } else {
        fmt::print("        [caminho O(n): contando de um em um]\n");
        typename std::iterator_traits<It>::difference_type n = 0;
        while (primeiro != ultimo) { ++primeiro; ++n; }
        return n;
    }
}

void demo_despacho_por_tag() {
    fmt::print("\n── 2.2 Despacho por categoria ──\n");

    std::vector<int> v(1000);
    std::list<int>   l(1000);

    fmt::print("      minha_distancia em vector:\n");
    fmt::print("        = {}\n", minha_distancia(v.begin(), v.end()));
    fmt::print("      minha_distancia em list:\n");
    fmt::print("        = {}\n", minha_distancia(l.begin(), l.end()));

    fmt::print("      A MESMA função template gera dois códigos diferentes.\n");
    fmt::print("      Antes do C++17 isto era feito com tag dispatch (overloads\n");
    fmt::print("      recebendo a tag); hoje `if constexpr` basta.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Escrevendo um iterador STL-compatível
//
//     Um contador preguiçoso: não guarda os números, gera sob demanda.
//     Para ser um iterador de verdade, precisa dos 5 typedefs e dos operadores.
// ─────────────────────────────────────────────────────────────────────────────
class Intervalo {
public:
    class iterator {
    public:
        // Os 5 typedefs que `std::iterator_traits` procura.
        using iterator_category = std::forward_iterator_tag;
        using value_type        = int;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const int*;
        using reference         = const int&;

        explicit iterator(int v, int passo) : valor_(v), passo_(passo) {}

        reference operator*()  const { return valor_; }
        pointer   operator->() const { return &valor_; }

        iterator& operator++()    { valor_ += passo_; return *this; }        // pré
        iterator  operator++(int) { auto t = *this; ++(*this); return t; }   // pós

        // Em C++20 basta o ==; o != é gerado automaticamente.
        bool operator==(const iterator& o) const { return valor_ >= o.valor_; }

    private:
        int valor_;
        int passo_;
    };

    Intervalo(int inicio, int fim, int passo = 1)
        : inicio_(inicio), fim_(fim), passo_(passo) {}

    iterator begin() const { return iterator(inicio_, passo_); }
    iterator end()   const { return iterator(fim_,    passo_); }

private:
    int inicio_, fim_, passo_;
};

void demo_iterador_proprio() {
    fmt::print("\n── 2.3 Um iterador escrito do zero ──\n");

    fmt::print("      range-for direto:      ");
    for (int x : Intervalo(0, 10, 2)) fmt::print("{} ", x);
    fmt::print("\n");

    // Como ele satisfaz o contrato, os algoritmos da STL simplesmente funcionam
    Intervalo r(1, 6);
    fmt::print("      std::accumulate:       {}\n",
               std::accumulate(r.begin(), r.end(), 0));
    fmt::print("      std::count_if (pares): {}\n",
               std::count_if(r.begin(), r.end(), [](int x) { return x % 2 == 0; }));

    std::vector<int> copiado;
    std::copy(r.begin(), r.end(), std::back_inserter(copiado));
    fmt::print("      std::copy → vector:    ");
    for (int x : copiado) fmt::print("{} ", x);
    fmt::print("\n");

    fmt::print("      Nenhum número foi armazenado: o iterador GERA sob demanda.\n");
    fmt::print("      É a mesma ideia por trás de `std::views::iota` (ver src/ranges/).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Iteradores adaptadores
//
//     Um iterador não precisa apontar para memória. Ele pode ser uma FACHADA
//     que transforma escrita em outra operação — inserir, imprimir, ler.
// ─────────────────────────────────────────────────────────────────────────────
void demo_adaptadores() {
    fmt::print("\n── 2.4 Iteradores adaptadores ──\n");

    std::vector<int> origem{1, 2, 3, 4, 5};

    // reverse_iterator — percorre ao contrário
    fmt::print("      rbegin/rend:        ");
    for (auto it = origem.rbegin(); it != origem.rend(); ++it) fmt::print("{} ", *it);
    fmt::print("\n");

    // back_insert_iterator — "escrever" vira push_back
    std::vector<int> destino;
    std::copy(origem.begin(), origem.end(), std::back_inserter(destino));
    fmt::print("      back_inserter:      destino tem {} elementos\n", destino.size());
    fmt::print("      ↑ sem ele, std::copy escreveria num vector VAZIO → UB\n");

    // ostream_iterator — "escrever" vira <<
    std::ostringstream oss;
    std::copy(origem.begin(), origem.end(), std::ostream_iterator<int>(oss, ","));
    fmt::print("      ostream_iterator:   {}\n", oss.str());

    // istream_iterator — "ler" vira >>
    std::istringstream iss("10 20 30 40");
    std::vector<int> lidos{std::istream_iterator<int>(iss),
                           std::istream_iterator<int>()};   // default = fim do stream
    fmt::print("      istream_iterator:   {} valores, soma = {}\n",
               lidos.size(), std::accumulate(lidos.begin(), lidos.end(), 0));

    fmt::print("      Todos são iteradores para a STL — o algoritmo não percebe\n");
    fmt::print("      a diferença entre escrever num vector e escrever num stream.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Sentinelas (C++20) — o fim não precisa ser do mesmo tipo
//
//     Até o C++17, `begin()` e `end()` tinham que ter o MESMO tipo. Isso
//     obrigava o "iterador de fim" a carregar estado inútil. Em C++20, o fim
//     pode ser um SENTINELA: um tipo diferente que só sabe responder "acabou?".
// ─────────────────────────────────────────────────────────────────────────────
struct SentinelaDeNulo {};              // não guarda nada

class IteradorDeCString {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = char;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const char*;
    using reference         = const char&;

    explicit IteradorDeCString(const char* p) : p_(p) {}

    reference operator*() const { return *p_; }
    IteradorDeCString& operator++() { ++p_; return *this; }
    IteradorDeCString operator++(int) { auto t = *this; ++p_; return t; }

    bool operator==(const IteradorDeCString& o) const { return p_ == o.p_; }
    // A comparação com o SENTINELA é o que fecha o laço:
    bool operator==(SentinelaDeNulo) const { return *p_ == '\0'; }

private:
    const char* p_;
};

void demo_sentinela() {
    fmt::print("\n── 2.5 Sentinelas (C++20) ──\n");

    const char* texto = "iteradores";

    fmt::print("      percorrendo uma C-string sem chamar strlen: ");
    for (IteradorDeCString it(texto); it != SentinelaDeNulo{}; ++it) {
        fmt::print("{}", *it);
    }
    fmt::print("\n");

    fmt::print("      sizeof(SentinelaDeNulo) = {} byte — não guarda estado\n",
               sizeof(SentinelaDeNulo));
    fmt::print("      Antes do C++20, o `end()` teria que ser um ponteiro para o\n");
    fmt::print("      '\\0' — o que exigiria percorrer a string ANTES de percorrê-la.\n");
    fmt::print("      É o que permite `views::take_while` ser preguiçoso de verdade.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_categorias();
    demo_despacho_por_tag();
    demo_iterador_proprio();
    demo_adaptadores();
    demo_sentinela();
}

} // namespace demo_iterators
