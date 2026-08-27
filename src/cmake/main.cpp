// =============================================================================
//  src/cmake/main.cpp
//  Ponto de entrada — guia de CMake
//
//  ⚠ Este subprojeto NÃO é construído pelo Meson da raiz. Ele tem o seu
//    próprio sistema de build DE PROPÓSITO — é o assunto. A mesma escolha de
//    src/modules/, que usa um Makefile próprio.
//
//        cd src/cmake
//        cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
//        cmake --build build -j
//        ctest --test-dir build --output-on-failure
//        ./build/cmake_guia
//
//  O `config.hpp` incluído abaixo é GERADO pelo `configure_file` do
//  CMakeLists.txt. Ele não existe na árvore de fontes — se o seu editor
//  reclamar, aponte-o para o diretório de build (ou use o
//  compile_commands.json, que o CMakeLists também gera).
// =============================================================================
#include "config.hpp"

#include "demo_alvos.hpp"
#include "demo_dependencias.hpp"
#include "demo_geracao.hpp"
#include "demo_paralelo.hpp"
#include "demo_testes.hpp"

#include <cstdio>

namespace {

// Sem o fmt aqui: este subprojeto é sobre o BUILD, e depender de menos coisas
// torna o CMakeLists mais fácil de ler. A moldura é a mesma dos demais.
void section(const char* titulo) {
    std::printf("\n\033[1;36m"
                "╔══════════════════════════════════════════════════════╗\n"
                "║  %-50s  ║\n"
                "╚══════════════════════════════════════════════════════╝"
                "\033[0m\n", titulo);
}

}  // namespace

int main() {
    std::printf("\n%s %s — construído por CMake\n", config::kProjeto, config::kVersao);
    std::printf("%s %s · C++%d · %s · %s\n",
                config::kCompilador, config::kVersaoCompilador,
                config::kPadraoCpp, config::kTipoBuild, config::kGerador);

    section("1 · Alvos e o modelo de escopo");
    demo_alvos::run();

    section("2 · Como o CMake acha as coisas");
    demo_dependencias::run();

    section("3 · Protobuf: código gerado no build");
    demo_geracao::run();

    section("4 · OpenMP, MPI e CUDA");
    demo_paralelo::run();

    section("5 · CTest e testes automatizados");
    demo_testes::run();

    std::printf("\n\033[1;33m→ Os programas de verdade são alvos separados:\n"
                "    ./build/omp_saxpy          (OpenMP, mede)\n"
                "    mpirun -np 4 ./build/mpi_anel\n"
                "    ./build/cuda_saxpy\n"
                "    ctest --test-dir build --output-on-failure\033[0m\n");

    std::printf("\n\033[1;32m✓ Todos os demos de cmake concluídos!\033[0m\n\n");
    return 0;
}
