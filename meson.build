project('academy', 'cpp', 'c',
    version: '1.0.0',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
    ],
)

# ============================================
# Opções do Projeto
# ============================================
# test_option = get_option('test_option')

# ============================================
# Dependências
# ============================================
# Só a `fmt` mora aqui: é a base de praticamente todos os subprojetos.
#
# Todas as demais são declaradas no meson.build do subprojeto que as usa
# (ex.: `bullet_dep` em src/collision/, src/sfml_bullet/, ...). Assim, comentar
# um `subdir()` em src/meson.build também deixa de exigir as dependências dele:
# o `meson setup` só procura o que os subprojetos HABILITADOS pedem.
#
# Declarar a mesma dependência em vários subprojetos não custa nada: o Meson
# guarda o resultado de `dependency()` em cache e só procura uma vez.
fmt_dep = dependency('fmt')


# ============================================
# Headers Públicos
# ============================================
inc = include_directories('include')
public_headers = include_directories('include')


# ============================================
# Subdiretórios
# ============================================
subdir('proto')
subdir('src')

# ⚠ O antigo `tests/` da raiz foi REMOVIDO. Ele estava 100% comentado desde
#   sempre e nunca construiu nada. Os testes automatizados agora moram nos
#   subprojetos que os ensinam e os exercitam de verdade:
#     src/testing/  — GoogleTest, fixtures, TEST_P, gmock, death tests
#     src/meson/    — o alvo test() do Meson
#     src/cmake/    — CTest (build próprio, fora do Meson)
#   `make test` roda os do Meson; os do CMake, com `ctest --test-dir`.

# ============================================
# pkg-config
# ============================================
pkg = import('pkgconfig')