observer_sources = files(
    'event_bus.cpp',
    'logger.cpp',
    'monitor.cpp',
)

observer_inc = include_directories('.')

executable('observer',
    ['main.cpp'] + observer_sources,
    dependencies: [fmt_dep],
    include_directories: observer_inc,
)