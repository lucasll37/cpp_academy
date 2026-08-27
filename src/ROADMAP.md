# Roadmap didático — cobertura da linguagem C++

Este documento rastreia **o que a linguagem oferece** versus **o que o `cpp_academy` já ensina**.

O repositório nasceu forte em *ecossistema* (SFML, Bullet, Qt, ONNX, JSBSim, AWS, Asio) e fraco
em *linguagem*. Este roadmap existe para fechar essa assimetria.

---

## Como medir a lacuna

O levantamento abaixo veio de um `grep` sobre os 230 arquivos `.cpp`/`.hpp` de `src/`
(excluindo `*/deps/` e `python/standalone`). Para reproduzir:

```bash
cd src
FILES=$(find . \( -name '*.cpp' -o -name '*.hpp' \) -not -path '*/deps/*' -not -path '*/standalone/*')
echo "$FILES" | tr '\n' '\0' | xargs -0 grep -l 'views::' | wc -l
```

---

## Estado da cobertura

Legenda: ✅ pronto · 🚧 em construção · ⬜ planejado

### Tier 1 — Fundamentos da linguagem

| Subprojeto | Tópicos | Estado |
|---|---|---|
| `syntax` | declarações, controle de fluxo, funções, tipos, classes, namespaces/pré-processador, expressões, templates/exceções | ✅ |
| `casts` | `static_cast`, `dynamic_cast`, `const_cast`, `reinterpret_cast`, C-style | ✅ |
| `smart-pointers` | `unique_ptr`, `shared_ptr`, `weak_ptr`, RAII, pimpl | ✅ |
| `templates` | function/class templates, especialização, variádicos, SFINAE, CRTP | ✅ |
| `concepts` | C++20 concepts, `requires`, constraints, subsumption | ✅ |
| `memory` | `new`/`delete` internos, pool, arena, `std::pmr` | ✅ |
| `value_semantics` | categorias de valor, Rule of 0/3/5, RVO, perfect forwarding, move-only | ✅ |
| `oop` | herança, vtable, slicing, herança múltipla, RTTI | ✅ |
| `errors` | exceções, garantias, `noexcept`, `error_code`, `std::expected` | ✅ |
| `stl` | containers, iteradores, `<algorithm>`, container customizado, armadilhas | ✅ |
| `ranges` | views, pipelines, projeções, range customizado | ✅ |

### Tier 2 — C++ moderno

| Subprojeto | Tópicos | Estado |
|---|---|---|
| `coroutines` | generator, task, internals, integração com Asio | ✅ |
| `vocabulary_types` | `optional`, `variant` + `visit` + overload idiom, `any`, `tuple`, `span` | ✅ |
| `compile_time` | `constexpr`, `consteval`, `constinit`, `static_assert`, NTTP, tabelas em compile-time | ✅ |
| `operators` | sobrecarga, `operator<=>`, conversão, literais de usuário, functors | ✅ |
| `lambdas` | capturas, `mutable`, generic lambdas, `std::function` vs template, `std::invoke` | ✅ |
| `strings` | SSO medido, `string_view`, `std::format` + formatter, `<charconv>`, `std::regex` | ✅ |
| `chrono` | durations, relógios e garantias, calendário C++20, **como cronometrar** | ✅ |
| `bits` | operadores e UB, `<bit>`, `bitset`, campos de bits, endianness | ✅ |
| `cpp23` | inventário **medido** por feature-test macros, e o substituto do que falta | ✅ |
| `modules` | módulos nomeados, `export import`, 2 bugs do GCC 13 documentados | ✅ (fora do Meson) |

### Tier 3 — Concorrência

| Subprojeto | Tópicos | Estado |
|---|---|---|
| `concurrency` | `thread`, mutex, `condition_variable`, future/promise, atomics, thread pool | ✅ |
| `mandelbrot` / `mandelbrot_openmp` | paralelismo por tiles, OpenMP | ✅ |
| `concurrency2` | `jthread`/`stop_token`, `latch`, `barrier`, `counting_semaphore`, memory model, lock-free | ✅ |

### Tier 4 — Modelo de objeto e engenharia

