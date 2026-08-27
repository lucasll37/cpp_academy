# =============================================================================
#  src/undefined_behavior/meson.build
#
#  Dois alvos, e a distinção importa:
#
#    undefined_behavior  — a NARRATIVA. Não executa UB. Instalado.
#    ub_exemplos         — contém UB DE PROPÓSITO, para rodar sob sanitizer.
#                          NÃO instalado, e cada caso exige um argumento.
# =============================================================================

executable('undefined_behavior',
    ['main.cpp'],
    dependencies: [fmt_dep],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)

# ⚠ Este alvo executa comportamento indefinido quando invocado com um caso.
#   `install: false` é deliberado: ele não deve acabar em dist/bin junto dos
#   demais. Compile-o à mão com sanitizer para ver os diagnósticos — as
#   instruções estão no topo de ub_exemplos.cpp e na seção 3.4 do demo.
#
#   `-O0` fixo aqui porque, como a seção 2.2 mede, o otimizador ELIMINA parte
#   do UB antes de o sanitizer poder instrumentá-lo.
executable('ub_exemplos',
    ['ub_exemplos.cpp'],
    cpp_args: ['-O0'],
    install: false,
)
