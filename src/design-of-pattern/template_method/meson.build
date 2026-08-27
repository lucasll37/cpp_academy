template_method_sources = files('pipeline.cpp')
template_method_inc = include_directories('.')

executable('template_method',
    ['main.cpp'] + template_method_sources,
    dependencies: [fmt_dep],
    include_directories: template_method_inc,
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
