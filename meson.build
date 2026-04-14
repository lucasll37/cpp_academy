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
# soci_dep        = dependency('soci-soci_core') # dependency('soci')
# soci_sqlite_dep = dependency('soci-soci_sqlite3')
# soci_pg_dep     = dependency('soci-soci_postgresql')
# aws_core_dep    = dependency('aws-cpp-sdk-core')
# aws_s3_dep      = dependency('aws-cpp-sdk-s3')
onnx_dep        = dependency('onnxruntime')
opencv_dep      = dependency('opencv')
amqpcpp_dep     = dependency('amqpcpp')
boost_dep       = dependency('boost')

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
subdir('docs')
subdir('proto')
subdir('src')
# subdir('tests')

# ============================================
# pkg-config
# ============================================
pkg = import('pkgconfig')