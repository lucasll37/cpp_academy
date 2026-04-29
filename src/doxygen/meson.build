# =============================================================================
#  src/doxygen/meson.build
# =============================================================================
#
#  Targets:
#    doxygen        — executável de demonstração (exercita a API)
#    docs           — gera a documentação HTML via Doxygen
#
#  Uso:
#    make build              → compila o executável
#    cd build && ninja docs  → gera docs/html/index.html
#
# =============================================================================

# ── Executável de demonstração ────────────────────────────────────────────────
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
#  Saída: ./docs/html/index.html


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
        output:          'html',
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