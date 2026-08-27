# =============================================================================
#  src/cmake/CMakeLists.txt
#
#  ESTE ARQUIVO É O MATERIAL DIDÁTICO.
#  ───────────────────────────────────
#  Assim como src/meson/meson.build faz para o Meson, aqui cada seção exercita
#  um recurso do CMake na ordem em que ele aparece na vida real:
#
#    1. project()          — o cabeçalho obrigatório, e o que ele define
#    2. alvos              — bibliotecas e executáveis, e o modelo de escopo
#    3. dependências       — as três formas de achar coisas
#    4. geração de código  — Protobuf compilado durante o build
#    5. paralelismo        — OpenMP, MPI e CUDA, cada um com a sua detecção
#    6. testes             — CTest + GoogleTest
#    7. instalação         — install(), e o que "instalar" significa
#    8. resumo             — o quadro impresso ao fim da configuração
#
#  ⚠ Este subprojeto NÃO é construído pelo Meson da raiz. Ele tem o seu próprio
#    sistema de build DE PROPÓSITO — é o assunto. É a mesma escolha de
#    src/modules/, que usa um Makefile próprio.
#
#  Uso:
#      cd src/cmake
#      cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#      cmake --build build -j
#      ctest --test-dir build --output-on-failure
#      ./build/cmake_guia
# =============================================================================

# ─────────────────────────────────────────────────────────────────────────────
#  1. cmake_minimum_required e project()
# ─────────────────────────────────────────────────────────────────────────────
#  `cmake_minimum_required` NÃO é só uma checagem de versão: ele liga as
#  POLÍTICAS daquela versão. Pedir 3.5 faz o CMake 3.28 se comportar como 3.5
#  em dezenas de detalhes — e emitir avisos de depreciação.
#  3.20 é um piso razoável hoje: tem `CMAKE_CXX_STANDARD 23`, targets modernos
#  e `cmake --install`.
cmake_minimum_required(VERSION 3.20)

#  `project()` faz mais do que dar um nome. Ele:
#    • define PROJECT_NAME, PROJECT_VERSION, PROJECT_SOURCE_DIR, ...;
#    • DETECTA os compiladores das linguagens listadas;
#    • carrega as variáveis de plataforma (CMAKE_SYSTEM_NAME, ...).
#  ⚠ Nada de `if(CMAKE_CXX_COMPILER_ID ...)` antes desta linha: a variável
#    ainda não existe.
project(academy_cmake
    VERSION      1.0.0
    DESCRIPTION  "Guia de CMake: alvos, dependências, protobuf, OpenMP, MPI, CUDA"
    LANGUAGES    CXX
)

#  O padrão da linguagem. As três linhas andam juntas:
#    STANDARD          qual padrão
#    STANDARD_REQUIRED ON = erro se o compilador não tiver (sem ON, ele
#                      silenciosamente cai para um padrão antigo)
#    EXTENSIONS OFF    usa -std=c++20 em vez de -std=gnu++20 (portabilidade)
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

#  Um buildtype padrão. Sem isto, `cmake -S . -B build` sem argumentos gera um
#  build SEM otimização e SEM -g — nem debug, nem release. É a pegadinha nº 1
#  do CMake para quem vem do Meson, onde o padrão é `debug`.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE não informado — assumindo Release")
endif()

#  Gera compile_commands.json, que clangd, clang-tidy e as IDEs consomem.
#  Custa nada e resolve metade dos problemas de "o editor não acha o header".
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#  Onde os binários saem. Sem isto, cada alvo vai para o diretório do seu
#  CMakeLists, e num projeto com subdiretórios eles ficam espalhados.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})


