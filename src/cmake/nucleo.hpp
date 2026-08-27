// =============================================================================
//  src/cmake/nucleo.hpp
//
//  A biblioteca de exemplo — o que os alvos do CMakeLists.txt constroem
//  ───────────────────────────────────────────────────────────────────
//  Deliberadamente pequena: SAXPY (`y = a*x + y`), a operação de BLAS nível 1.
//  Ela serve aos cinco demos porque é a mesma conta em todos os lugares:
//
//    • serial      → a referência de corretude (nucleo.cpp)
//    • OpenMP      → o mesmo laço com uma diretiva  (omp_saxpy.cpp)
//    • MPI         → o vetor dividido entre processos (mpi_anel.cpp)
//    • CUDA        → o mesmo laço num kernel        (cuda_saxpy.cu)
//    • testes      → todos comparados contra o serial (test_nucleo.cpp)
//
//  Ter UMA operação em quatro tecnologias é o que torna a comparação honesta.
// =============================================================================
#pragma once
#include <cstddef>
#include <span>
#include <vector>

namespace nucleo {

// y[i] = a * x[i] + y[i]   — a versão de referência, single-thread
void saxpy(float a, std::span<const float> x, std::span<float> y);

// Soma de todos os elementos, para verificar resultados sem comparar
// elemento a elemento em ponto flutuante.
double soma(std::span<const float> v);

// Erro máximo absoluto entre dois vetores — a forma correta de comparar
// resultados de float vindos de caminhos de execução diferentes.
// Ver src/testing/demo_anatomia.hpp, 2.5.
double erro_maximo(std::span<const float> a, std::span<const float> b);

// Gera dados determinísticos (mesma semente sempre) para os benchmarks.
std::vector<float> gerar(std::size_t n, unsigned semente = 42);

}  // namespace nucleo
