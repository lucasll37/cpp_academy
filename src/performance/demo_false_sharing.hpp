// =============================================================================
//  src/performance/demo_false_sharing.hpp
//
//  False sharing — quando threads independentes brigam
//  ───────────────────────────────────────────────────
//  Duas threads, duas variáveis DIFERENTES, nenhum mutex, nenhum data race.
//  E o programa fica várias vezes mais lento do que se rodasse numa thread só.
//
//  A causa é que a unidade de coerência de cache não é a variável — é a
//  LINHA. Se `a` e `b` caem na mesma linha de 64 bytes e o núcleo 0 escreve
//  em `a`, o protocolo MESI invalida aquela linha no núcleo 1 — que tem que
//  buscá-la de novo para ler `b`. Os dois ficam trocando a mesma linha entre
//  os caches, ida e volta, milhões de vezes.
//
//  É o bug de desempenho mais fácil de introduzir e mais difícil de enxergar:
//  o código está CORRETO.
//
//  Tópicos:
//  • A medição
//  • A correção: alignas
//  • ⚠ Onde ele se esconde
// =============================================================================
#pragma once
#include "bench.hpp"
#include <fmt/core.h>
#include <atomic>
#include <cstddef>
#include <new>
#include <thread>
#include <vector>

namespace demo_false_sharing {

#ifdef __cpp_lib_hardware_interference_size
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Winterference-size"
inline constexpr std::size_t kLinha = std::hardware_destructive_interference_size;
#  pragma GCC diagnostic pop
#else
inline constexpr std::size_t kLinha = 64;
#endif

constexpr int kThreads = 4;
constexpr long kIters  = 20'000'000;

// Os contadores GRUDADOS: quatro `long` de 8 bytes = 32 bytes, tudo na mesma
// linha de 64.
struct Grudados {
    std::atomic<long> c[kThreads]{};
};

// Os contadores SEPARADOS: cada um na sua linha.
struct Separados {
    struct alignas(kLinha) Isolado {
        std::atomic<long> valor{0};
    };
    Isolado c[kThreads]{};
};

void demo_medicao() {
    fmt::print("\n── 3.1 A medição ──\n");

    fmt::print("      sizeof(Grudados)  = {:3} bytes  → {} contadores na MESMA linha\n",
               sizeof(Grudados), kThreads);
    fmt::print("      sizeof(Separados) = {:3} bytes  → um por linha de {} bytes\n",
               sizeof(Separados), kLinha);

    auto roda = [](auto& contadores, auto acessa) {
        std::vector<std::jthread> ts;
        for (int t = 0; t < kThreads; ++t)
            ts.emplace_back([t, &contadores, &acessa] {
                for (long i = 0; i < kIters; ++i)
                    acessa(contadores, t).fetch_add(1, std::memory_order_relaxed);
            });
    };

    Grudados  g;
    Separados s;

    const double t_grudado = bench::mediana_ms([&] {
        roda(g, [](Grudados& x, int t) -> std::atomic<long>& { return x.c[t]; });
    }, 3);

    const double t_separado = bench::mediana_ms([&] {
        roda(s, [](Separados& x, int t) -> std::atomic<long>& { return x.c[t].valor; });
    }, 3);

    fmt::print("\n      {} threads × {} incrementos, cada uma no SEU contador:\n",
               kThreads, kIters);
    fmt::print("        contadores na MESMA linha : {:8.2f} ms\n", t_grudado);
    fmt::print("        contadores SEPARADOS      : {:8.2f} ms   ({:.1f}× mais rápido)\n",
               t_separado, t_grudado / t_separado);

    fmt::print("\n      Não há mutex, não há data race, cada thread mexe só no\n");
    fmt::print("      SEU contador. O código está correto nos dois casos. A\n");
    fmt::print("      única diferença é o `alignas({})`.\n", kLinha);

    fmt::print("\n      O custo em memória da correção:\n");
    fmt::print("        {} bytes → {} bytes ({:.0f}× mais)\n",
               sizeof(Grudados), sizeof(Separados),
               static_cast<double>(sizeof(Separados)) / sizeof(Grudados));
    fmt::print("      Para 4 contadores, 224 bytes a mais é irrelevante. Para\n");
    fmt::print("      4 milhões de objetos, seria proibitivo — e aí a resposta\n");
    fmt::print("      é outra: acumular em variável LOCAL e somar no fim.\n");
}

void demo_correcao() {
    fmt::print("\n── 3.2 As correções, em ordem ──\n");

    fmt::print("      (1) A MELHOR: não compartilhar. Acumule numa variável\n");
    fmt::print("          LOCAL da thread e escreva UMA vez no fim:\n\n");
    fmt::print("            long local = 0;\n");
    fmt::print("            for (...) ++local;              // registrador!\n");
    fmt::print("            total.fetch_add(local);         // uma escrita\n\n");

    // Medindo a versão local
    std::atomic<long> total{0};
    const double t_local = bench::mediana_ms([&] {
        total = 0;
        std::vector<std::jthread> ts;
        for (int t = 0; t < kThreads; ++t)
            ts.emplace_back([&total] {
                long local = 0;
                for (long i = 0; i < kIters; ++i) ++local;
                total.fetch_add(local, std::memory_order_relaxed);
            });
    }, 3);

    fmt::print("          medido: {:8.2f} ms — e o resultado ({}) confere.\n",
               t_local, total.load());
    fmt::print("          Não é só \"sem false sharing\": o `++local` vira um\n");
    fmt::print("          registrador, sem tocar em memória nenhuma.\n\n");

    fmt::print("      (2) `alignas(hardware_destructive_interference_size)` — quando\n");
    fmt::print("          o compartilhamento é inevitável. Foi o de 3.1.\n\n");

    fmt::print("      (3) PADDING manual, se precisar de portabilidade estrita:\n");
    fmt::print("            struct Contador {{\n");
    fmt::print("                std::atomic<long> valor;\n");
    fmt::print("                char preenchimento[64 - sizeof(std::atomic<long>)];\n");
    fmt::print("            }};\n\n");

    fmt::print("      ⚠  `hardware_destructive_interference_size` tem uma\n");
    fmt::print("         controvérsia: o valor pode mudar entre versões do\n");
    fmt::print("         compilador e quebrar ABI, e o GCC AVISA\n");
    fmt::print("         (-Winterference-size). Neste arquivo ele está suprimido\n");
    fmt::print("         com pragma, porque o uso é local e não atravessa\n");
    fmt::print("         fronteira de biblioteca. Ver src/layout/ e src/linkage/.\n");
}

void demo_onde_se_esconde() {
    fmt::print("\n── 3.3 ⚠ Onde ele se esconde ──\n");

    fmt::print("      (a) Contadores de estatística por thread num array:\n");
    fmt::print("            std::atomic<long> acessos[kThreads];   // ⚠\n\n");

    fmt::print("      (b) Uma struct compartilhada com campos \"independentes\":\n");
    fmt::print("            struct Fila {{\n");
    fmt::print("                std::atomic<size_t> cabeca;   // só o produtor\n");
    fmt::print("                std::atomic<size_t> cauda;    // só o consumidor\n");
    fmt::print("            }};                                // ⚠ mesma linha!\n");
    fmt::print("          É exatamente por isso que a FilaSPSC de\n");
    fmt::print("          src/concurrency2/demo_lockfree.hpp separa os dois\n");
    fmt::print("          índices com `alignas`.\n\n");

    fmt::print("      (c) Um `std::vector<bool>` escrito por várias threads.\n");
    fmt::print("          Aqui é pior que false sharing: os bits COMPARTILHAM\n");
    fmt::print("          bytes, então é DATA RACE de verdade — UB, não só\n");
    fmt::print("          lentidão. Use `std::vector<char>` ou\n");
    fmt::print("          `std::deque<bool>`. Ver src/bits/demo_bitset.hpp.\n\n");

    fmt::print("      (d) Objetos pequenos alocados em sequência e distribuídos\n");
    fmt::print("          entre threads — o alocador os põe lado a lado.\n\n");

    fmt::print("      Como DIAGNOSTICAR (não adivinhar):\n");
    fmt::print("        perf stat -e cache-misses,cache-references ./prog\n");
    fmt::print("        perf c2c record ./prog && perf c2c report\n");
    fmt::print("          ↑ o `c2c` (cache-to-cache) foi feito exatamente para\n");
    fmt::print("            isto: mostra QUAIS linhas estão pingando entre\n");
    fmt::print("            núcleos e quais variáveis moram nelas.\n");
    fmt::print("        valgrind --tool=cachegrind ./prog\n");
}

inline void run() {
    demo_medicao();
    demo_correcao();
    demo_onde_se_esconde();
}

}  // namespace demo_false_sharing
