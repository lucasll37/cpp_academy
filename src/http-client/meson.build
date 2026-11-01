json_dep = dependency('nlohmann_json')
curl_dep = dependency('libcurl')


executable('http_client',
    files('main.cpp', 'http_client.cpp'),
    dependencies: [fmt_dep, json_dep, curl_dep],
    include_directories: include_directories('.'),
    install: true
)