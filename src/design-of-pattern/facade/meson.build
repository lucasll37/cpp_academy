facade_sources = files('subsistemas.cpp', 'pipeline.cpp')
facade_inc = include_directories('.')

# `std::expected` (pipeline.hpp) é C++23.
executable('facade',
    ['main.cpp'] + facade_sources,
    dependencies: [fmt_dep],
    include_directories: facade_inc,
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
