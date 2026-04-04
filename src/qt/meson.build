# ============================================================
# src/qt/meson.build
#
# Três etapas automáticas do Qt que o Meson executa:
#
# 1. compile_ui   — uic processa mainwindow.ui
#                   gera ui_mainwindow.h com a classe Ui::MainWindow
#                   contendo todos os widgets do Designer
#
# 2. compile_moc  — moc processa os headers com Q_OBJECT
#                   gera código para signals/slots funcionarem
#                   em runtime (tabela de despacho de metaobjetos)
#
# 3. executable   — compila tudo junto e linka com Qt + SFML
# ============================================================

# uic: mainwindow.ui → ui_mainwindow.h
ui_files = qt6.compile_ui(
    sources: ['mainwindow.ui'],
)

# moc: processa Q_OBJECT nos headers
moc_files = qt6.compile_moc(
    headers: [
        'mainwindow.hpp',
        'sfml_widget.hpp',
    ],
    include_directories: include_directories('.'),
)

executable('qt_academy',
    [
        'main.cpp',
        'mainwindow.cpp',
        'sfml_widget.cpp',
        ui_files,    # ui_mainwindow.h gerado pelo uic
        moc_files,   # moc_mainwindow.cpp e moc_sfml_widget.cpp gerados pelo moc
    ],
    dependencies: [qt_dep, sfml_dep],
    include_directories: include_directories('.'),
)
