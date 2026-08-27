// =============================================================================
//  src/cmake/demo_testes.hpp
//
//  CTest — o lado do BUILD dos testes automatizados
//  ────────────────────────────────────────────────
//  src/testing/ cobre o GoogleTest: asserções, fixtures, TEST_P, gmock, death
//  tests. Aqui o assunto é o outro lado: como o CMake acha o gtest, cria o
//  alvo, e registra os casos no CTest.
//
//  Tópicos:
//  • enable_testing e add_test
//  • gtest_discover_tests vs add_test simples
//  • As propriedades de teste que importam
//  • Rodando: ctest e suas flags
//  • ⚠ Sanitizers e cobertura no CMake
// =============================================================================
#pragma once
#include "config.hpp"

#include <cstdio>

namespace demo_testes {

inline void demo_registro() {
    std::printf("\n── 5.1 Registrando testes ──\n");

    std::printf("        include(CTest)\n");
    std::printf("        enable_testing()          ← ANTES de qualquer add_test\n\n");
    std::printf("        add_executable(test_nucleo test_nucleo.cpp)\n");
    std::printf("        target_link_libraries(test_nucleo PRIVATE\n");
    std::printf("            academy::nucleo GTest::gtest GTest::gtest_main)\n\n");

    std::printf("      Duas formas de registrar, e a diferença é grande:\n\n");
    std::printf("        add_test(NAME nucleo COMMAND test_nucleo)\n");
    std::printf("          → UM teste no CTest. Se um caso falhar, o CTest diz\n");
    std::printf("            \"nucleo falhou\" e nada mais.\n\n");
    std::printf("        include(GoogleTest)\n");
    std::printf("        gtest_discover_tests(test_nucleo)\n");
    std::printf("          → RODA o binário durante o build com\n");
    std::printf("            --gtest_list_tests e registra CADA CASO. A saída do\n");
    std::printf("            ctest passa a nomear o caso exato, e `ctest -R\n");
    std::printf("            Saxpy` funciona.\n\n");

    std::printf("      ⚠ `gtest_discover_tests` EXECUTA o binário na hora do\n");
    std::printf("        build. Isso quebra em cross-compilation (o binário é de\n");
    std::printf("        outra arquitetura). Nesse caso use `gtest_add_tests`,\n");
    std::printf("        que lê o FONTE procurando as macros TEST, ou o\n");
    std::printf("        `add_test` simples.\n\n");

    std::printf("      Um executável comum também pode virar teste de fumaça:\n");
    std::printf("        add_test(NAME omp_roda COMMAND omp_saxpy 100000)\n");
    std::printf("      Ele passa se sair com código 0. É barato e pega crash,\n");
    std::printf("      abort e — sob sanitizer — UB. Este CMakeLists faz isso\n");
    std::printf("      com `omp_saxpy` e com o próprio `cmake_guia`.\n");
}

inline void demo_propriedades() {
    std::printf("\n── 5.2 Propriedades de teste ──\n");

    std::printf("        set_tests_properties(meu_teste PROPERTIES\n");
    std::printf("            TIMEOUT              60      # segundos até matar\n");
    std::printf("            WILL_FAIL            TRUE    # espera-se que FALHE\n");
    std::printf("            LABELS               \"rapido;unitario\"\n");
    std::printf("            RUN_SERIAL           TRUE    # não paralelize\n");
    std::printf("            ENVIRONMENT          \"ASAN_OPTIONS=detect_leaks=1\"\n");
    std::printf("            DEPENDS              outro_teste\n");
    std::printf("            PASS_REGULAR_EXPRESSION \"tudo ok\"\n");
    std::printf("            FIXTURES_REQUIRED    banco_no_ar)\n\n");

    std::printf("      As mais úteis, na prática:\n\n");
    std::printf("        TIMEOUT     — o padrão é 1500 s. Um teste que trava\n");
    std::printf("                      segura o CI por 25 minutos. Ponha um\n");
    std::printf("                      valor realista em TODO teste.\n\n");
    std::printf("        LABELS      — `ctest -L rapido` roda só os rápidos no\n");
    std::printf("                      commit, e tudo no merge.\n\n");
    std::printf("        RUN_SERIAL  — para quem disputa um recurso (uma porta,\n");
    std::printf("                      um arquivo, a GPU). Sem isso, `ctest -j8`\n");
    std::printf("                      produz falhas intermitentes que parecem\n");
    std::printf("                      flakiness do teste. Ver src/testing/ 1.4.\n\n");
    std::printf("        FIXTURES_*  — SETUP/CLEANUP/REQUIRED montam e desmontam\n");
    std::printf("                      um recurso caro uma vez para vários\n");
    std::printf("                      testes, e o CTest garante a ordem.\n");
}

inline void demo_rodando() {
    std::printf("\n── 5.3 Rodando ──\n");

    std::printf("        ctest --test-dir build                 # tudo\n");
    std::printf("        ctest --test-dir build --output-on-failure   ← use SEMPRE\n");
    std::printf("        ctest --test-dir build -j8              # em paralelo\n");
    std::printf("        ctest --test-dir build -R Saxpy         # filtro por regex\n");
    std::printf("        ctest --test-dir build -E lento         # excluir\n");
    std::printf("        ctest --test-dir build -L rapido        # por label\n");
    std::printf("        ctest --test-dir build --rerun-failed   # só o que falhou\n");
    std::printf("        ctest --test-dir build --repeat until-fail:100\n");
    std::printf("        ctest --test-dir build -V               # verboso\n\n");

    std::printf("      ⚠ Sem `--output-on-failure`, o CTest esconde a saída do\n");
    std::printf("        teste que falhou e mostra só \"Failed\". É a flag mais\n");
    std::printf("        importante da ferramenta, e não é o padrão.\n\n");

    std::printf("      `--repeat until-fail:100` é o caçador de teste instável,\n");
    std::printf("      o equivalente do `--gtest_repeat` + `--gtest_shuffle`.\n");
    std::printf("      Ver src/testing/demo_organizacao.hpp, 3.3.\n\n");

    std::printf("      Comparado ao Meson (src/meson/), que faz o mesmo:\n");
    std::printf("        meson test -C build --print-errorlogs   (é o PADRÃO lá)\n");
    std::printf("        meson test -C build --repeat 100\n");
    std::printf("        meson test -C build --gdb\n");
    std::printf("      O Meson imprime a saída da falha por padrão; o CTest não.\n");
}

inline void demo_sanitizers() {
    std::printf("\n── 5.4 ⚠ Sanitizers e cobertura ──\n");

    std::printf("      O CMake NÃO tem `-Db_sanitize` como o Meson. É manual:\n\n");
    std::printf("        set(CMAKE_CXX_FLAGS_ASAN\n");
    std::printf("            \"-O1 -g -fsanitize=address,undefined\n");
    std::printf("             -fno-omit-frame-pointer -fno-sanitize-recover=all\")\n");
    std::printf("        cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Asan\n\n");
    std::printf("      Ou, mais simples e por alvo:\n");
    std::printf("        target_compile_options(alvo PRIVATE -fsanitize=address)\n");
    std::printf("        target_link_options(alvo    PRIVATE -fsanitize=address)\n\n");
    std::printf("      ⚠ O flag precisa estar na COMPILAÇÃO **e** na LINKAGEM.\n");
    std::printf("        Esquecer o segundo dá `undefined reference to __asan_*`.\n");
    std::printf("        `target_link_options` existe desde o CMake 3.13; antes\n");
    std::printf("        disso a gambiarra era pôr o flag em CMAKE_EXE_LINKER_FLAGS.\n\n");

    std::printf("      Cobertura:\n");
    std::printf("        target_compile_options(alvo PRIVATE --coverage)\n");
    std::printf("        target_link_options(alvo    PRIVATE --coverage)\n");
    std::printf("        # depois: ctest && lcov --capture ... && genhtml\n\n");
    std::printf("      ⚠ Compile com -O0 para cobertura, ou as linhas do\n");
    std::printf("        relatório não correspondem ao fonte.\n");
    std::printf("        Ver src/testing/demo_cobertura.hpp.\n\n");

    std::printf("      Estado desta configuração: GoogleTest %s.\n",
                config::kTemGTest ? "encontrado — os testes existem"
                                  : "AUSENTE — os testes foram desabilitados");
    std::printf("      Rode:  ctest --test-dir build --output-on-failure\n");
}

inline void run() {
    demo_registro();
    demo_propriedades();
    demo_rodando();
    demo_sanitizers();
}

}  // namespace demo_testes
