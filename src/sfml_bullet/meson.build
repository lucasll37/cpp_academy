sfml_bullet_sources = files(
    'physics_world.cpp',
    'simulation.cpp',
)

sfml_bullet_inc = include_directories('.')

executable('sfml_bullet',
    ['main.cpp'] + sfml_bullet_sources,
    dependencies: [fmt_dep, sfml_dep, bullet_dep],
    include_directories: sfml_bullet_inc,
)
