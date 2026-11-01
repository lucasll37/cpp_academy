# meson — O Build System deste Repositório

## O que é este projeto?

Um guia de **Meson** ancorado no build real do `cpp_academy`: cada conceito é explicado e
depois apontado para o arquivo onde ele já está em uso — a raiz, o `proto/`, o `doxygen/`,
o `plugin_registry/`.

Junto vem um **exemplo vivo**: o [meson.build](meson.build) desta pasta exercita os
recursos principais, e o executável `meson_demo` imprime em runtime tudo o que o build
injetou nele. É o mesmo material duas vezes — uma para ler, outra para rodar e mexer.

```bash
make build && ./dist/bin/meson_demo
```

---

## Os três programas envolvidos

| Programa | Papel | Quando roda |
|---|---|---|
| **Conan** | baixa e compila as dependências (fmt, gtest, protobuf, OpenCV…) e gera os `.pc` | `make configure` |
| **Meson** | lê os `meson.build`, resolve tudo e **gera arquivos de build** | `meson setup` / `meson configure` |
| **Ninja** | executa o build gerado, em paralelo e incrementalmente | `meson compile` |

O Meson **não compila nada**. Ele é um gerador: transforma a descrição declarativa em
`build.ninja`. Quem chama o `g++` é o Ninja. Entender essa divisão explica quase todo
comportamento estranho — inclusive por que existe "configuração" e "build" como duas fases.

---

## Os dois tempos

Esta é a ideia que organiza tudo o mais:

| | Tempo de **configuração** | Tempo de **build** |
|---|---|---|
| Quem roda | `meson setup` / `meson configure` | `ninja` (via `meson compile`) |
| Frequência | uma vez, ou quando um `meson.build` muda | a cada compilação |
| O que acontece | procura compilador, resolve `dependency()`, avalia `if`/`foreach`, escreve `configure_file()` | compila, linka, roda `custom_target` |
| O que existe | as *variáveis* do Meson (`fmt_dep`, `qtd_itens`…) | os *arquivos* (`.o`, `.a`, executáveis) |

No exemplo desta pasta os dois aparecem lado a lado sobre o **mesmo** arquivo:
`fs.read('tabela.txt')` conta as linhas na **configuração**, e o `custom_target` gera o
header a partir dele no **build**. O programa imprime os dois números e eles batem.

E o Meson se reconfigura sozinho quando precisa: acrescente uma linha em
[tabela.txt](tabela.txt), rode `meson compile` e o contador muda — sem nenhum `meson setup`
manual, porque `fs.read()` registrou o arquivo como dependência de configuração.

---

## O ciclo de vida

Quatro comandos, **num projeto Meson qualquer** — como o `/tmp/oi` do exemplo mínimo, sem
Conan e sem dependência externa:

```bash
meson setup build .          # configura: fonte "." → build dir "build"
meson compile -C build       # compila (chama o Ninja)
meson test -C build          # roda os alvos registrados com test()
meson install -C build       # copia para o prefix
```

> ⚠ **Copiar a primeira linha para dentro DESTE repositório não funciona.** Aqui o `fmt` vem
> do Conan, e um `meson setup build .` puro morre em:
>
> ```
> ERROR: Dependency "fmt" not found, tried pkgconfig and cmake
> ```
>
> Faltam duas coisas: o ambiente do Conan e o caminho dos `.pc`. A variante correta **neste
> repositório** é sempre:
>
> ```bash
> source build/conanbuild.sh                                    # 1. ambiente do Conan
> meson setup --reconfigure -Dpkg_config_path=$PWD/build build/ .   # 2. onde achar os .pc
> ```
>
> E, na prática, você não digita nem isso: digita `make configure`, que faz os dois.

Neste repositório o [Makefile](../../Makefile) embrulha esses quatro passos e acrescenta o
Conan. `make configure` faz, em ordem:

```bash
conan install ./ --build=missing --settings=build_type=Debug ...   # 1. dependências → build/*.pc
source build/conanbuild.sh                                          # 2. PATH/env do Conan
meson setup --reconfigure --backend ninja \
    --buildtype=debug \
    --prefix=$(PWD)/dist --libdir=$(PWD)/dist/lib \
    -Dpkg_config_path=$(PWD)/build \                                # 3. onde achar os .pc
    build/ .
```

