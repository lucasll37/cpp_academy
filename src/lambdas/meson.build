# Lambdas sem captura como argumento de template em contexto não avaliado
# (demo_padroes, seção 5.5) exigem C++20+; usamos c++23 por consistência com
# os demais subprojetos do Tier 2. O padrão da raiz continua c++20.
executable('lambdas',
    ['main.cpp'],
    dependencies: [fmt_dep],
    override_options: ['cpp_std=c++23'],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
