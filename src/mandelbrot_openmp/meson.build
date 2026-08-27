# ============================================================
# src/mandelbrot/meson.build
#
# Subprojeto: Mandelbrot Zoom com OpenMP
#
# Conceitos demonstrados:
#   - #pragma omp parallel for  (paralelismo de loop)
#   - schedule(dynamic, N)      (balanceamento de carga)
#   - num_threads(N)            (controle em tempo de execução)
#   - Ausência de mutex: offsets disjuntos → sem corrida
#   - Visualização interativa com Qt6
# ============================================================

moc_files = qt6.compile_moc(
    headers: [
        'mainwindow.hpp',
    ],
    include_directories: include_directories('.'),
    dependencies: qt_dep,
)

executable('mandelbrot_openmp',
    [
        'main.cpp',
        'mainwindow.cpp',
        'mandelbrot_renderer.cpp',
        moc_files,
    ],
    dependencies: [qt_dep, omp_dep],
    include_directories: include_directories('.'),
    install: true,
)