# ─────────────────────────────────────────────────────────────────────────────
#  2. Alvos, e o modelo de escopo que define o CMake moderno
# ─────────────────────────────────────────────────────────────────────────────
#  "CMake moderno" (3.0+) significa uma coisa: propriedades vivem em ALVOS, não
#  em variáveis globais. O que se abandona:
#
#     include_directories(...)          → target_include_directories(alvo ...)
#     add_definitions(-DX)              → target_compile_definitions(alvo ...)
#     link_libraries(...)               → target_link_libraries(alvo ...)
#     set(CMAKE_CXX_FLAGS "-Wall ...")  → target_compile_options(alvo ...)
#
#  A razão é PROPAGAÇÃO. Cada `target_*` tem três escopos:
#
#     PRIVATE   → só para compilar ESTE alvo
#     INTERFACE → só para quem usa este alvo (nunca para ele)
#     PUBLIC    → os dois
#
#  Quem linka contra um alvo herda automaticamente o INTERFACE dele: includes,
#  defines, flags e dependências transitivas. É o que elimina a repetição.

add_library(nucleo STATIC
    nucleo.cpp
)

#  `.` é PUBLIC porque `nucleo.hpp` aparece na API: quem usa `nucleo` precisa
#  achá-lo. Se fosse um header só interno, seria PRIVATE.
#
#  ⚠ `${CMAKE_CURRENT_SOURCE_DIR}` e não `.`: caminhos relativos em
#    target_include_directories são resolvidos contra o diretório de quem
#    CONSOME o alvo, e isso quebra quando o alvo é exportado.
target_include_directories(nucleo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

#  Avisos como PRIVATE: eles são política de COMO compilar este alvo, e não
#  devem ser impostos a quem apenas o consome.
#  `$<...>` são generator expressions — avaliadas na GERAÇÃO, não na
#  configuração. É como se escreve algo específico de compilador sem `if`.
target_compile_options(nucleo PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

#  Um ALIAS com `::` no nome. Duas razões:
#    • deixa o uso interno idêntico ao uso via find_package();
#    • um nome com `::` que não existe vira ERRO DE CONFIGURAÇÃO, enquanto um
#      nome simples inexistente vira `-lnome_que_nao_existe` e só falha no
#      linker, com uma mensagem pior.
add_library(academy::nucleo ALIAS nucleo)


# ─────────────────────────────────────────────────────────────────────────────
#  3. Dependências — as três formas de achar coisas
# ─────────────────────────────────────────────────────────────────────────────
#  (a) find_package(Nome) — procura um `NomeConfig.cmake` instalado, ou um
#      módulo `FindNome.cmake` que vem com o CMake. É a forma canônica.
#
#  (b) FetchContent — baixa e COMPILA a dependência junto. Bom para
#      cabeçalho-único e para CI reprodutível; ruim para dependências grandes,
#      que passam a ser recompiladas em todo clone limpo.
#
#  (c) um gerenciador de pacotes (Conan, vcpkg) que produz os arquivos que o
#      (a) consome. É o que este repositório usa: `conanfile.py` na raiz.
#
#  `QUIET` + o `_FOUND` correspondente é o idioma para dependência OPCIONAL:
#  sem ele, `find_package` ABORTA a configuração quando não acha. Todo este
#  subprojeto usa esse padrão, porque MPI e CUDA podem não existir na máquina.

find_package(Threads REQUIRED)                 # REQUIRED: aborta se faltar

find_package(Protobuf QUIET)
find_package(GTest    QUIET)
find_package(OpenMP   QUIET COMPONENTS CXX)
find_package(MPI      QUIET COMPONENTS CXX)

#  CUDA não é uma "package": é uma LINGUAGEM. `check_language` pergunta se há
#  compilador para ela sem abortar, e `enable_language` a habilita depois.
#  ⚠ `enable_language(CUDA)` no topo, sem checar, ABORTA a configuração inteira
#    em qualquer máquina sem nvcc. É o erro que faz um projeto não configurar
#    na máquina de quem não tem GPU.
include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD          17)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    #  Sem isto o nvcc compila para a arquitetura padrão dele, que pode não ser
    #  a da GPU presente. `native` (CMake 3.24+) detecta a GPU local.
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
        set(CMAKE_CUDA_ARCHITECTURES native)
    endif()
    set(TEM_CUDA TRUE)
else()
    set(TEM_CUDA FALSE)
endif()

set(TEM_PROTOBUF ${Protobuf_FOUND})
set(TEM_GTEST    ${GTest_FOUND})
set(TEM_OPENMP   ${OpenMP_CXX_FOUND})
set(TEM_MPI      ${MPI_CXX_FOUND})


# ─────────────────────────────────────────────────────────────────────────────
#  4. Geração de código: Protobuf compilado DURANTE o build
# ─────────────────────────────────────────────────────────────────────────────
#  O problema: `medida.proto` não é C++. Ele precisa virar `medida.pb.cc` e
#  `medida.pb.h` ANTES de o compilador rodar — e precisa rodar DE NOVO se o
#  .proto mudar, e SÓ se ele mudar.
#
#  `add_custom_command(OUTPUT ...)` é a ferramenta certa: ela declara uma
#  REGRA (estas saídas vêm destas entradas, por este comando). O ninja/make
#  então trata os .pb.cc como qualquer outro arquivo com dependência.
#
#  ⚠ A confusão clássica: `add_custom_command(TARGET ... POST_BUILD)` é OUTRA
#    coisa — roda depois de um alvo, sempre, sem rastrear dependência. Para
#    gerar código, use a forma com OUTPUT.

if(TEM_PROTOBUF)
    set(PROTO_GERADO_DIR ${CMAKE_CURRENT_BINARY_DIR}/gerado)
    file(MAKE_DIRECTORY ${PROTO_GERADO_DIR})

    set(PROTO_FONTE ${CMAKE_CURRENT_SOURCE_DIR}/medida.proto)
    set(PROTO_CC    ${PROTO_GERADO_DIR}/medida.pb.cc)
    set(PROTO_H     ${PROTO_GERADO_DIR}/medida.pb.h)

    #  `protobuf::protoc` é um alvo IMPORTADO EXECUTÁVEL que o
    #  ProtobufConfig.cmake define. Usá-lo em vez de um `protoc` do PATH
    #  garante que o compilador e a biblioteca são da MESMA versão — a
    #  incompatibilidade entre eles é o erro mais comum com Protobuf.
    if(TARGET protobuf::protoc)
        set(PROTOC_EXE protobuf::protoc)
    else()
        set(PROTOC_EXE ${Protobuf_PROTOC_EXECUTABLE})
    endif()

    add_custom_command(
        OUTPUT  ${PROTO_CC} ${PROTO_H}
        COMMAND ${PROTOC_EXE}
                --cpp_out=${PROTO_GERADO_DIR}
                -I${CMAKE_CURRENT_SOURCE_DIR}
                ${PROTO_FONTE}
        DEPENDS ${PROTO_FONTE}                # ← a regra: mudou o .proto, regera
        COMMENT "protoc: medida.proto → medida.pb.{cc,h}"
        VERBATIM                              # ← escapa caminhos com espaço
    )

    add_library(medida_proto STATIC ${PROTO_CC})

    #  O .pb.h está no diretório de BUILD, não no de fonte. PUBLIC porque quem
    #  usa `medida_proto` precisa incluir `medida.pb.h`.
    target_include_directories(medida_proto PUBLIC ${PROTO_GERADO_DIR})
    target_link_libraries(medida_proto PUBLIC protobuf::libprotobuf)

    #  ⚠ Código GERADO não é código seu: ele dispara avisos que você não pode
    #    corrigir. Suprimir localmente é correto aqui — e é o mesmo raciocínio
    #    dos #pragma locais em src/layout/ e src/linkage/.
    target_compile_options(medida_proto PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-w>
    )
endif()


# ─────────────────────────────────────────────────────────────────────────────
#  5. Paralelismo: OpenMP, MPI e CUDA
# ─────────────────────────────────────────────────────────────────────────────
#  Os três seguem a MESMA forma no CMake moderno: um alvo importado que carrega
#  flags, includes e bibliotecas. Nada de `set(CMAKE_CXX_FLAGS "-fopenmp ...")`.

# ── OpenMP ───────────────────────────────────────────────────────────────────
#  `OpenMP::OpenMP_CXX` traz `-fopenmp` (que afeta compilação E linkagem) e a
#  libgomp. No Clang do macOS ele traz `-Xpreprocessor -fopenmp` e a libomp,
#  que são flags completamente diferentes — é exatamente por isso que o alvo
#  importado existe.
if(TEM_OPENMP)
    add_executable(omp_saxpy omp_saxpy.cpp)
    target_link_libraries(omp_saxpy PRIVATE academy::nucleo OpenMP::OpenMP_CXX)
endif()

# ── MPI ──────────────────────────────────────────────────────────────────────
#  `MPI::MPI_CXX` substitui o wrapper `mpicxx`: o CMake extrai dele os flags e
#  bibliotecas e os aplica ao alvo. Isso permite compilar com o compilador
#  normal e continuar linkando com o MPI certo.
#  ⚠ O binário resultante roda com `mpirun -np N ./mpi_anel`, nunca direto.
if(TEM_MPI)
    add_executable(mpi_anel mpi_anel.cpp)
    target_link_libraries(mpi_anel PRIVATE academy::nucleo MPI::MPI_CXX)
endif()

# ── CUDA ─────────────────────────────────────────────────────────────────────
#  Um `.cu` na lista de fontes é suficiente: com a linguagem CUDA habilitada
#  (seção 3), o CMake chama o `nvcc` para ele e o `g++` para o resto, e linka
#  os dois. Não é preciso `cuda_add_executable` — isso é da era pré-3.8.
if(TEM_CUDA)
    add_executable(cuda_saxpy cuda_saxpy.cu)
    target_link_libraries(cuda_saxpy PRIVATE academy::nucleo)

    #  Separable compilation permite chamar `__device__` de outra TU. Custa
    #  tempo de link; só ligue se precisar.
    set_target_properties(cuda_saxpy PROPERTIES
        CUDA_SEPARABLE_COMPILATION OFF
    )
endif()


# ─────────────────────────────────────────────────────────────────────────────
#  6. configure_file — levar valores do build para dentro do C++
# ─────────────────────────────────────────────────────────────────────────────
#  Substitui @NOME@ pelo valor da variável CMake de mesmo nome, e resolve os
#  `#cmakedefine01`. Roda na CONFIGURAÇÃO, e o arquivo gerado vai para o
#  diretório de build.
#
#  `@ONLY` limita a substituição a @VAR@, deixando ${VAR} em paz — importante
#  quando o molde é um script de shell ou um arquivo que usa ${} para si.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.hpp
    @ONLY
)


