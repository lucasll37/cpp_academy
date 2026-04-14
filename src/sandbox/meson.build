# Executable
src_main = files('main.cpp')

executable('main',
    [src_main, common_proto_cc, common_proto_h],
    dependencies: [protobuf_dep, fmt_dep, spdlog_dep, json_dep],
    link_with: [],
    include_directories: public_headers,
    install: true
)