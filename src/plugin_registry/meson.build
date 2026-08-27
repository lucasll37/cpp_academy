# ── src/plugin-registry/meson.build ─────────────────────────────────────────

fmt_dep = dependency('fmt')

# Plugins primeiro: o executável precisa que os .so existam no build dir.
subdir('plugins')

executable('plugin_registry',
    files('main.cpp'),
    dependencies: [fmt_dep],

    # dl é a biblioteca do sistema que fornece dlopen/dlsym/dlclose.
    # No Linux ela é separada da libc — precisa ser linkada explicitamente.
    link_args: ['-ldl'],

    include_directories: ['.'],
    install: true,
)
