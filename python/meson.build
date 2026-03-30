py = import('python').find_installation('python3')

py.extension_module('academy',
    'academy_module.cpp',
    dependencies: [py.dependency(), omp_dep],
    install: true,
)