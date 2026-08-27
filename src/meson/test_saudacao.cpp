// =============================================================================
//  src/meson/test_saudacao.cpp
//
//  O alvo de teste do exemplo. Não é sobre GoogleTest — é sobre `test()`:
//
//      test_exe = executable('test_saudacao', ..., install: false)
//      test('saudacao', test_exe)
//
//  A partir daí, `meson test -C build` roda este binário e reporta pelo código
//  de saída (protocol padrão: 'exitcode'). O `main()` explícito existe para o
//  alvo depender só de `gtest`, sem precisar de `gtest_main`.
//
//  Rodar só este teste:  meson test -C build saudacao --print-errorlogs
// =============================================================================
#include "saudacao.hpp"

#include <gtest/gtest.h>

TEST(Saudacao, IncluiONome) {
    const auto texto = saudacao::cumprimentar("Lima");
    EXPECT_NE(texto.find("Lima"), std::string::npos);
}

TEST(Saudacao, TamanhoBateComOTexto) {
    EXPECT_EQ(saudacao::tamanho_da_saudacao("Lima"),
              saudacao::cumprimentar("Lima").size());
}

TEST(Saudacao, NomeVazioAindaProduzTexto) {
    EXPECT_FALSE(saudacao::cumprimentar("").empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
