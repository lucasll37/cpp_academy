memento_sources = files('memento.cpp')
memento_inc = include_directories('.')

executable('memento',
    ['main.cpp'] + memento_sources,
    dependencies: [fmt_dep],
    include_directories: memento_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
