iterator_inc = include_directories('.')

executable('iterator',
    ['main.cpp'],
    dependencies: [fmt_dep],
    include_directories: iterator_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
