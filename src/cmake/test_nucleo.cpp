// =============================================================================
//  src/cmake/test_nucleo.cpp
//
//  Os testes de verdade — registrados no CTest pelo CMakeLists.txt
//  ───────────────────────────────────────────────────────────────
//  O que este arquivo acrescenta a src/testing/ (que cobre o GoogleTest em si)
//  é o lado do BUILD: como o CMake acha o gtest, cria o alvo, e registra os
//  casos no CTest com `gtest_discover_tests`.
//
//  Rode com:
//      cd src/cmake && cmake -S . -B build && cmake --build build
//      ctest --test-dir build --output-on-failure
// =============================================================================
#include "nucleo.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <span>
#include <vector>

namespace {

TEST(Saxpy, CalculaCorretamente) {
    const std::vector<float> x{1.0f, 2.0f, 3.0f};
    std::vector<float>       y{10.0f, 20.0f, 30.0f};

    nucleo::saxpy(2.0f, x, y);

    ASSERT_EQ(y.size(), 3u);
    EXPECT_FLOAT_EQ(y[0], 12.0f);      // 2*1 + 10
    EXPECT_FLOAT_EQ(y[1], 24.0f);      // 2*2 + 20
    EXPECT_FLOAT_EQ(y[2], 36.0f);      // 2*3 + 30
}

TEST(Saxpy, AlfaZeroNaoAltera) {
    const std::vector<float> x{1.0f, 2.0f, 3.0f};
    const std::vector<float> original{10.0f, 20.0f, 30.0f};
    std::vector<float>       y = original;

    nucleo::saxpy(0.0f, x, y);

    EXPECT_DOUBLE_EQ(nucleo::erro_maximo(y, original), 0.0);
}

TEST(Saxpy, VetorVazioNaoQuebra) {
    const std::vector<float> x;
    std::vector<float>       y;
    EXPECT_NO_THROW(nucleo::saxpy(1.0f, x, y));
}

// ⚠ `std::span` não carrega tamanho comum: se x e y têm tamanhos diferentes,
//   a implementação usa o MENOR. Este teste trava esse contrato — sem ele,
//   alguém "otimizaria" removendo o `std::min` e criaria um overflow.
TEST(Saxpy, TamanhosDiferentesUsamOMenor) {
    const std::vector<float> x{1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float>       y{0.0f, 0.0f};

    nucleo::saxpy(1.0f, x, y);

    ASSERT_EQ(y.size(), 2u);
    EXPECT_FLOAT_EQ(y[0], 1.0f);
    EXPECT_FLOAT_EQ(y[1], 2.0f);
}

TEST(Gerar, EhDeterministico) {
    // A mesma semente precisa dar o mesmo vetor — senão nenhum benchmark
    // deste subprojeto seria reprodutível.
    const auto a = nucleo::gerar(1000, 42);
    const auto b = nucleo::gerar(1000, 42);
    const auto c = nucleo::gerar(1000, 43);

    EXPECT_DOUBLE_EQ(nucleo::erro_maximo(a, b), 0.0) << "mesma semente";
    EXPECT_GT(nucleo::erro_maximo(a, c), 0.0)        << "sementes diferentes";
}

TEST(Gerar, RespeitaAFaixa) {
    for (float v : nucleo::gerar(10'000, 7)) {
        EXPECT_GE(v, -1.0f);
        EXPECT_LE(v,  1.0f);
    }
}

TEST(Soma, AcumulaEmDouble) {
    // 10 milhões de 0.1f somados: em `float` o resultado degrada de forma
    // visível; em `double`, não. É por isso que `nucleo::soma` acumula em
    // double mesmo recebendo floats.
    const std::vector<float> v(10'000'000, 0.1f);

    const double em_double = nucleo::soma(v);

    float em_float = 0.0f;
    for (float x : v) em_float += x;

    // O valor exato de 0.1f não é 0.1, então o esperado tem o seu próprio erro.
    const double esperado = 10'000'000.0 * static_cast<double>(0.1f);

    EXPECT_NEAR(em_double, esperado, esperado * 1e-9)
        << "acumulando em double";
    EXPECT_GT(std::fabs(static_cast<double>(em_float) - esperado), esperado * 1e-3)
        << "acumulando em float, o erro precisa ser GRANDE — se este teste "
           "falhar, o compilador vetorizou o laço e mudou a ordem da soma";
}

TEST(ErroMaximo, ZeroParaIguais) {
    const auto v = nucleo::gerar(100, 1);
    EXPECT_DOUBLE_EQ(nucleo::erro_maximo(v, v), 0.0);
}

TEST(ErroMaximo, EncontraAMaiorDiferenca) {
    const std::vector<float> a{1.0f, 2.0f,  3.0f};
    const std::vector<float> b{1.0f, 2.5f, -1.0f};
    EXPECT_NEAR(nucleo::erro_maximo(a, b), 4.0, 1e-9);
}

}  // namespace
