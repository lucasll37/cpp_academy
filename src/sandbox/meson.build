# =============================================================================
#  src/sandbox/meson.build
#
#  ⚠ O alvo se chamava `main`, que COLIDE com o `main` de src/collision/:
#    os dois instalavam em dist/bin/main e um sobrescrevia o outro. Agora
#    chama-se `sandbox`, e o binario e dist/bin/sandbox.
#
#  ⚠ O alvo tambem linkava protobuf_dep, spdlog_dep, fmt_dep e os arquivos
#    gerados common_proto_cc/common_proto_h — nenhum deles usado por main.cpp.
#    Dependencia que nao se usa e tempo de build e superficie de quebra de
#    graca. Sobrou a unica de verdade: nlohmann_json.
# =============================================================================
executable('sandbox',
    files('main.cpp'),
    dependencies: [json_dep],
    install: true
)
