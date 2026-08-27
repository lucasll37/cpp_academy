# `std::byteswap` (demo_endianness, 5.3) é C++23; o `<bit>` é C++20.
# O padrão da raiz continua c++20 — a exceção é local a este alvo.
executable('bits',
    ['main.cpp'],
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
