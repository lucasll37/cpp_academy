// =============================================================================
//  src/cmake/demo_alvos.hpp
//
//  O modelo de alvos — o que separa CMake moderno de CMake antigo
//  ──────────────────────────────────────────────────────────────
//  "CMake moderno" (3.0+) significa uma coisa só: propriedades vivem em
//  ALVOS, não em variáveis globais. Todo o resto decorre daí.
//
//  Tópicos:
//  • O antes e o depois
//  • PRIVATE / PUBLIC / INTERFACE — o que cada um propaga
//  • Generator expressions
//  • ⚠ As armadilhas que sobraram do CMake antigo
// =============================================================================
#pragma once
#include "config.hpp"

#include <cstdio>

namespace demo_alvos {

inline void demo_antes_depois() {
    std::printf("\n── 1.1 O antes e o depois ──\n");

    std::printf("      CMake ANTIGO — variáveis GLOBAIS, que vazam para tudo:\n");
    std::printf("        include_directories(include/)\n");
    std::printf("        add_definitions(-DUSA_X)\n");
    std::printf("        set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -Wall\")\n");
    std::printf("        link_libraries(pthread)\n");
    std::printf("        add_executable(a a.cpp)\n");
    std::printf("        add_executable(b b.cpp)   ← herdou TUDO acima\n\n");

    std::printf("      CMake MODERNO — tudo preso ao alvo:\n");
    std::printf("        add_library(nucleo STATIC nucleo.cpp)\n");
    std::printf("        target_include_directories(nucleo PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})\n");
    std::printf("        target_compile_options(nucleo    PRIVATE -Wall -Wextra)\n");
    std::printf("        target_link_libraries(nucleo     PUBLIC  Threads::Threads)\n\n");

    std::printf("      A diferença prática: no primeiro modelo, acrescentar um\n");
    std::printf("      `-D` para UM alvo obriga a mexer numa variável que\n");
    std::printf("      TODOS os alvos leem. Num projeto grande isso vira uma\n");
    std::printf("      teia impossível de auditar — e a raiz de metade das\n");
    std::printf("      violações de ODR entre alvos (src/linkage/ 2.4).\n");
}

inline void demo_escopos() {
    std::printf("\n── 1.2 PRIVATE / PUBLIC / INTERFACE ──\n");

    std::printf("      | escopo    | vale para o alvo | vale para quem o usa |\n");
    std::printf("      |-----------|------------------|----------------------|\n");
    std::printf("      | PRIVATE   |       SIM        |         não          |\n");
    std::printf("      | INTERFACE |       não        |         SIM          |\n");
    std::printf("      | PUBLIC    |       SIM        |         SIM          |\n\n");

    std::printf("      Como decidir, em uma pergunta: **isto aparece no meu\n");
    std::printf("      cabeçalho público?**\n\n");
    std::printf("        o header público faz #include <x.hpp>  → PUBLIC\n");
    std::printf("        só o .cpp usa                          → PRIVATE\n");
    std::printf("        biblioteca só de cabeçalho (INTERFACE) → INTERFACE\n\n");

    std::printf("      Errar para PUBLIC \"por segurança\" é o erro mais comum, e\n");
    std::printf("      ele custa: o projeto inteiro passa a recompilar quando\n");
    std::printf("      qualquer detalhe interno muda, e dependências privadas\n");
    std::printf("      viram parte da sua API sem ninguém decidir isso.\n\n");

    std::printf("      Uma biblioteca INTERFACE — só de cabeçalho, sem fonte:\n");
    std::printf("        add_library(minha_hdr INTERFACE)\n");
    std::printf("        target_include_directories(minha_hdr INTERFACE include/)\n");
    std::printf("        target_compile_features(minha_hdr    INTERFACE cxx_std_20)\n");
    std::printf("      Ela não produz .a nem .so; existe só para PROPAGAR\n");
    std::printf("      propriedades a quem a linka.\n");
}

inline void demo_generator() {
    std::printf("\n── 1.3 Generator expressions ──\n");

    std::printf("      `$<...>` é avaliado na GERAÇÃO, não na configuração. É a\n");
    std::printf("      forma de escrever algo condicional sem `if()`:\n\n");
    std::printf("        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra>\n");
    std::printf("        $<$<CONFIG:Debug>:-DDEPURACAO=1>\n");
    std::printf("        $<$<PLATFORM_ID:Linux>:dl>\n");
    std::printf("        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>\n");
    std::printf("        $<INSTALL_INTERFACE:include>\n\n");

    std::printf("      Por que não `if()`? Porque com geradores multi-config\n");
    std::printf("      (Visual Studio, Xcode) o buildtype só é escolhido na hora\n");
    std::printf("      de COMPILAR, e nesse momento o `if()` já rodou há muito.\n");
    std::printf("      `$<CONFIG:Debug>` sobrevive até lá.\n\n");

    std::printf("      O par BUILD_INTERFACE/INSTALL_INTERFACE resolve um\n");
    std::printf("      problema real: o header está em `src/` no build e em\n");
    std::printf("      `include/` depois de instalado. O mesmo alvo precisa\n");
    std::printf("      dizer as duas coisas.\n");
}

inline void demo_armadilhas() {
    std::printf("\n── 1.4 ⚠ As armadilhas ──\n");

    std::printf("      (a) `file(GLOB fontes \"*.cpp\")` — o CMake avalia isso na\n");
    std::printf("          CONFIGURAÇÃO. Um arquivo novo NÃO é notado até você\n");
    std::printf("          reconfigurar à mão, e o build \"funciona\" sem ele.\n");
    std::printf("          `CONFIGURE_DEPENDS` (3.12+) melhora, mas custa uma\n");
    std::printf("          varredura a cada build e ainda tem casos de borda.\n");
    std::printf("          LISTE OS ARQUIVOS. É chato e é correto.\n\n");

    std::printf("      (b) Alvos sem `::`. `target_link_libraries(a foo)` com\n");
    std::printf("          `foo` inexistente vira `-lfoo` e falha no LINKER, com\n");
    std::printf("          mensagem ruim. `Foo::foo` inexistente falha na\n");
    std::printf("          CONFIGURAÇÃO, dizendo o nome.\n\n");

    std::printf("      (c) `target_link_libraries(alvo x)` sem escopo. A forma\n");
    std::printf("          antiga ainda compila e trata como PUBLIC-ish. Sempre\n");
    std::printf("          diga PRIVATE, PUBLIC ou INTERFACE.\n\n");

    std::printf("      (d) Mexer em `CMAKE_CXX_FLAGS` numa biblioteca que outros\n");
    std::printf("          projetos incluem via add_subdirectory. Você acabou de\n");
    std::printf("          mudar os flags do projeto INTEIRO de outra pessoa.\n\n");

    std::printf("      (e) Sem `CMAKE_BUILD_TYPE`, o build sai SEM otimização e\n");
    std::printf("          SEM -g. Nem debug, nem release. É a pegadinha nº 1\n");
    std::printf("          para quem vem do Meson, cujo padrão é `debug`.\n");
    std::printf("          Este CMakeLists assume Release quando não informado.\n");
    std::printf("          (build atual: %s)\n", config::kTipoBuild);
}

inline void run() {
    demo_antes_depois();
    demo_escopos();
    demo_generator();
    demo_armadilhas();
}

}  // namespace demo_alvos
