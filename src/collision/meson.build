# =============================================================================
#  src/collision/meson.build
#
#  ⚠ O alvo se chama `collision`, NÃO `main`.
#
#    Antes ele era `executable('main', ...)`. Como `src/sandbox/meson.build`
#    também declara um alvo `main`, os dois instalavam em `dist/bin/main` —
#    e quem chegasse depois sobrescrevia o outro. Na prática o binário desta
#    física NUNCA chegava a `dist/bin/`: o que estava lá era o do sandbox.
#
#    Nome de alvo é nome de arquivo instalado. `main` é um nome sem dono.
# =============================================================================

executable('collision',
    ['main.cpp'],
    dependencies: [fmt_dep, bullet_dep],
    include_directories: public_headers,
    install: true
)
