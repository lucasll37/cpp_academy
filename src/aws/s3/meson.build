# ⚠ Os nomes corretos são `aws-sdk-cpp-*`, não `aws-cpp-sdk-*`. A troca das duas
#   primeiras palavras é o motivo de este alvo ter ficado desabilitado.
aws_core_dep = dependency('aws-sdk-cpp-core')
aws_s3_dep   = dependency('aws-sdk-cpp-s3')


executable('aws-s3',
    ['main.cpp'],
    dependencies: [fmt_dep, aws_core_dep, aws_s3_dep],
    install: true
)