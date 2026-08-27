builder_sources = files('relatorio.cpp')
builder_inc = include_directories('.')

executable('builder',
    ['main.cpp'] + builder_sources,
    dependencies: [fmt_dep],
    include_directories: builder_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
