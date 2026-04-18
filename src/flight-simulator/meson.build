jsbsim_dep = dependency('JSBSim')

flight_inc = include_directories('.', is_system: true)

flight_sources = files(
    'fdm.cpp',
    'autopilot.cpp',
    'logger.cpp',
)

executable('flight',
    ['main.cpp'] + flight_sources,
    dependencies: [jsbsim_dep, fmt_dep, spdlog_dep],
    include_directories: [flight_inc, public_headers],
    install: true
)
