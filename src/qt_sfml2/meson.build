ui_files = qt6.compile_ui(
  sources: ['./mainwindow.ui'],
)

moc_files = qt6.compile_moc(
  headers: [
    './mainwindow.hpp',
    './sfml_widget.hpp',
  ],
  include_directories: include_directories('.'),
  dependencies: qt_dep,
)

executable('qt_sfml2',
  [
    './main.cpp',
    './mainwindow.cpp',
    './sfml_widget.cpp',
    './physics_world.cpp',
    moc_files,
    ui_files,
  ],
  dependencies : [qt_dep, sfml_dep, bullet_dep],
  include_directories : include_directories('.'),
  install : true
)
