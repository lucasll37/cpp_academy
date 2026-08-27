# C++ Academy

Repositório de **subprojetos didáticos** em C++ moderno (C++17/20), cobrindo desde fundamentos da linguagem até integração com bibliotecas reais de indústria. Cada subprojeto é autocontido, compilável individualmente e acompanhado de um README detalhado.

**Sistema de build**: Meson + Ninja  
**Gerenciamento de dependências**: Conan 2  
**Padrão de linguagem**: C++20 (subprojetos pontuais em C++23 — ver [src/ROADMAP.md](src/ROADMAP.md))

> O planejamento de cobertura da linguagem — o que já existe, o que falta e em que ordem —
> está em **[src/ROADMAP.md](src/ROADMAP.md)**.

---

## Início rápido

### Pré-requisitos

| Ferramenta | Versão mínima | Instalação |
|---|---|---|
| GCC ou Clang | C++20 | `sudo apt install g++` |
| Meson | 1.0+ | `pip install meson` |
| Ninja | 1.10+ | `sudo apt install ninja-build` |
| Conan | 2.0+ | `pip install conan` |

### Build completo

```bash
# 1. Clonar
git clone <url>
cd cpp_academy

# 2. Configurar (baixa dependências via Conan + configura Meson)
make configure

# 3. Compilar todos os subprojetos
make build

# 4. Instalar binários em dist/bin/
make install
```

### Build de um subprojeto específico

```bash
make configure        # necessário apenas uma vez
make build            # compila tudo

# Ou compilar e rodar só o que interessa:
source build/conanbuild.sh
meson compile -C build concurrency
./dist/bin/concurrency
```

---

## Comandos disponíveis

```bash
make configure   # Instala dependências (Conan) e configura Meson
make build       # Compila todos os alvos
make install     # Instala binários em dist/bin/
make test        # Executa testes (Google Test)
make clean       # Remove build/, dist/ e cache do Conan
make help        # Lista todos os comandos com descrição
```

---

## Estrutura do repositório

