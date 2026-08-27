// =============================================================================
//  src/cmake/omp_saxpy.cpp
//
//  OpenMP — paralelismo de memória COMPARTILHADA
//  ─────────────────────────────────────────────
//  O modelo: um processo, várias threads, todas enxergando a mesma memória.
//  A paralelização é uma DIRETIVA de pragma — o código serial continua
//  válido e compilável sem `-fopenmp` (o compilador ignora pragmas que não
//  conhece, com um aviso).
//
//  ⚠ Essa propriedade é a maior virtude do OpenMP e a sua maior armadilha:
//    o código COMPILA sem o flag e roda serial, silenciosamente. Um build
//    mal configurado não falha — só fica lento. É por isso que o
//    CMakeLists.txt deste subprojeto imprime explicitamente se achou OpenMP.
//
//  Este alvo é construído pelo CMake com `OpenMP::OpenMP_CXX` — ver a seção 5
//  do CMakeLists.txt.
// =============================================================================
#include "nucleo.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#ifdef _OPENMP
#  include <omp.h>
#endif

namespace {

// A versão paralela. É o MESMO laço de nucleo::saxpy, com uma linha a mais.
void saxpy_omp(float a, std::span<const float> x, std::span<float> y) {
    const long n = static_cast<long>(std::min(x.size(), y.size()));

    // `schedule(static)` porque cada iteração custa o mesmo — não há por que
    // pagar o overhead de distribuição dinâmica. Ver a discussão no README.
#pragma omp parallel for schedule(static)
    for (long i = 0; i < n; ++i) y[static_cast<std::size_t>(i)] =
        a * x[static_cast<std::size_t>(i)] + y[static_cast<std::size_t>(i)];
}

// Uma REDUÇÃO — o segundo padrão fundamental do OpenMP.
// Sem `reduction(+:s)`, todas as threads escreveriam em `s` ao mesmo tempo:
// data race, UB. Ver src/undefined_behavior/ e src/concurrency2/.
double soma_omp(std::span<const float> v) {
    double     s = 0.0;
    const long n = static_cast<long>(v.size());

#pragma omp parallel for reduction(+ : s) schedule(static)
    for (long i = 0; i < n; ++i) s += v[static_cast<std::size_t>(i)];
    return s;
}

// ⚠ Medir uma vez só produz um número falso. A primeira região paralela paga
//   a CRIAÇÃO do pool de threads, e o primeiro toque em cada vetor paga page
//   fault. A primeira versão deste arquivo media assim e reportou um speedup
//   de 0.32x — a versão "paralela" saiu 3x MAIS LENTA que a serial.
//   O método correto está em src/performance/demo_metodo.hpp: aqueça primeiro,
//   e use a MEDIANA de várias execuções.
//   ⚠ E a PREPARAÇÃO fica fora do cronômetro. A segunda versão deste arquivo
//     restaurava o vetor `y` DENTRO da região medida — uma cópia de 80 MB que
//     dominava as duas medições igualmente e escondia a diferença entre elas.
//     O resultado saiu 1.03x, e não porque o paralelo não ajudasse: porque o
//     que estava sendo medido era o `memcpy`.
template <typename Preparar, typename Medir>
double mediana_ms(Preparar&& preparar, Medir&& medir, int repeticoes = 7) {
    std::vector<double> amostras;
    amostras.reserve(static_cast<std::size_t>(repeticoes));

    preparar();
    medir();                                   // aquecimento, fora da conta

    for (int i = 0; i < repeticoes; ++i) {
        preparar();                            // ← FORA do cronômetro
        const auto t0 = std::chrono::steady_clock::now();
        medir();                               // ← só isto é medido
        amostras.push_back(std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - t0).count());
    }
    std::sort(amostras.begin(), amostras.end());
    return amostras[amostras.size() / 2];
}

}  // namespace

