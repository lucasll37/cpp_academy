# As operações monádicas de std::optional (and_then/transform/or_else) são
# C++23. O padrão da raiz continua c++20 — a exceção é local a este alvo.
executable('vocabulary_types',
    ['main.cpp'],
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
