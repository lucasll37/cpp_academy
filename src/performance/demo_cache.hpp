// =============================================================================
//  src/performance/demo_cache.hpp
//
//  Localidade de cache — o fator que domina tudo
//  ─────────────────────────────────────────────
//  A distância entre o processador e a RAM é a maior assimetria do hardware
//  moderno. Em ciclos de clock, aproximadamente:
//
//      registrador ....... 0
//      cache L1 .......... 4
//      cache L2 ......... 12
//      cache L3 ......... 40
//      RAM ............. 200      ← 50× mais lento que L1
//
//  Em 200 ciclos o processador executaria centenas de instruções. Por isso,
//  em código que percorre dados, "quantas operações" quase nunca é a pergunta
//  certa — "quantos cache miss" é.
//
//  ⚠ Todos os números deste arquivo são MEDIDOS nesta máquina, agora. Eles
//    variam com o processador, com a carga e com o compilador. O que NÃO
//    varia é a ORDEM DE GRANDEZA entre os casos.
//
//  Tópicos:
//  • A linha de cache: você nunca lê 4 bytes
//  • Percurso por linha vs por coluna
//  • vector vs list — o mesmo algoritmo, ordens de grandeza
//  • O prefetcher, e como pará-lo
// =============================================================================
#pragma once
#include "bench.hpp"
#include <fmt/core.h>
#include <algorithm>
#include <cstddef>
#include <list>
#include <numeric>
#include <random>
#include <vector>