O `-Dpkg_config_path=build` é a costura entre Conan e Meson: os `.pc` que o Conan gerou
ficam em `build/`, e é ali que o `dependency()` vai procurar.

| Comando | Faz |
|---|---|
| `make configure` | Conan + `meson setup` |
| `make build` | `meson compile -C build` |
| `make install` | `meson install -C build` → `dist/` |
| `make test` | `meson test` com saída verbosa |
| `make clean` | apaga `build/` e `dist/` |

**Regra de ouro do build dir**: ele é 100% descartável e nunca vai para o Git. Se algo
ficou inexplicável, `rm -rf build && make configure` resolve — e não perde nada.

---

## Anatomia do `meson.build` da raiz

O [meson.build](../../meson.build) da raiz tem quatro blocos, nesta ordem obrigatória:

```meson
project('academy', 'cpp', 'c',                 # 1. nome + linguagens
    version: '1.0.0',
    default_options: [                          #    padrões para o projeto todo
        'cpp_std=c++20',
        'warning_level=3',                      #    = -Wall -Wextra -Wpedantic
    ],
)

fmt_dep   = dependency('fmt')                   # 2. dependências, uma vez só...
gtest_dep = dependency('gtest', fallback: ['gtest', 'gtest_dep'])
qt6       = import('qt6')                       #    ...e módulos

inc = include_directories('include')            # 3. caminhos

subdir('proto')                                 # 4. e só então os subdiretórios
subdir('src')
```

Três consequências práticas desse arranjo:

1. **`project()` é a primeira linha executável.** Só existe na raiz (e em `subprojects/`).
   Nenhum `src/*/meson.build` tem um — eles são *incluídos*, não são projetos.
2. **`subdir()` não cria escopo novo.** É inclusão textual: `fmt_dep` declarado na raiz
   está visível em `src/syntax/meson.build` sem nenhum `import`. (As demais dependências
   são declaradas no `meson.build` de cada subprojeto, para que comentar um `subdir()`
   também deixe de exigir o que ele usa.) É por isso que a ordem
   importa — `subdir('proto')` vem antes de `subdir('src')` porque o `src/` consome o que
   o `proto/` gera.
3. **`default_options` é padrão, não imposição.** Um alvo pode discordar com
   `override_options` (ver abaixo).

---

## Alvos

### `executable()` — o caso comum

O padrão que todo subprojeto deste repositório segue:

```meson
executable('syntax',                              # nome do binário
    ['main.cpp'],                                 # fontes
    dependencies: [fmt_dep],                      # o que precisa
    install: true,
    install_dir: get_option('prefix') / 'bin',    # → dist/bin/syntax
)
```

Argumentos que aparecem no repositório:

| Argumento | Para quê | Onde ver |
|---|---|---|
| `dependencies` | dependências empacotadas (flags + link + includes) | todos |
| `cpp_args` | flags só deste alvo | [coroutines](../coroutines/meson.build), [doxygen](../doxygen/meson.build) |
| `link_args` | flags de link só deste alvo (`-ldl`) | [plugin_registry](../plugin_registry/meson.build) |
| `override_options` | discordar do `default_options` da raiz | [lambdas](../lambdas/meson.build) (`cpp_std=c++23`) |
| `include_directories` | caminhos de header extras | [plugin_registry](../plugin_registry/meson.build) |
| `install` / `install_dir` | se e onde instalar | todos |

**`files()` vs string**: `files('main.cpp')` devolve um objeto de arquivo com caminho
resolvido, que pode ser passado adiante entre `meson.build`. A string `'main.cpp'` é
relativa ao diretório atual e não viaja. Prefira `files()`.

### Bibliotecas

**Antes das três funções, a diferença que elas escolhem.** Uma biblioteca é um monte de código
já compilado. A pergunta é **quando** ele entra no seu programa:

| | Estática — `.a` | Dinâmica — `.so` |
|---|---|---|
| quando o código entra no programa | **no link**, copiado para dentro do executável | **ao iniciar o processo**, carregado pelo sistema |
| o executável | maior; contém a biblioteca | menor; contém só uma referência a ela |
| precisa do arquivo em runtime? | **não** — pode apagar o `.a` | **sim** — sumiu o `.so`, o programa não abre |
| atualizar a biblioteca | é preciso **relinkar** tudo | basta trocar o `.so` |
| ver as dependências | não aparecem | `ldd ./programa` |
| o que é um `.a` | um **arquivo compactado de `.o`** (literalmente: `ar` é o mesmo formato do `tar`) | um executável sem `main()`, relocável |

