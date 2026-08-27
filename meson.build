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
gtest_dep       = dependency('gtest', fallback: ['gtest', 'gtest_dep'])
fmt_dep         = dependency('fmt')
omp_dep         = dependency('openmp')
protobuf_dep    = dependency('protobuf')
spdlog_dep      = dependency('spdlog')
json_dep        = dependency('nlohmann_json')
eigen_dep       = dependency('eigen3')
pybind_dep      = dependency('pybind11')
bullet_dep      = dependency('bullet')
sfml_dep        = dependency('sfml-all')
curl_dep        = dependency('libcurl')
asio_dep        = dependency('asio')
soci_dep        = dependency('soci-soci_core')
soci_sqlite_dep = dependency('soci-soci_sqlite3')
# ⚠ `soci-soci_postgresql` existe como .pc mas tem um `Requires: soci-soci_core none`
#   malformado (o `none` é um bug do gerador do Conan) e o pkg-config o rejeita.
#   O subprojeto `database` usa apenas o backend SQLite, que não precisa de servidor.
soci_pg_dep     = dependency('soci-soci_postgresql', required: false)

# ⚠ Os nomes corretos são `aws-sdk-cpp-*`, não `aws-cpp-sdk-*`. A troca das duas
#   primeiras palavras é o motivo de estes alvos terem ficado desabilitados.
aws_core_dep    = dependency('aws-sdk-cpp-core')
aws_s3_dep      = dependency('aws-sdk-cpp-s3')
onnx_dep        = dependency('onnxruntime')
opencv_dep      = dependency('opencv')
amqpcpp_dep     = dependency('amqpcpp')
boost_dep       = dependency('boost')
jsbsim_dep      = dependency('jsbsim')
# jsbsim_dep      = dependency('JSBSim')

qt6          = import('qt6')
qt_dep       = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'])

# ============================================
# Programas Utilitários
# ============================================
protoc = '@0@/bin/protoc'.format(protobuf_dep.get_variable('prefix'))


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