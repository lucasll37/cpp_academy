# =============================================================================
#  src/testing/meson.build
#
#  Dois tipos de alvo aqui:
#
#    1. `testing`      — o executável DIDÁTICO, que narra o assunto.
#    2. test_*.cpp     — binários de teste DE VERDADE, cada um registrado com
#                        `test()`. Rode com `meson test -C build`.
#
#  ⚠ Este subprojeto NÃO reativa o `tests/` da raiz, que segue desabilitado
#    por decisão de projeto. Tudo aqui é auto-contido.
#
#  `gtest_dep` vem do meson.build da raiz:
#      gtest_dep = dependency('gtest', fallback: ['gtest', 'gtest_dep'])
#  e traz gtest + gtest_main. Para gmock é preciso pedir separado.
# =============================================================================

# ⚠ `gtest_dep` da raiz traz APENAS `libgtest.a` — sem o `main()`. Linkar só
#   com ele produz:
#       undefined reference to `main'
#   O GoogleTest empacota o `main()` padrão à parte, e o Conan expõe os quatro
#   como pacotes pkg-config separados:
#
#     gtest       asserções e o runner            (sem main)
#     gtest_main  só o main() padrão              (linkar JUNTO com gtest)
#     gmock       matchers e MOCK_METHOD          (sem main)
#     gmock_main  main() que chama InitGoogleMock
#
#   A separação existe para quem escreve o próprio `main()` — é o caso de
#   src/meson/test_saudacao.cpp. Aqui usamos o pronto.
gtest_main_dep = dependency('gtest_main')
gmock_dep      = dependency('gmock',      required: false)
gmock_main_dep = dependency('gmock_main', required: false)

# ── A biblioteca sob teste ───────────────────────────────────────────────────
#  Compilada UMA vez e reusada pelos cinco binários de teste. Sem isto,
#  carrinho.cpp seria compilado seis vezes.
carrinho_lib = static_library('carrinho', ['carrinho.cpp'])

carrinho_dep = declare_dependency(
    link_with:           carrinho_lib,
    include_directories: include_directories('.'),
)

# ── 1. O executável didático ─────────────────────────────────────────────────
executable('testing',
    ['main.cpp'],
    dependencies: [fmt_dep],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)

# ── 2. Os testes de verdade ──────────────────────────────────────────────────
#  Cada arquivo vira um binário e um `test()`. Separá-los assim significa que
#  `meson test` mostra qual GRUPO falhou antes mesmo de olhar a saída — e que
#  `--gtest_filter` raramente é necessário.
testes_com_gtest = {
    'basico':         'test_basico.cpp',
    'fixture':        'test_fixture.cpp',
    'parametrizado':  'test_parametrizado.cpp',
    'death':          'test_death.cpp',
}

foreach nome, fonte : testes_com_gtest
    exe = executable('testing_' + nome, [fonte],
        dependencies: [carrinho_dep, gtest_dep, gtest_main_dep],
        install: false,          # binários de teste não vão para dist/bin
    )
    # `protocol: 'gtest'` faz o Meson entender a saída do GoogleTest e
    # reportar caso a caso, em vez de só "passou/falhou" o binário inteiro.
    test('testing_' + nome, exe, protocol: 'gtest', timeout: 60)
endforeach

# gmock é um alvo separado porque precisa da própria dependência.
if gmock_dep.found() and gmock_main_dep.found()
    exe_mock = executable('testing_mock', ['test_mock.cpp'],
        dependencies: [carrinho_dep, gtest_dep, gmock_dep, gmock_main_dep],
        install: false,
    )
    test('testing_mock', exe_mock, protocol: 'gtest', timeout: 60)
else
    warning('gmock não encontrado — o alvo `testing_mock` foi desabilitado.')
    warning('Ele vem junto do pacote gtest do Conan; rode `make deps`.')
endif
