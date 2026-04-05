project('academy-python', 'cpp',
    version: '1.0.0',
    default_options: ['cpp_std=c++17'],
)

omp_dep = dependency('openmp')
# pybind_dep = dependency('pybind11')

py  = import('python').find_installation('python3')

py.extension_module('academy_cp',
    '../academy_cpython.cpp',
    dependencies: [py.dependency(), omp_dep],
    install: true
)


# py.extension_module('academy_pb',
#     '../academy_pybind.cpp',
#     dependencies: [py.dependency(), pybind_dep, omp_dep],
#     install: false,
# )