```
cpp_academy/
├── src/                  ← subprojetos (um diretório por projeto)
│   ├── asm_course/          ← Curso de assembly x86-64 — 15 módulos, do zero
│   ├── aws/                 ← AWS SDK for C++ — S3
│   ├── bits/                ← Bits: operadores, <bit>, bitset, endianness
│   ├── calculus/            ← Álgebra linear com Eigen3
│   ├── casts/               ← Guia de type casting
│   ├── chrono/              ← Tempo, datas e medição
│   ├── cmake/               ← CMake, CTest, Protobuf, OpenMP, MPI, CUDA (build próprio)
│   ├── collision/           ← Física com Bullet3
│   ├── compile_time/        ← constexpr, consteval, NTTP
│   ├── concepts/            ← C++20 Concepts
│   ├── concurrency/         ← Multithreading e primitivos de sync
│   ├── concurrency2/        ← jthread, latch, barrier, modelo de memória, lock-free
│   ├── coroutines/          ← Corrotinas C++20 — generator, task, internals, Asio
│   ├── cpp23/               ← O C++23 neste toolchain, medido
│   ├── database/            ← SOCI — SQLite/PostgreSQL
│   ├── design-of-pattern/   ← os 23 padrões GoF + null_object
│   ├── doxygen/             ← Documentação gerada dos comentários do código
│   ├── ecs/                 ← Entity Component System
│   ├── embed-python/        ← C++ como host do interpretador Python
│   ├── errors/              ← Exceções, error_code, std::expected
│   ├── file_io/             ← Streams, sstream, filesystem
│   ├── flight-simulator/    ← JSBSim + autopiloto PID
│   ├── http-client/         ← libcurl + nlohmann/json
│   ├── inline_asm/          ← Assembly inline x86-64
│   ├── lambdas/             ← Capturas, genéricas, type erasure
│   ├── layout/              ← Alinhamento, padding, [[no_unique_address]], vptr
│   ├── linkage/             ← ODR medida com 2 TUs reais, extern "C", ABI, pimpl
│   ├── mandelbrot/          ← Fractal com Qt + std::thread
│   ├── mandelbrot_openmp/   ← Fractal com Qt + OpenMP
│   ├── memory/              ← Alocadores customizados + std::pmr
│   ├── meson/               ← O build system: guia + exemplo executável
│   ├── modules/             ← Módulos C++20 (build próprio)
│   ├── network/             ← Sockets com Asio (TCP/UDP/HTTP)
│   ├── oop/                 ← Herança, vtable, slicing, RTTI
│   ├── operators/           ← Sobrecarga, <=>, conversões, literais
│   ├── outline_asm/         ← Assembly em arquivo .s separado
│   ├── parser/              ← Flex/Bison — parser de expressões
│   ├── performance/         ← Cache, AoS vs SoA, false sharing, como medir
│   ├── plugin_registry/     ← dlopen/dlsym — sistema de plugins
│   ├── python/              ← PyBind11 + CPython API
│   ├── qt_sfml/             ← Qt com widget SFML embutido
│   ├── qt_sfml2/            ← Qt + SFML + Bullet3
│   ├── rabbitmq/            ← AMQP-CPP — produtor/consumidor
│   ├── ranges/              ← C++20 Ranges — views e pipelines
│   ├── sandbox/             ← Espaço livre de experimentação
│   ├── sfml_bullet/         ← SFML + Bullet3 — física 2D visual
│   ├── sfml_bullet2/        ← SFML + Bullet3 — ragdoll e cordas
│   ├── smart-pointers/      ← unique_ptr, shared_ptr, weak_ptr
│   ├── solid/               ← Os 5 princípios — violação vs conformidade
│   ├── stl/                 ← Containers, iteradores, algoritmos
│   ├── strings/             ← SSO, string_view, format, charconv
│   ├── syntax/              ← Toda a sintaxe da linguagem, de ponta a ponta
│   ├── templates/           ← Metaprogramação de templates
│   ├── testing/             ← GoogleTest, fixtures, TEST_P, gmock, death tests
│   ├── type_erasure/        ← std::function do zero, SBO medido, virtual vs variant
│   ├── undefined_behavior/  ← Como o UB licencia o otimizador; sanitizers medidos
│   ├── value_semantics/     ← Move, forwarding, copy elision
│   ├── vocabulary_types/    ← optional, variant, any, tuple, span
│   └── yolo/                ← YOLOv10 + ONNX Runtime + OpenCV
├── include/              ← headers públicos da biblioteca
├── docs/                 ← documentação gerada pelo Doxygen
├── proto/                ← definições Protobuf
├── scripts/              ← scripts auxiliares
├── meson.build           ← build principal
├── meson_options.txt     ← opções configuráveis
├── conanfile.py          ← receita e dependências Conan
└── Makefile              ← interface de conveniência
```

---

## Subprojetos por categoria

### Fundamentos da Linguagem

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **syntax** | declarações, `if`/`switch`/loops, funções, tipos, classes, namespaces e pré-processador, expressões, templates e exceções | [→](src/syntax/README.md) |
| **casts** | `static_cast`, `dynamic_cast`, `const_cast`, `reinterpret_cast`, C-style cast | [→](src/casts/README.md) |
| **smart-pointers** | `unique_ptr`, `shared_ptr`, `weak_ptr`, RAII, pimpl, factory | [→](src/smart-pointers/README.md) |
| **templates** | function/class templates, specialization, variadic, SFINAE, type_traits, CRTP | [→](src/templates/README.md) |
| **concepts** | C++20 concepts, `requires`, constraints, subsumption | [→](src/concepts/README.md) |
| **memory** | `new`/`delete` internos, pool allocator, arena allocator, `std::pmr` | [→](src/memory/README.md) |
| **value_semantics** | categorias de valor, Rule of 0/3/5, RVO/NRVO, perfect forwarding, move-only | [→](src/value_semantics/README.md) |
| **oop** | herança, vtable/vptr, `override`/`final`, slicing, diamante, RTTI | [→](src/oop/README.md) |
| **errors** | exceções, garantias, `noexcept`, `error_code`, `std::expected` (C++23) | [→](src/errors/README.md) |
| **stl** | containers e complexidade, iteradores, `<algorithm>`, container próprio | [→](src/stl/README.md) |
| **ranges** | views preguiçosas, pipelines, projeções, adaptador próprio | [→](src/ranges/README.md) |

