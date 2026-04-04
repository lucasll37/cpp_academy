project('academy-python', 'cpp',
    version: '1.0.0',
    default_options: ['cpp_std=c++17'],
)

py  = import('python').find_installation('python3')
omp = dependency('openmp')

py.extension_module('academy',
    '../academy_module.cpp',
    dependencies: [py.dependency(), omp],
    install: true,
)