O `.a` deste exemplo, e o símbolo dentro dele:

```bash
$ ls -la build/src/meson/libsaudacao.a
-rw-r--r-- 1 lima lima 2144 build/src/meson/libsaudacao.a

$ nm -C build/src/meson/libsaudacao.a | grep cumprimentar
0000000000000000 T saudacao::cumprimentar(std::__cxx11::basic_string<...> const&)
```

O `T` quer dizer *text*: símbolo **definido**, na seção de código, com linkage externa. É esse
`T` que o linker procura ao resolver a chamada feita em `main.cpp`. Fosse `U` (*undefined*),
seria uma chamada ainda por resolver.

**Quando escolher qual:** estática para um executável que você distribui e quer autocontido, e
para tudo o que é interno ao próprio build (é o caso desta `libsaudacao.a`, que nem chega a ser
instalada); dinâmica quando várias coisas compartilham o mesmo código, ou quando ele é
carregado **em runtime** — plugins.

As três funções do Meson:

```meson
static_library('saudacao', files('saudacao.cpp'), dependencies: [fmt_dep])
shared_library('pid',      files('pid_controller.cpp'), name_prefix: '')
library('x', ...)   # decide por -Ddefault_library=static|shared|both
```

Use `library()` quando **não** quiser decidir: quem consome o seu projeto escolhe com
`-Ddefault_library=`. Use `static_library`/`shared_library` quando a escolha for parte do
desenho, como nos dois casos acima.

O [plugins/meson.build](../plugin_registry/plugins/meson.build) mostra o caso interessante:
`name_prefix: ''` para o `.so` sair como `pid.so` (e não `libpid.so`) porque quem o abre é um
**`dlopen`** — a função que carrega uma biblioteca dinâmica **durante a execução**, pelo nome
do arquivo, em vez de na hora do link; é assim que se faz um sistema de plugins, e o nome do
arquivo passa a ser parte do contrato. Junto vem **`-fvisibility=hidden`**, que torna
*invisível de fora do `.so`* todo símbolo não marcado explicitamente como exportado — de modo
que só as funções `extern "C"` da API do plugin fiquem alcançáveis pelo `dlsym`. Sem essa
flag, cada `.so` publica milhares de símbolos internos, o carregamento fica lento e dois
plugins podem colidir.

### `declare_dependency()` — o que consumidores enxergam

Este é o recurso que separa um build limpo de um cheio de repetição:

```meson
lib_saudacao = static_library('saudacao', files('saudacao.cpp'), dependencies: [fmt_dep])

saudacao_dep = declare_dependency(
    link_with:           lib_saudacao,
    include_directories: include_directories('.'),
    dependencies:        [fmt_dep],      # transitiva: quem usa a lib também ganha fmt
)
```

Sem ele, cada consumidor repete três argumentos e erra um deles cedo ou tarde. Com ele,
consumir é uma linha — e é exatamente a mesma sintaxe de uma dependência externa:

```meson
executable('meson_demo', files('main.cpp'), dependencies: [saudacao_dep])
executable('test_saudacao', files('test_saudacao.cpp'), dependencies: [saudacao_dep, gtest_dep])
```

---

## Dependências

```meson
fmt_dep   = dependency('fmt')                                   # obrigatória
gtest_dep = dependency('gtest', fallback: ['gtest', 'gtest_dep'])  # com fallback p/ wrap
opt_dep   = dependency('libfoo', required: false)               # opcional
qt_dep    = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'])  # com módulos
```

O `dependency()` procura, nesta ordem: pkg-config (os `.pc` em `build/`, graças ao
`-Dpkg_config_path`), depois os métodos nativos do sistema, e por fim o `fallback` — um
subprojeto em `subprojects/`, baixado a partir de um `.wrap`.

**Dependência opcional** é sempre este par:

```meson
doxygen_prog = find_program('doxygen', required: false)
if doxygen_prog.found()
    custom_target('docs', ...)
else
    warning('doxygen não encontrado — target `docs` desabilitado.')
endif
```

