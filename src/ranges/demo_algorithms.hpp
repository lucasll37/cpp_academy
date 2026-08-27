// =============================================================================
//  src/ranges/demo_algorithms.hpp
//
//  std::ranges::* — os algoritmos, agora com projeções
//  ───────────────────────────────────────────────────
//  Os algoritmos de <ranges> parecem só uma conveniência ("passo o container
//  em vez de dois iteradores"). O ganho real é outro: a PROJEÇÃO.
//
//  Toda função de <ranges> aceita um argumento extra que diz "compare/teste
//  ESTE campo, não o objeto inteiro". Isso elimina a maior parte dos lambdas
//  que a gente escrevia só para acessar um membro.
//
//      std::sort(v.begin(), v.end(),
//                [](const P& a, const P& b) { return a.idade < b.idade; });
//      std::ranges::sort(v, {}, &P::idade);          // ← a mesma coisa
//
//  Tópicos:
//  • Passar o range inteiro
//  • Projeções — o argumento que muda tudo
//  • Comparadores como objetos (ranges::less, greater)
//  • Os tipos de retorno mais informativos
//  • Proteção contra dangling em tempo de compilação
//  • O que ainda NÃO existe em ranges
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <functional>
#include <numeric>
#include <ranges>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace demo_ranges_algorithms {

namespace rg = std::ranges;
namespace vw = std::views;

struct Pessoa {
    std::string nome;
    int         idade;
    double      salario;
};

inline std::vector<Pessoa> equipe() {
    return {
        {"Ada",     36,  9500.0},
        {"Linus",   28,  8200.0},
        {"Grace",   45, 11000.0},
        {"Dennis",  28,  7800.0},
        {"Barbara", 52, 12500.0},
        {"Alan",    41, 10200.0},
    };
}

