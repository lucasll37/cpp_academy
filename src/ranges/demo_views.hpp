// =============================================================================
//  src/ranges/demo_views.hpp
//
//  Views — sequências PREGUIÇOSAS e sem posse
//  ──────────────────────────────────────────
//  Um algoritmo clássico da STL faz uma passada e produz um container novo.
//  Encadear três deles = três passadas + dois containers temporários.
//
//  Uma VIEW não faz nada quando é criada. Ela guarda a receita e só executa
//  quando alguém itera — e mesmo aí, elemento por elemento, sem materializar
//  nada. Três views encadeadas continuam sendo UMA passada e ZERO containers.
//
//  Duas propriedades definem uma view:
//    • preguiçosa — o trabalho acontece na iteração, não na construção
//    • sem posse  — ela referencia os dados, não os copia (cópia é O(1))
//
//  Tópicos:
//  • A preguiça, provada com prints
//  • O catálogo: filter, transform, take, drop, reverse, iota
//  • Views infinitas
//  • take_while / drop_while
//  • O custo real: zero abstração em -O2
// =============================================================================
#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <numeric>
#include <string_view>
#include <utility>
#include <ranges>
#include <string>
#include <vector>

namespace demo_views {

namespace vw = std::views;

// ─────────────────────────────────────────────────────────────────────────────
//  1. A preguiça, provada
//
//     Os lambdas abaixo imprimem quando são chamados. Se a view fosse ansiosa,
//     todos os prints sairiam na linha da construção. Eles não saem.
// ─────────────────────────────────────────────────────────────────────────────
void demo_preguica() {
    fmt::print("\n── 1.1 A preguiça, provada ──\n");

    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};

    fmt::print("      construindo a view (nada deve ser impresso):\n");
    auto pipeline = v
        | vw::filter([](int x)    { fmt::print("        filter({})\n", x);
                                     return x % 2 == 0; })
        | vw::transform([](int x) { fmt::print("        transform({})\n", x);
                                     return x * 10; });
    fmt::print("      ...view construída. Nenhum elemento foi tocado.\n");

    fmt::print("      agora iterando (aí sim o trabalho acontece):\n");
    for (int x : pipeline) fmt::print("        → {}\n", x);

