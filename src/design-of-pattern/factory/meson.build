factory_sources = files(
    'circle.cpp',
    'square.cpp',
)

factory_inc = include_directories('.')

executable('factory',
    ['main.cpp'] + factory_sources,
    dependencies: [fmt_dep],
    include_directories: factory_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
