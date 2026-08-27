# =============================================================================
#  src/doxygen/meson.build
# =============================================================================
#
#  Targets:
#    doxygen        — executável de demonstração (exercita a API)
#    docs           — gera a documentação HTML via Doxygen
#
#  Uso:
#    make build                → compila o executável (dist/bin/doxygen_demo)
#    ninja -C build docs       → gera build/src/doxygen/docs/html/index.html
#
# =============================================================================

# ── Executável de demonstração ────────────────────────────────────────────────
#
#  Nota histórica: este alvo já carregou `cpp_args: ['-Wno-comment']`, porque o
#  @mainpage de main.cpp exibia `/**` literal DENTRO de um comentário `/* */`.
#  Comentários C não aninham: o GCC avisava "'/*' within comment" e o Doxygen
#  avisava "Reached end of file while still inside a (nested) comment".
#  O exemplo foi reescrito como tabela e a supressão saiu junto — regra geral:
#  desligar um aviso é sempre a segunda melhor opção; a primeira é remover a
#  causa.
executable('doxygen_demo',
    files('main.cpp'),
    dependencies: [fmt_dep],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)

# ── Documentação com Doxygen ──────────────────────────────────────────────────
#
#  O target `docs` usa o programa `doxygen` (declarado como tool_requires em
#  conanfile.py) para gerar documentação HTML a partir dos comentários no
#  código-fonte.
#
#  O Doxyfile.in é um TEMPLATE: contém @SOURCE_ROOT@ e @BUILD_ROOT@, que só são
#  conhecidos durante o `meson setup`. O configure_file() abaixo substitui os
#  dois e escreve o Doxyfile real em build/src/doxygen/Doxyfile.
#
#  Saída: build/src/doxygen/docs/html/index.html


doxyfile = configure_file(
    input:         'Doxyfile.in',
    output:        'Doxyfile',
    configuration: {
        'SOURCE_ROOT': meson.project_source_root(),
        'BUILD_ROOT':  meson.current_build_dir(),
    }
)

doxygen_prog = find_program('doxygen', required: false)

if doxygen_prog.found()
    docs_target = custom_target('docs',
        input:           doxyfile,
        # O Doxyfile tem OUTPUT_DIRECTORY = @BUILD_ROOT@/docs, ou seja, o
        # Doxygen cria build/src/doxygen/docs/. O `output:` declarado tem de
        # ser exatamente esse nome, senão o Ninja procura um artefato que nunca
        # aparece e re-executa o alvo em TODA build.
        output:          'docs',
        command:         [doxygen_prog, '@INPUT@'],
        build_by_default: false,
        install:          false,
    )

    # Alias conveniente: `ninja docs` em vez de `ninja src/doxygen/docs`
    alias_target('docs', docs_target)

else
    warning('doxygen não encontrado — target `docs` desabilitado.')
    warning('Instale com: pip install conan && conan install . --build=missing')
endif