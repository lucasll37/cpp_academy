# =============================================================================
#  src/meson/meson.build
#
#  Este arquivo É o material didático. Cada seção exercita um recurso do Meson,
#  na ordem em que ele aparece na vida real:
#
#    1. Contexto        — quem está compilando, em que máquina, com que opções
#    2. configure_file  — levar valores do build para dentro do C++
#    3. custom_target   — gerar código-fonte durante o build
#    4. static_library  — compilar uma vez, usar em vários alvos
#    5. executable      — o alvo principal
#    6. test            — registrar o binário de teste
#    7. run_target      — comandos utilitários (`ninja rodar_guia_meson`)
#    8. summary         — o quadro que o `meson setup` imprime no fim
#
#  ⚠ Este arquivo NÃO tem `project()`. Ele é incluído pelo `subdir('meson')` de
#    src/meson.build e roda dentro do projeto da raiz — por isso `fmt_dep`,
#    `gtest_dep` e as opções já existem aqui. Um `project()` só aparece na raiz
#    de um projeto ou dentro de `subprojects/`.
# =============================================================================


# ─────────────────────────────────────────────────────────────────────────────
#  1. Contexto: compilador, máquina e opções
# ─────────────────────────────────────────────────────────────────────────────
#  O objeto do compilador permite PERGUNTAR coisas em vez de assumi-las:
#  .get_id(), .version(), .has_header(), .has_function(), .compiles(), .links()
cpp = meson.get_compiler('cpp')

# `build_machine` é quem compila; `host_machine` é quem executa (diferem em
# cross-compilation). Para builds nativos os dois são a mesma coisa.
message('Compilando com @0@ @1@ em @2@/@3@'.format(
    cpp.get_id(), cpp.version(),
    build_machine.cpu_family(), build_machine.system()))

# Opções BUILT-IN — existem em todo projeto Meson, sem declarar nada.
tipo_build     = get_option('buildtype')       # debug | release | minsize | ...
padrao_cpp     = get_option('cpp_std')         # herdado do default_options da raiz
nivel_warning  = get_option('warning_level')   # 3 neste projeto (-Wall -Wextra -Wpedantic)
otimizacao     = get_option('optimization')    # 0 | g | 1 | 2 | 3 | s

# Opção DO PROJETO — declarada em meson_options.txt, na raiz.
# Ligue com: meson configure build -Dtest_option=true
com_test_option = get_option('test_option')

# Um programa opcional: `required: false` devolve um objeto que responde
# .found(), em vez de abortar a configuração. É o padrão para features opcionais.
doxygen_prog = find_program('doxygen', required: false)

# O módulo `fs` lê arquivos em tempo de CONFIGURAÇÃO (não de build).
fs = import('fs')
linhas_tabela = fs.read('tabela.txt').strip().split('\n')

qtd_itens = 0
foreach linha : linhas_tabela
    limpa = linha.strip()
    if limpa != '' and not limpa.startswith('#') and limpa.contains('=')
        qtd_itens += 1
    endif
endforeach


# ─────────────────────────────────────────────────────────────────────────────
#  2. configure_file() — o build atravessa para dentro do C++
# ─────────────────────────────────────────────────────────────────────────────
#  `configuration_data()` é um dicionário tipado. Três formas de alimentar:
#
#      conf.set('N', 42)          → @N@ vira 42            (e #mesondefine N)
#      conf.set_quoted('N', 'ab') → @N@ vira "ab"          (com as aspas!)
#      conf.set10('N', true)      → @N@ vira 1 ou 0
#
#  Um valor booleano em `set()` alimenta `#mesondefine`: vira `#define N`
#  quando true e `/* #undef N */` quando false.
conf = configuration_data()

conf.set_quoted('PROJETO_NOME',   meson.project_name())
conf.set_quoted('PROJETO_VERSAO', meson.project_version())

conf.set_quoted('BUILD_TIPO',     tipo_build)
conf.set_quoted('PADRAO_CPP',     padrao_cpp)
conf.set_quoted('NIVEL_WARNING',  nivel_warning)
conf.set_quoted('OTIMIZACAO',     otimizacao)
conf.set_quoted('PREFIX',         get_option('prefix'))

conf.set_quoted('COMPILADOR_ID',  cpp.get_id())
conf.set_quoted('COMPILADOR_VER', cpp.version())
conf.set_quoted('MAQUINA_CPU',    build_machine.cpu_family())
conf.set_quoted('MAQUINA_SO',     build_machine.system())

conf.set('QTD_ITENS_TABELA',      qtd_itens)          # número: sem aspas
conf.set('COM_TEST_OPTION',       com_test_option)    # bool: alimenta #mesondefine
conf.set10('TEM_DOXYGEN',         doxygen_prog.found())

# Gera build/src/meson/build_info.hpp a partir do template .in.
# O Meson põe o diretório de build atual no include path automaticamente —
# por isso o main.cpp escreve apenas `#include "build_info.hpp"`.
build_info_h = configure_file(
    input:         'build_info.hpp.in',
    output:        'build_info.hpp',
    configuration: conf,
)