| Subprojeto | Tópicos | Estado |
|---|---|---|
| `type_erasure` | `std::function` por dentro, `any`, virtual vs CRTP vs variant | ✅ |
| `layout` | alinhamento, padding, `alignas`, POD/trivial/standard-layout, `[[no_unique_address]]` | ✅ |
| `linkage` | ODR, unidades de tradução, `inline`, linkage interna/externa, `extern "C"`, ABI | ✅ |
| `meson` | build system: alvos, dependências, opções, `configure_file`, `custom_target`, testes, integração com Conan | ✅ |
| `cmake` | CMake moderno, `find_package`/FetchContent, Protobuf gerado no build, OpenMP/MPI/CUDA, CTest | ✅ (build próprio; MPI e CUDA não compilados aqui) |
| `testing` | GoogleTest, fixtures, testes parametrizados, `gmock`, death tests | ✅ — 5 alvos, 41 casos; o `tests/` da raiz foi removido |
| `undefined_behavior` | ASan/UBSan/TSan, strict aliasing, overflow assinado, dangling | ✅ |
| `performance` | cache locality, false sharing, branch prediction, AoS vs SoA, `perf` | ✅ |

### Tier 5 — Arquitetura

| Subprojeto | Estado |
|---|---|
| `design-of-pattern` | ✅ — 24 padrões, com "versão moderna" em cada um |
| `solid` | ✅ — os 5 princípios implementados |
| `ecs`, `plugin_registry` | ✅ |

---

## Padrões implementados

| Categoria | Padrões |
|---|---|
| Criacionais | `factory`, `abstract_factory`, `builder`, `prototype`, `singleton` |
| Estruturais | `adapter`, `bridge`, `composite`, `decorator`, `facade`, `flyweight`, `proxy` |
| Comportamentais | `chain`, `command`, `interpreter`, `iterator`, `mediator`, `memento`, `observer`, `state`, `strategy`, `template_method`, `visitor` |
| Fora do GoF | `null_object` |

Cada um dos 13 acrescentados na Fase 3c traz uma seção **"versão moderna"**, mostrando
quando `std::function`, `std::variant`, designated initializers ou concepts substituem a
hierarquia virtual clássica — e, principalmente, **quando não substituem**.

Faltam do catálogo original: `adapter de classe` (variante discutida dentro de `adapter`),
`interpreter` (já existe) e os três de concorrência que o GoF não cataloga.

---

## Limites do toolchain

O compilador local é **GCC 13.3** (`g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1)`).

| Recurso | Padrão | GCC 13.3 |
|---|---|---|
| `std::expected` | C++23 | ✅ |
| deducing this (`this auto&&`) | C++23 | ❌ (P0847 chegou no GCC 14) |
| `if consteval` | C++23 | ✅ |
| `std::ranges` / views | C++20 | ✅ |
| `std::format` | C++20 | ✅ |
| `jthread`, `latch`, `barrier`, `semaphore` | C++20 | ✅ |
| `std::stacktrace` | C++23 | ✅ com `-lstdc++_libbacktrace` |
| `views::zip` | C++23 | ✅ |
| `std::byteswap`, `to_underlying`, `unreachable` | C++23 | ✅ |
| subscript multidimensional, `static operator()` | C++23 | ✅ |
| `std::print` / `std::println` | C++23 | ❌ (precisa GCC 14) |
| `std::mdspan` | C++23 | ❌ (precisa GCC 14) |
| `std::generator` | C++23 | ❌ (precisa GCC 14) |
| `std::flat_map` / `flat_set` | C++23 | ❌ (precisa GCC 15) |
| `ranges::to<C>()` | C++23 | ❌ (precisa GCC 14) |
| `views::enumerate` | C++23 | ❌ (precisa GCC 14) |
| `ranges::starts_with` / `ends_with` | C++23 | ❌ |
| `std::out_ptr` / `inout_ptr` | C++23 | ❌ |
| `ranges::fold_left` | C++23 | ✅ |
| `views::chunk` / `slide` | C++23 | ✅ |
| `std::move_only_function` | C++23 | ✅ |
| `std::spanstream` | C++23 | ✅ |
| `import std;` | C++23 | ❌ |

> Verificado com `__cpp_*` feature-test macros — ver `src/cpp23/demo_inventario.hpp`,
> que roda essa checagem e imprime o resultado. Duas entradas desta tabela estavam
> ERRADAS antes de serem medidas: `stacktrace`, `views::zip`, `ranges::fold_left`,
> `views::chunk`, `move_only_function` e `spanstream` foram listados ou presumidos
> ausentes, e todos funcionam. A lição está no próprio demo: não presuma, meça.

O `meson.build` da raiz fixa `cpp_std=c++20`. Subprojetos que precisem de C++23 declaram
localmente, sem tocar na raiz:

```meson
executable('errors',
    ['main.cpp'],
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
```

---

## Dívida estrutural conhecida

