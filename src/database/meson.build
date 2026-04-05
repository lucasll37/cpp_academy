soci_dep        = dependency('soci')
soci_sqlite_dep = dependency('soci-soci_sqlite3')
soci_pg_dep = dependency('soci-soci_postgresql')

executable('database',
    files('main.cpp', 'db.cpp'),
    dependencies: [fmt_dep, soci_dep, soci_sqlite_dep, soci_pg_dep],
    include_directories: include_directories('.'),
    install: true
)