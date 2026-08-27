visitor_sources = files('visitantes.cpp')
visitor_inc = include_directories('.')

executable('visitor',
    ['main.cpp'] + visitor_sources,
    dependencies: [fmt_dep],
    include_directories: visitor_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
