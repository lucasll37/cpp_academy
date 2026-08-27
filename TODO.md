# TODO

## Cobertura da linguagem

O planejamento dos subprojetos de **linguagem e biblioteca padrão** vive em
[src/ROADMAP.md](src/ROADMAP.md) — lá está o levantamento das lacunas, o estado de cada
subprojeto e os limites do GCC 13.3.

## Ideias soltas (ecossistema / infra)

- ~~protobuf — hoje só existe `proto/`, sem subprojeto didático~~ ✅ feito em
  [src/cmake/](src/cmake/README.md): `medida.proto` é compilado pelo `protoc` **durante o
  build**, via `add_custom_command(OUTPUT ...)`, e o demo serializa e desserializa de verdade.
- microservices
- documentação
- signal (POSIX signals, `sigaction`, async-signal-safety)
- `src/docker/` → database, rabbitmq, redis
- embarcado? ESP-32
