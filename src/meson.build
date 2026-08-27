# =============================================================================
#  src/meson.build — o índice dos subprojetos
#
#  ⚠ TODOS os subprojetos estão HABILITADOS. Não há nenhum `subdir()` comentado
#    neste arquivo, e isso é uma decisão de projeto, não um acidente:
#
#      um alvo que não é construído é um alvo que apodrece.
#
#    Este repositório já pagou esse preço quatro vezes. Todos os bugs abaixo
#    estavam LATENTES porque o alvo nunca era compilado, e todos apareceram no
#    minuto seguinte a habilitá-lo:
#
#      src/solid/srp/demo_pipeline.hpp   faltava #include <algorithm>
#      src/concepts/demo_constraints.hpp faltava #include <array>
#      src/file_io/demo_sstream.hpp      faltava #include <cmath>
#      src/aws/s3/main.cpp               faltava #include <fmt/core.h>
#
#    E dois bugs de ABI que só apareceram ao EXECUTAR os binários:
#
#      src/asm_course/07_calling_asm.s   `subq $8, %rsp` QUEBRAVA o alinhamento
#                                        de 16 bytes exigido no CALL → SIGSEGV
#                                        dentro do fmt, quatro quadros acima
#      src/asm_course/10_string_ops.cpp  RDI/RSI/RCX declarados como entrada
#                                        pura, mas `rep movsb` os modifica →
#                                        o GCC reusava registrador corrompido
#
#    A regra que este arquivo encarna: se um subprojeto não pode ser
#    construído, o problema é dele — conserte-o ou apague-o. Não o esconda
#    atrás de um `#`.
#
#  ─────────────────────────────────────────────────────────────────────────
#  Dois subprojetos NÃO aparecem aqui, e também de propósito:
#
#    src/modules/   usa Makefile próprio — módulos C++20 no GCC 13 exigem
#                   `-fmodules-ts -x c++` e ordem manual de compilação
#    src/cmake/     usa CMake próprio — o subprojeto É sobre CMake
#
#  Os dois documentam a escolha nos seus READMEs.
#  ─────────────────────────────────────────────────────────────────────────
#
#  A ordem abaixo segue a TRILHA DIDÁTICA do README.md da raiz, não o alfabeto:
#  quem lê de cima para baixo lê na ordem em que os assuntos se apoiam.
# =============================================================================

# ── Tier 1 · Fundamentos da linguagem ────────────────────────────────────────
subdir('syntax')            # toda a sintaxe, de ponta a ponta
subdir('casts')             # os quatro casts e o C-style
subdir('value_semantics')   # categorias de valor, Rule of 0/3/5, RVO, forwarding
subdir('smart-pointers')    # unique_ptr, shared_ptr, weak_ptr, RAII, pimpl
subdir('memory')            # new/delete por dentro, pool, arena, pmr
subdir('oop')               # herança, vtable, slicing, RTTI
subdir('errors')            # exceções, garantias, noexcept, error_code, expected
subdir('stl')               # containers, iteradores, algoritmos
subdir('ranges')            # views, pipelines, projeções

# ── Tier 2 · Programação genérica ────────────────────────────────────────────
subdir('templates')         # function/class templates, variádicos, SFINAE, CRTP
subdir('concepts')          # requires, constraints, subsunção
subdir('lambdas')           # capturas, genéricas, std::function, invoke

# ── Tier 2 · C++ moderno ─────────────────────────────────────────────────────
subdir('vocabulary_types')  # optional, variant, any, tuple, span
subdir('compile_time')      # constexpr, consteval, constinit, NTTP
subdir('operators')         # sobrecarga canônica, <=>, literais de usuário
subdir('strings')           # SSO medido, string_view, format, charconv, regex
subdir('chrono')            # durations, relógios, calendário, como cronometrar
subdir('bits')              # operadores e UB, <bit>, bitset, endianness
subdir('file_io')           # streams, sstream, filesystem
subdir('cpp23')             # inventário medido por feature-test macros
subdir('coroutines')        # generator, task, internals, Asio

# ── Tier 3 · Concorrência e paralelismo ──────────────────────────────────────
subdir('concurrency')       # thread, mutex, condition_variable, future, pool
subdir('concurrency2')      # jthread, latch, barrier, memory model, lock-free
subdir('mandelbrot')        # paralelismo por tiles com std::thread
subdir('mandelbrot_openmp') # o mesmo com #pragma omp

# ── Tier 4 · Modelo de objeto e engenharia ───────────────────────────────────
subdir('type_erasure')      # std::function do zero, SBO medido, virtual vs CRTP
subdir('layout')            # alinhamento, padding, [[no_unique_address]], vptr
subdir('linkage')           # ODR medida com 2 TUs, extern "C", ABI, pimpl
subdir('testing')           # GoogleTest, fixtures, TEST_P, gmock, death tests
subdir('undefined_behavior')# como o UB licencia o otimizador; sanitizers medidos
subdir('performance')       # cache, AoS vs SoA, false sharing, como medir

# ── Tier 5 · Arquitetura ─────────────────────────────────────────────────────
subdir('solid')             # os 5 princípios, violação vs conformidade
subdir('design-of-pattern') # os 23 GoF + null_object, com "versão moderna"
subdir('ecs')               # Entity Component System
subdir('plugin_registry')   # dlopen/dlsym, registry, unique_ptr com deleter

# ── Baixo nível e hardware ───────────────────────────────────────────────────
subdir('asm_course')        # curso de assembly x86-64, 10 módulos
subdir('inline_asm')        # asm inline no GCC: constraints, clobbers
subdir('outline_asm')       # arquivos .s separados, ABI System V

# ── Ferramentas ──────────────────────────────────────────────────────────────
subdir('meson')             # o build system, com o próprio meson.build de aula
subdir('doxygen')           # documentação a partir dos comentários
subdir('parser')            # Flex + Bison, gramática, precedência

# ── Rede, dados e mensageria ─────────────────────────────────────────────────
subdir('network')           # Asio: TCP, UDP, timer, HTTP cru
subdir('http-client')       # libcurl + nlohmann/json
subdir('database')          # SOCI, repository, prepared statements, transações
subdir('rabbitmq')          # AMQP-CPP + Boost.Asio, produtor/consumidor
subdir('aws/s3')            # AWS SDK, S3Client, o modelo Outcome<R,E>

# ── Gráficos, física e simulação ─────────────────────────────────────────────
subdir('calculus')          # Eigen3: LU/QR/SVD, mínimos quadrados, PCA
subdir('collision')         # Bullet3: broadphase/narrowphase
subdir('sfml_bullet')       # SFML + Bullet3, física 2D visual
subdir('sfml_bullet2')      # ragdoll, cordas, constraints
subdir('qt_sfml')           # Qt com widget SFML embutido
subdir('qt_sfml2')          # Qt + SFML + Bullet3
subdir('flight-simulator')  # JSBSim, autopiloto PID, loop de 120 Hz

# ── Interoperabilidade e visão ───────────────────────────────────────────────
subdir('python')            # PyBind11 vs CPython API, GIL
subdir('embed-python')      # o inverso: C++ hospeda o interpretador
subdir('yolo')              # YOLOv10 + ONNX Runtime + OpenCV

# ── Experimentação ───────────────────────────────────────────────────────────
subdir('sandbox')           # espaço livre para experimentos
