proxy_sources = files('proxies.cpp')
proxy_inc = include_directories('.')

executable('proxy',
    ['main.cpp'] + proxy_sources,
    dependencies: [fmt_dep],
    include_directories: proxy_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