    fmt::print("      Repare na ORDEM: filter(1), filter(2), transform(2), →20, …\n");
    fmt::print("      Cada elemento atravessa o pipeline INTEIRO antes do próximo.\n");
    fmt::print("      Não há vetor intermediário em lugar nenhum.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. Uma view não possui — e a cópia é O(1)
// ─────────────────────────────────────────────────────────────────────────────
void demo_sem_posse() {
    fmt::print("\n── 1.2 Views não possuem dados ──\n");

    std::vector<int> v{1, 2, 3, 4, 5};
    auto pares = v | vw::filter([](int x) { return x % 2 == 0; });

    fmt::print("      v          = {}\n", fmt::join(v, " "));
    fmt::print("      pares      = {}\n", fmt::join(pares, " "));

    // Mudar a origem muda a view — ela só referencia
    v.push_back(6);
    v[0] = 10;
    fmt::print("      após v.push_back(6) e v[0] = 10:\n");
    fmt::print("      pares      = {}  ← a view enxerga a mudança\n", fmt::join(pares, " "));

    fmt::print("      sizeof(vector<int>)     = {} bytes\n", sizeof(std::vector<int>));
    fmt::print("      sizeof(a view de filter)= {} bytes — não guarda elementos\n",
               sizeof(pares));

    fmt::print("      ⚠  o outro lado da moeda: se `v` morrer, a view fica\n");
    fmt::print("         pendurada. É a mesma regra do string_view.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. O catálogo essencial
// ─────────────────────────────────────────────────────────────────────────────
void demo_catalogo() {
    fmt::print("\n── 1.3 O catálogo essencial ──\n");

    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    fmt::print("      base                       {}\n", fmt::join(v, " "));

    fmt::print("      filter(par)                {}\n",
               fmt::join(v | vw::filter([](int x) { return x % 2 == 0; }), " "));
    fmt::print("      transform(quadrado)        {}\n",
               fmt::join(v | vw::transform([](int x) { return x * x; }), " "));
    fmt::print("      take(3)                    {}\n", fmt::join(v | vw::take(3), " "));
    fmt::print("      drop(7)                    {}\n", fmt::join(v | vw::drop(7), " "));
    fmt::print("      reverse                    {}\n", fmt::join(v | vw::reverse, " "));

    // take_while para no PRIMEIRO que falha; filter continua testando todos
    fmt::print("      take_while(< 5)            {}\n",
               fmt::join(v | vw::take_while([](int x) { return x < 5; }), " "));
    fmt::print("      drop_while(< 5)            {}\n",
               fmt::join(v | vw::drop_while([](int x) { return x < 5; }), " "));

    // A diferença entre take_while e filter, num vetor não ordenado:
    std::vector<int> desordenado{1, 2, 9, 3, 4};
    fmt::print("      em {}:\n", fmt::join(desordenado, " "));
    fmt::print("        take_while(< 5)          {}  ← parou no 9\n",
               fmt::join(desordenado | vw::take_while([](int x) { return x < 5; }), " "));
    fmt::print("        filter(< 5)              {}  ← testou todos\n",
               fmt::join(desordenado | vw::filter([](int x) { return x < 5; }), " "));

    // elements/keys/values para tuplas e mapas
    std::vector<std::pair<std::string, int>> placar{{"Ada", 10}, {"Grace", 8}, {"Alan", 9}};
    fmt::print("      keys                       {}\n", fmt::join(placar | vw::keys, " "));
    fmt::print("      values                     {}\n", fmt::join(placar | vw::values, " "));

    // join achata um range de ranges
    std::vector<std::vector<int>> aninhado{{1, 2}, {3, 4}, {5}};
    fmt::print("      join (achatar)             {}\n",
               fmt::join(aninhado | vw::join, " "));

    // split quebra por delimitador
    std::string csv = "alfa,beta,gama";
    fmt::print("      split(',')                 ");
    for (auto parte : csv | vw::split(',')) {
        fmt::print("'{}' ", std::string_view(parte.begin(), parte.end()));
    }
    fmt::print("\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  4. Views infinitas
//
//     `iota` sem limite gera uma sequência infinita. Isso só é possível porque
//     nada é materializado — o `take` corta antes de qualquer trabalho extra.
//     Tente fazer isso com um container e você fica sem memória.
// ─────────────────────────────────────────────────────────────────────────────
void demo_infinitas() {
    fmt::print("\n── 1.4 Views infinitas ──\n");

    // iota(1) — 1, 2, 3, ... sem fim
    fmt::print("      iota(1) | take(10)         {}\n",
               fmt::join(vw::iota(1) | vw::take(10), " "));

    // Os 10 primeiros quadrados perfeitos ímpares
    auto quadrados_impares = vw::iota(1)
        | vw::transform([](int n) { return n * n; })
        | vw::filter([](int n)    { return n % 2 == 1; })
        | vw::take(8);
    fmt::print("      quadrados ímpares          {}\n", fmt::join(quadrados_impares, " "));

    // repeat (C++23) e a versão C++20 com iota+transform
    fmt::print("      iota(0,5)|transform(const) {}\n",
               fmt::join(vw::iota(0, 5) | vw::transform([](int) { return 7; }), " "));

    // Fibonacci preguiçoso: mantém estado no lambda (mutable)
    auto fib = vw::iota(0) | vw::transform([a = 0, b = 1](int) mutable {
        int r = a;
        int prox = a + b;
        a = b; b = prox;
        return r;
    }) | vw::take(12);
    fmt::print("      fibonacci                  {}\n", fmt::join(fib, " "));

    fmt::print("      ⚠  um lambda `mutable` numa view é frágil: a view pode ser\n");
    fmt::print("         percorrida mais de uma vez e o estado NÃO reinicia.\n");
    fmt::print("         Prefira `iota` puro; para estado, use uma corrotina\n");
    fmt::print("         (ver src/coroutines/demo_generator.hpp).\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  5. Uma passada, zero temporários — a comparação
// ─────────────────────────────────────────────────────────────────────────────
void demo_versus_classico() {
    fmt::print("\n── 1.5 Views vs a versão clássica ──\n");

    std::vector<int> v(20);
    std::iota(v.begin(), v.end(), 1);

    fmt::print("      Objetivo: dos 20 primeiros, pegar os múltiplos de 3,\n");
    fmt::print("      elevar ao quadrado, e ficar com os 4 primeiros.\n");

    // ── versão clássica: 3 passadas, 2 vetores temporários ──
    std::vector<int> passo1;
    std::copy_if(v.begin(), v.end(), std::back_inserter(passo1),
                 [](int x) { return x % 3 == 0; });
    std::vector<int> passo2;
    std::transform(passo1.begin(), passo1.end(), std::back_inserter(passo2),
                   [](int x) { return x * x; });
    std::vector<int> passo3(passo2.begin(), passo2.begin() + 4);

    fmt::print("      clássico → {}   ({} temporários, {} passadas)\n",
               fmt::join(passo3, " "), 2, 3);

    // ── versão com views: 1 passada, 0 temporários, e ainda para cedo ──
    auto resultado = v
        | vw::filter([](int x)    { return x % 3 == 0; })
        | vw::transform([](int x) { return x * x; })
        | vw::take(4);

    fmt::print("      views    → {}   (0 temporários, 1 passada)\n",
               fmt::join(resultado, " "));

    fmt::print("      A versão com views nem chega a olhar os últimos elementos:\n");
    fmt::print("      o `take(4)` interrompe assim que junta 4. A clássica\n");
    fmt::print("      processou os 20 antes de descartar.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  6. Materializar quando precisar
//
//     Uma view é ótima para consumir. Se você precisa GUARDAR o resultado,
//     materialize num container.
// ─────────────────────────────────────────────────────────────────────────────
void demo_materializar() {
    fmt::print("\n── 1.6 Materializando uma view ──\n");

    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8};
    auto view = v | vw::filter([](int x) { return x % 2 == 0; })
                  | vw::transform([](int x) { return x * 100; });

    // C++20: construir a partir dos iteradores
    std::vector<int> materializado(view.begin(), view.end());
    fmt::print("      vector(view.begin(), view.end()) → {}\n",
               fmt::join(materializado, " "));

    // Ou com ranges::to (C++23) — não disponível no GCC 13:
    fmt::print("      Em C++23 seria: `auto v2 = view | std::ranges::to<std::vector>();`\n");
    fmt::print("      O GCC 13 ainda não implementa `ranges::to` (chegou no GCC 14).\n");

    fmt::print("      Regra: mantenha como view enquanto está PROCESSANDO;\n");
    fmt::print("      materialize só quando precisar GUARDAR ou atravessar\n");
    fmt::print("      uma fronteira de API.\n");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ponto de entrada
// ─────────────────────────────────────────────────────────────────────────────
inline void run() {
    demo_preguica();
    demo_sem_posse();
    demo_catalogo();
    demo_infinitas();
    demo_versus_classico();
    demo_materializar();
}

} // namespace demo_views