### C++ Moderno

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **vocabulary_types** | `optional`, `variant` + overload idiom, `any`, `tuple`, `span` | [→](src/vocabulary_types/README.md) |
| **compile_time** | `constexpr`, `consteval`, `constinit`, NTTP, tabelas geradas na compilação | [→](src/compile_time/README.md) |
| **operators** | sobrecarga canônica, `operator<=>`, `explicit`, functors, literais de usuário | [→](src/operators/README.md) |
| **lambdas** | capturas, genéricas, type erasure medido, `std::invoke`, scope guard | [→](src/lambdas/README.md) |
| **strings** | SSO medido, `string_view`, `std::format`, `<charconv>`, `std::regex` | [→](src/strings/README.md) |
| **chrono** | durations, relógios e garantias, calendário C++20, como cronometrar | [→](src/chrono/README.md) |
| **bits** | operadores e UB, `<bit>`, `bitset`, campos de bits, endianness | [→](src/bits/README.md) |
| **cpp23** | inventário medido por feature-test macros, e o substituto do que falta | [→](src/cpp23/README.md) |
| **modules** | módulos nomeados, `export import`, e as armadilhas do GCC 13 | [→](src/modules/README.md) |

### Concorrência e Paralelismo

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **concurrency** | `std::thread`, mutex, `condition_variable`, future/promise, atomics, thread pool | [→](src/concurrency/README.md) |
| **concurrency2** | `jthread`/`stop_token`, `latch`, `barrier`, `semaphore`, modelo de memória, lock-free medido | [→](src/concurrency2/README.md) |
| **mandelbrot** | Paralelismo por tiles, `std::thread`, `condition_variable` como sincronizador de tiles | [→](src/mandelbrot/README.md) |
| **mandelbrot_openmp** | `#pragma omp parallel for`, scheduling dinâmico, reduction | [→](src/mandelbrot_openmp/README.md) |

### Assembly e Hardware

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **inline_asm** | AT&T syntax, constraints, CPUID, RDTSC, clobbers, `volatile`, loops | [→](src/inline_asm/README.md) |
| **outline_asm** | Arquivos `.s` separados, ABI x86-64 System V, `rep movsb`, `cmov` | [→](src/outline_asm/README.md) |

### Modelo de Objeto e Engenharia

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **type_erasure** | `std::function` do zero, SBO medido, `any`, virtual vs CRTP vs variant vs erasure | [→](src/type_erasure/README.md) |
| **layout** | alinhamento, padding, `alignas`, trivial/standard-layout/agregado, `[[no_unique_address]]`, vptr | [→](src/layout/README.md) |
| **linkage** | ODR medida com 2 TUs reais, linkage interna/externa, `extern "C"` com C de verdade, ABI, pimpl | [→](src/linkage/README.md) |
| **testing** | GoogleTest, fixtures, `TEST_P`, gmock, death tests — 5 alvos, 41 casos, `meson test` | [→](src/testing/README.md) |
| **undefined_behavior** | as 4 categorias, como o UB licencia o otimizador, matriz de sanitizers medida, strict aliasing | [→](src/undefined_behavior/README.md) |
| **performance** | cache (35×), AoS vs SoA, false sharing (11.7×), predição de desvio, `perf`, como medir | [→](src/performance/README.md) |

### Padrões de Projeto e Arquitetura

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **ecs** | Entity Component System, type erasure com `std::type_index`, query genérica com fold expressions | [→](src/ecs/README.md) |
| **solid** | SRP, OCP, LSP, ISP, DIP — violação vs. conformidade, com testes de contrato, e a alternativa moderna de cada um | [→](src/solid/README.md) |
| **design-of-pattern** | os 23 padrões GoF + `null_object`, cada um com a "versão moderna" | [→](src/design-of-pattern/README.md) |
| **plugin_registry** | `dlopen`/`dlsym`, interface virtual, singleton registry, `unique_ptr` com deleter customizado | [→](src/plugin_registry/README.md) |

### Rede e Mensageria

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **network** | Asio: TCP síncrono/assíncrono, UDP, `steady_timer`, HTTP raw | [→](src/network/README.md) |
| **http-client** | libcurl wrapper, nlohmann/json, deserialização automática de structs | [→](src/http-client/README.md) |
| **rabbitmq** | AMQP-CPP + Boost.Asio, produtor/consumidor, ACK, exchange/queue | [→](src/rabbitmq/README.md) |
| **aws** | AWS SDK for C++, S3Client, modelo `Outcome<R,E>`, credenciais automáticas | [→](src/aws/README.md) |

### Banco de Dados

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **database** | SOCI, repository pattern, prepared statements, transações, DSN portável | [→](src/database/README.md) |

