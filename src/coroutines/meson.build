executable('coroutines',
    ['main.cpp'],
    dependencies: [fmt_dep, asio_dep],
    cpp_args: [
        '-DASIO_HAS_CO_AWAIT',
        # GCC emite -Wsubobject-linkage para lambdas capturadas em frames de
        # coroutine (o frame vive na heap e a lambda vira campo do frame).
        # Known false-positive — nenhum problema real de linkage ocorre.
        '-Wno-subobject-linkage',
    ],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