# ─────────────────────────────────────────────────────────────────────────────
#  3. custom_target() — gerar código durante o build
# ─────────────────────────────────────────────────────────────────────────────
#  A mesma construção que proto/meson.build usa para rodar o `protoc`.
#  O Meson só substitui os placeholders e registra o nó no grafo; quem executa
#  o comando é o Ninja, e só quando `input` estiver mais novo que `output`.
#
#      @INPUT@   → os arquivos de entrada         @OUTPUT@  → os de saída
#      @OUTDIR@  → o diretório de saída           @BASENAME@ → nome sem extensão
#      @CURRENT_SOURCE_DIR@ → o dir deste meson.build
python = find_program('python3')

tabela_gerada_h = custom_target('tabela_gerada',
    input:   'tabela.txt',
    output:  'tabela_gerada.hpp',
    command: [python, files('gerar_tabela.py'), '@INPUT@', '@OUTPUT@'],

    # build_by_default: false + ser fonte de um executável = só gera quando
    # alguém precisa. Aqui deixamos true por ser o assunto do subprojeto.
    build_by_default: true,
)


# ─────────────────────────────────────────────────────────────────────────────
#  4. static_library() + declare_dependency()
# ─────────────────────────────────────────────────────────────────────────────
#  static_library / shared_library / library (decide pelo -Ddefault_library).
#  Aqui: estática, não instalada — é detalhe de implementação do exemplo.
lib_saudacao = static_library('saudacao',
    files('saudacao.cpp'),
    dependencies: [fmt_dep],
    install:      false,
)

#  `declare_dependency()` empacota TUDO o que um consumidor precisa saber:
#  o que linkar, onde procurar headers, e as dependências transitivas.
#  Consumir vira uma linha (`dependencies: [saudacao_dep]`) em vez de três
#  (link_with + include_directories + dependencies) repetidas em cada alvo.
saudacao_dep = declare_dependency(
    link_with:            lib_saudacao,
    include_directories:  include_directories('.'),
    dependencies:         [fmt_dep],
)

# Headers públicos vão para <prefix>/include/meson_demo/.
install_headers('saudacao.hpp', subdir: 'meson_demo')


# ─────────────────────────────────────────────────────────────────────────────
#  5. executable() — o alvo principal
# ─────────────────────────────────────────────────────────────────────────────
#  Os dois headers gerados entram na lista de FONTES. Não é para compilá-los:
#  é para o Ninja saber que precisa gerá-los ANTES de compilar o main.cpp.
#  Esquecer isso dá o clássico "erro só no primeiro build, some no segundo".
demo_exe = executable('meson_demo',
    files('main.cpp'),
    build_info_h,
    tabela_gerada_h,

    dependencies: [saudacao_dep],

    # Como o subprojeto `syntax`, este compila com o C++20 da raiz. Se
    # precisasse de C++23, a linha seria:
    #     override_options: ['cpp_std=c++23'],

    install:     true,
    install_dir: get_option('prefix') / 'bin',
)


# ─────────────────────────────────────────────────────────────────────────────
#  6. test() — registrar o binário no `meson test`
# ─────────────────────────────────────────────────────────────────────────────
#  O executável de teste NÃO é instalado: existe só para o `meson test`.
test_exe = executable('test_saudacao',
    files('test_saudacao.cpp'),
    dependencies: [saudacao_dep, gtest_dep],
    install:      false,
)

#  Argumentos úteis de test():
#    suite:      agrupa (`meson test --suite guia`)
#    timeout:    segundos até matar o processo (padrão 30)
#    protocol:   'exitcode' (padrão) | 'tap' | 'gtest' | 'rust'
#    is_parallel: false para testes que disputam um recurso
#    env:        variáveis de ambiente do processo de teste
test('saudacao', test_exe,
    suite:    'guia',
    timeout:  30,
    protocol: 'exitcode',
)


# ─────────────────────────────────────────────────────────────────────────────
#  7. run_target() e alias_target()
# ─────────────────────────────────────────────────────────────────────────────
#  run_target   → um comando sob demanda (`ninja -C build rodar_guia_meson`).
#                 Nunca roda sozinho; não produz arquivo que outro alvo consuma.
#  alias_target → um apelido para um conjunto de alvos já existentes.
run_target('rodar_guia_meson', command: [demo_exe])

alias_target('guia_meson', demo_exe, test_exe)


# ─────────────────────────────────────────────────────────────────────────────
#  8. summary() — o quadro impresso no fim do `meson setup`
# ─────────────────────────────────────────────────────────────────────────────
#  Vale por toda a documentação de "como foi configurado": aparece na tela e
#  fica gravado em build/meson-logs/meson-log.txt.
summary({
        'buildtype':        tipo_build,
        'cpp_std':          padrao_cpp,
        'warning_level':    nivel_warning,
        'compilador':       '@0@ @1@'.format(cpp.get_id(), cpp.version()),
        'doxygen':          doxygen_prog.found(),
        'test_option':      com_test_option,
        'itens em tabela':  qtd_itens,
    },
    section:    'src/meson — guia do Meson',
    bool_yn:    true,
)