### Gráficos, Física e Simulação

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **collision** | Bullet3: broadphase/narrowphase, corpos estáticos/dinâmicos, restitution | [→](src/collision/README.md) |
| **sfml_bullet** | SFML + Bullet3, simulação 2D, conversão metros↔pixels, Y-flip | [→](src/sfml_bullet/README.md) |
| **sfml_bullet2** | Ragdoll (7 segmentos), cordas, `btHingeConstraint`, `btConeTwistConstraint` | [→](src/sfml_bullet2/README.md) |
| **qt_sfml** | Qt + SFML integrado, `winId()` trick, `QTimer` como game loop | [→](src/qt_sfml/README.md) |
| **qt_sfml2** | Qt + SFML + Bullet3, criação dinâmica de corpos físicos | [→](src/qt_sfml2/README.md) |
| **flight-simulator** | JSBSim FDM, autopiloto PID, loop de controle 120 Hz, CSV logging | [→](src/flight-simulator/README.md) |
| **calculus** | Eigen3: LU/QR/SVD/autovalores, mínimos quadrados, PCA, número de condição | [→](src/calculus/README.md) |

### Python Interop

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **python** | PyBind11 vs CPython API, GIL, OpenMP em extensões Python | [→](src/python/README.md) |
| **embed-python** | C++ como host do interpretador Python, RAII guard, gerenciamento de `PyObject*` | [→](src/embed-python/README.md) |

### Machine Learning e Visão

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **yolo** | YOLOv10 + ONNX Runtime, pré/pós-processamento, webcam, benchmark p95/p99 | [→](src/yolo/README.md) |

### Ferramentas e Geração de Código

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **meson** | build system: alvos, `declare_dependency`, opções, `configure_file`, `custom_target`, `test()`, integração com Conan | [→](src/meson/README.md) |
| **cmake** | CMake moderno: alvos e escopos, `find_package`, Protobuf gerado no build, OpenMP/MPI/CUDA, CTest — **build próprio, fora do Meson** | [→](src/cmake/README.md) |
| **parser** | Flex + Bison, análise léxica/sintática, gramática BNF, precedência de operadores | [→](src/parser/README.md) |
| **sandbox** | Espaço livre para experimentos — atualmente: nlohmann/json | [→](src/sandbox/README.md) |

---

## Ordem de estudo sugerida

O repositório não tem uma ordem obrigatória — cada subprojeto compila sozinho. Mas os conceitos
se apoiam uns nos outros, e estudar fora de ordem custa caro (é difícil entender `shared_ptr`
sem entender destrutor e cópia; é difícil entender `ranges` sem entender iteradores).

A trilha abaixo vai do concreto ao abstrato. **As etapas 1 a 5 formam o núcleo** — quem só quer
aprender C++ moderno pode parar na 5. As etapas 6 em diante são ecossistema e escolha por
interesse, não por sequência.

> **Todos os subprojetos compilam por padrão.** Não há nenhum `subdir()` comentado em
> [src/meson.build](src/meson.build) — `make build` constrói os **57** e instala **98
> binários**, com zero warnings sob `-Wall -Wextra -Wpedantic`.
>
> Dois subprojetos ficam fora do Meson **de propósito**, porque o sistema de build *é* o
> assunto deles: [src/modules/](src/modules/README.md) (Makefile próprio, por causa de
> `-fmodules-ts`) e [src/cmake/](src/cmake/README.md) (CMake próprio).

### Etapa 1 — Fundamentos da linguagem

Aqui se aprende o que o compilador faz com seu código. Nada mais no repositório faz sentido antes disto.

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 0 | 🟢 [syntax](src/syntax/README.md) | O mapa da linguagem. Toda construção sintática num só lugar — leia antes de tudo, volte como referência |
| 1 | 🟢 [casts](src/casts/README.md) | Menor pré-requisito de todos. Ensina o sistema de tipos e por que `reinterpret_cast` não é "converter" |
| 2 | 🟢 [value_semantics](src/value_semantics/README.md) | Categorias de valor, Rule of 0/3/5, move, RVO. É a base conceitual de *tudo* o que vem depois |
| 3 | 🟢 [smart-pointers](src/smart-pointers/README.md) | RAII aplicado. Só faz sentido depois de entender destrutores e move (etapa 2) |
| 4 | 🟢 [oop](src/oop/README.md) | Herança, vtable, slicing, RTTI. Fecha o ciclo com `dynamic_cast` da etapa 1 |
| 5 | 🟢 [errors](src/errors/README.md) | Exceções e garantias dependem de RAII (3) e de saber o que acontece durante o unwinding |

