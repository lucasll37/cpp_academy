prototype_sources = files('formas_concretas.cpp')
prototype_inc = include_directories('.')

executable('prototype',
    ['main.cpp'] + prototype_sources,
    dependencies: [fmt_dep],
    include_directories: prototype_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