1. ~~**`src/meson.build` tem quase tudo comentado.**~~ — **resolvido por completo na Fase 6.**
   Não há mais nenhum `subdir()` comentado: os **57** subprojetos são construídos, e o
   `make build` instala **98 binários** com zero warnings. Os alvos pesados (Qt, SFML,
   Bullet, ONNX, JSBSim, AWS SDK, RabbitMQ, SOCI) foram habilitados junto — as dependências
   já estavam no `conanfile.py`, só faltava usá-las.

   Habilitar tudo revelou **seis bugs latentes** no mesmo dia, e é a melhor justificativa
   possível para a política. Ver "O que os alvos desabilitados escondiam", abaixo.
2. ~~**`tests/meson.build` está 100% comentado**, apesar de `gtest/1.14.0` estar em
   `conanfile.py` e de existir um alvo `make test`.~~ — **resolvido por remoção.** O `tests/`
   da raiz nunca construiu nada e foi apagado. Os testes agora moram onde são ensinados:
   `src/testing/` (5 alvos, 41 casos), `src/meson/` (1 alvo) e `src/cmake/` (CTest, 11 casos).
   O `sandbox/` da raiz também foi removido na mesma limpeza — era um rascunho solto, e
   `src/sandbox/` continua existindo para experimentação.
3. ~~`solid/{dip,isp,lsp,ocp}/meson.build` têm 0 bytes.~~ — resolvido: os 4
   princípios foram implementados e habilitados em `solid/meson.build`.
4. **`srp/demo_pipeline.hpp` não compilava** (faltava `#include <algorithm>`) — o bug
   estava latente porque `solid/` nunca era construído. Corrigido.

---

## Ordem de execução sugerida

1. ~~**Fase 1** — `value_semantics`, `oop`, `errors`, `stl`, `ranges`.~~ ✅ concluída.
2. ~~**Fase 2** — fechar `solid` (4 princípios) e adicionar os GoF pendentes.~~ ✅
3. ~~**Fase 3** — Tier 2 completo.~~ ✅ concluída: os 9 subprojetos.
4. ~~**Fase 4** — Tier 3 (`concurrency2`) e Tier 4 (`type_erasure`, `layout`,
   `linkage`, `testing`, `undefined_behavior`, `performance`).~~ ✅ concluída.

5. ~~**Fase 5** — `cmake` (build system alternativo, com Protobuf, OpenMP, MPI e CUDA)
   e limpeza da raiz (`sandbox/` e `tests/` removidos).~~ ✅ concluída.
6. ~~**Fase 6** — habilitar TODOS os subprojetos, corrigir os bugs que isso revelou,
   e reescrever o relatório como livro.~~ ✅ concluída.

**O roadmap está fechado.** Os cinco tiers cobrem o que a linguagem oferece, do
`syntax` ao modelo de memória. O que sobra é aprofundamento em temas específicos
(veja "Além do roadmap", abaixo).

---

## O método, e o que ele custou

A regra que atravessa os 37 subprojetos: **toda alegação de custo ou de
comportamento sai de um contador ou de um `chrono` no próprio demo**. Quando a
medição contradiz o texto, o texto é reescrito — não a medição.

Isso aconteceu com frequência suficiente para virar conteúdo. Os casos da Fase 4:

| Onde | O que eu tinha escrito | O que a medição mostrou |
|---|---|---|
| `performance` 4.1 | o vetor ordenado é ~3× mais rápido | **1.0×** em `-O1`+ — o GCC gera `cmov`; o efeito só existe em `-O0` |
| `performance` 1.1 | o tempo estabiliza depois do passo 16 | **cai** em 256/512 — o *wrap-around* do meu benchmark encolheu o conjunto até caber em L3 |
| `performance` 2.2 | usando todos os campos, o AoS ganha | **SoA ganhou 1.6×** — `sizeof(Particula)` é 40, e 64/40 = 1 |
| `performance` 2.1 | a razão de tempo acompanha a de bytes | bytes **10×**, tempo **1.5×** — o prefetcher esconde a diferença |
| `type_erasure` 2.3 | o SBO da `std::function` cabe 16 bytes | confirmado, mas só depois de instrumentar `operator new` |
| `concurrency2` 3.4 | `seq_cst` é mais caro que `relaxed` | **1.01×** no x86-64 sem contenção |
| `concurrency2` 4.1 | um laço de CAS mostra retrabalho | as duas primeiras versões deram **0%** — as threads não disputavam |
| `linkage` 1.2 | uma tabela de custo de `#include` | cinco dos seis números estavam errados; `<cstddef>` por um fator de 4 |
| `undefined_behavior` 3.2 | o ASan pega use-after-free | pega em `-O0` e **não pega** em `-O1`; e o UBSan do GCC não pega desalinhamento (o do clang pega) |
| `cmake` 4.2 (1ª versão) | OpenMP dá speedup no SAXPY | **0.32×** — a versão paralela saiu 3× mais LENTA; faltava aquecimento do pool de threads |
| `cmake` 4.2 (2ª versão) | corrigido com aquecimento | **1.03×** — o reset do vetor (80 MB) estava DENTRO da região cronometrada |
| `cmake` 4.2 (3ª versão) | um speedup só | **3.09× → 1.20×** conforme os dados saem do cache; a queda É a lição |

