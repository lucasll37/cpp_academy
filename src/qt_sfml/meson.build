moc_files = qt6.compile_moc(headers: [
    'mainwindow.hpp',
    'sfml_widget.hpp',
])

executable('qt_sfml',
    [
        'main.cpp',
        'mainwindow.cpp',
        'sfml_widget.cpp',
        moc_files,
    ],
    dependencies: [qt_dep, sfml_dep, fmt_dep],
)