# ── plugins/meson.build ──────────────────────────────────────────────────────
#
# Cada plugin é uma shared_library independente.
#
# Flags importantes:
#   name_prefix: '' → o Meson por padrão gera "libpid.so"; sem prefixo
#                     geramos "pid.so", que é mais limpo para dlopen.
#
#   gnu_symbol_visibility: 'hidden' → todos os símbolos internos ficam
#     ocultos por padrão. Só os marcados __attribute__((visibility("default")))
#     — que é o que extern "C" gera — ficam visíveis. Isso evita colisões
#     entre plugins que possam ter funções internas com o mesmo nome.
#
#   install: false → plugins ficam só no build dir neste subprojeto didático.
# ─────────────────────────────────────────────────────────────────────────────

# plugins/meson.build

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