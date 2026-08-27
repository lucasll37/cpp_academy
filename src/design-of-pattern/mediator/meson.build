mediator_sources = files('formulario.cpp')
mediator_inc = include_directories('.')

executable('mediator',
    ['main.cpp'] + mediator_sources,
    dependencies: [fmt_dep],
    include_directories: mediator_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
