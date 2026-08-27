# Este subprojeto usa std::expected, que é C++23. O padrão da raiz
# (cpp_std=c++20, em ../../meson.build) fica intacto — a exceção é local.
# GCC 13 / libstdc++ 13 já implementam <expected>.
executable('errors',
    ['main.cpp'],
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
