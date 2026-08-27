composite_sources = files('nos.cpp')
composite_inc = include_directories('.')

executable('composite',
    ['main.cpp'] + composite_sources,
    dependencies: [fmt_dep],
    include_directories: composite_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
