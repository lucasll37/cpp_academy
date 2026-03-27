src = files('hello_world.cpp')

lib_academy = library('hello_world',
    src,
    include_directories: inc,
    install: true
)

install_headers(meson.project_source_root() / 'include' / 'hello_world.hpp')

executable('main',
    'main.cpp',
    link_with: lib_academy,
    include_directories: inc,
    install: true
)