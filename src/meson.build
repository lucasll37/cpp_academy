# Hello World library
src_hello_world = files('hello_world.cpp')

lib_hello_world = library('hello_world',
    src_hello_world,
    dependencies: [fmt_dep],
    include_directories: inc,
    install: true
)

install_headers(meson.project_source_root() / 'include' / 'hello_world.hpp')

# Gretter library
src_greeter = files('greeter.cpp')

lib_greeter = library('greeter',
    src_greeter,
    dependencies: [fmt_dep],
    include_directories: inc,
    install: true
)

install_headers(meson.project_source_root() / 'include' / 'greeter.hpp')

# Executable
src_main = files('main.cpp')

executable('main',
    [src_main, common_proto_cc, common_proto_h],
    dependencies: [protobuf_dep, fmt_dep],
    link_with: [lib_hello_world, lib_greeter],
    include_directories: inc,
    install: true
)

# executable('main',
#     src_main,
#     link_with: [lib_hello_world, lib_greeter],
#     include_directories: inc,
#     install: true
# )