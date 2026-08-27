# cmake — CMake, CTest, Protobuf, OpenMP, MPI e CUDA

## O que é este projeto?

O par de [`src/meson/`](../meson/): ali o `meson.build` **é** o material didático; aqui o
`CMakeLists.txt` é. Ele exercita, na ordem em que aparecem na vida real:

| seção | assunto |
|---|---|
| 1 | `project()`, padrão da linguagem, buildtype |
| 2 | **alvos** e o modelo de escopo `PRIVATE`/`PUBLIC`/`INTERFACE` |
| 3 | **dependências** — `find_package`, `FetchContent`, Conan/vcpkg |
| 4 | **Protobuf** compilado durante o build (`add_custom_command`) |
| 5 | **OpenMP, MPI e CUDA** — cada um com a sua detecção |
| 6 | `configure_file` |
| 8 | **CTest + GoogleTest** |
| 9 | `install()` |
| 10 | o resumo impresso ao fim da configuração |

> ⚠ Este subprojeto **não é construído pelo Meson da raiz**. Ele tem o próprio sistema de build
> **de propósito** — é o assunto. É a mesma escolha de [`src/modules/`](../modules/), que usa um
> Makefile próprio. O diretório `build/` daqui já é ignorado pelo `build/` da linha 2 do
> `.gitignore` (confirmado com `git check-ignore -v src/cmake/build`).

## Antes de começar: o que é um build system

### O problema que ele resolve

Compilar um arquivo é fácil: `g++ main.cpp -o prog`. O problema aparece quando há
57 subprojetos, oito bibliotecas externas, dois compiladores possíveis e três
sistemas operacionais. Aí você precisa responder, para cada arquivo:

- quais flags? (`-std=c++20`, `-O2`, `-Wall`, e mais quinze)
- quais diretórios de include?
- contra o que linkar, e em que ordem?
- **o que precisa ser recompilado** quando eu mudo um `.hpp`?

Um build system é a ferramenta que responde isso a partir de uma **descrição**.

### As duas etapas, que não são a mesma coisa

Esta distinção é a que mais confunde quem chega ao CMake:

```
  CONFIGURAÇÃO                        BUILD
  cmake -S . -B build                 cmake --build build
  ───────────────────                 ──────────────────
  roda o CMakeLists.txt               roda o make/ninja gerado
  descobre o compilador               compila os .cpp
  acha as dependências                linka
  GERA os arquivos de build           produz os binários
  (Makefile ou build.ninja)
  acontece UMA vez                    acontece a cada mudança
```

O `CMakeLists.txt` é uma **receita lida na configuração**, não um script
executado a cada build. É por isso que `file(GLOB *.cpp)` não vê arquivos novos:
ele rodou na configuração e não roda de novo.

### O que é um alvo

Um **alvo** (*target*) é uma coisa que o build produz: um executável, uma
biblioteca, ou uma regra. No CMake moderno, o alvo é a unidade que carrega
**tudo** — flags, includes, defines, dependências:

```cmake
add_library(nucleo STATIC nucleo.cpp)             # cria o alvo
target_include_directories(nucleo PUBLIC .)       # tudo se prende a ele
target_link_libraries(nucleo PRIVATE Threads::Threads)
```

Quem linka contra `nucleo` **herda** o que for `PUBLIC` — includes, defines e
dependências transitivas. É o mecanismo central do CMake moderno, e a seção 1 o
detalha.

### O que é um gerador

O CMake não compila nada. Ele **gera** arquivos para outra ferramenta compilar:
`Unix Makefiles`, `Ninja`, `Visual Studio 17 2022`, `Xcode`. O mesmo
`CMakeLists.txt` produz projeto de Visual Studio no Windows e `build.ninja` no
Linux — e é essa indireção que dá ao CMake a portabilidade que ele tem.

## Como rodar

