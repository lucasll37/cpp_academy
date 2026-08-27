// =============================================================================
//  src/stl/demo_algorithms.hpp
//
//  <algorithm> — o vocabulário que substitui o loop cru
//  ────────────────────────────────────────────────────
//  Sean Parent: "No raw loops." A ideia não é dogma — é que um loop escrito à
//  mão esconde a INTENÇÃO no meio da mecânica. `std::partition` diz o que faz
//  no nome; um `for` com dois índices e três `if` não diz.
//
//  Além da clareza, você ganha: correção (os casos de borda já estão certos),
//  complexidade documentada, e otimizações que ninguém escreve à mão.
//
//  Tópicos:
//  • As famílias de algoritmos
//  • Busca: find, find_if, binary_search, lower_bound, equal_range
//  • Ordenação: sort, stable_sort, partial_sort, nth_element
//  • Partição: partition, stable_partition
//  • Transformação: transform, for_each, generate
//  • Numéricos: accumulate, reduce, inner_product, partial_sum
//  • Comparadores e o strict weak ordering
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <iterator>
#include <numeric>
#include <string>
#include <tuple>
#include <vector>

namespace demo_algorithms {

// ─────────────────────────────────────────────────────────────────────────────
//  Dados de trabalho
// ─────────────────────────────────────────────────────────────────────────────
struct Pessoa {
    std::string nome;
    int         idade;
    double      salario;
};

inline std::vector<Pessoa> equipe() {
    return {
        {"Ada",     36, 9500.0},
        {"Linus",   28, 8200.0},
        {"Grace",   45, 11000.0},
        {"Dennis",  28, 7800.0},
        {"Barbara", 52, 12500.0},
        {"Alan",    41, 10200.0},
    };
}

inline void mostrar(const std::vector<Pessoa>& v, const char* rotulo) {
    fmt::print("      {:<22}", rotulo);
    for (const auto& p : v) fmt::print("{}({}) ", p.nome, p.idade);
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  1. Busca
//
//     find/find_if são O(n) e funcionam em qualquer sequência.
//     binary_search/lower_bound são O(log n) mas EXIGEM a sequência ordenada —
//     e não avisam se não estiver: é UB silencioso.
// ─────────────────────────────────────────────────────────────────────────────
void demo_busca() {
    fmt::print("\n── 3.1 Busca ──\n");

    std::vector<int> v{1, 3, 5, 7, 9, 11, 13};

    // Linear — não exige ordenação
    auto it = std::find(v.begin(), v.end(), 7);
    fmt::print("      find(7)              → índice {}\n", std::distance(v.begin(), it));

    auto it2 = std::find_if(v.begin(), v.end(), [](int x) { return x > 8; });
    fmt::print("      find_if(> 8)         → {}\n", *it2);

    // Binária — exige ordenação, devolve só sim/não
    fmt::print("      binary_search(9)     → {}\n", std::binary_search(v.begin(), v.end(), 9));

    // lower_bound: primeiro elemento NÃO MENOR que o valor (>=)
    // upper_bound: primeiro elemento MAIOR que o valor (>)
    auto lb = std::lower_bound(v.begin(), v.end(), 7);
    auto ub = std::upper_bound(v.begin(), v.end(), 7);
    fmt::print("      lower_bound(7)       → {} (índice {})\n", *lb, std::distance(v.begin(), lb));
    fmt::print("      upper_bound(7)       → {} (índice {})\n", *ub, std::distance(v.begin(), ub));

    // O uso mais útil: descobrir ONDE inserir mantendo a ordem
    int novo = 8;
    v.insert(std::lower_bound(v.begin(), v.end(), novo), novo);
    fmt::print("      inserção ordenada de 8 → {}\n", fmt::join(v, " "));

    // equal_range devolve os dois de uma vez (o intervalo dos iguais)
    std::vector<int> comRepetidos{1, 2, 2, 2, 3, 4};
    auto [ini, fim] = std::equal_range(comRepetidos.begin(), comRepetidos.end(), 2);
    fmt::print("      equal_range(2) em {} → {} ocorrências\n",
               fmt::join(comRepetidos, ""), std::distance(ini, fim));

    fmt::print("      ⚠  binary_search/lower_bound em sequência NÃO ordenada é UB\n");
    fmt::print("         — sem erro, sem warning, só resposta errada.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Ordenação — quatro algoritmos, quatro custos
// ─────────────────────────────────────────────────────────────────────────────
void demo_ordenacao() {
    fmt::print("\n── 3.2 Ordenação ──\n");

    // sort — O(n log n), NÃO preserva a ordem relativa dos iguais
    auto v1 = equipe();
    std::sort(v1.begin(), v1.end(),
              [](const Pessoa& a, const Pessoa& b) { return a.idade < b.idade; });
    mostrar(v1, "sort por idade:");

    // stable_sort — O(n log n) com memória extra, PRESERVA a ordem dos iguais
    auto v2 = equipe();
    std::stable_sort(v2.begin(), v2.end(),
                     [](const Pessoa& a, const Pessoa& b) { return a.idade < b.idade; });
    mostrar(v2, "stable_sort:");

    // ⚠ Com 6 elementos os dois resultados costumam COINCIDIR: o introsort do
    // libstdc++ cai no insertion sort para intervalos pequenos, e esse é
    // estável por acidente. A instabilidade só aparece com volume — então
    // vamos medi-la em vez de afirmá-la.
    std::vector<Pessoa> grande;
    for (int i = 0; i < 200; ++i) {
        // Só 4 idades distintas → muitos empates, e `nome` marca a ordem original
        grande.push_back({fmt::format("#{:03}", i), 20 + (i % 4), 1000.0});
    }
    auto porIdade = [](const Pessoa& a, const Pessoa& b) { return a.idade < b.idade; };

    auto ins = grande;   std::sort(ins.begin(), ins.end(), porIdade);
    auto est = grande;   std::stable_sort(est.begin(), est.end(), porIdade);

    const bool iguais = std::equal(ins.begin(), ins.end(), est.begin(),
                                   [](const Pessoa& a, const Pessoa& b) {
                                       return a.nome == b.nome;
                                   });
    fmt::print("      {:<22}com 200 itens e 4 idades distintas:\n", "instabilidade:");
    fmt::print("      {:<22}  sort == stable_sort ? {}\n", "",
               iguais ? "sim" : "NÃO — sort embaralhou os empates");
    fmt::print("      {:<22}  primeiros 6 do sort       : ", "");
    for (int i = 0; i < 6; ++i) fmt::print("{} ", ins[i].nome);
    fmt::print("\n      {:<22}  primeiros 6 do stable_sort: ", "");
    for (int i = 0; i < 6; ++i) fmt::print("{} ", est[i].nome);
    fmt::print("\n      {:<22}  ↑ o stable_sort devolve #000 #004 #008 ... em ordem\n", "");

    // partial_sort — só os K primeiros ficam corretos. O(n log k), mais barato.
    auto v3 = equipe();
    std::partial_sort(v3.begin(), v3.begin() + 3, v3.end(),
                      [](const Pessoa& a, const Pessoa& b) { return a.salario > b.salario; });
    fmt::print("      {:<22}", "top-3 por salário:");
    for (int i = 0; i < 3; ++i) fmt::print("{}({:.0f}) ", v3[i].nome, v3[i].salario);
    fmt::print("\n");

    // nth_element — só GARANTE que a posição n está certa e que tudo antes é
    // menor. O(n) em média. É o jeito de achar a mediana sem ordenar.
    auto v4 = equipe();
    auto meio = v4.begin() + v4.size() / 2;
    std::nth_element(v4.begin(), meio, v4.end(),
                     [](const Pessoa& a, const Pessoa& b) { return a.idade < b.idade; });
    fmt::print("      {:<22}{} ({} anos) — sem ordenar o resto\n",
               "mediana por idade:", meio->nome, meio->idade);

    fmt::print("\n      | algoritmo     | custo      | garante                  |\n");
    fmt::print("      |---------------|------------|--------------------------|\n");
    fmt::print("      | sort          | O(n log n) | tudo ordenado            |\n");
    fmt::print("      | stable_sort   | O(n log n) | + ordem dos iguais        |\n");
    fmt::print("      | partial_sort  | O(n log k) | os k primeiros           |\n");
    fmt::print("      | nth_element   | O(n) médio | só a posição n           |\n");
    fmt::print("      Peça a MENOR garantia que resolve — é a mais barata.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Strict weak ordering — o contrato do comparador
//
//     O comparador precisa responder "a vem ANTES de b?" e obedecer:
//       • irreflexivo : comp(a,a) é sempre false
//       • assimétrico : se comp(a,b) então !comp(b,a)
//       • transitivo  : comp(a,b) && comp(b,c) ⟹ comp(a,c)
//
//     Usar `<=` em vez de `<` viola a irreflexividade — e `std::sort` pode
//     sair do intervalo e corromper memória. Não é exagero: é UB de verdade.
// ─────────────────────────────────────────────────────────────────────────────
void demo_comparador() {
    fmt::print("\n── 3.3 Strict weak ordering ──\n");

    fmt::print("      ✓ [](auto& a, auto& b) {{ return a.idade < b.idade; }}\n");
    fmt::print("      ✗ [](auto& a, auto& b) {{ return a.idade <= b.idade; }}\n");
    fmt::print("        ↑ comp(a,a) devolveria true → std::sort pode ler fora\n");
    fmt::print("          do intervalo. O libstdc++ em modo debug detecta com\n");
    fmt::print("          -D_GLIBCXX_DEBUG.\n");

    // Ordenar por múltiplos critérios: std::tie faz a comparação lexicográfica
    auto v = equipe();
    std::sort(v.begin(), v.end(), [](const Pessoa& a, const Pessoa& b) {
        return std::tie(a.idade, a.nome) < std::tie(b.idade, b.nome);
    });
    mostrar(v, "idade, depois nome:");
    fmt::print("      `std::tie` dá o desempate lexicográfico de graça —\n");
    fmt::print("      bem melhor que uma cascata de if/else escrita à mão.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Partição — separar sem ordenar
// ─────────────────────────────────────────────────────────────────────────────
void demo_particao() {
    fmt::print("\n── 3.4 Partição ──\n");

    auto v = equipe();

    // partition: O(n), reorganiza para que os que satisfazem venham primeiro.
    // Devolve o iterador da fronteira. NÃO preserva a ordem relativa.
    auto fronteira = std::partition(v.begin(), v.end(),
                                    [](const Pessoa& p) { return p.idade < 40; });

    fmt::print("      {:<22}", "< 40 anos:");
    for (auto it = v.begin(); it != fronteira; ++it) fmt::print("{} ", it->nome);
    fmt::print("\n      {:<22}", ">= 40 anos:");
    for (auto it = fronteira; it != v.end(); ++it) fmt::print("{} ", it->nome);
    fmt::print("\n");

    // stable_partition preserva a ordem, ao custo de memória extra
    auto w = equipe();
    std::stable_partition(w.begin(), w.end(),
                          [](const Pessoa& p) { return p.salario > 9000; });
    mostrar(w, "stable_partition:");

    fmt::print("      Partição é O(n); ordenar é O(n log n). Se você só precisa\n");
    fmt::print("      SEPARAR em dois grupos, não ordene.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Transformação
// ─────────────────────────────────────────────────────────────────────────────
void demo_transformacao() {
    fmt::print("\n── 3.5 Transformação ──\n");

    auto v = equipe();

    // transform unário: um container → outro
    std::vector<std::string> nomes;
    nomes.reserve(v.size());
    std::transform(v.begin(), v.end(), std::back_inserter(nomes),
                   [](const Pessoa& p) { return p.nome; });
    fmt::print("      transform (extrair):  {}\n", fmt::join(nomes, ", "));

    // transform in-place
    std::transform(nomes.begin(), nomes.end(), nomes.begin(), [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    });
    fmt::print("      transform (in-place): {}\n", fmt::join(nomes, ", "));

    // transform binário: dois containers → um
    std::vector<int>    base{100, 200, 300};
    std::vector<int>    bonus{10, 20, 30};
    std::vector<int>    total;
    std::transform(base.begin(), base.end(), bonus.begin(), std::back_inserter(total),
                   std::plus<>{});
    fmt::print("      transform (binário):  {}\n", fmt::join(total, " "));

    // generate — preenche com uma fábrica
    std::vector<int> seq(8);
    int n = 0;
    std::generate(seq.begin(), seq.end(), [&n] { n += 3; return n; });
    fmt::print("      generate:             {}\n", fmt::join(seq, " "));

    // iota — a sequência mais comum, já pronta
    std::vector<int> ids(8);
    std::iota(ids.begin(), ids.end(), 100);
    fmt::print("      iota(100):            {}\n", fmt::join(ids, " "));
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Numéricos — <numeric>
// ─────────────────────────────────────────────────────────────────────────────
void demo_numericos() {
    fmt::print("\n── 3.6 Algoritmos numéricos ──\n");

    auto v = equipe();
    std::vector<int> nums{1, 2, 3, 4, 5, 6, 7, 8};

    // accumulate: dobra à ESQUERDA, sequencial e determinístico
    fmt::print("      accumulate (soma):    {}\n",
               std::accumulate(nums.begin(), nums.end(), 0));

    // ⚠ o tipo do valor inicial DECIDE o tipo da acumulação
    std::vector<double> reais{1.5, 2.5, 3.5};
    fmt::print("      accumulate com 0 (int):    {}  ← truncou!\n",
               std::accumulate(reais.begin(), reais.end(), 0));
    fmt::print("      accumulate com 0.0 (double): {}\n",
               std::accumulate(reais.begin(), reais.end(), 0.0));

    // accumulate com operação customizada — aqui, somar um campo
    double folha = std::accumulate(v.begin(), v.end(), 0.0,
                                   [](double soma, const Pessoa& p) { return soma + p.salario; });
    fmt::print("      folha salarial:       {:.2f}\n", folha);

    // reduce (C++17): como accumulate mas SEM garantia de ordem — permite
    // paralelizar. Por isso a operação precisa ser associativa e comutativa.
    fmt::print("      reduce (C++17):       {}\n",
               std::reduce(nums.begin(), nums.end(), 0));

    // inner_product — soma dos produtos par a par
    std::vector<int> pesos{1, 1, 2, 2, 3, 3, 4, 4};
    fmt::print("      inner_product:        {}\n",
               std::inner_product(nums.begin(), nums.end(), pesos.begin(), 0));

    // partial_sum — soma acumulada (prefix sum)
    std::vector<int> acumulado;
    std::partial_sum(nums.begin(), nums.end(), std::back_inserter(acumulado));
    fmt::print("      partial_sum:          {}\n", fmt::join(acumulado, " "));

    // adjacent_difference — o inverso do partial_sum
    std::vector<int> diferencas;
    std::adjacent_difference(acumulado.begin(), acumulado.end(),
                             std::back_inserter(diferencas));
    fmt::print("      adjacent_difference:  {}  ← voltou ao original\n",
               fmt::join(diferencas, " "));

    // minmax_element numa passada só
    auto [mn, mx] = std::minmax_element(v.begin(), v.end(),
                                        [](const Pessoa& a, const Pessoa& b) {
                                            return a.salario < b.salario;
                                        });
    fmt::print("      minmax por salário:   {} ({:.0f}) … {} ({:.0f})\n",
               mn->nome, mn->salario, mx->nome, mx->salario);
}

// ─────────────────────────────────────────────────────────────────────────────
//  7. Predicados de conjunto e verificação
// ─────────────────────────────────────────────────────────────────────────────
void demo_predicados() {
    fmt::print("\n── 3.7 Verificação e conjuntos ──\n");

    std::vector<int> v{2, 4, 6, 8, 10};

    fmt::print("      all_of  (pares)  → {}\n",
               std::all_of(v.begin(), v.end(), [](int x) { return x % 2 == 0; }));
    fmt::print("      any_of  (> 9)    → {}\n",
               std::any_of(v.begin(), v.end(), [](int x) { return x > 9; }));
    fmt::print("      none_of (ímpares)→ {}\n",
               std::none_of(v.begin(), v.end(), [](int x) { return x % 2; }));
    fmt::print("      is_sorted        → {}\n", std::is_sorted(v.begin(), v.end()));

    // Operações de conjunto — exigem sequências ORDENADAS
    std::vector<int> a{1, 2, 3, 4, 5};
    std::vector<int> b{4, 5, 6, 7};
    std::vector<int> r;

    std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(r));
    fmt::print("      set_intersection → {}\n", fmt::join(r, " "));

    r.clear();
    std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(r));
    fmt::print("      set_difference   → {}\n", fmt::join(r, " "));

    r.clear();
    std::set_union(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(r));
    fmt::print("      set_union        → {}\n", fmt::join(r, " "));

    fmt::print("      ⚠  todas as operações de conjunto exigem entrada ORDENADA\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_busca();
    demo_ordenacao();
    demo_comparador();
    demo_particao();
    demo_transformacao();
    demo_numericos();
    demo_predicados();
}

} // namespace demo_algorithms
