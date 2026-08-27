// =============================================================================
//  src/performance/bench.hpp
//
//  O mínimo de infraestrutura para medir sem se enganar
//  ────────────────────────────────────────────────────
//  Um benchmark ingênuo em C++ mede o otimizador, não o código. Três coisas
//  precisam estar certas antes de qualquer número valer:
//
//    1. o resultado precisa ser CONSUMIDO, ou o compilador apaga o cálculo;
//    2. o primeiro acesso à memória paga page fault e cache frio — o
//       AQUECIMENTO precisa ficar fora da medição;
//    3. uma execução só é ruído. Precisa da MEDIANA de várias.
//
//  Ver src/chrono/demo_medicao.hpp, que trata disto em detalhe.
// =============================================================================
#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <vector>

namespace bench {

// ── O sumidouro ─────────────────────────────────────────────────────────────
//  `asm volatile("" : : "r"(x) : "memory")` diz ao compilador: "esta função
//  leu `x` e pode ter mexido em qualquer memória". Ele não pode mais provar
//  que o cálculo é inútil, e não pode reordenar acessos por cima dela.
//  É o mesmo truque de `benchmark::DoNotOptimize` do Google Benchmark.
template <typename T>
inline void nao_otimize(T const& valor) {
    asm volatile("" : : "r,m"(valor) : "memory");
}

inline void barreira_de_memoria() {
    asm volatile("" : : : "memory");
}

// ── O cronômetro ────────────────────────────────────────────────────────────
//  Devolve a MEDIANA de `repeticoes` execuções, em milissegundos. A mediana,
//  e não a média: uma interrupção do sistema operacional no meio de uma
//  execução distorce a média e não move a mediana.
template <typename F>
double mediana_ms(F&& corpo, int repeticoes = 7) {
    std::vector<double> amostras;
    amostras.reserve(static_cast<std::size_t>(repeticoes));

    corpo();                                     // aquecimento, fora da conta

    for (int i = 0; i < repeticoes; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        corpo();
        const auto t1 = std::chrono::steady_clock::now();
        amostras.push_back(
            std::chrono::duration<double, std::milli>(t1 - t0).count());
    }

    std::sort(amostras.begin(), amostras.end());
    return amostras[amostras.size() / 2];
}

}  // namespace bench
