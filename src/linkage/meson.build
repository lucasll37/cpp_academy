# =============================================================================
#  src/linkage/meson.build
#
#  Este é o único subprojeto com MÚLTIPLAS unidades de tradução — e não por
#  capricho: o assunto é justamente o que acontece quando o mesmo cabeçalho
#  é compilado mais de uma vez.
#
#    unidade_a.cpp / unidade_b.cpp  ← idênticos a menos do nome; provam a ODR
#    widget.cpp                     ← a metade escondida do pimpl
#    c_api.c                        ← compilado pelo compilador C, não pelo C++
#
#  O Meson escolhe o compilador pela EXTENSÃO. `c_api.c` vai para o `cc`
#  porque o project() da raiz declara as duas linguagens:
#      project('academy', 'cpp', 'c', ...)
#  É isso que torna a demonstração de `extern "C"` honesta: os símbolos sem
#  mangling vêm de um compilador que não sabe manglar.
# =============================================================================
executable('linkage',
    [
        'main.cpp',
        'unidade_a.cpp',
        'unidade_b.cpp',
        'widget.cpp',
        'c_api.c',          # ← .c: compilado como C
    ],
    dependencies: [fmt_dep],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
