# Este subprojeto é sobre C++23, então o padrão é obrigatório aqui.
# A raiz continua em c++20.
#
# Nota: `std::stacktrace` existe neste libstdc++ mas exige
# `-lstdc++_libbacktrace` para linkar. O demo documenta isso em vez de usar,
# porque a biblioteca não está presente em toda distribuição e quebraria o
# build de quem clonar o repositório.
executable('cpp23',
    ['main.cpp'],
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