### Etapa 2 — Biblioteca padrão

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 6 | 🟢 [stl](src/stl/README.md) | Containers, iteradores e complexidade. Requer value semantics para entender custo de cópia/move |
| 7 | 🟢 [ranges](src/ranges/README.md) | Views são iteradores repensados — vem obrigatoriamente depois de `stl` |
| 8 | 🟢 [file_io](src/file_io/README.md) | Streams, `fstream`, `sstream`, `std::filesystem`, logging. Independente, mas usa idiomas da STL |
| 9 | 🟢 [memory](src/memory/README.md) | Alocadores, pool, arena, `std::pmr`. Só rende depois de conhecer o que os containers alocam |

### Etapa 3 — Programação genérica

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 10 | 🟢 [templates](src/templates/README.md) | Especialização, variádicos, SFINAE, CRTP, `type_traits` |
| 11 | 🟢 [concepts](src/concepts/README.md) | Concepts é a resposta do C++20 ao SFINAE — estudar antes de `templates` inverte a história |

### Etapa 4 — Design e arquitetura

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 12 | 🟢 [solid](src/solid/) | Na ordem `srp` → `ocp` → `lsp` → `isp` → `dip`. Cada princípio tem versão violada e conforme |
| 13 | 🟢 [design-of-pattern](src/design-of-pattern/README.md) | Padrões GoF. As seções "versão moderna" exigem templates (10) e concepts (11) |
| 14 | 🟢 [ecs](src/ecs/README.md) | Type erasure e composição sobre herança — contraponto direto ao `oop` da etapa 1 |
| 15 | 🟢 [plugin_registry](src/plugin_registry/README.md) | `dlopen`/`dlsym`. É DIP (12) levado até o limite do sistema operacional |

### Etapa 5 — Concorrência

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 16 | 🟢 [concurrency](src/concurrency/README.md) | `thread`, mutex, `condition_variable`, future/promise, atomics, thread pool |
| 17 | 🟢 [coroutines](src/coroutines/README.md) | Generator, task, internals. Precisa de templates (10) e do modelo de execução de (16) |
| 18 | 🟢 [mandelbrot](src/mandelbrot/README.md) | Aplicação visual do que se aprendeu em (16): paralelismo por tiles |
| 19 | 🟢 [mandelbrot_openmp](src/mandelbrot_openmp/README.md) | O mesmo problema com OpenMP — comparação direta de esforço e ganho |

### Etapa 6 — Ecossistema: rede, dados e mensageria

A partir daqui a ordem é por interesse. Todos dependem apenas do núcleo (etapas 1–5).

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 20 | 🟢 [network](src/network/README.md) | Asio: TCP/UDP, síncrono e assíncrono. Melhor logo após `coroutines` (17) |
| 21 | 🟢 [http-client](src/http-client/README.md) | libcurl + JSON — camada de aplicação sobre o que (20) mostrou em sockets |
| 22 | 🟢 [database](src/database/README.md) | SOCI, repository pattern, transações. Aplica DIP (12) em persistência |
| 23 | 🟢 [rabbitmq](src/rabbitmq/README.md) | AMQP sobre Asio — combina (20) com mensageria |
| 24 | 🟢 [aws](src/aws/README.md) | S3 e o modelo `Outcome<R,E>` — contraste com o `errors` da etapa 1 |

### Etapa 7 — Gráficos, física e simulação

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 25 | 🟢 [collision](src/collision/README.md) | Bullet3 puro, sem renderização — entender a física antes de desenhar |
| 26 | 🟢 [sfml_bullet](src/sfml_bullet/README.md) | Adiciona renderização 2D a (25) |
| 27 | 🟢 [sfml_bullet2](src/sfml_bullet2/README.md) | Constraints, ragdoll, cordas — evolução direta de (26) |
| 28 | 🟢 [qt_sfml](src/qt_sfml/README.md) | Embutir SFML em Qt |
| 29 | 🟢 [qt_sfml2](src/qt_sfml2/README.md) | Qt + SFML + Bullet — junta (27) e (28) |
| 30 | 🟢 [calculus](src/calculus/README.md) | Eigen3: LU/QR/SVD, PCA. Pré-requisito matemático de (31) |
| 31 | 🟢 [flight-simulator](src/flight-simulator/README.md) | JSBSim + PID a 120 Hz. O mais integrado do repositório — deixe por último |