namespace {

// Roda um tamanho e imprime a linha correspondente.
void mede_um_tamanho(std::size_t n) {
    const auto x         = nucleo::gerar(n, 1);
    const auto y_inicial = nucleo::gerar(n, 2);
    auto       y_ref     = y_inicial;
    auto       y_par     = y_inicial;

    constexpr float a = 2.5f;

    const double t_serial = mediana_ms(
        [&] { y_ref = y_inicial; },                    // preparar (fora do relógio)
        [&] { nucleo::saxpy(a, x, y_ref); });          // medir

    const double t_paralelo = mediana_ms(
        [&] { y_par = y_inicial; },
        [&] { saxpy_omp(a, x, y_par); });

    const double mb   = 3.0 * n * sizeof(float) / (1024.0 * 1024.0);
    const double erro = nucleo::erro_maximo(y_ref, y_par);

    std::printf("  %10zu | %8.0f | %8.2f | %8.2f | %6.2fx | %.1e\n",
                n, mb, t_serial, t_paralelo, t_serial / t_paralelo, erro);
}

}  // namespace

int main(int argc, char** argv) {

#ifdef _OPENMP
    std::printf("OpenMP ATIVO — versão %d, %d threads disponíveis\n\n", _OPENMP,
                omp_get_max_threads());
#else
    std::printf("⚠ OpenMP INATIVO: compilado sem -fopenmp. Os pragmas foram\n"
                "  ignorados e tudo roda serial.\n\n");
#endif

    std::printf("SAXPY: y = 2.5*x + y, mediana de 7 execuções\n\n");
    std::printf("  %10s | %8s | %8s | %8s | %7s | %s\n",
                "elementos", "MB", "serial", "OpenMP", "speedup", "erro");
    std::printf("  -----------|----------|----------|----------|---------|------\n");

    if (argc > 1) {
        mede_um_tamanho(std::strtoul(argv[1], nullptr, 10));
    } else {
        // Dois regimes de propósito: um que CABE no cache de último nível e
        // outro que não cabe. A diferença entre as duas linhas é a lição.
        for (std::size_t n : {250'000u, 2'000'000u, 20'000'000u, 60'000'000u})
            mede_um_tamanho(n);
    }

    // ── reduction, o segundo padrão fundamental ─────────────────────────────
    {
        const auto v = nucleo::gerar(20'000'000, 7);

        const double s_serial = nucleo::soma(v);
        const double s_omp    = soma_omp(v);

        std::printf("\n  reduction(+:s) sobre 20M elementos:\n");
        std::printf("    serial : %.6f\n", s_serial);
        std::printf("    OpenMP : %.6f\n", s_omp);
        const double dif = std::fabs(s_serial - s_omp);
        std::printf("    diferença: %.3e\n", dif);

        if (dif == 0.0) {
            std::printf("    Nesta execução deu ZERO — mas isso é sorte, não\n");
            std::printf("    garantia.\n");
        }
        std::printf("    ⚠ A soma paralela agrupa os termos em outra ORDEM, e\n");
        std::printf("      adição de ponto flutuante NÃO é associativa:\n");
        std::printf("      (a+b)+c != a+(b+c) em binário. Um resultado paralelo\n");
        std::printf("      bit-a-bit idêntico ao serial é COINCIDÊNCIA, não\n");
        std::printf("      contrato — ele muda com o número de threads, com o\n");
        std::printf("      `schedule` e com a versão do compilador.\n");
        std::printf("      Testes que exigem igualdade exata de resultado\n");
        std::printf("      paralelo falham por isso, não por bug. Compare com\n");
        std::printf("      tolerância: src/testing/demo_anatomia.hpp, 2.5.\n");
    }

    std::printf("\n⚠ Repare no speedup CAINDO conforme os dados crescem.\n");
    std::printf("  SAXPY faz 1 multiplicação-soma para cada 12 bytes movidos —\n");
    std::printf("  é limitado por BANDA DE MEMÓRIA, não por CPU.\n\n");
    std::printf("  Enquanto os dados cabem no cache de último nível, as threads\n");
    std::printf("  somam a banda dos caches e o speedup aparece. Quando os dados\n");
    std::printf("  vêm da RAM, todas as threads disputam os MESMOS canais de\n");
    std::printf("  memória, e acrescentar threads não acrescenta banda.\n\n");
#ifdef _OPENMP
    std::printf("  Esta máquina tem %d threads, e ainda assim o speedup satura\n",
                omp_get_max_threads());
    std::printf("  bem antes disso. NÃO é decepção — é o teto do hardware, e o\n");
    std::printf("  motivo pelo qual \"paralelizar\" é o item 4 da lista de\n");
    std::printf("  otimização em src/performance/demo_metodo.hpp, não o item 1.\n");
#endif
    return 0;
}
