sfml_bullet2_sources = files(
    'physics_world.cpp',
    'ragdoll.cpp',
    'rope.cpp',
    'simulation.cpp',
)

sfml_bullet2_inc = include_directories('.')

executable('sfml_bullet2',
    ['main.cpp'] + sfml_bullet2_sources,
    dependencies: [fmt_dep, sfml_dep, bullet_dep],
    include_directories: sfml_bullet2_inc,
    install: true
)
