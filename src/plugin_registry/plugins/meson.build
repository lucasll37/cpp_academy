# ── plugins/meson.build ──────────────────────────────────────────────────────
#
# Cada plugin é uma shared_library independente.
#
# Flags importantes:
#   name_prefix: '' → o Meson por padrão gera "libpid.so"; sem prefixo
#                     geramos "pid.so", que é mais limpo para dlopen.
#
#   -fvisibility=hidden → todo símbolo do plugin nasce OCULTO. Só ficam
#     visíveis os marcados com __attribute__((visibility("default"))), que é o
#     que a macro REGISTER_CONTROLLER (icontroller.hpp) coloca nas três funções
#     de entrada. Isso evita colisão entre plugins com funções internas de mesmo
#     nome — e é o motivo pelo qual `extern "C"` SOZINHO não basta aqui:
#     sem o atributo, dlsym devolve nullptr e o plugin não carrega.
#
#   install: true + install_dir: 'plugins' → os .so vão para <prefix>/plugins,
#     ou seja dist/plugins/, que é onde main.cpp procura por padrão
#     (<dir-do-exe>/../plugins, a partir de dist/bin/).
# ─────────────────────────────────────────────────────────────────────────────

plugin_flags = ['-fvisibility=hidden']

shared_library('pid',
    files('pid_controller.cpp'),
    include_directories: ['..'],
    name_prefix: '',
    cpp_args: plugin_flags,
    install: true,
    install_dir: 'plugins',   # ← instala em dist/plugins/
)

shared_library('lqr',
    files('lqr_controller.cpp'),
    include_directories: ['..'],
    name_prefix: '',
    cpp_args: plugin_flags,
    install: true,
    install_dir: 'plugins',
)