Exatamente o que o [doxygen/meson.build](../doxygen/meson.build) faz. Sem `required: false`,
a ausência do programa aborta a configuração do projeto **inteiro**.

**Perguntar em vez de assumir** — o objeto do compilador responde:

```meson
cpp = meson.get_compiler('cpp')
cpp.get_id()               # 'gcc', 'clang', 'msvc'
cpp.version()              # '13.3.0'
cpp.has_header('sys/mman.h')
cpp.has_function('memfd_create')
cpp.compiles('...', name: 'suporte a X')
```

É assim que se escreve um build portátil: testando o ambiente, não adivinhando.

---

## Opções

Duas famílias:

**Built-in** — existem sempre, sem declarar nada:

| Opção | Valores | Efeito |
|---|---|---|
| `buildtype` | `debug`, `release`, `debugoptimized`, `minsize` | conjunto de flags |
| `optimization` | `0`, `g`, `1`, `2`, `3`, `s` | `-O…` |
| `warning_level` | `0`–`3` (aqui: **3**) | `-Wall -Wextra -Wpedantic` |
| `werror` | `true`/`false` | `-Werror` |
| `cpp_std` | `c++17`, `c++20`, `c++23`… | `-std=…` |
| `default_library` | `static`, `shared`, `both` | o que `library()` gera |
| `b_sanitize` | `address`, `undefined`, `address,undefined` | ASan/UBSan |
| `prefix`, `bindir`, `libdir` | caminhos | destino do `install` |

**Do projeto** — declaradas em [meson_options.txt](../../meson_options.txt) na raiz:

```meson
option('test_option',
    type:        'boolean',        # boolean | string | combo | array | integer | feature
    value:       false,
    description: 'Teste option for demonstration purposes.',
)
```

Lidas com `get_option('test_option')` em qualquer `meson.build` do projeto. O exemplo desta
pasta finalmente **usa** essa opção: ela vira uma macro C++ via `#mesondefine`, e o
programa imprime se está ligada.

```bash
meson configure build                        # lista TODAS as opções e seus valores
meson configure build -Dtest_option=true     # muda sem reconfigurar do zero
meson compile -C build meson_demo            # e o binário muda
meson setup build-asan -Db_sanitize=address,undefined .   # ou um build dir paralelo
```

Ter **vários build dirs ao mesmo tempo** (debug, release, asan) é o modo normal de
trabalhar em Meson — eles não se atrapalham.

---

## Arquivos gerados

### `configure_file()` — do build para dentro do C++

Roda em tempo de **configuração**. Pega um template e substitui valores:

```meson
conf = configuration_data()
conf.set_quoted('PROJETO_VERSAO', meson.project_version())   # → "1.0.0" (com aspas)
conf.set('QTD_ITENS', 6)                                     # → 6
conf.set('COM_TEST_OPTION', get_option('test_option'))       # → alimenta #mesondefine
conf.set10('TEM_DOXYGEN', doxygen_prog.found())              # → 1 ou 0

configure_file(input: 'build_info.hpp.in', output: 'build_info.hpp', configuration: conf)
```

No template ([build_info.hpp.in](build_info.hpp.in)):

| No `.in` | Vira |
|---|---|
| `@PROJETO_VERSAO@` | `"1.0.0"` |
| `@QTD_ITENS@` | `6` |
| `#mesondefine COM_TEST_OPTION` | `#define COM_TEST_OPTION` ou `/* #undef … */` |

⚠ **O `configure_file` não sabe o que é comentário.** Um `@NOME@` escrito dentro de um
comentário do template também é substituído — e, se `NOME` não existir na configuração,
o setup emite `WARNING: The variable(s) 'NOME' … are not present`. Por isso o template
deste exemplo escreve `<at>NOME<at>` ao documentar a própria sintaxe.

O [doxygen/meson.build](../doxygen/meson.build) usa a mesma função para gerar o `Doxyfile`
a partir de um `Doxyfile.in`, injetando `meson.project_source_root()`.

### `custom_target()` — do script para o código-fonte

Roda em tempo de **build**, e só quando a entrada muda:

```meson
python = find_program('python3')

tabela_gerada_h = custom_target('tabela_gerada',
    input:   'tabela.txt',
    output:  'tabela_gerada.hpp',
    command: [python, files('gerar_tabela.py'), '@INPUT@', '@OUTPUT@'],
)
```

