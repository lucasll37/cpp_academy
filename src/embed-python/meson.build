py = import('python').find_installation('python3')

executable('embed_python',
    ['main.cpp'],
    dependencies: [
        fmt_dep,
        py.dependency(embed: true),  # embed:true linka libpython3.x.so (necessário quando C++ é o host)
                                     # sem embed:true o Meson omite a lib para evitar símbolo duplo
                                     # em extension modules — mas aqui somos o executável principal
    ],
    install: true,
)
