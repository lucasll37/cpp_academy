# ============================================================
# src/mandelbrot/meson.build
#
# Subprojeto: Mandelbrot Zoom com std::mutex + std::thread
#
# Conceitos demonstrados:
#   - Thread pool manual com std::thread
#   - std::mutex  (proteção do buffer de pixels compartilhado)
#   - std::condition_variable (sinalização de conclusão de tile)
#   - std::lock_guard / std::unique_lock
#   - Visualização interativa com Qt6 (QLabel + QImage)
#   - Zoom (scroll) e pan (drag) em tempo real
#
# Não usa .ui (sem uic) — todo o layout é programático.
# ============================================================

moc_files = qt6.compile_moc(
    headers: [
        'mainwindow.hpp',
    ],
    include_directories: include_directories('.'),
    dependencies: qt_dep,
)

executable('mandelbrot',
    [
        'main.cpp',
        'mainwindow.cpp',
        'mandelbrot_renderer.cpp',
        moc_files,
    ],
    dependencies: [qt_dep],
    include_directories: include_directories('.'),
    install: true,
)