Placeholders disponíveis no `command`:

| Placeholder | Vira |
|---|---|
| `@INPUT@` / `@INPUT0@` | os arquivos de entrada |
| `@OUTPUT@` / `@OUTPUT0@` | os de saída |
| `@OUTDIR@` | o diretório de saída |
| `@BASENAME@` | o nome da entrada sem extensão |
| `@CURRENT_SOURCE_DIR@` | o diretório deste `meson.build` |

É exatamente o padrão do [proto/meson.build](../../proto/meson.build), que roda o `protoc`
sobre `common.proto` com `@BASENAME@.pb.cc` como saída.

**A regra que evita o bug clássico**: passe o `custom_target` na lista de **fontes** do
alvo que o consome.

```meson
executable('meson_demo',
    files('main.cpp'),
    build_info_h,        # ← não é para compilar: é para ORDENAR
    tabela_gerada_h,     # ← o Ninja gera antes de compilar o main.cpp
    dependencies: [saudacao_dep],
)
```

Sem isso, o build funciona por acaso (às vezes) e falha no primeiro build limpo, com
"arquivo não encontrado" — um sintoma clássico de dependência não declarada. O Meson põe
o diretório de build atual no include path sozinho, então o C++ escreve apenas
`#include "tabela_gerada.hpp"`.

---

## Testes

```meson
test_exe = executable('test_saudacao',
    files('test_saudacao.cpp'),
    dependencies: [saudacao_dep, gtest_dep],
    install: false,                      # teste não se instala
)

test('saudacao', test_exe,
    suite:    'guia',                    # meson test --suite guia
    timeout:  30,
    protocol: 'exitcode',                # 'exitcode' | 'tap' | 'gtest' | 'rust'
)
```

```bash
meson test -C build                          # tudo
meson test -C build saudacao                 # um teste
meson test -C build --suite guia             # uma suíte
meson test -C build --print-errorlogs -v     # com a saída do binário
meson test -C build --repeat 10              # caçar flakiness
```

O `test()` só precisa de um comando que devolva **0 em sucesso**. GoogleTest é
conveniência, não requisito.

> O antigo `tests/` da raiz — que estava inteiramente comentado e nunca construiu nada — foi
> **removido**. Os testes agora moram onde são ensinados: [`src/testing/`](../testing/) (5 alvos,
> 41 casos), este subprojeto (1 alvo, com `main()` próprio) e [`src/cmake/`](../cmake/), que faz
> o mesmo com **CTest** num build separado.

---

## Instalação

```meson
install:     true,
install_dir: get_option('prefix') / 'bin',    # o `/` concatena caminhos
```

Com `--prefix=$(PWD)/dist` no `meson setup`, `meson install` produz:

```
dist/
├── bin/          ← executáveis (syntax, meson_demo, oop, ...)
├── lib/          ← bibliotecas
├── include/      ← install_headers(..., subdir: 'meson_demo')
└── plugins/      ← install_dir customizado (plugin_registry)
```

Outras funções de instalação: `install_headers()`, `install_data()`, `install_subdir()`.
Para checar antes de sujar o sistema: `meson install -C build --dry-run`.

---

## Receita: adicionar um subprojeto novo

O passo a passo real, na ordem (foi assim que `src/syntax/` e este entraram):

```bash
mkdir src/novo_topico
```

**1.** `src/novo_topico/meson.build`:

```meson
executable('novo_topico',
    files('main.cpp'),
    dependencies: [fmt_dep],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
```

**2.** Uma linha em [src/meson.build](../meson.build), em ordem alfabética:

```meson
subdir('novo_topico')
```

**3.** Reconfigure — este passo é obrigatório e é onde todo mundo tropeça:

```bash
source build/conanbuild.sh
meson setup --reconfigure build .
meson compile -C build novo_topico
```

**4.** `README.md` no subprojeto, linha no [ROADMAP](../ROADMAP.md) e no
[README da raiz](../../README.md).

Se o novo código precisar de C++23, **não** mexa na raiz:

```meson
executable('novo_topico',
    files('main.cpp'),
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],   # local a este alvo
    install: true,
)
```

---

## Armadilhas e diagnóstico

