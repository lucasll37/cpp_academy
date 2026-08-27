// =============================================================================
//  src/ranges/demo_custom_range.hpp
//
//  Escrevendo os seus próprios ranges e views
//  ──────────────────────────────────────────
//  Depois de consumir views, o passo natural é produzi-las. Há três níveis,
//  em ordem crescente de esforço e de integração:
//
//    1. um tipo com begin()/end()      → vira `range`, funciona no for e nos
//                                        algoritmos de rg::
//    2. herdar de view_interface       → ganha size(), empty(), front(), back(),
//                                        operator[] e operator bool de graça
//    3. um adaptador com operator|     → participa dos pipelines como os
//                                        adaptadores da biblioteca
//
//  Tópicos:
//  • Os conceitos de <ranges> e o que cada um exige
//  • Um range próprio (nível 1)
//  • view_interface — o CRTP da biblioteca (nível 2)
//  • Um adaptador que entra no pipeline (nível 3)
//  • Mensagens de erro: concepts vs SFINAE
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace demo_custom_range {

namespace rg = std::ranges;
namespace vw = std::views;

// ─────────────────────────────────────────────────────────────────────────────
//  1. Os conceitos — o vocabulário que substituiu o SFINAE
//
//     Antes, "este tipo é iterável?" era uma pergunta que só a instanciação
//     respondia — com 300 linhas de erro. Agora é um predicado booleano que
//     você pode consultar, imprimir e usar em static_assert.
// ─────────────────────────────────────────────────────────────────────────────
struct NaoEhRange { int x; };

void demo_conceitos() {
    fmt::print("\n── 4.1 Os conceitos de <ranges> ──\n");

    fmt::print("      {:<40}{:<8}{}\n", "tipo", "range?", "categoria");
    fmt::print("      {:-<40}{:-<8}{:-<20}\n", "", "", "");

    auto linha = [](const char* nome, bool eh_range, const char* cat) {
        fmt::print("      {:<40}{:<8}{}\n", nome, eh_range ? "sim" : "não", cat);
    };

    linha("std::vector<int>",      rg::range<std::vector<int>>,      "random access");
    linha("std::string",           rg::range<std::string>,           "contiguous");
    linha("int[5]",                rg::range<int[5]>,                "contiguous");
    linha("NaoEhRange",            rg::range<NaoEhRange>,            "—");

    fmt::print("\n      A hierarquia de conceitos:\n");
    fmt::print("        range              → tem begin() e end()\n");
    fmt::print("        sized_range        → + size() em O(1)\n");
    fmt::print("        borrowed_range     → + iteradores sobrevivem ao objeto\n");
    fmt::print("        view               → + copiável em O(1)\n");
    fmt::print("        input/forward/…    → conforme a categoria do iterador\n");

    fmt::print("\n      Isto é um predicado de COMPILAÇÃO: dá para usar em\n");
    fmt::print("      static_assert, em `requires`, e em if constexpr.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Nível 1 — um range próprio
//
//     Uma matriz esparsa em coordenadas. Só precisa de begin()/end() para o
//     range-for e os algoritmos de rg:: funcionarem.
// ─────────────────────────────────────────────────────────────────────────────
struct Celula { int linha, coluna, valor; };

class MatrizEsparsa {
public:
    void definir(int l, int c, int v) { celulas_.push_back({l, c, v}); }

    auto begin()       { return celulas_.begin(); }
    auto end()         { return celulas_.end(); }
    auto begin() const { return celulas_.begin(); }
    auto end()   const { return celulas_.end(); }

private:
    std::vector<Celula> celulas_;
};

void demo_range_proprio() {
    fmt::print("\n── 4.2 Nível 1: um range próprio ──\n");

    MatrizEsparsa m;
    m.definir(0, 0, 5);  m.definir(1, 2, 7);
    m.definir(2, 1, 3);  m.definir(0, 3, 9);

    static_assert(rg::range<MatrizEsparsa>);
    static_assert(rg::random_access_range<MatrizEsparsa>);

    fmt::print("      range-for:        ");
    for (const auto& c : m) fmt::print("({},{})={} ", c.linha, c.coluna, c.valor);
    fmt::print("\n");

    // Os algoritmos de ranges aceitam o objeto direto — com projeção, inclusive
    rg::sort(m, {}, &Celula::valor);
    fmt::print("      após rg::sort por valor: ");
    for (const auto& c : m) fmt::print("{} ", c.valor);
    fmt::print("\n");

    fmt::print("      rg::max(m, {{}}, &Celula::valor).valor = {}\n",
               rg::max(m, {}, &Celula::valor).valor);

    // E ele já pode ser o início de um pipeline de views
    auto grandes = m | vw::filter([](const Celula& c) { return c.valor > 4; })
                     | vw::transform(&Celula::valor);
    fmt::print("      pipeline (valores > 4):  {}\n", fmt::join(grandes, " "));

    fmt::print("      Custo de integração: DOIS métodos.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Nível 2 — view_interface
//
//     Herdar de `std::ranges::view_interface<Derivada>` (CRTP, igual ao de
//     src/templates/demo_crtp.hpp) dá os métodos derivados de graça.
//
//     Aqui: uma view que percorre uma sequência de trás para frente, de N em N.
// ─────────────────────────────────────────────────────────────────────────────
template <rg::random_access_range R>
    requires rg::view<R>
class PassoReverso : public rg::view_interface<PassoReverso<R>> {
public:
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using iterator_concept  = std::forward_iterator_tag;
        using value_type        = rg::range_value_t<R>;
        using difference_type   = std::ptrdiff_t;

        iterator() = default;
        iterator(rg::iterator_t<R> base, difference_type pos, difference_type passo)
            : base_(std::move(base)), pos_(pos), passo_(passo) {}

        decltype(auto) operator*() const { return *(base_ + pos_); }

        iterator& operator++() { pos_ -= passo_; return *this; }
        iterator  operator++(int) { auto t = *this; ++(*this); return t; }

        bool operator==(const iterator& o) const { return pos_ <= o.pos_; }

    private:
        rg::iterator_t<R> base_{};
        difference_type   pos_   = 0;
        difference_type   passo_ = 1;
    };

    PassoReverso() = default;
    PassoReverso(R base, std::ptrdiff_t passo)
        : base_(std::move(base)), passo_(passo) {}

    iterator begin() { return {rg::begin(base_), rg::distance(base_) - 1, passo_}; }
    iterator end()   { return {rg::begin(base_), -1, passo_}; }

private:
    R              base_{};
    std::ptrdiff_t passo_ = 1;
};

// Deduction guide: permite escrever PassoReverso(v, 2) sem os templates
template <rg::viewable_range R>
PassoReverso(R&&, std::ptrdiff_t) -> PassoReverso<vw::all_t<R>>;

void demo_view_interface() {
    fmt::print("\n── 4.3 Nível 2: view_interface ──\n");

    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    PassoReverso pr(v, 3);

    fmt::print("      base:                    {}\n", fmt::join(v, " "));
    fmt::print("      PassoReverso(v, 3):      {}\n", fmt::join(pr, " "));

    // Estes métodos NÃO foram escritos — vieram de view_interface,
    // que os deriva a partir de begin()/end().
    fmt::print("      pr.empty()  = {}   ← de graça, via view_interface\n", pr.empty());
    fmt::print("      pr.front()  = {}   ← de graça\n", pr.front());
    fmt::print("      bool(pr)    = {}   ← de graça\n", static_cast<bool>(pr));

    static_assert(rg::view<PassoReverso<vw::all_t<std::vector<int>&>>>);
    fmt::print("      satisfaz o conceito `view`? sim (static_assert passou)\n");

    fmt::print("      Escrevemos begin() e end(); ganhamos empty, front, back,\n");
    fmt::print("      operator[], operator bool e size() quando aplicável.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Nível 3 — um adaptador que entra no pipeline
//
//     Para `v | passo_reverso(3)` funcionar, precisamos de um objeto que:
//       • chamado com (range, args) devolva a view
//       • chamado só com (args) devolva um adaptador parcial
//       • defina operator| para o adaptador parcial
//
//     Em C++23 há `std::ranges::range_adaptor_closure` para isso. Em C++20
//     escrevemos as ~15 linhas à mão.
// ─────────────────────────────────────────────────────────────────────────────
namespace detalhe {

struct AdaptadorParcial {
    std::ptrdiff_t passo;

    // range | adaptador_parcial  →  a view
    template <rg::viewable_range R>
    friend auto operator|(R&& r, AdaptadorParcial a) {
        return PassoReverso(vw::all(std::forward<R>(r)), a.passo);
    }
};

struct Adaptador {
    // forma de chamada direta: passo_reverso(v, 3)
    template <rg::viewable_range R>
    auto operator()(R&& r, std::ptrdiff_t passo) const {
        return PassoReverso(vw::all(std::forward<R>(r)), passo);
    }
    // forma parcial: passo_reverso(3), para usar com |
    AdaptadorParcial operator()(std::ptrdiff_t passo) const { return {passo}; }
};

} // namespace detalhe

inline constexpr detalhe::Adaptador passo_reverso{};

void demo_adaptador() {
    fmt::print("\n── 4.4 Nível 3: um adaptador no pipeline ──\n");

    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    fmt::print("      base:                          {}\n", fmt::join(v, " "));
    fmt::print("      passo_reverso(v, 4)            {}\n",
               fmt::join(passo_reverso(v, 4), " "));
    fmt::print("      v | passo_reverso(4)           {}\n",
               fmt::join(v | passo_reverso(4), " "));

    // E o principal: ele COMPÕE com os adaptadores da biblioteca
    auto composto = v
        | passo_reverso(2)
        | vw::filter([](int x) { return x % 3 == 0; })
        | vw::transform([](int x) { return x * 100; });

    fmt::print("      passo_reverso(2) | filter(%3) | transform(×100):\n");
    fmt::print("        {}\n", fmt::join(composto, " "));

    // ⚠ A ORDEM importa, e não é estética — é o sistema de tipos.
    //   `PassoReverso` exige `random_access_range`. Cada adaptador PRESERVA ou
    //   DEGRADA a categoria do range que recebe:
    //
    //     vector          → contiguous / random access
    //     | transform     → preserva  (random access)
    //     | filter        → DEGRADA   (bidirectional: não dá para saltar N
    //                                  elementos sem testar o predicado em cada)
    //     | take          → preserva
    //
    //   Por isso `v | vw::filter(p) | passo_reverso(2)` NÃO COMPILA: depois do
    //   filter o range já não é random access. E a mensagem do GCC diz exatamente
    //   isso: "'std::random_access_iterator_tag' is not a base of
    //   'std::bidirectional_iterator_tag'".
    fmt::print("\n      ⚠  a ordem no pipeline não é estética — é o sistema de tipos:\n");
    fmt::print("         `filter` DEGRADA random access → bidirectional, porque\n");
    fmt::print("         não dá para saltar N elementos sem testar o predicado\n");
    fmt::print("         em cada um. Como PassoReverso exige random access,\n");
    fmt::print("         `v | filter(p) | passo_reverso(2)` não compila.\n");
    fmt::print("         Categorias: vector=random | transform=preserva |\n");
    fmt::print("                     filter=degrada | take=preserva\n");

    fmt::print("\n      O nosso adaptador é indistinguível dos da biblioteca.\n");
    fmt::print("      Em C++23, herdar de `std::ranges::range_adaptor_closure`\n");
    fmt::print("      dispensa o `operator|` escrito à mão.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Mensagens de erro: o verdadeiro ganho dos concepts
// ─────────────────────────────────────────────────────────────────────────────
void demo_erros() {
    fmt::print("\n── 4.5 Concepts e as mensagens de erro ──\n");

    fmt::print("      Passar um tipo errado para um template SFINAE gerava um\n");
    fmt::print("      erro no FUNDO da instanciação — centenas de linhas sobre\n");
    fmt::print("      um `operator++` que não existe, três níveis abaixo.\n");

    fmt::print("\n      Com concepts, o erro é na CHAMADA e diz o que faltou:\n");
    fmt::print("        rg::sort(NaoEhRange{{}});\n");
    fmt::print("          error: no match for 'operator()'\n");
    fmt::print("          note: constraints not satisfied\n");
    fmt::print("          note: the required expression 'ranges::begin(t)' is invalid\n");

    fmt::print("\n      E você pode CHECAR antes de tentar:\n");
    fmt::print("        rg::range<NaoEhRange>              = {}\n", rg::range<NaoEhRange>);
    fmt::print("        rg::range<std::vector<int>>        = {}\n", rg::range<std::vector<int>>);
    fmt::print("        rg::sized_range<std::vector<int>>  = {}\n",
               rg::sized_range<std::vector<int>>);

    fmt::print("\n      Ver src/concepts/ para a mecânica dos concepts em si.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_conceitos();
    demo_range_proprio();
    demo_view_interface();
    demo_adaptador();
    demo_erros();
}

} // namespace demo_custom_range
