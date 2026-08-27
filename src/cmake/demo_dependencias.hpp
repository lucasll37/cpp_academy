// =============================================================================
//  src/cmake/demo_dependencias.hpp
//
//  Como o CMake acha as coisas
//  ───────────────────────────
//  Três mecanismos, e escolher o certo evita a maior parte do sofrimento.
//
//  Tópicos:
//  • find_package: os dois modos
//  • FetchContent vs find_package
//  • Conan e vcpkg — o que eles realmente fazem
//  • ⚠ O que este subprojeto achou, e o que não achou
// =============================================================================
#pragma once
#include "config.hpp"

#include <cstdio>

namespace demo_dependencias {

inline void demo_find_package() {
    std::printf("\n── 2.1 find_package: os dois modos ──\n");

    std::printf("      MODO CONFIG (o preferido) — procura um arquivo\n");
    std::printf("      `<Nome>Config.cmake` que a PRÓPRIA biblioteca instalou:\n");
    std::printf("        find_package(fmt CONFIG REQUIRED)\n");
    std::printf("        target_link_libraries(a PRIVATE fmt::fmt)\n");
    std::printf("      A biblioteca descreve a si mesma: versão, alvos,\n");
    std::printf("      dependências transitivas. É preciso e é responsabilidade\n");
    std::printf("      de quem publica.\n\n");

    std::printf("      MODO MODULE (o legado) — procura um `Find<Nome>.cmake`,\n");
    std::printf("      um script que o CMake (ou você) escreve para caçar a\n");
    std::printf("      biblioteca com `find_library` e `find_path`:\n");
    std::printf("        find_package(Threads REQUIRED)     ← FindThreads.cmake\n");
    std::printf("        find_package(MPI COMPONENTS CXX)   ← FindMPI.cmake\n");
    std::printf("      Necessário para bibliotecas antigas que não exportam\n");
    std::printf("      config. O CMake traz dezenas desses módulos prontos.\n\n");

    std::printf("      O idioma OPCIONAL, usado em todo este subprojeto:\n");
    std::printf("        find_package(MPI QUIET COMPONENTS CXX)\n");
    std::printf("        if(MPI_CXX_FOUND)\n");
    std::printf("            add_executable(mpi_anel mpi_anel.cpp)\n");
    std::printf("            target_link_libraries(mpi_anel PRIVATE MPI::MPI_CXX)\n");
    std::printf("        endif()\n\n");
    std::printf("      ⚠ Sem `QUIET`, e sem o `if`, `find_package` ABORTA a\n");
    std::printf("        configuração inteira quando não acha. Um projeto assim\n");
    std::printf("        simplesmente não configura na máquina de quem não tem\n");
    std::printf("        GPU — e isso é um bug de build, não uma limitação.\n\n");

    std::printf("      Onde ele procura, em ordem: <Nome>_ROOT, CMAKE_PREFIX_PATH,\n");
    std::printf("      variáveis de ambiente, caminhos de sistema. Quando não\n");
    std::printf("      acha o que você sabe que está lá:\n");
    std::printf("        cmake --debug-find -S . -B build\n");
}

inline void demo_fetchcontent() {
    std::printf("\n── 2.2 FetchContent ──\n");

    std::printf("        include(FetchContent)\n");
    std::printf("        FetchContent_Declare(googletest\n");
    std::printf("            GIT_REPOSITORY https://github.com/google/googletest\n");
    std::printf("            GIT_TAG        v1.14.0)      ← TAG FIXA, nunca `main`\n");
    std::printf("        FetchContent_MakeAvailable(googletest)\n\n");

    std::printf("      Ele BAIXA e COMPILA a dependência como parte do seu build.\n\n");
    std::printf("      | aspecto              | FetchContent | find_package |\n");
    std::printf("      |----------------------|--------------|--------------|\n");
    std::printf("      | funciona sem instalar|     SIM      |     não      |\n");
    std::printf("      | build reprodutível   |     SIM      | depende      |\n");
    std::printf("      | precisa de rede      |     SIM      |     não      |\n");
    std::printf("      | tempo do 1º build    |    ALTO      |    baixo     |\n");
    std::printf("      | mesmos flags que você|     SIM      |     não      |\n\n");

    std::printf("      \"Mesmos flags\" corta os dois lados: garante compatibilidade\n");
    std::printf("      de ABI (src/linkage/ 5.1) e triplica o tempo de build.\n\n");

    std::printf("      ⚠ `GIT_TAG main` faz o seu build depender do que outra\n");
    std::printf("        pessoa commitou hoje. Use tag ou hash — sempre.\n");
    std::printf("      ⚠ Prefira `find_package` PRIMEIRO e caia para FetchContent\n");
    std::printf("        só se não achar. O CMake 3.24 tem isso pronto:\n");
    std::printf("          FetchContent_Declare(fmt ... FIND_PACKAGE_ARGS)\n");
}

inline void demo_gerenciadores() {
    std::printf("\n── 2.3 Conan e vcpkg ──\n");

    std::printf("      Os dois fazem a MESMA coisa, no fim: baixam/compilam a\n");
    std::printf("      dependência e GERAM os arquivos que `find_package`\n");
    std::printf("      consome. Eles não substituem o CMake — alimentam ele.\n\n");

    std::printf("      Conan (o que este repositório usa, ver conanfile.py):\n");
    std::printf("        conan install . --build=missing\n");
    std::printf("        cmake -S . -B build \\\n");
    std::printf("              -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake\n");
    std::printf("      O toolchain file preenche CMAKE_PREFIX_PATH, e a partir\n");
    std::printf("      daí `find_package(protobuf)` simplesmente acha.\n\n");

    std::printf("      vcpkg:\n");
    std::printf("        cmake -S . -B build \\\n");
    std::printf("              -DCMAKE_TOOLCHAIN_FILE=$VCPKG/scripts/buildsystems/vcpkg.cmake\n\n");

    std::printf("      ⚠ Só UM toolchain file por configuração. Combinar Conan e\n");
    std::printf("        vcpkg no mesmo projeto exige trabalho manual.\n");
    std::printf("      ⚠ O toolchain file precisa ser passado na PRIMEIRA\n");
    std::printf("        configuração. Depois de o cache existir, mudá-lo não\n");
    std::printf("        tem efeito — apague o diretório de build.\n");
}

inline void demo_encontrado() {
    std::printf("\n── 2.4 ⚠ O que ESTA configuração encontrou ──\n");

    auto linha = [](const char* nome, bool achou, const char* nota) {
        std::printf("        %-12s %-12s %s\n", nome,
                    achou ? "ENCONTRADO" : "ausente", nota);
    };

    std::printf("      Valores vindos do `configure_file` (config.hpp gerado):\n\n");
    linha("Protobuf",   config::kTemProtobuf, "");
    linha("GoogleTest", config::kTemGTest,    "");
    linha("OpenMP",     config::kTemOpenMP,   "");
    linha("MPI",        config::kTemMPI,
          config::kTemMPI ? "" : "→ sudo apt install libopenmpi-dev");
    linha("CUDA",       config::kTemCUDA,
          config::kTemCUDA ? "" : "→ sudo apt install nvidia-cuda-toolkit");

    std::printf("\n      Estes valores vieram de `#cmakedefine01`, não de\n");
    std::printf("      `#ifdef`. A diferença importa: `#cmakedefine01` sempre\n");
    std::printf("      define o símbolo (como 0 ou 1), então dá para escrever\n");
    std::printf("        if constexpr (config::kTemCUDA) { ... }\n");
    std::printf("      e o compilador ainda CHECA A SINTAXE do ramo desligado.\n");
    std::printf("      Com `#ifdef`, o código morto nem é analisado, e apodrece.\n\n");

    std::printf("      Compilado por %s %s, padrão C++%d, build %s, gerador %s.\n",
                config::kCompilador, config::kVersaoCompilador,
                config::kPadraoCpp, config::kTipoBuild, config::kGerador);
}

inline void run() {
    demo_find_package();
    demo_fetchcontent();
    demo_gerenciadores();
    demo_encontrado();
}

}  // namespace demo_dependencias
