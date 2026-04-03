project('academy', 'cpp',
    version: '1.0.0',
    default_options: [
        'cpp_std=c++17',
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
gtest_dep    = dependency('gtest', fallback: ['gtest', 'gtest_dep'])
fmt_dep      = dependency('fmt')
omp_dep      = dependency('openmp')
protobuf_dep = dependency('protobuf')
spdlog_dep   = dependency('spdlog')
json_dep     = dependency('nlohmann_json')

protoc = '@0@/bin/protoc'.format(protobuf_dep.get_variable('prefix'))
# ============================================
# Subdiretórios
# ============================================
inc = include_directories('include')
# public_headers = include_directories('include')

subdir('proto')
subdir('src')
subdir('python')
subdir('tests')

# Design Patterns
subdir('design-of-pattern/factory')
subdir('design-of-pattern/observer')
subdir('design-of-pattern/strategy')
subdir('design-of-pattern/decorator')
subdir('design-of-pattern/command')
# ============================================
# pkg-config
# ============================================
pkg = import('pkgconfig')

pkg.generate(
    lib_hello_world,
    name:        'academy',
    description: 'Academy library.',
    requires_private: ['gtest', 'protobuf'],
)