inline void mostrar(const std::vector<Pessoa>& v, const char* rotulo) {
    fmt::print("      {:<30}", rotulo);
    for (const auto& p : v) fmt::print("{}({}) ", p.nome, p.idade);
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. O range inteiro em vez do par de iteradores
// ─────────────────────────────────────────────────────────────────────────────
void demo_range_inteiro() {
    fmt::print("\n── 3.1 Passando o range inteiro ──\n");

    std::vector<int> v{5, 3, 9, 1, 7};

    // clássico
    std::sort(v.begin(), v.end());
    fmt::print("      std::sort(v.begin(), v.end())  → {}\n", fmt::join(v, " "));

    // ranges
    std::vector<int> w{5, 3, 9, 1, 7};
    rg::sort(w);
    fmt::print("      rg::sort(w)                    → {}\n", fmt::join(w, " "));

    fmt::print("      rg::max(w)      = {}\n", rg::max(w));
    fmt::print("      rg::count(w, 9) = {}\n", rg::count(w, 9));
    fmt::print("      rg::contains(w, 7) exigiria C++23 — em C++20 use rg::find\n");

    // E funciona direto sobre uma VIEW, sem materializar
    fmt::print("      rg::max sobre uma view: {}\n",
               rg::max(vw::iota(1, 100) | vw::filter([](int x) { return x % 7 == 0; })));
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Projeções — o argumento que muda tudo
//
//     A assinatura é sempre: algoritmo(range, comparador = {}, projeção = {})
//     A projeção é aplicada a CADA elemento antes de o comparador/predicado
//     olhar para ele. E ela aceita um ponteiro-para-membro direto.
// ─────────────────────────────────────────────────────────────────────────────
void demo_projecoes() {
    fmt::print("\n── 3.2 Projeções ──\n");

    auto v = equipe();

    // O `{}` no meio é o comparador default (rg::less)
    rg::sort(v, {}, &Pessoa::idade);
    mostrar(v, "sort(v, {}, &P::idade)");

    // Comparador explícito + projeção
    rg::sort(v, rg::greater{}, &Pessoa::salario);
    mostrar(v, "sort(v, greater, &P::salario)");

    // A projeção também vale para busca e contagem
    auto it = rg::find(v, 45, &Pessoa::idade);
    fmt::print("      {:<30}{}\n", "find(v, 45, &P::idade)",
               it != v.end() ? it->nome : "(não achou)");

    fmt::print("      {:<30}{}\n", "count(v, 28, &P::idade)",
               rg::count(v, 28, &Pessoa::idade));

    auto mais_velho = rg::max_element(v, {}, &Pessoa::idade);
    fmt::print("      {:<30}{} ({})\n", "max_element(v,{},&P::idade)",
               mais_velho->nome, mais_velho->idade);

    // Projeção pode ser qualquer invocável, não só ponteiro-para-membro
    rg::sort(v, {}, [](const Pessoa& p) { return p.nome.size(); });
    mostrar(v, "sort por tamanho do nome");

    fmt::print("\n      Compare as duas formas de ordenar por idade:\n");
    fmt::print("        std::sort(v.begin(), v.end(),\n");
    fmt::print("                  [](const P& a, const P& b) {{ return a.idade < b.idade; }});\n");
    fmt::print("        rg::sort(v, {{}}, &P::idade);\n");
    fmt::print("      A segunda diz o QUE ordenar; a primeira, COMO comparar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Comparadores como objetos
//
//     `rg::less`, `rg::greater` etc. são functores transparentes e, ao
//     contrário de `std::less<T>`, exigem que a comparação seja um
//     `totally_ordered_with` — pegando comparações sem sentido em compilação.
// ─────────────────────────────────────────────────────────────────────────────
void demo_comparadores() {
    fmt::print("\n── 3.3 Comparadores como objetos ──\n");

    auto v = equipe();

    rg::sort(v, rg::less{},    &Pessoa::idade);   mostrar(v, "less por idade");
    rg::sort(v, rg::greater{}, &Pessoa::idade);   mostrar(v, "greater por idade");

    // Múltiplos critérios continuam funcionando via projeção para tupla
    rg::sort(v, {}, [](const Pessoa& p) { return std::tie(p.idade, p.nome); });
    mostrar(v, "idade, depois nome");

    fmt::print("      `rg::less{{}}` é o default — o `{{}}` das chamadas anteriores.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Retornos mais informativos
//
//     Os algoritmos clássicos jogam informação fora. `std::copy` devolve só o
//     fim do destino; se você também precisa de onde parou na origem, não tem.
//     Os de ranges devolvem structs nomeadas com tudo.
// ─────────────────────────────────────────────────────────────────────────────
void demo_retornos() {
    fmt::print("\n── 3.4 Retornos com mais informação ──\n");

    std::vector<int> origem{1, 2, 3, 4, 5};
    std::vector<int> destino(3);                  // menor que a origem!

    // rg::copy_n devolve {in, out} — os DOIS iteradores
    auto resultado = rg::copy_n(origem.begin(), 3, destino.begin());
    fmt::print("      copy_n → destino = {}\n", fmt::join(destino, " "));
    fmt::print("      parou na origem em: {} (o 4º elemento)\n", *resultado.in);
    fmt::print("      clássico devolveria só o fim do destino.\n");

    // rg::minmax devolve {min, max} nomeados
    auto mm = rg::minmax(origem);
    fmt::print("      minmax → min={} max={}\n", mm.min, mm.max);

    // rg::mismatch devolve {in1, in2}
    std::vector<int> a{1, 2, 3, 9, 5};
    std::vector<int> b{1, 2, 3, 4, 5};
    auto dif = rg::mismatch(a, b);
    fmt::print("      mismatch → primeiro par diferente: {} vs {}\n", *dif.in1, *dif.in2);

    // rg::for_each devolve {in, fun} — permite recuperar o functor com estado
    int soma = 0;
    auto fe = rg::for_each(origem, [&soma](int x) { soma += x; });
    fmt::print("      for_each → soma acumulada = {} (e devolve o functor)\n", soma);
    (void)fe;
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Proteção contra dangling, em compilação
// ─────────────────────────────────────────────────────────────────────────────
std::vector<int> produz() { return {1, 2, 3, 4, 5}; }

void demo_dangling() {
    fmt::print("\n── 3.5 Proteção contra dangling ──\n");

    // Passar um TEMPORÁRIO devolve std::ranges::dangling, não um iterador.
    auto it = rg::find(produz(), 3);
    fmt::print("      rg::find(produz(), 3) → tipo é ranges::dangling? {}\n",
               std::is_same_v<decltype(it), rg::dangling>);
    fmt::print("      `*it` NÃO COMPILA. O bug virou erro de compilação.\n");

    // O clássico aceita alegremente e devolve um iterador pendurado:
    fmt::print("      Já `std::find(produz().begin(), produz().end(), 3)`\n");
    fmt::print("      compila, roda, e é UB duplo (dois vetores diferentes!).\n");

    // A forma correta: manter a origem viva
    auto v = produz();
    auto it2 = rg::find(v, 3);
    fmt::print("      com o vetor nomeado: encontrou {} ✓\n", *it2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. O que ainda falta no GCC 13
// ─────────────────────────────────────────────────────────────────────────────
void demo_limitacoes() {
    fmt::print("\n── 3.6 Limitações práticas ──\n");

    fmt::print("      Nem tudo da STL clássica tem versão em ranges:\n");
    fmt::print("        • os algoritmos NUMÉRICOS (<numeric>) não têm —\n");
    fmt::print("          accumulate, reduce, inner_product, partial_sum.\n");
    fmt::print("          Em C++23 chegou `std::ranges::fold_left`.\n");
    fmt::print("        • as versões PARALELAS (std::execution) não têm.\n");

    // Como fazer accumulate sobre uma view hoje, em C++20:
    auto v = equipe();
    auto salarios = v | vw::transform(&Pessoa::salario);
    double folha = std::accumulate(salarios.begin(), salarios.end(), 0.0);
    fmt::print("\n      accumulate sobre uma view (C++20):\n");
    fmt::print("        auto sal = v | vw::transform(&P::salario);\n");
    fmt::print("        std::accumulate(sal.begin(), sal.end(), 0.0) = {:.2f}\n", folha);

    fmt::print("\n      No GCC 13 ainda faltam (chegaram no GCC 14):\n");
    fmt::print("        • std::ranges::to<Container>()\n");
    fmt::print("        • views::zip, views::enumerate, views::chunk, views::slide\n");
    fmt::print("        • std::ranges::fold_left / fold_right\n");
    fmt::print("      Ver src/ROADMAP.md para a lista completa do toolchain.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_range_inteiro();
    demo_projecoes();
    demo_comparadores();
    demo_retornos();
    demo_dangling();
    demo_limitacoes();
}

} // namespace demo_ranges_algorithms
