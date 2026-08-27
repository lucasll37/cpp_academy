decorator_sources = files(
    'file_stream.cpp',
    'compression.cpp',
    'encryption.cpp',
    'logging.cpp',
)

decorator_inc = include_directories('.')

executable('decorator',
    ['main.cpp'] + decorator_sources,
    dependencies: [fmt_dep],
    include_directories: decorator_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