### Etapa 8 — Baixo nível e hardware

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 32 | 🟢 [inline_asm](src/inline_asm/README.md) | `asm volatile`, constraints, clobbers, CPUID, RDTSC |
| 33 | 🟢 [outline_asm](src/outline_asm/README.md) | Arquivos `.s` e a ABI System V — o passo seguinte a (32) |
| 34 | 🟢 [asm_course](src/asm_course/README.md) | 15 módulos numerados (registradores → SIMD → syscalls → disassembly), cada um um executável `asm_NN_*`. O README ensina do zero: o que é um registrador, a pilha, as flags, as duas sintaxes |

### Etapa 9 — Interop, tooling e visão

| # | Subprojeto | Por que nesta posição |
|---|---|---|
| 35 | 🟢 [python](src/python/README.md) | PyBind11 vs CPython API, GIL. Requer entender lifetime (etapa 1) |
| 36 | 🟢 [embed-python](src/embed-python/README.md) | Inverte a direção de (35): C++ hospeda o interpretador |
| 37 | 🟢 [parser](src/parser/README.md) | Flex + Bison, gramática, precedência. Independente de todo o resto |
| 38 | 🟢 [yolo](src/yolo/README.md) | ONNX + OpenCV, benchmark p95/p99. Fecha com um pipeline de produção completo |
| — | 🟢 [meson](src/meson/README.md) · 🟢 [cmake](src/cmake/README.md) · 🟢 [doxygen](src/doxygen/) · 🟢 [sandbox](src/sandbox/README.md) | Tooling e experimentação livre — consulte quando precisar, não em sequência |

### Atalhos por objetivo

Se você já sabe C++ e quer algo específico, não precisa passar por tudo:

| Objetivo | Trilha mínima |
|---|---|
| Passar em entrevista de C++ | 1 → 2 → 3 → 4 → 5 → 6 → 10 → 16 |
| Escrever backend / serviço | 1 → 3 → 5 → 6 → 12 → 16 → 17 → 20 → 21 → 22 |
| Games / simulação | 2 → 6 → 9 → 14 → 25 → 26 → 27 |
| Performance e otimização | 2 → 6 → 9 → 16 → 32 → 33 → 34 |
| Arquitetura e code review | 4 → 12 → 13 → 14 → 15 |
| Só o C++20/23 que eu não conheço | 7 → 11 → 17 |

> **Nada precisa ser descomentado.** Os alvos pesados (Qt, SFML, Bullet, ONNX, JSBSim, AWS SDK,
> RabbitMQ, SOCI) estão habilitados e compilando — o `make configure` traz tudo pelo Conan.
>
> Isso foi uma decisão deliberada, e ela se pagou no primeiro minuto: habilitar os alvos que
> estavam comentados revelou **quatro `#include` faltando** e **dois bugs de ABI** que davam
> segfault, todos latentes há meses. A lista está em [src/meson.build](src/meson.build), e a
> lição é curta: **um alvo que não é construído é um alvo que apodrece.**
>
> O estado da cobertura da linguagem está em [src/ROADMAP.md](src/ROADMAP.md).
---

## Dependências

Todas gerenciadas pelo Conan 2 e declaradas em `conanfile.py`:

| Biblioteca | Versão | Usada em |
|---|---|---|
| `fmt` | 10.2.1 | Todos os subprojetos (formatação) |
| `spdlog` | 1.14.1 | `flight-simulator` (logging estruturado) |
| `nlohmann_json` | 3.11.3 | `http-client`, `sandbox` |
| `eigen` | 3.4.0 | `calculus` (álgebra linear) |
| `bullet3` | 3.25 | `collision`, `sfml_bullet`, `sfml_bullet2`, `qt_sfml2` |
| `sfml` | 2.6.1 | `sfml_bullet*`, `qt_sfml*`, `mandelbrot*` |
| `libcurl` | 8.6.0 | `http-client` |
| `asio` | 1.28.0 | `network` |
| `soci` | 4.0.3 | `database` (SQLite + PostgreSQL) |
| `sqlite3` | 3.45.0 | `database` (backend) |
| `pybind11` | 3.0.1 | `python` |
| `aws-sdk-cpp` | 1.11.692 | `aws` (componente S3) |
| `amqp-cpp` | 4.3.27 | `rabbitmq` |
| `jsbsim` | 1.1.11 | `flight-simulator` |
| `onnxruntime` | 1.17.3 | `yolo` |
| `opencv` | 4.9.0 | `yolo` |
| `gtest` | 1.14.0 | `testing`, `meson`, `cmake` |
| `protobuf` | 3.21.12 | `proto/` |
| OpenMP | — | `mandelbrot_openmp`, `python` |
| Qt 6 | — | `mandelbrot*`, `qt_sfml*` (sistema) |