| Sintoma | Causa | Solução |
|---|---|---|
| `ERROR: Can't invoke target 'x': target not found` | alvo novo, build dir antigo | `meson setup --reconfigure build .` |
| `Dependency "fmt" not found` | `.pc` fora do caminho | `source build/conanbuild.sh` e confira `-Dpkg_config_path` |
| `fatal error: gerado.hpp: No such file` | `custom_target` fora das fontes do alvo | passe o target na lista de fontes |
| `WARNING: The variable(s) 'X' … not present` | `@X@` no template (às vezes num comentário!) | defina `X` ou não escreva o literal |
| Flag nova não fez efeito | `cpp_args` num alvo não afeta os outros | `default_options` na raiz, ou repita por alvo |
| `Current directory is not a meson build directory` | rodou `-C build` de dentro de `src/…` | rode da raiz, ou passe caminho absoluto |
| Build "por acaso" quebra depois de `make clean` | dependência não declarada | declare a ordem; nunca conte com a sorte do Ninja |

Ferramentas de inspeção:

```bash
meson introspect build --targets       # todos os alvos, em JSON
meson introspect build --dependencies  # dependências resolvidas
meson configure build                  # todas as opções e valores atuais
ninja -C build -t targets              # o que o Ninja acha que existe
ninja -C build -t commands src/meson/meson_demo   # a linha de compilação exata
cat build/meson-logs/meson-log.txt     # o log completo da configuração
cat build/meson-logs/testlog.txt       # o log completo dos testes
```

---

## Cheatsheet

```bash
# ciclo
meson setup build .                      # configurar
meson setup --reconfigure build .        # reconfigurar (alvo/opção nova)
meson setup --wipe build .               # zerar mantendo as opções
meson compile -C build                   # compilar tudo
meson compile -C build alvo1 alvo2       # compilar alvos específicos
meson compile -C build --verbose         # mostrar as linhas de comando
meson test -C build --print-errorlogs    # testar
meson install -C build                   # instalar no prefix

# opções
meson configure build                    # listar
meson configure build -Dbuildtype=release
meson setup build-asan -Db_sanitize=address,undefined .

# neste repositório
make configure && make build && make test
source build/conanbuild.sh               # antes de chamar o meson na mão
```

| Função | Quando | O que devolve |
|---|---|---|
| `project()` | 1ª linha da raiz | — |
| `executable()` / `static_library()` / `shared_library()` | build | alvo |
| `declare_dependency()` | configuração | dependência |
| `dependency()` / `find_program()` | configuração | dep / programa (`.found()`) |
| `configure_file()` | configuração | arquivo gerado |
| `custom_target()` | build | alvo gerado |
| `test()` | configuração | — |
| `subdir()` | configuração | — (inclui, sem criar escopo) |
| `run_target()` / `alias_target()` | sob demanda | alvo |
| `summary()` | configuração | — (imprime no fim do setup) |
| `message()` / `warning()` / `error()` | configuração | — |

---

## O exemplo desta pasta

```
meson/
├── meson.build          ← o exemplo vivo, comentado seção a seção
├── meson_options.txt    → não existe aqui: opções só na RAIZ do projeto
├── build_info.hpp.in    ← template do configure_file()
├── tabela.txt           ← entrada do custom_target()
├── gerar_tabela.py      ← o gerador chamado pelo Ninja
├── saudacao.hpp/.cpp    ← static_library() + declare_dependency()
├── main.cpp             ← imprime tudo o que o build injetou
└── test_saudacao.cpp    ← o alvo registrado com test()
```

```bash
make build
./dist/bin/meson_demo
```

Ou isoladamente:

```bash
source build/conanbuild.sh
meson compile -C build meson_demo
./build/src/meson/meson_demo
```

### Três experimentos

```bash
# 1. o build atravessando para dentro do C++
meson configure build -Dtest_option=true
meson compile -C build meson_demo && ./build/src/meson/meson_demo | grep TEST_OPTION

# 2. o grafo de dependências trabalhando
echo "wrap = baixa um subprojeto declarado em .wrap" >> src/meson/tabela.txt
meson compile -C build meson_demo    # o gerador roda de novo, e o contador sobe

# 3. o teste
meson test -C build saudacao --print-errorlogs
```

Saída esperada ao final: `✓ Guia do Meson: exemplo executado com sucesso!`
