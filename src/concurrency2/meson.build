# Todo o subprojeto é C++20: `jthread`, `latch`, `barrier`, `counting_semaphore`
# e `atomic::wait` chegaram nessa versão — que já é o padrão da raiz.
#
# `-pthread` vem de `dependency('threads')`, e não de um `-lpthread` solto: o
# flag também afeta a COMPILAÇÃO (define _REENTRANT), não só a linkagem.
thread_dep = dependency('threads')

executable('concurrency2',
    ['main.cpp'],
    dependencies: [fmt_dep, thread_dep],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