namespace demo_cache {

void demo_linha() {
    fmt::print("\n── 1.1 A linha de cache ──\n");

    fmt::print("      O processador nunca lê 4 bytes da RAM. Ele lê uma LINHA\n");
    fmt::print("      inteira — 64 bytes no x86-64. Ler `v[0]` traz `v[0..15]`\n");
    fmt::print("      junto, de graça.\n\n");

    // Medindo o efeito: mesmo NÚMERO de leituras, passos diferentes
    constexpr std::size_t kN = 16 * 1024 * 1024;   // 64 MB, muito maior que L3
    std::vector<int> v(kN, 1);

    fmt::print("      Somando 1 milhão de elementos de um vetor de {} MB,\n",
               kN * sizeof(int) / (1024 * 1024));
    fmt::print("      variando o PASSO (mesmo número de somas em todos):\n\n");
    fmt::print("        passo | bytes entre acessos | tempo\n");
    fmt::print("        ------|---------------------|--------\n");

    double base = 0;
    for (std::size_t passo : {1u, 2u, 4u, 8u, 16u, 32u}) {
        const std::size_t n_somas = 1'000'000;
        const double ms = bench::mediana_ms([&] {
            long s = 0;
            for (std::size_t i = 0, k = 0; k < n_somas; ++k, i += passo) {
                if (i >= kN) i = 0;
                s += v[i];
            }
            bench::nao_otimize(s);
        }, 5);
        if (passo == 1) base = ms;
        fmt::print("        {:5} | {:19} | {:6.2f} ms  ({:.1f}×)\n",
                   passo, passo * sizeof(int), ms, ms / base);
    }

    fmt::print("\n      O JOELHO está no passo 16 — que é exatamente\n");
    fmt::print("      64 bytes / sizeof(int). Até ali, vários acessos\n");
    fmt::print("      compartilham a mesma linha e o custo mal se move. A partir\n");
    fmt::print("      dali, cada acesso paga uma linha inteira.\n\n");

    fmt::print("      O passo 32 é AINDA pior que o 16, e não por mais linhas:\n");
    fmt::print("      é o prefetcher. Ele busca linhas ADJACENTES, e com 128 B de\n");
    fmt::print("      passo metade do que ele traz nunca é usada.\n\n");

    fmt::print("      ⚠  NOTA DE MÉTODO — a primeira versão deste demo ia até o\n");
    fmt::print("         passo 1024 e afirmava que o tempo \"estabiliza\" depois de\n");
    fmt::print("         16. A medição desmentiu as duas coisas:\n\n");
    fmt::print("           passo  256 →  1.75 ms   ← CAIU\n");
    fmt::print("           passo  512 →  1.56 ms   ← caiu mais\n");
    fmt::print("           passo 1024 →  4.05 ms   ← disparou\n\n");
    fmt::print("         A causa é um defeito do EXPERIMENTO, não do hardware: o\n");
    fmt::print("         `if (i >= kN) i = 0` faz o índice dar a volta. Com passo\n");
    fmt::print("         256 e 1 milhão de acessos, só {} posições distintas são\n",
               kN / 256);
    fmt::print("         tocadas — {} MB, que CABEM no L3. O benchmark deixou de\n",
               (kN / 256) * 64 / (1024 * 1024));
    fmt::print("         medir latência de RAM e passou a medir L3.\n");
    fmt::print("         (o salto em 1024 é outro efeito: 4096 B é o tamanho da\n");
    fmt::print("          página, e aí começa a pressão sobre o TLB)\n\n");
    fmt::print("         Por isso a tabela acima para no passo 32: é a faixa em\n");
    fmt::print("         que o experimento mede o que diz medir. Um benchmark que\n");
    fmt::print("         produz um número não produziu, com isso, um número CERTO.\n");
}

void demo_linha_vs_coluna() {
    fmt::print("\n── 1.2 Percurso por linha vs por coluna ──\n");

    constexpr std::size_t kLado = 2048;                    // 2048² × 4 B = 16 MB
    std::vector<int> m(kLado * kLado, 1);

    const double por_linha = bench::mediana_ms([&] {
        long s = 0;
        for (std::size_t i = 0; i < kLado; ++i)
            for (std::size_t j = 0; j < kLado; ++j)
                s += m[i * kLado + j];                     // CONTÍGUO
        bench::nao_otimize(s);
    }, 5);

    const double por_coluna = bench::mediana_ms([&] {
        long s = 0;
        for (std::size_t j = 0; j < kLado; ++j)
            for (std::size_t i = 0; i < kLado; ++i)
                s += m[i * kLado + j];                     // salta kLado × 4 B
        bench::nao_otimize(s);
    }, 5);

    fmt::print("      Matriz {}×{} ({} MB), somando TODOS os elementos:\n",
               kLado, kLado, kLado * kLado * sizeof(int) / (1024 * 1024));
    fmt::print("        por linha  (i, depois j) : {:7.2f} ms\n", por_linha);
    fmt::print("        por coluna (j, depois i) : {:7.2f} ms   ({:.1f}× mais lento)\n",
               por_coluna, por_coluna / por_linha);

    fmt::print("\n      MESMO número de somas, MESMO número de elementos, MESMA\n");
    fmt::print("      complexidade O(n²). A única diferença é a ORDEM dos\n");
    fmt::print("      acessos — e ela custa o fator acima.\n\n");

    fmt::print("      Por coluna, cada acesso salta {} bytes: uma linha de cache\n",
               kLado * sizeof(int));
    fmt::print("      nova por elemento, e 15 dos 16 ints trazidos são\n");
    fmt::print("      descartados antes de serem usados.\n\n");

    fmt::print("      Esta é a razão de bibliotecas de álgebra linear (BLAS,\n");
    fmt::print("      Eigen) fazerem TILING: quebram a matriz em blocos que\n");
    fmt::print("      cabem em L1 e processam bloco a bloco.\n");
}

void demo_vector_vs_list() {
    fmt::print("\n── 1.3 vector vs list ──\n");

    constexpr std::size_t kN = 2'000'000;

    std::vector<int> v(kN);
    std::iota(v.begin(), v.end(), 0);

    // A lista é preenchida na ordem — o alocador ainda tende a dar blocos
    // próximos. Embaralhar os nós na memória é o caso REAL de uma lista que
    // viveu: inserções e remoções ao longo do tempo espalham os nós.
    std::list<int> l(v.begin(), v.end());
    {
        std::vector<int> tmp(v);
        std::mt19937 rng{12345};
        std::shuffle(tmp.begin(), tmp.end(), rng);
        std::list<int> espalhada;
        for (int x : tmp) espalhada.push_back(x);
        l = std::move(espalhada);
    }

    const double t_vec = bench::mediana_ms([&] {
        long s = 0; for (int x : v) s += x; bench::nao_otimize(s);
    }, 5);
    const double t_list = bench::mediana_ms([&] {
        long s = 0; for (int x : l) s += x; bench::nao_otimize(s);
    }, 5);

    fmt::print("      Somando {} elementos:\n", kN);
    fmt::print("        std::vector<int> : {:7.2f} ms\n", t_vec);
    fmt::print("        std::list<int>   : {:7.2f} ms   ({:.1f}× mais lento)\n",
               t_list, t_list / t_vec);

    fmt::print("\n      Memória por elemento:\n");
    fmt::print("        vector : {} bytes (só o dado)\n", sizeof(int));
    fmt::print("        list   : {} bytes (dado + 2 ponteiros + overhead do\n",
               sizeof(int) + 2 * sizeof(void*));
    fmt::print("                 alocador), cada nó numa alocação separada\n\n");

    fmt::print("      O `std::list` tem inserção O(1) no meio — e ainda assim,\n");
    fmt::print("      na prática, o `std::vector` costuma ganhar até nesse caso:\n");
    fmt::print("      o `memmove` de N elementos contíguos é mais rápido que\n");
    fmt::print("      PERCORRER a lista até a posição de inserção.\n\n");
    fmt::print("      É a palestra de Bjarne Stroustrup em Going Native 2012, e\n");
    fmt::print("      continua valendo. `std::list` é a estrutura certa quando\n");
    fmt::print("      você já tem o iterador E precisa de estabilidade de\n");
    fmt::print("      referências. Fora disso, comece com `vector`.\n");
}

void demo_prefetch() {
    fmt::print("\n── 1.4 O prefetcher ──\n");

    constexpr std::size_t kN = 8'000'000;
    std::vector<int> v(kN, 1);

    // Sequencial: o prefetcher acerta sempre
    const double sequencial = bench::mediana_ms([&] {
        long s = 0; for (std::size_t i = 0; i < kN; ++i) s += v[i];
        bench::nao_otimize(s);
    }, 5);

    // Aleatório: o prefetcher não tem padrão para prever
    std::vector<std::size_t> indices(kN);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), std::mt19937{42});

    const double aleatorio = bench::mediana_ms([&] {
        long s = 0; for (std::size_t i = 0; i < kN; ++i) s += v[indices[i]];
        bench::nao_otimize(s);
    }, 5);

    // Regressivo: sequencial ao contrário — o prefetcher moderno também prevê
    const double regressivo = bench::mediana_ms([&] {
        long s = 0; for (std::size_t i = kN; i-- > 0;) s += v[i];
        bench::nao_otimize(s);
    }, 5);

    fmt::print("      Somando os mesmos {} elementos, em ordens diferentes:\n", kN);
    fmt::print("        crescente  : {:7.2f} ms  (1.0×)\n", sequencial);
    fmt::print("        decrescente: {:7.2f} ms  ({:.1f}×)\n",
               regressivo, regressivo / sequencial);
    fmt::print("        aleatória  : {:7.2f} ms  ({:.1f}×)\n",
               aleatorio, aleatorio / sequencial);

    fmt::print("\n      ⚠  o caso aleatório percorre um SEGUNDO vetor de índices,\n");
    fmt::print("         então parte da diferença é esse tráfego a mais. Ainda\n");
    fmt::print("         assim o fator é grande demais para ser só isso.\n\n");

    fmt::print("      O prefetcher do hardware detecta padrões — passo constante,\n");
    fmt::print("      para frente ou para trás — e busca as linhas ANTES de serem\n");
    fmt::print("      pedidas. Acesso aleatório não tem padrão: cada miss é pago\n");
    fmt::print("      integralmente.\n\n");

    fmt::print("      Consequência de projeto: uma tabela hash aberta com\n");
    fmt::print("      sondagem linear costuma ganhar de uma com encadeamento,\n");
    fmt::print("      não por fazer menos operações, mas por o prefetcher\n");
    fmt::print("      conseguir ajudar. É por isso que `absl::flat_hash_map`\n");
    fmt::print("      ganha de `std::unordered_map` (ver src/linkage/ 5.5).\n");
}

inline void run() {
    demo_linha();
    demo_linha_vs_coluna();
    demo_vector_vs_list();
    demo_prefetch();
}

}  // namespace demo_cache