Três bugs foram introduzidos por mim e encontrados pela própria execução:

- a fila SPSC de `concurrency2` perdia dados porque o consumidor usava
  `stop_token` — e `~jthread()` pede a parada **antes** do `join()`, com a fila
  ainda cheia. É a armadilha que o §1.3 do mesmo subprojeto descreve.
- `demo_abi.hpp` de `linkage` deu `multiple definition` ao ser incluído por
  duas TUs. É o erro que o §1.4 do mesmo subprojeto descreve.
- o `ON_CALL` genérico de `test_mock.cpp` foi declarado **depois** do
  específico, e o gmock casa regras da mais recente para a mais antiga.

Os três viraram seção documentada no fonte e no README correspondente.

---

## Além do roadmap

O que ficou de fora, e por quê:

| Tema | Situação |
|---|---|
| `std::execution` / senders-receivers | C++26; nem o GCC 15 tem |
| reflexão estática | C++26 |
| contratos (`pre`/`post`) | C++26 |
| `std::simd` | C++26; hoje é `<experimental/simd>` ou intrínsecos |
| MPI e CUDA compilados | escritos e integrados ao CMake em `src/cmake/`, mas **não compilados** nesta máquina: não há `mpicxx` nem `nvcc`. `sudo apt install libopenmpi-dev nvidia-cuda-toolkit` habilita os dois alvos |
| `std::mdspan`, `std::generator`, `flat_map` | C++23, mas exigem GCC 14/15 — inventariados em `src/cpp23/` |
| alocadores customizados avançados | parcialmente em `src/memory/` (`std::pmr`) |
| networking | fora do padrão; há Asio em `src/coroutines/` |

Os subprojetos de ecossistema que seguem comentados em `src/meson.build`
(Qt, SFML, Bullet, ONNX, JSBSim, AWS) dependem de bibliotecas pesadas e não
fazem parte do currículo de linguagem.

---

## O que os alvos desabilitados escondiam

Habilitar os 23 `subdir()` comentados revelou **seis bugs**, todos latentes há meses. Nenhum
deles era detectável sem construir o alvo — e é por isso que hoje não há nenhum `subdir()`
comentado em `src/meson.build`.

| Onde | Bug | Como se manifestava |
|---|---|---|
| `src/aws/s3/main.cpp` | faltava `#include <fmt/core.h>` | `error: 'fmt' has not been declared` |
| `src/asm_course/07_calling_asm.s` | `subq $8, %rsp` **quebrava** o alinhamento de 16 B exigido no `CALL` | **SIGSEGV dentro do fmt**, 4 quadros acima — a marca do bug de alinhamento: estoura em quem usa SSE, não em quem o quebrou |
| `src/asm_course/10_string_ops.cpp` | `RDI/RSI/RCX` declarados como entrada pura, mas `rep movsb` os modifica | o GCC confiou na promessa e reusou registrador corrompido → **SIGSEGV na linha seguinte ao `asm`** |
| `src/http-client/main.cpp` | `std::string(n, '─')` — U+2500 tem 3 bytes; entre aspas simples vira *multi-character constant* truncada. E `40 - title.size()` é aritmética **sem sinal** | separador com bytes inválidos; e um título de 41 caracteres pediria 16 exabytes |
| `src/rabbitmq/{consumer,producer}.cpp` | a API do AMQP-CPP mudou: os callbacks de ciclo de vida são métodos virtuais do **handler**, não do `TcpConnection` | não compilava |
| `src/database/main.cpp` | o demo não era idempotente | `UNIQUE constraint failed` na **segunda** execução |

Antes destes, a Fase 3a já havia encontrado três `#include` faltando pela mesma razão
(`solid/srp`, `concepts`, `file_io`).

**Nove bugs, todos escondidos atrás de um `#`.** A política que este repositório adota:

> Se um subprojeto não pode ser construído, o problema é dele — conserte-o ou apague-o.
> Não o esconda atrás de um comentário.

Os dois bugs de ABI viraram material didático em `src/asm_course/README.md`: eles ensinam
**por que** o System V ABI existe melhor que qualquer explicação teórica.
