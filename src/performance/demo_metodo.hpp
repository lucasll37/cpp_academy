// =============================================================================
//  src/performance/demo_metodo.hpp
//
//  Como medir sem se enganar
//  ─────────────────────────
//  Este arquivo é o mais importante do subprojeto. Os quatro anteriores dão
//  números; este ensina a desconfiar deles.
//
//  A razão é concreta: ao escrever os demos anteriores, TRÊS medições
//  contrariaram o texto que eu tinha escrito antes de medir. Nos três casos
//  o texto estava errado, não a medição — e nos três a explicação era mais
//  interessante que a expectativa.
//
//  Tópicos:
//  • Os cinco jeitos de produzir um número falso
//  • O que o profiler responde e o benchmark não
//  • perf, e o mínimo que vale saber
//  • ⚠ A ordem certa de otimizar
// =============================================================================
#pragma once
#include "bench.hpp"
#include <fmt/core.h>
#include <chrono>
#include <cstddef>
#include <numeric>
#include <vector>

namespace demo_metodo {

void demo_enganos() {
    fmt::print("\n── 5.1 Os cinco jeitos de produzir um número falso ──\n");

    fmt::print("      (1) O CÓDIGO FOI ELIMINADO.\n");
    {
        constexpr std::size_t kN = 10'000'000;
        std::vector<int> v(kN, 1);

        // Aquecimento: o primeiro toque paga page fault. Sem isto, o que a
        // comparação abaixo mediria seria (2), não (1) — foi o que aconteceu
        // na primeira versão deste demo, e o número saiu invertido.
        { long w = 0; for (int x : v) w += x; bench::nao_otimize(w); }

        // Versão MORTA: `s` nunca é lido depois. O compilador tem licença
        // para apagar o laço inteiro.
        const auto t0 = std::chrono::steady_clock::now();
        {
            long s = 0;
            for (int x : v) s += x;
        }                                          // ← `s` morre aqui, sem uso
        const auto t1 = std::chrono::steady_clock::now();
        const double morto =
            std::chrono::duration<double, std::milli>(t1 - t0).count();

        // Versão VIVA: o sumidouro impede a eliminação.
        const auto t2 = std::chrono::steady_clock::now();
        {
            long s = 0;
            for (int x : v) s += x;
            bench::nao_otimize(s);
        }
        const auto t3 = std::chrono::steady_clock::now();
        const double vivo =
            std::chrono::duration<double, std::milli>(t3 - t2).count();

        fmt::print("          somando {} ints, resultado DESCARTADO : {:6.3f} ms\n",
                   kN, morto);
        fmt::print("          o mesmo, com `nao_otimize(s)`        : {:6.3f} ms\n",
                   vivo);
        if (morto < vivo / 4) {
            fmt::print("          → o primeiro foi ELIMINADO: {:.0f}× \"mais rápido\"\n",
                       vivo / (morto > 0 ? morto : 1e-9));
            fmt::print("            porque simplesmente não aconteceu.\n");
        } else {
            fmt::print("          → nesta execução o compilador NÃO eliminou\n");
            fmt::print("            ({:.2f}×). Acontece: a heurística varia com o\n",
                       vivo / morto);
            fmt::print("            nível de otimização e com o entorno. O risco\n");
            fmt::print("            continua real, e o sumidouro custa nada.\n");
        }
    }

    fmt::print("\n      (2) A PRIMEIRA EXECUÇÃO PAGA A MAIS. Page faults, cache\n");
    fmt::print("          frio, TLB frio, e às vezes o próprio carregamento de\n");
    fmt::print("          bibliotecas. Sempre AQUEÇA antes de cronometrar — é o\n");
    fmt::print("          que `bench::mediana_ms` faz na primeira chamada.\n\n");

    fmt::print("      (3) UMA AMOSTRA É RUÍDO. Uma interrupção do sistema\n");
    fmt::print("          operacional no meio da medição distorce a MÉDIA e não\n");
    fmt::print("          move a MEDIANA. Use a mediana de 5 a 11 execuções.\n\n");

    fmt::print("      (4) O NÍVEL DE OTIMIZAÇÃO ERRADO. A seção 4.1 mede um caso\n");
    fmt::print("          em que o efeito é 1.7× em -O0 e 1.0× em -O2. Um\n");
    fmt::print("          benchmark em -O0 não diz nada sobre o binário que você\n");
    fmt::print("          entrega.\n\n");

    fmt::print("      (5) O DADO É AMIGÁVEL DEMAIS. Um vetor recém-alocado,\n");
    fmt::print("          ordenado, todo com o mesmo valor, ou pequeno o\n");
    fmt::print("          bastante para caber em L1, mede outra coisa. A seção\n");
    fmt::print("          1.1 traz um caso em que o próprio benchmark encolheu o\n");
    fmt::print("          conjunto de trabalho sem querer e passou a medir L3 em\n");
    fmt::print("          vez de RAM.\n");
}

void demo_profiler() {
    fmt::print("\n── 5.2 Benchmark ou profiler? ──\n");

    fmt::print("      Eles respondem perguntas DIFERENTES:\n\n");
    fmt::print("        benchmark → \"a versão B é mais rápida que a A?\"\n");
    fmt::print("        profiler  → \"ONDE o programa gasta o tempo?\"\n\n");

    fmt::print("      A ordem quase sempre é: profiler PRIMEIRO, para achar o\n");
    fmt::print("      trecho que importa; benchmark DEPOIS, para comparar\n");
    fmt::print("      alternativas naquele trecho.\n\n");

    fmt::print("      Otimizar sem profiler é o erro mais caro que existe em\n");
    fmt::print("      desempenho — não porque a otimização não funcione, mas\n");
    fmt::print("      porque ela se aplica ao lugar errado. Acelerar em 10× uma\n");
    fmt::print("      função que consome 2% do tempo melhora o total em 1.8%.\n");
    fmt::print("      (é a Lei de Amdahl, e ela é implacável)\n\n");

    fmt::print("      As ferramentas, por tipo:\n");
    fmt::print("        AMOSTRAGEM (baixo custo, roda em produção):\n");
    fmt::print("          perf record / perf report        Linux, o padrão\n");
    fmt::print("          gprofng, VTune, Instruments\n");
    fmt::print("        INSTRUMENTAÇÃO (preciso, e distorce):\n");
    fmt::print("          valgrind --tool=callgrind\n");
    fmt::print("          -pg + gprof                      (obsoleto)\n");
    fmt::print("        CONTADORES DE HARDWARE:\n");
    fmt::print("          perf stat                        cache miss, IPC, branch\n");
    fmt::print("        MEMÓRIA:\n");
    fmt::print("          valgrind --tool=massif           quem alocou o quê\n");
    fmt::print("          heaptrack\n");
}

void demo_perf() {
    fmt::print("\n── 5.3 perf, o mínimo que vale saber ──\n");

    fmt::print("      O panorama, em um comando:\n");
    fmt::print("        perf stat ./prog\n");
    fmt::print("      Ele imprime, entre outros:\n");
    fmt::print("        instructions per cycle (IPC)  ← abaixo de 1.0 = esperando\n");
    fmt::print("                                        memória; acima de 2.0 = bom\n");
    fmt::print("        branch-misses                 ← acima de 5% investigue\n");
    fmt::print("        cache-misses\n\n");

    fmt::print("      Onde o tempo é gasto:\n");
    fmt::print("        perf record -g ./prog        # -g = pilha de chamadas\n");
    fmt::print("        perf report                  # navegável\n");
    fmt::print("        perf annotate                # linha a linha, com asm\n\n");

    fmt::print("      Eventos específicos:\n");
    fmt::print("        perf stat -e cache-misses,cache-references,\\\n");
    fmt::print("                     branch-misses,branches ./prog\n");
    fmt::print("        perf stat -e LLC-load-misses ./prog     # miss no último nível\n");
    fmt::print("        perf c2c record ./prog                  # false sharing (3.3)\n\n");

    fmt::print("      Flame graph — a visualização que mais rende:\n");
    fmt::print("        perf record -g ./prog\n");
    fmt::print("        perf script | stackcollapse-perf.pl | flamegraph.pl > g.svg\n\n");

    fmt::print("      ⚠  Compile com -g MESMO em release, e com\n");
    fmt::print("         -fno-omit-frame-pointer, ou o `perf report` mostra\n");
    fmt::print("         endereços em vez de nomes de função. `-g` não deixa o\n");
    fmt::print("         binário mais lento: só maior.\n\n");

    fmt::print("      ⚠  Em WSL e em contêineres, o `perf` costuma precisar de:\n");
    fmt::print("         sudo sysctl kernel.perf_event_paranoid=1\n");
    fmt::print("         e alguns eventos de hardware não existem em VM.\n");
}

void demo_ordem() {
    fmt::print("\n── 5.4 ⚠ A ordem certa de otimizar ──\n");

    fmt::print("      Do que mais rende para o que menos rende:\n\n");
    fmt::print("      1. O ALGORITMO. O(n log n) no lugar de O(n²) ganha de\n");
    fmt::print("         qualquer micro-otimização, em qualquer hardware. Antes\n");
    fmt::print("         de mexer em cache, verifique se não há um laço aninhado\n");
    fmt::print("         desnecessário.\n\n");

    fmt::print("      2. FAZER MENOS. O trabalho mais rápido é o que não\n");
    fmt::print("         acontece: cache de resultado, saída antecipada,\n");
    fmt::print("         processar em lote, não recalcular o que não mudou.\n\n");

    fmt::print("      3. O LAYOUT DOS DADOS. É o assunto das seções 1 a 3, e é\n");
    fmt::print("         onde estão os fatores de 10× e 35× que este subprojeto\n");
    fmt::print("         mediu. Vem ANTES de mexer em instrução.\n\n");

    fmt::print("      4. PARALELIZAR. Ganho limitado pelo número de núcleos, e\n");
    fmt::print("         acrescenta uma classe inteira de bugs\n");
    fmt::print("         (src/concurrency2/). Só depois de 1-3.\n\n");

    fmt::print("      5. MICRO-OTIMIZAÇÃO. Código sem desvio, SIMD manual,\n");
    fmt::print("         alinhamento fino. Ganhos de 10 a 30%, custo alto de\n");
    fmt::print("         legibilidade, e o compilador frequentemente já fez\n");
    fmt::print("         (seção 4.2). Por último, e só com profiler apontando.\n\n");

    fmt::print("      E as flags, que são de graça:\n");
    fmt::print("        -O2                      o padrão sensato\n");
    fmt::print("        -O3                      mais vetorização; MEÇA, às vezes piora\n");
    fmt::print("        -march=native            usa AVX2/AVX-512 da máquina local\n");
    fmt::print("                                 ⚠ o binário não roda em CPU mais antiga\n");
    fmt::print("        -flto                    otimização entre unidades de tradução\n");
    fmt::print("        -fprofile-generate/-use   PGO: o ganho mais consistente,\n");
    fmt::print("                                 tipicamente 5-15%, e quase nunca usado\n\n");

    fmt::print("      A regra que resume tudo: MEÇA, MUDE, MEÇA DE NOVO. Sem o\n");
    fmt::print("      primeiro passo você não sabe onde mexer; sem o terceiro,\n");
    fmt::print("      não sabe se melhorou.\n");
}

inline void run() {
    demo_enganos();
    demo_profiler();
    demo_perf();
    demo_ordem();
}

}  // namespace demo_metodo
