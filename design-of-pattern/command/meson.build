command_sources = files(
    'editor.cpp',
    'insert_command.cpp',
    'delete_command.cpp',
    'replace_command.cpp',
    'history.cpp',
)

command_inc = include_directories('.')

executable('command',
    ['main.cpp'] + command_sources,
    dependencies: [fmt_dep],
    include_directories: command_inc,
)