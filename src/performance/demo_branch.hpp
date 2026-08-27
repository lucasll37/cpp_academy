// =============================================================================
//  src/performance/demo_branch.hpp
//
//  Predição de desvio — o custo de um `if` imprevisível
//  ────────────────────────────────────────────────────
//  Um processador moderno tem um pipeline de 15 a 20 estágios. Quando chega
//  a um `if`, ele não pode esperar a condição ser calculada — pararia por
//  dezenas de ciclos. Em vez disso, ele APOSTA num caminho e continua
//  executando especulativamente.
//
//  Se acertar, custo zero. Se errar, o pipeline inteiro é descartado e
//  recomeçado: 15 a 20 ciclos perdidos.
//
//  O preditor moderno acerta mais de 95% em código típico. O problema é o
//  `if` que depende de dados IMPREVISÍVEIS — e aí o custo aparece.
//
//  Tópicos:
//  • O experimento clássico: vetor ordenado vs embaralhado
//  • Eliminar o desvio: código sem branch
//  • ⚠ Quando NÃO se preocupar
// =============================================================================
#pragma once
#include "bench.hpp"
#include <fmt/core.h>
#include <algorithm>
#include <cstddef>
#include <random>
#include <vector>

namespace demo_branch {

constexpr std::size_t kN = 8'000'000;

void demo_ordenado() {
    fmt::print("\n── 4.1 Ordenado vs embaralhado ──\n");

    std::mt19937 rng{12345};
    std::uniform_int_distribution<int> dist{0, 255};

    std::vector<int> dados(kN);
    for (auto& x : dados) x = dist(rng);

    std::vector<int> ordenados = dados;
    std::sort(ordenados.begin(), ordenados.end());

    // O MESMO laço, sobre os MESMOS valores. Só a ordem muda.
    auto soma_condicional = [](const std::vector<int>& v) {
        long s = 0;
        for (int x : v) if (x >= 128) s += x;      // ← o desvio
        return s;
    };

    const double t_ordenado    = bench::mediana_ms([&] {
        bench::nao_otimize(soma_condicional(ordenados));
    }, 5);
    const double t_embaralhado = bench::mediana_ms([&] {
        bench::nao_otimize(soma_condicional(dados));
    }, 5);

    fmt::print("      `for (int x : v) if (x >= 128) s += x;` sobre {} valores\n", kN);
    fmt::print("      aleatórios em [0, 255] — os MESMOS valores nos dois casos:\n\n");
    fmt::print("        vetor ORDENADO    : {:7.2f} ms\n", t_ordenado);
    fmt::print("        vetor EMBARALHADO : {:7.2f} ms   ({:.1f}×)\n",
               t_embaralhado, t_embaralhado / t_ordenado);

    fmt::print("\n      Mesma quantidade de trabalho, mesma soma, mesmo acesso\n");
    fmt::print("      sequencial à memória. A ÚNICA diferença é que, no vetor\n");
    fmt::print("      ordenado, a condição é falsa por metade do vetor e\n");
    fmt::print("      verdadeira pela outra metade — duas trocas ao todo, e o\n");
    fmt::print("      preditor acerta praticamente sempre.\n");
    fmt::print("      No embaralhado ela é aleatória: ~50% de erro.\n\n");

    fmt::print("      ⚠  MEDIDO NESTE TOOLCHAIN, em vários níveis de otimização:\n\n");
    fmt::print("        nível | ordenado | embaralhado | razão | cmov no binário\n");
    fmt::print("        ------|----------|-------------|-------|----------------\n");
    fmt::print("        -O0   | 41.50 ms |   69.31 ms  | 1.7×  |  18\n");
    fmt::print("        -O1   |  3.40 ms |    3.41 ms  | 1.0×  |  45\n");
    fmt::print("        -O2   |  3.39 ms |    3.42 ms  | 1.0×  |  36\n");
    fmt::print("        -O3   |  1.38 ms |    1.38 ms  | 1.0×  |  52\n\n");

    fmt::print("      O efeito aparece em -O0 e DESAPARECE a partir de -O1.\n");
    fmt::print("      A causa está na última coluna: com otimização, o GCC 13\n");
    fmt::print("      converte o `if` em `cmov` — movimento condicional, SEM\n");
    fmt::print("      desvio. Não há aposta, logo não há como errar a aposta.\n\n");

    fmt::print("      Esta é a resposta mais votada de todo o Stack Overflow\n");
    fmt::print("      (\"Why is processing a sorted array faster than an\n");
    fmt::print("      unsorted array?\", 2012) — e, no compilador de hoje com\n");
    fmt::print("      otimização ligada, ela NÃO REPRODUZ mais.\n\n");

    fmt::print("      Duas lições, e a segunda é a que vale:\n");
    fmt::print("        1. o compilador moderno já elimina boa parte dos desvios\n");
    fmt::print("           imprevisíveis por conta própria;\n");
    fmt::print("        2. um benchmark medido em -O0 não diz nada sobre o\n");
    fmt::print("           programa que você vai entregar. Meça no nível de\n");
    fmt::print("           otimização de PRODUÇÃO — sempre.\n\n");

    fmt::print("      (o efeito de predição continua real; ele só não está mais\n");
    fmt::print("       NESTE laço. Um desvio que o compilador não consegue\n");
    fmt::print("       converter — com chamada de função no corpo, por exemplo —\n");
    fmt::print("       continua pagando os ~15 ciclos por erro)\n");
}

void demo_sem_branch() {
    fmt::print("\n── 4.2 Código sem desvio ──\n");

    std::mt19937 rng{999};
    std::uniform_int_distribution<int> dist{0, 255};
    std::vector<int> dados(kN);
    for (auto& x : dados) x = dist(rng);

    // (a) com `if` explícito
    const double t_if = bench::mediana_ms([&] {
        long s = 0;
        for (int x : dados) if (x >= 128) s += x;
        bench::nao_otimize(s);
    }, 5);

    // (b) aritmética: a máscara vale 0 ou -1 (todos os bits), sem desvio
    const double t_mascara = bench::mediana_ms([&] {
        long s = 0;
        for (int x : dados) {
            const long mascara = -static_cast<long>(x >= 128);   // 0 ou -1
            s += x & mascara;
        }
        bench::nao_otimize(s);
    }, 5);

    // (c) multiplicação pela condição — outra forma sem desvio
    const double t_mult = bench::mediana_ms([&] {
        long s = 0;
        for (int x : dados) s += x * static_cast<long>(x >= 128);
        bench::nao_otimize(s);
    }, 5);

    fmt::print("      Três formas do mesmo cálculo:\n");
    fmt::print("        (a) if (x >= 128) s += x;      {:7.2f} ms\n", t_if);
    fmt::print("        (b) s += x & -(x >= 128);      {:7.2f} ms  ({:.2f}×)\n",
               t_mascara, t_mascara / t_if);
    fmt::print("        (c) s += x * (x >= 128);       {:7.2f} ms  ({:.2f}×)\n",
               t_mult, t_mult / t_if);

    fmt::print("\n      ⚠  Antes de sair reescrevendo `if` como aritmética: com\n");
    fmt::print("         -O2 o GCC muitas vezes JÁ faz essa transformação, usando\n");
    fmt::print("         a instrução `cmov` (conditional move). Se os três tempos\n");
    fmt::print("         acima são parecidos, foi isso que aconteceu — e o\n");
    fmt::print("         trabalho manual só piorou a legibilidade.\n\n");

    fmt::print("      Para confirmar em vez de supor:\n");
    fmt::print("        g++ -O2 -S -masm=intel arq.cpp -o - | grep -c cmov\n");
    fmt::print("        objdump -d ./prog | grep -E 'cmov|jle|jge'\n\n");

    fmt::print("      As dicas explícitas ao compilador:\n");
    fmt::print("        if (v) [[likely]]   {{ ... }}      // C++20\n");
    fmt::print("        if (v) [[unlikely]] {{ ... }}\n");
    fmt::print("        __builtin_expect(cond, 1)        // GCC/Clang, pré-C++20\n");
    fmt::print("      ⚠  Elas ajudam quando você SABE a proporção (o caminho de\n");
    fmt::print("         erro é raro). Chutar errado piora — e o PGO\n");
    fmt::print("         (-fprofile-generate / -fprofile-use) mede em vez de\n");
    fmt::print("         chutar, o que quase sempre é melhor.\n");
}

void demo_quando_nao() {
    fmt::print("\n── 4.3 ⚠ Quando NÃO se preocupar ──\n");

    fmt::print("      A predição só importa quando TRÊS coisas coincidem:\n");
    fmt::print("        1. o desvio está num laço muito quente;\n");
    fmt::print("        2. a condição é genuinamente imprevisível;\n");
    fmt::print("        3. o corpo é CURTO — se ele faz trabalho de verdade,\n");
    fmt::print("           os ~15 ciclos da penalidade desaparecem no meio.\n\n");

    fmt::print("      Desvios que o preditor acerta quase sempre, e com os quais\n");
    fmt::print("      você não deve gastar tempo:\n");
    fmt::print("        • a condição de parada de um `for` (erra UMA vez, no fim);\n");
    fmt::print("        • checagem de erro que quase nunca dispara;\n");
    fmt::print("        • `if (ponteiro)` num código onde ele quase sempre é válido;\n");
    fmt::print("        • qualquer desvio fora de um laço quente.\n\n");

    fmt::print("      E o custo real de uma exceção NÃO LANÇADA é zero: o modelo\n");
    fmt::print("      \"zero-cost\" do C++ não emite desvio nenhum no caminho\n");
    fmt::print("      feliz — a informação de unwinding fica numa tabela à parte.\n");
    fmt::print("      Ver src/errors/demo_exceptions.hpp, onde isso é medido.\n\n");

    fmt::print("      Como MEDIR em vez de supor:\n");
    fmt::print("        perf stat -e branches,branch-misses ./prog\n");
    fmt::print("      Uma taxa de erro abaixo de 1% é normal. Acima de 5% num\n");
    fmt::print("      laço quente, vale investigar. Acima de 20%, você achou.\n");
}

inline void run() {
    demo_ordenado();
    demo_sem_branch();
    demo_quando_nao();
}

}  // namespace demo_branch
