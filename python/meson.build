py = import('python').find_installation('python3')

py.extension_module('academy',
    './standalone/academy_module.cpp',
    dependencies: [py.dependency(), omp_dep],   # omp_dep vem do meson.build raiz
    install: true,
)