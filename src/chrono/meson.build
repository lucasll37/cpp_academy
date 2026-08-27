# `make_unique_for_overwrite` (demo_medicao, 5.3) é C++20; o resto do
# subprojeto usa C++23 por consistência com os demais alvos do Tier 2.
# O padrão da raiz continua c++20 — a exceção é local a este alvo.
executable('chrono',
    ['main.cpp'],
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