```bash
cd src/cmake

# O CMAKE_PREFIX_PATH aponta para os pacotes que o Conan já baixou na raiz.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$(sed -n 's|^prefix=||p' ../../build/gtest.pc);$(sed -n 's|^prefix=||p' ../../build/protobuf.pc)"

cmake --build build -j
ctest --test-dir build --output-on-failure
./build/cmake_guia        # a narrativa
./build/omp_saxpy         # o benchmark de OpenMP
```

## O que foi verificado nesta máquina

Esta é a parte que importa, e ela é honesta:

| | estado | observação |
|---|---|---|
| CMake 3.24 + Unix Makefiles | ✅ verificado | configura, compila, instala |
| GoogleTest via CTest | ✅ verificado | **11/11 testes passando** |
| Protobuf | ✅ verificado | `protoc` roda no build; serialização testada |
| OpenMP | ✅ verificado | **medido**: speedup de 3.09× a 1.20× |
| **MPI** | ⚠ **não compilado** | não há MPI nesta máquina |
| **CUDA** | ⚠ **não compilado** | há GPU e driver, mas **não há `nvcc`** |

`mpi_anel.cpp` e `cuda_saxpy.cu` estão **completos e comentados**, e o `CMakeLists.txt` os
detecta e constrói quando as ferramentas existem. Mas eles **não foram compilados nem
executados aqui** — trate-os como referência, não como resultado verificado. Para habilitar:

```bash
sudo apt install libopenmpi-dev        # → o alvo mpi_anel aparece
sudo apt install nvidia-cuda-toolkit   # → o alvo cuda_saxpy aparece
```

O resumo impresso pelo `cmake` diz exatamente isso, em toda configuração:

```
--   Protobuf         : TRUE
--   GoogleTest       : TRUE
--   OpenMP           : TRUE
--   MPI              : FALSE
--   CUDA             : FALSE
--   ↳ para MPI  : sudo apt install libopenmpi-dev
--   ↳ para CUDA : sudo apt install nvidia-cuda-toolkit
```

## Estrutura de arquivos

```
src/cmake/
├── CMakeLists.txt        ← O MATERIAL DIDÁTICO CENTRAL (419 linhas comentadas)
├── main.cpp              ← a narrativa
├── demo_alvos.hpp        ← 1 · alvos, escopos, generator expressions
├── demo_dependencias.hpp ← 2 · find_package, FetchContent, Conan
├── demo_geracao.hpp      ← 3 · Protobuf gerado no build
├── demo_paralelo.hpp     ← 4 · OpenMP / MPI / CUDA lado a lado
├── demo_testes.hpp       ← 5 · CTest
│
├── nucleo.hpp / .cpp     ← a biblioteca: SAXPY, a MESMA conta nos 4 caminhos
├── medida.proto          ← o schema que o protoc compila no build
├── cmake/config.hpp.in   ← o molde do configure_file
├── omp_saxpy.cpp         ← ✅ OpenMP, com benchmark medido
├── mpi_anel.cpp          ← ⚠ MPI, não compilado aqui
├── cuda_saxpy.cu         ← ⚠ CUDA, não compilado aqui
├── test_nucleo.cpp       ← 9 casos gtest, via CTest
└── README.md
```

**Uma operação em quatro tecnologias.** `nucleo::saxpy` (`y = a*x + y`, o BLAS nível 1) é
calculada em serial, com OpenMP, com MPI e com CUDA — sempre a mesma conta. É isso que torna a
comparação honesta, e é por isso que cada versão compara o resultado contra a referência serial.

---

## Demo 1: Alvos

"CMake moderno" (3.0+) significa uma coisa: **propriedades vivem em alvos, não em variáveis
globais**. Todo o resto decorre daí.

| CMake antigo | CMake moderno |
|---|---|
| `include_directories(...)` | `target_include_directories(alvo ...)` |
| `add_definitions(-DX)` | `target_compile_definitions(alvo ...)` |
| `set(CMAKE_CXX_FLAGS "-Wall")` | `target_compile_options(alvo ...)` |
| `link_libraries(...)` | `target_link_libraries(alvo ...)` |

