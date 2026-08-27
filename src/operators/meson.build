# Subscript multidimensional (`m[i,j,k]`) e `static operator()` são C++23.
# O padrão da raiz continua c++20 — a exceção é local a este alvo.
executable('operators',
    ['main.cpp'],
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
