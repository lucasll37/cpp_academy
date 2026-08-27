flyweight_sources = files('flyweight.cpp')
flyweight_inc = include_directories('.')

executable('flyweight',
    ['main.cpp'] + flyweight_sources,
    dependencies: [fmt_dep],
    include_directories: flyweight_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
