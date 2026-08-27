abstract_factory_sources = files('temas.cpp')
abstract_factory_inc = include_directories('.')

executable('abstract_factory',
    ['main.cpp'] + abstract_factory_sources,
    dependencies: [fmt_dep],
    include_directories: abstract_factory_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
