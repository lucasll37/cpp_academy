adapter_sources = files('legado.cpp', 'interface_moderna.cpp')
adapter_inc = include_directories('.')

# `std::expected` (interface_moderna.hpp) é C++23.
executable('adapter',
    ['main.cpp'] + adapter_sources,
    dependencies: [fmt_dep],
    include_directories: adapter_inc,
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
