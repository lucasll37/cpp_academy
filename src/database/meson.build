# =============================================================================
#  src/database/meson.build
#
#  ⚠ O backend PostgreSQL é OPCIONAL, e por dois motivos independentes:
#
#    1. o `.pc` que o Conan gera para `soci-soci_postgresql` traz um
#       `Requires: soci-soci_core none` malformado — o `none` é um bug do
#       gerador — e o pkg-config o rejeita;
#    2. mesmo achando o pacote, o cabeçalho `soci-postgresql.h` inclui
#       `libpq-fe.h`, que exige `libpq-dev` instalado no sistema.
#
#  Um subprojeto didático não deve exigir um servidor de banco para compilar.
#  O SQLite é embutido e ensina as mesmas coisas: sessão, prepared statement,
#  transação, binding e mapeamento de tipos.
#
#  Para habilitar o PostgreSQL:  sudo apt install libpq-dev  (e reconfigure)
# =============================================================================

# `cc.has_header` PERGUNTA ao compilador em vez de presumir — é o mesmo idioma
# de `check_language(CUDA)` no CMake (ver src/cmake/, seção 3).
cc_db = meson.get_compiler('cpp')
tem_pg = soci_pg_dep.found() and cc_db.has_header('libpq-fe.h')

if tem_pg
    message('database: backend PostgreSQL HABILITADO')
else
    message('database: só SQLite (falta libpq-dev ou o .pc do backend)')
endif

executable('database',
    files('main.cpp', 'db.cpp'),
    dependencies: [fmt_dep, soci_dep, soci_sqlite_dep] +
                  (tem_pg ? [soci_pg_dep] : []),
    cpp_args: tem_pg ? ['-DTEM_POSTGRESQL=1'] : ['-DTEM_POSTGRESQL=0'],
    include_directories: include_directories('.'),
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