No primeiro modelo, acrescentar um `-D` para **um** alvo obriga a mexer numa variável que
**todos** leem. Num projeto grande vira uma teia impossível de auditar — e a raiz de metade das
violações de ODR entre alvos ([`src/linkage/`](../linkage/) 2.4).

### Os três escopos

| escopo | vale para o alvo | vale para quem o usa |
|---|---|---|
| `PRIVATE` | sim | **não** |
| `INTERFACE` | **não** | sim |
| `PUBLIC` | sim | sim |

**Como decidir, em uma pergunta:** *isto aparece no meu cabeçalho público?*

- o header público faz `#include <x.hpp>` → **PUBLIC**
- só o `.cpp` usa → **PRIVATE**
- biblioteca só de cabeçalho → **INTERFACE**

Errar para `PUBLIC` "por segurança" é o erro mais comum, e custa: o projeto inteiro recompila
quando qualquer detalhe interno muda, e dependências privadas viram parte da sua API sem
ninguém decidir isso.

### Generator expressions

```cmake
$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra>
$<$<CONFIG:Debug>:-DDEPURACAO=1>
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
$<INSTALL_INTERFACE:include>
```

São avaliadas na **geração**, não na configuração. Por que não `if()`? Porque com geradores
multi-config (Visual Studio, Xcode) o buildtype só é escolhido na hora de **compilar**, e nesse
momento o `if()` já rodou há muito. `$<CONFIG:Debug>` sobrevive até lá.

### ⚠ As armadilhas

- **`file(GLOB)`** é avaliado na configuração. Um arquivo novo não é notado até você
  reconfigurar à mão, e o build "funciona" sem ele. **Liste os arquivos.**
- **Alvos sem `::`** — `target_link_libraries(a foo)` com `foo` inexistente vira `-lfoo` e falha
  no **linker**. `Foo::foo` inexistente falha na **configuração**, dizendo o nome.
- **`target_link_libraries` sem escopo** — a forma antiga ainda compila. Sempre diga
  `PRIVATE`/`PUBLIC`/`INTERFACE`.
- **Sem `CMAKE_BUILD_TYPE`**, o build sai **sem otimização e sem `-g`**. Nem debug, nem release.
  É a pegadinha nº 1 para quem vem do Meson, cujo padrão é `debug`. Este `CMakeLists` assume
  `Release` quando não informado.

---

## Demo 2: Dependências

### `find_package`: os dois modos

**Modo CONFIG** (preferido) — procura um `<Nome>Config.cmake` que a **própria biblioteca**
instalou. Ela descreve a si mesma: versão, alvos, dependências transitivas.

**Modo MODULE** (legado) — procura um `Find<Nome>.cmake`, script que caça a biblioteca com
`find_library`/`find_path`. Necessário para bibliotecas antigas; o CMake traz dezenas prontos
(`FindThreads`, `FindMPI`, ...).

### O idioma da dependência opcional

```cmake
find_package(MPI QUIET COMPONENTS CXX)
if(MPI_CXX_FOUND)
    add_executable(mpi_anel mpi_anel.cpp)
    target_link_libraries(mpi_anel PRIVATE MPI::MPI_CXX)
endif()
```

> ⚠ **Sem `QUIET` e sem o `if`, `find_package` aborta a configuração inteira quando não acha.**
> Um projeto assim simplesmente não configura na máquina de quem não tem GPU — e isso é um bug
> de build, não uma limitação. Todo este subprojeto usa esse padrão.

Quando ele não acha o que você sabe que está lá: `cmake --debug-find -S . -B build`.

### `FetchContent` vs `find_package`

| aspecto | FetchContent | find_package |
|---|---|---|
| funciona sem instalar | **sim** | não |
| build reprodutível | **sim** | depende |
| precisa de rede | sim | **não** |
| tempo do 1º build | alto | **baixo** |
| mesmos flags que você | sim | não |

"Mesmos flags" corta os dois lados: garante compatibilidade de ABI
([`src/linkage/`](../linkage/) 5.1) e triplica o tempo de build.

