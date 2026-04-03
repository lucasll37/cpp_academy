chain_sources = files(
    'auth_handler.cpp',
    'rate_limit_handler.cpp',
    'validation_handler.cpp',
    'business_handler.cpp',
)

chain_inc = include_directories('.')

executable('chain',
    ['main.cpp'] + chain_sources,
    dependencies: [fmt_dep],
    include_directories: chain_inc,
)