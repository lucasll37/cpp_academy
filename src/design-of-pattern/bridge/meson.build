bridge_sources = files('bridge.cpp')
bridge_inc = include_directories('.')

executable('bridge',
    ['main.cpp'] + bridge_sources,
    dependencies: [fmt_dep],
    include_directories: bridge_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
