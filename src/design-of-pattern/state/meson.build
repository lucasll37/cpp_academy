state_sources = files(
    'tcp_state.cpp',
    'tcp_connection.cpp',
    'tcp_states.cpp',
)

state_inc = include_directories('.')

executable('state',
    ['main.cpp'] + state_sources,
    dependencies: [fmt_dep],
    include_directories: state_inc,
)