> ⚠ `GIT_TAG main` faz o seu build depender do que outra pessoa commitou hoje. **Tag ou hash,
> sempre.** E prefira `find_package` primeiro — o CMake 3.24 tem `FIND_PACKAGE_ARGS` pronto para
> isso.

### Conan e vcpkg

Os dois fazem a mesma coisa no fim: baixam/compilam a dependência e **geram os arquivos que
`find_package` consome**. Eles não substituem o CMake — alimentam ele.

> ⚠ O toolchain file precisa ser passado na **primeira** configuração. Depois de o cache
> existir, mudá-lo não tem efeito — apague o diretório de build.

---

## Demo 3: Protobuf — código gerado no build

O assunto não é o Protobuf: é o padrão **"um arquivo que não é C++ vira C++ antes da
compilação"**. Vale igual para Qt (moc/uic), FlatBuffers, gRPC, bison/flex e geradores caseiros.

```cmake
add_custom_command(
    OUTPUT  ${PROTO_CC} ${PROTO_H}
    COMMAND protobuf::protoc --cpp_out=${PROTO_GERADO_DIR}
            -I${CMAKE_CURRENT_SOURCE_DIR} ${PROTO_FONTE}
    DEPENDS ${PROTO_FONTE}          # ← a dependência
    VERBATIM)
add_library(medida_proto STATIC ${PROTO_CC})
target_include_directories(medida_proto PUBLIC ${PROTO_GERADO_DIR})
```

Isso declara: *"estas saídas vêm destas entradas, por este comando"*. O ninja/make passa a
tratar os `.pb.cc` como qualquer arquivo com dependência — toca no `.proto`, regera; não toca,
não regera. Verificado no build: `[9%] protoc: medida.proto → medida.pb.{cc,h}`.

> ⚠ **A confusão clássica.** Existe uma segunda forma —
> `add_custom_command(TARGET x POST_BUILD ...)` — que é **outra coisa**: roda depois de um alvo,
> **sempre**, sem rastrear dependência. Serve para copiar um arquivo ou assinar um binário.
> Usá-la para gerar código produz um build que regenera tudo toda vez, ou que não regenera
> nunca — dependendo da ordem dos alvos.
>
> ⚠ `VERBATIM` não é opcional: sem ele, um caminho com espaço vira dois argumentos.
>
> ⚠ A saída vai para o diretório de **build**, nunca para o de fonte. Gerar dentro do fonte suja
> o `git status`, quebra builds paralelos e cria conflito entre Debug e Release.

### O Protobuf em si, medido

```
serializado  :  66 bytes
texto (debug): 210 bytes
```

O binário é compacto porque cada campo é identificado por um **número** (o `= 1`, `= 2` do
`.proto`), não pelo nome — e campos com valor padrão nem são escritos. É essa numeração que dá
a compatibilidade: campo novo com número novo não quebra quem lê a versão antiga.

> ⚠ Por isso **reutilizar um número já usado é o pecado capital** do Protobuf: dados antigos
> passam a ser lidos como o campo errado. Use `reserved 3, 7;`.
>
> ⚠ **Versão do `protoc` diferente da `libprotobuf`** dá um erro críptico
> (*"This file was generated by a newer version of protoc..."*). A defesa é usar o alvo
> importado `protobuf::protoc` em vez de um `protoc` do PATH — é o que este `CMakeLists` faz.
>
> ⚠ Código gerado dispara avisos que você não pode corrigir. Suprima **no alvo dele**:
> `target_compile_options(medida_proto PRIVATE -w)`.

---

## Demo 4: OpenMP, MPI e CUDA

