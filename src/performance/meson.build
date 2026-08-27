# =============================================================================
#  src/performance/meson.build
#
#  ⚠ Este é o único subprojeto que FIXA a otimização. O padrão do Meson vem do
#    `buildtype` (debug → -O0), e a seção 4.1 mede um efeito que aparece em -O0
#    e desaparece em -O1: um benchmark compilado sem otimização não diz nada
#    sobre o binário que se entrega.
#
#    `-O2` aqui torna os números comparáveis independentemente do buildtype.
#    `-g` não custa desempenho e faz o `perf report` mostrar nomes de função.
# =============================================================================
thread_dep = dependency('threads')

executable('performance',
    ['main.cpp'],
    dependencies: [fmt_dep, thread_dep],
    cpp_args: ['-O2', '-g', '-fno-omit-frame-pointer'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