> Qt 6 é instalado via sistema (`sudo apt install qt6-base-dev`), não via Conan.

---

## Adicionando uma nova dependência

**1.** Declare em `conanfile.py`:

```python
def requirements(self):
    self.requires("boost/1.84.0")
```

**2.** Adicione no `meson.build` do subprojeto:

```meson
boost_dep = dependency('boost')

executable('meu_projeto',
    'src/meu_projeto/main.cpp',
    dependencies: [boost_dep, fmt_dep],
)
```

**3.** Reconfigure:

```bash
make configure
make build
```

---

## Adicionando um novo subprojeto

**1.** Crie o diretório e os arquivos:

```bash
mkdir src/meu_projeto
touch src/meu_projeto/main.cpp
touch src/meu_projeto/meson.build
touch src/meu_projeto/README.md
```

**2.** Escreva o `meson.build`:

```meson
executable('meu_projeto',
    'main.cpp',
    dependencies: [fmt_dep],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
```

**3.** Registre em [`src/meson.build`](src/meson.build) (não no da raiz), em ordem alfabética:

```meson
subdir('meu_projeto')
```

**4.** Reconfigure e compile — o `--reconfigure` é obrigatório para um alvo novo:

```bash
source build/conanbuild.sh
meson setup --reconfigure build .
meson compile -C build meu_projeto
./build/src/meu_projeto/meu_projeto
```

Sem o passo 4, o `meson compile` responde `ERROR: Can't invoke target 'meu_projeto':
target not found` — o build dir ainda não sabe que o alvo existe. Depois disso, `make build`
já basta.

> Guia completo do build system, com todos os recursos do Meson usados neste repositório:
> [src/meson/README.md](src/meson/README.md).

---

## Testes

O antigo `tests/` da raiz foi **removido**: ele estava 100% comentado desde sempre e nunca
construiu nada. Os testes automatizados agora moram nos subprojetos que os **ensinam e
exercitam de verdade**.

```bash
make test                                    # todos os testes do Meson
meson test -C build --print-errorlogs        # o mesmo, com a saída das falhas
meson test -C build testing_parametrizado -v # um alvo específico
meson test -C build --repeat 100             # caçando teste instável
```

| onde | o que roda | alvos |
|---|---|---|
| [`src/testing/`](src/testing/README.md) | GoogleTest, fixtures, `TEST_P`, gmock, death tests | 5 alvos, 41 casos |
| [`src/meson/`](src/meson/README.md) | o `test()` do Meson, com `main()` próprio | 1 alvo |
| [`src/cmake/`](src/cmake/README.md) | **CTest** — build próprio, fora do Meson | 11 casos |

O `src/cmake/` tem o seu próprio sistema de build de propósito (é o assunto dele), então roda à
parte:

```bash
cd src/cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$(sed -n 's|^prefix=||p' ../../build/gtest.pc);$(sed -n 's|^prefix=||p' ../../build/protobuf.pc)"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

---

## Documentação

```bash
# Gera HTML com Doxygen
cd build && ninja docs

# Abre no browser:
xdg-open docs/html/index.html
```

---

## Release e CI/CD

O workflow `.github/workflows/release.yml` produz binários para Linux, macOS e Windows quando uma tag `v*` é publicada:

```bash
# Criar release
make release VERSION=1.2.3
# → cria tag v1.2.3 → dispara CI → publica artefatos na GitHub Release
```

| Plataforma | Artefato |
|---|---|
| Linux | `academy-linux.AppImage` |
| macOS | `academy-macos.dmg` |
| Windows | `academy-windows-setup.exe` |

### Testar CI localmente com `act`

```bash
# Instalar act (roda GitHub Actions via Docker)
curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Simular job Linux
act push --job build --matrix os:ubuntu-22.04
```

---

## Licença

MIT — veja [LICENSE](LICENSE) para detalhes.