| aspecto | OpenMP | MPI | CUDA |
|---|---|---|---|
| unidade | thread | **processo** | thread de GPU |
| quantidade | dezenas | dezenas/milhares | **dezenas de milhares** |
| memória | compartilhada | privada | separada (VRAM) |
| comunicação | variáveis | mensagens | cópias host↔device |
| escala até | 1 máquina | um cluster | 1 GPU (ou N) |
| como se lança | `./prog` | `mpirun -np 4 ./prog` | `./prog` |
| risco típico | data race | **deadlock** | erro silencioso |
| no CMake | `OpenMP::OpenMP_CXX` | `MPI::MPI_CXX` | linguagem `CUDA` + `.cu` |

Eles se **combinam**, e é o padrão em HPC: MPI entre os nós, OpenMP dentro de cada nó, CUDA nas
GPUs de cada nó — o modelo "MPI+X".

### OpenMP — o resultado medido

```
 elementos |       MB |   serial |   OpenMP | speedup
-----------|----------|----------|----------|--------
    250000 |        3 |     0.06 |     0.02 |   2.60x
   2000000 |       23 |     0.52 |     0.17 |   3.09x
  20000000 |      229 |     7.01 |     5.50 |   1.27x
  60000000 |      687 |    22.10 |    18.49 |   1.20x
```

**O speedup cai conforme os dados crescem**, com 24 threads disponíveis o tempo todo. SAXPY faz
1 multiplicação-soma para cada 12 bytes movidos: é limitado por **banda de memória**, não por
CPU. Enquanto cabe no cache, as threads somam a banda dos caches; quando vem da RAM, todas
disputam os **mesmos** canais — e acrescentar thread não acrescenta banda.
Ver [`src/performance/`](../performance/).

> **Nota de método.** A primeira versão deste benchmark reportou speedup de **0.32×** — a
> versão "paralela" saiu 3× mais lenta. Causa: sem aquecimento, a primeira região paralela paga
> a criação do pool de threads. A segunda versão reportou **1.03×** — dessa vez porque o reset
> do vetor (uma cópia de 80 MB) estava **dentro** da região cronometrada e dominava as duas
> medições igualmente. Só a terceira versão, com preparação fora do relógio e mediana de 7
> execuções, produziu a tabela acima. É a disciplina de
> [`src/performance/demo_metodo.hpp`](../performance/demo_metodo.hpp), aplicada na marra.

As quatro diretivas que resolvem 90% dos casos:

```cpp
#pragma omp parallel for                      // o laço
#pragma omp parallel for reduction(+ : soma)  // a redução
#pragma omp critical                          // seção exclusiva
#pragma omp atomic                            // uma operação, mais barato
```

> ⚠ Sem `reduction`, a soma paralela é **data race**: todas as threads escrevendo em `soma`.
> Compila, roda, e dá resultado diferente a cada execução — UB, não "erro de arredondamento".
>
> ⚠ **A armadilha nº 1:** o código compila **sem** `-fopenmp` e roda **serial**, em silêncio. Os
> pragmas viram comentário. Um build mal configurado não falha — só fica lento. Por isso o
> `CMakeLists` imprime se achou OpenMP, e o `omp_saxpy.cpp` checa `#ifdef _OPENMP`.
>
> ⚠ A soma paralela agrupa os termos em outra **ordem**, e adição de ponto flutuante **não é
> associativa**. Um resultado bit-a-bit idêntico ao serial é **coincidência**, não contrato — ele
> muda com o número de threads e com o `schedule`. Testes que exigem igualdade exata falham por
> isso, não por bug.

`schedule`: `static` quando toda iteração custa o mesmo (o padrão, e o certo aqui); `dynamic`
para custo irregular ([`src/mandelbrot_openmp/`](../mandelbrot_openmp/)). ⚠ `dynamic` tem
overhead por fatia — usá-lo num laço uniforme costuma **piorar**.

### MPI

```cpp
MPI_Init(&argc, &argv);                  // SEMPRE primeiro
MPI_Comm_rank(MPI_COMM_WORLD, &rank);    // quem sou (0..N-1)
MPI_Comm_size(MPI_COMM_WORLD, &n);       // quantos somos
...
MPI_Finalize();                          // SEMPRE último
```