# ─────────────────────────────────────────────────────────────────────────────
#  7. O executável principal — a narrativa
# ─────────────────────────────────────────────────────────────────────────────
add_executable(cmake_guia main.cpp)

target_link_libraries(cmake_guia PRIVATE academy::nucleo Threads::Threads)

#  O `config.hpp` gerado mora no diretório de BUILD.
target_include_directories(cmake_guia PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(TEM_PROTOBUF)
    target_link_libraries(cmake_guia PRIVATE medida_proto)
endif()
if(TEM_OPENMP)
    target_link_libraries(cmake_guia PRIVATE OpenMP::OpenMP_CXX)
endif()

target_compile_options(cmake_guia PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)


# ─────────────────────────────────────────────────────────────────────────────
#  8. Testes — CTest + GoogleTest
# ─────────────────────────────────────────────────────────────────────────────
#  `enable_testing()` precisa vir ANTES de qualquer `add_test`, e no
#  CMakeLists de mais alto nível que tenha testes — senão o CTest não os
#  enxerga.
include(CTest)
enable_testing()

if(TEM_GTEST)
    add_executable(test_nucleo test_nucleo.cpp)
    target_link_libraries(test_nucleo PRIVATE
        academy::nucleo
        GTest::gtest
        GTest::gtest_main            # ← o main() pronto; ver src/testing/ 3.4
    )

    #  Duas formas de registrar:
    #
    #    add_test(NAME x COMMAND test_nucleo)
    #      → UM teste no CTest. Se um caso falhar, o CTest diz "test_nucleo
    #        falhou" e nada mais.
    #
    #    gtest_discover_tests(test_nucleo)
    #      → RODA o binário na hora do build com --gtest_list_tests e registra
    #        CADA CASO separadamente. `ctest -R Saxpy` passa a funcionar, e a
    #        saída diz exatamente qual caso falhou.
    #
    #  ⚠ `gtest_discover_tests` executa o binário durante o BUILD, o que
    #    quebra em cross-compilation. Nesse caso use `gtest_add_tests` (que lê
    #    o FONTE, sem executar) ou o `add_test` simples.
    include(GoogleTest)
    gtest_discover_tests(test_nucleo)
else()
    message(WARNING "GTest não encontrado — os testes foram desabilitados.")
endif()

#  Os executáveis de demonstração também viram testes: rodá-los no CTest é uma
#  checagem de fumaça barata (não travou, não abortou, saiu com 0).
if(TEM_OPENMP)
    add_test(NAME omp_saxpy_roda COMMAND omp_saxpy 100000)
    set_tests_properties(omp_saxpy_roda PROPERTIES TIMEOUT 120)
endif()

add_test(NAME cmake_guia_roda COMMAND cmake_guia)
set_tests_properties(cmake_guia_roda PROPERTIES TIMEOUT 120)


# ─────────────────────────────────────────────────────────────────────────────
#  9. Instalação
# ─────────────────────────────────────────────────────────────────────────────
#  `install()` descreve o que `cmake --install build` copia. O prefixo padrão
#  é /usr/local; aqui apontamos para o dist/ do repositório.
#  ⚠ `CMAKE_INSTALL_PREFIX` só pode ser mudado na CONFIGURAÇÃO, não depois.
include(GNUInstallDirs)

install(TARGETS cmake_guia
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(TEM_OPENMP)
    install(TARGETS omp_saxpy RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TEM_MPI)
    install(TARGETS mpi_anel  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TEM_CUDA)
    install(TARGETS cuda_saxpy RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()


# ─────────────────────────────────────────────────────────────────────────────
#  10. O resumo — o quadro impresso ao fim da configuração
# ─────────────────────────────────────────────────────────────────────────────
#  O Meson imprime isto sozinho com `summary()`. No CMake é manual, e vale a
#  pena: sem ele, "por que o alvo CUDA não existe?" vira meia hora de
#  investigação.
message(STATUS "")
#  ⚠ Nada de moldura com largura fixa aqui: `${PROJECT_VERSION}` tem largura
#    variável, e o CMake não formata — a borda direita ficaria torta na
#    primeira mudança de versão. Uma régua simples não tem esse problema.
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS " ${PROJECT_NAME} ${PROJECT_VERSION} — resumo da configuração")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "  CMake            : ${CMAKE_VERSION}")
message(STATUS "  Gerador          : ${CMAKE_GENERATOR}")
message(STATUS "  Compilador C++   : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Padrão C++       : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type       : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Sistema          : ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  ---------------------------------------------------------")
message(STATUS "  Protobuf         : ${TEM_PROTOBUF}")
message(STATUS "  GoogleTest       : ${TEM_GTEST}")
message(STATUS "  OpenMP           : ${TEM_OPENMP}")
message(STATUS "  MPI              : ${TEM_MPI}")
message(STATUS "  CUDA             : ${TEM_CUDA}")
message(STATUS "  ---------------------------------------------------------")
if(NOT TEM_MPI)
    message(STATUS "  ↳ para MPI  : sudo apt install libopenmpi-dev")
endif()
if(NOT TEM_CUDA)
    message(STATUS "  ↳ para CUDA : sudo apt install nvidia-cuda-toolkit")
endif()
message(STATUS "")
