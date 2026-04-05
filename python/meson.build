py = import('python').find_installation('python3')

py.extension_module('academy_cp',
    './academy_cpython.cpp',
    dependencies: [py.dependency(), omp_dep],
    install: true,
)

py.extension_module('academy_pb',
    './academy_pybind.cpp',
    dependencies: [py.dependency(), pybind_dep, omp_dep],
    install: true,
)