Coletivas: `MPI_Bcast`, `MPI_Scatter`, `MPI_Gather`, `MPI_Reduce`, `MPI_Allreduce`,
`MPI_Barrier`.

> ⚠ **Todos** os processos precisam chamar a coletiva, com os mesmos argumentos de tamanho. Um
> `if (rank == 0)` em volta de um `MPI_Bcast` **trava o programa inteiro**.
>
> ⚠ **O deadlock ponto a ponto**, o erro clássico: `MPI_Send` em todos, depois `MPI_Recv` em
> todos. Se a mensagem couber no buffer interno, funciona; se crescer, **todos bloqueiam**. O bug
> aparece só com dados grandes — em produção. Use `MPI_Sendrecv`, ou `MPI_Isend`/`MPI_Irecv` +
> `MPI_Wait`.
>
> ⚠ Rodar `-np 1` e `-np 4` na **mesma** máquina não mede escalabilidade: os processos disputam
> os mesmos núcleos e a mesma banda.

### CUDA

```cpp
__global__ void saxpy(int n, float a, const float* x, float* y) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) y[i] = a*x[i] + y[i];       // ← a GUARDA é obrigatória
}
saxpy<<<blocos, 256>>>(n, 2.5f, x_dev, y_dev);
```

> ⚠ A guarda `if (i < n)` é **obrigatória**: o número de threads é arredondado para cima até um
> múltiplo do tamanho do bloco, então as últimas têm índice além do vetor. Quem acusa é o
> `compute-sanitizer`, não o ASan.
>
> ⚠ Kernels são **assíncronos**. Um erro dentro do kernel só aparece na próxima chamada
> sincronizante — daí o par `cudaGetLastError()` (lançamento) + `cudaDeviceSynchronize()`
> (execução). Ignorar o código de retorno é o erro nº 1 em CUDA: o kernel falha em silêncio e o
> resultado sai zerado.
>
> ⚠ **A mentira comum de benchmark de GPU:** reportar só o tempo do **kernel**. Com as cópias
> host↔device incluídas, o ganho encolhe ou some. SAXPY é o caso clássico em que a GPU **não**
> compensa, porque o gargalo é o PCIe. Ela compensa quando os dados **ficam** nela entre vários
> kernels, ou quando a intensidade aritmética é alta (matmul, convolução, FFT).

No CMake, CUDA é uma **linguagem**, não um pacote:

```cmake
include(CheckLanguage)
check_language(CUDA)                       # pergunta sem abortar
if(CMAKE_CUDA_COMPILER)
    enable_language(CUDA)
    set(CMAKE_CUDA_ARCHITECTURES native)   # detecta a GPU local (3.24+)
    add_executable(cuda_saxpy cuda_saxpy.cu)
endif()
```

> ⚠ `enable_language(CUDA)` direto, sem checar, **aborta a configuração** em qualquer máquina
> sem `nvcc`.

---

## Demo 5: CTest

```cmake
include(CTest)
enable_testing()                    # ANTES de qualquer add_test

add_executable(test_nucleo test_nucleo.cpp)
target_link_libraries(test_nucleo PRIVATE academy::nucleo GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_nucleo)   # registra CADA CASO
```

A diferença entre as duas formas de registrar é grande:

| forma | resultado |
|---|---|
| `add_test(NAME nucleo COMMAND test_nucleo)` | **1** teste no CTest; a falha diz só "nucleo falhou" |
| `gtest_discover_tests(test_nucleo)` | **N** testes; a falha nomeia o caso, e `ctest -R Saxpy` funciona |

Verificado: os 9 casos aparecem individualmente
(`Saxpy.VetorVazioNaoQuebra`, `Gerar.EhDeterministico`, ...), mais 2 testes de fumaça, total
**11/11 passando**.

> ⚠ `gtest_discover_tests` **executa** o binário durante o build — isso quebra em
> cross-compilation. Nesse caso use `gtest_add_tests` (que lê o **fonte**) ou `add_test` simples.

### Propriedades que importam

```cmake
set_tests_properties(meu_teste PROPERTIES
    TIMEOUT     60           # o padrão é 1500 s — um teste travado segura o CI 25 min
    LABELS      "rapido"     # ctest -L rapido
    RUN_SERIAL  TRUE)        # para quem disputa porta, arquivo ou GPU
```

`RUN_SERIAL` merece atenção: sem ele, `ctest -j8` produz falhas intermitentes que **parecem**
flakiness do teste ([`src/testing/`](../testing/) 1.4).

### Rodando

```bash
ctest --test-dir build --output-on-failure   # ← use SEMPRE
ctest --test-dir build -j8
ctest --test-dir build -R Saxpy
ctest --test-dir build --rerun-failed
ctest --test-dir build --repeat until-fail:100   # o caçador de flaky
```

> ⚠ **Sem `--output-on-failure`, o CTest esconde a saída do teste que falhou** e mostra só
> "Failed". É a flag mais importante da ferramenta, e não é o padrão. (O Meson imprime por
> padrão — ver [`src/meson/`](../meson/).)

### ⚠ Sanitizers e cobertura

O CMake **não** tem `-Db_sanitize` como o Meson. É manual, e por alvo é o mais simples:

```cmake
target_compile_options(alvo PRIVATE -fsanitize=address,undefined)
target_link_options(alvo    PRIVATE -fsanitize=address,undefined)
```

> ⚠ O flag precisa estar na **compilação e na linkagem**. Esquecer o segundo dá
> `undefined reference to __asan_*`.

---

## CMake vs Meson, lado a lado

Este repositório tem os dois ([`src/meson/`](../meson/)), e a comparação é justa:

| | CMake | Meson |
|---|---|---|
| linguagem | DSL própria, imperativa, sem tipos | DSL própria, mais restrita |
| curva | íngreme; 20 anos de camadas | suave |
| buildtype padrão | **nenhum** (sem `-O` nem `-g`) | `debug` |
| sanitizers | manual | `-Db_sanitize=address` |
| saída de teste na falha | precisa de `--output-on-failure` | **padrão** |
| resumo de configuração | manual (`message`) | `summary()` embutido |
| adoção | **o padrão de facto** | crescente |
| suporte de IDE/ferramenta | universal | bom |
| CUDA, cross, toolchains | maduro | menos maduro |

A escolha honesta: **Meson é mais agradável; CMake é o que o ecossistema fala.** Se você publica
uma biblioteca C++, exportar um `<Nome>Config.cmake` não é opcional — é o que permite que os
outros a usem.

---

## Como validar

```bash
cd src/cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$(sed -n 's|^prefix=||p' ../../build/gtest.pc);$(sed -n 's|^prefix=||p' ../../build/protobuf.pc)"
cmake --build build -j
ctest --test-dir build --output-on-failure     # 11/11
./build/cmake_guia
./build/omp_saxpy
```

Estado atual: configura e compila sem warnings com `-Wall -Wextra -Wpedantic`; **11/11 testes**
passando; OpenMP e Protobuf verificados; MPI e CUDA detectados como ausentes e pulados, com a
instrução de instalação impressa.

## Referências

- [`src/meson/`](../meson/) — o mesmo assunto, no outro build system
- [`src/testing/`](../testing/) — o GoogleTest em si: asserções, fixtures, gmock, death tests
- [`src/performance/`](../performance/) — por que o speedup do OpenMP satura, e como medir
- [`src/concurrency2/`](../concurrency2/) — o data race que `reduction` evita
- [`src/mandelbrot_openmp/`](../mandelbrot_openmp/) — `schedule(dynamic)` num caso irregular
- [`src/linkage/`](../linkage/) — ABI, e por que "mesmos flags" importa
- *Professional CMake: A Practical Guide*, Craig Scott — a referência
- Daniel Pfeifer, *Effective CMake* (C++Now 2017) — a palestra que definiu "CMake moderno"
- Documentação oficial do OpenMP, do MPI Forum e do CUDA C++ Programming Guide
