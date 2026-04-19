# C++ Academy

Repositório de **subprojetos didáticos** em C++ moderno (C++17/20), cobrindo desde fundamentos da linguagem até integração com bibliotecas reais de indústria. Cada subprojeto é autocontido, compilável individualmente e acompanhado de um README detalhado.

**Sistema de build**: Meson + Ninja  
**Gerenciamento de dependências**: Conan 2  
**Padrão de linguagem**: C++20

---

## Início rápido

### Pré-requisitos

| Ferramenta | Versão mínima | Instalação |
|---|---|---|
| GCC ou Clang | C++20 | `sudo apt install g++` |
| Meson | 1.0+ | `pip install meson` |
| Ninja | 1.10+ | `sudo apt install ninja-build` |
| Conan | 2.0+ | `pip install conan` |

### Build completo

```bash
# 1. Clonar
git clone <url>
cd cpp_academy

# 2. Configurar (baixa dependências via Conan + configura Meson)
make configure

# 3. Compilar todos os subprojetos
make build

# 4. Instalar binários em dist/bin/
make install
```

### Build de um subprojeto específico

```bash
make configure        # necessário apenas uma vez
make build            # compila tudo

# Ou compilar e rodar só o que interessa:
source build/conanbuild.sh
meson compile -C build concurrency
./dist/bin/concurrency
```

---

## Comandos disponíveis

```bash
make configure   # Instala dependências (Conan) e configura Meson
make build       # Compila todos os alvos
make install     # Instala binários em dist/bin/
make test        # Executa testes (Google Test)
make clean       # Remove build/, dist/ e cache do Conan
make help        # Lista todos os comandos com descrição
```

---

## Estrutura do repositório

```
cpp_academy/
├── src/                  ← subprojetos (um diretório por projeto)
│   ├── aws/              ← AWS SDK for C++ — S3
│   ├── calculus/         ← Álgebra linear com Eigen3
│   ├── casts/            ← Guia de type casting
│   ├── collision/        ← Física com Bullet3
│   ├── concepts/         ← C++20 Concepts
│   ├── concurrency/      ← Multithreading e primitivos de sync
│   ├── database/         ← SOCI — SQLite/PostgreSQL
│   ├── design-of-pattern/← 8 padrões GoF
│   ├── ecs/              ← Entity Component System
│   ├── embed-python/     ← C++ como host do interpretador Python
│   ├── flight-simulator/ ← JSBSim + autopiloto PID
│   ├── http-client/      ← libcurl + nlohmann/json
│   ├── inline_asm/       ← Assembly inline x86-64
│   ├── mandelbrot/       ← Fractal com Qt + std::thread
│   ├── mandelbrot_openmp/← Fractal com Qt + OpenMP
│   ├── memory/           ← Alocadores customizados + std::pmr
│   ├── network/          ← Sockets com Asio (TCP/UDP/HTTP)
│   ├── outline_asm/      ← Assembly em arquivo .s separado
│   ├── parser/           ← Flex/Bison — parser de expressões
│   ├── plugin_registry/  ← dlopen/dlsym — sistema de plugins
│   ├── python/           ← PyBind11 + CPython API
│   ├── qt_sfml/          ← Qt com widget SFML embutido
│   ├── qt_sfml2/         ← Qt + SFML + Bullet3
│   ├── rabbitmq/         ← AMQP-CPP — produtor/consumidor
│   ├── sandbox/          ← Espaço livre de experimentação
│   ├── sfml_bullet/      ← SFML + Bullet3 — física 2D visual
│   ├── sfml_bullet2/     ← SFML + Bullet3 — ragdoll e cordas
│   ├── smart-pointers/   ← unique_ptr, shared_ptr, weak_ptr
│   ├── templates/        ← Metaprogramação de templates
│   └── yolo/             ← YOLOv10 + ONNX Runtime + OpenCV
├── include/              ← headers públicos da biblioteca
├── tests/                ← testes (Google Test)
├── docs/                 ← documentação gerada pelo Doxygen
├── proto/                ← definições Protobuf
├── scripts/              ← scripts auxiliares
├── meson.build           ← build principal
├── meson_options.txt     ← opções configuráveis
├── conanfile.py          ← receita e dependências Conan
└── Makefile              ← interface de conveniência
```

---

## Subprojetos por categoria

### Fundamentos da Linguagem

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **casts** | `static_cast`, `dynamic_cast`, `const_cast`, `reinterpret_cast`, C-style cast | [→](src/casts/README.md) |
| **smart-pointers** | `unique_ptr`, `shared_ptr`, `weak_ptr`, RAII, pimpl, factory | [→](src/smart-pointers/README.md) |
| **templates** | function/class templates, specialization, variadic, SFINAE, type_traits, CRTP | [→](src/templates/README.md) |
| **concepts** | C++20 concepts, `requires`, constraints, subsumption | [→](src/concepts/README.md) |
| **memory** | `new`/`delete` internos, pool allocator, arena allocator, `std::pmr` | [→](src/memory/README.md) |

### Concorrência e Paralelismo

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **concurrency** | `std::thread`, mutex, `condition_variable`, future/promise, atomics, thread pool | [→](src/concurrency/README.md) |
| **mandelbrot** | Paralelismo por tiles, `std::thread`, `condition_variable` como sincronizador de tiles | [→](src/mandelbrot/README.md) |
| **mandelbrot_openmp** | `#pragma omp parallel for`, scheduling dinâmico, reduction | [→](src/mandelbrot_openmp/README.md) |

### Assembly e Hardware

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **inline_asm** | AT&T syntax, constraints, CPUID, RDTSC, clobbers, `volatile`, loops | [→](src/inline_asm/README.md) |
| **outline_asm** | Arquivos `.s` separados, ABI x86-64 System V, `rep movsb`, `cmov` | [→](src/outline_asm/README.md) |

### Padrões de Projeto e Arquitetura

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **design-of-pattern** | Chain of Responsibility, Command, Decorator, Factory, Iterator, Observer, State, Strategy | [→](src/design-of-pattern/README.md) |
| **ecs** | Entity Component System, type erasure com `std::type_index`, query genérica com fold expressions | [→](src/ecs/README.md) |
| **plugin_registry** | `dlopen`/`dlsym`, interface virtual, singleton registry, `unique_ptr` com deleter customizado | [→](src/plugin_registry/README.md) |

### Rede e Mensageria

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **network** | Asio: TCP síncrono/assíncrono, UDP, `steady_timer`, HTTP raw | [→](src/network/README.md) |
| **http-client** | libcurl wrapper, nlohmann/json, deserialização automática de structs | [→](src/http-client/README.md) |
| **rabbitmq** | AMQP-CPP + Boost.Asio, produtor/consumidor, ACK, exchange/queue | [→](src/rabbitmq/README.md) |
| **aws** | AWS SDK for C++, S3Client, modelo `Outcome<R,E>`, credenciais automáticas | [→](src/aws/README.md) |

### Banco de Dados

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **database** | SOCI, repository pattern, prepared statements, transações, DSN portável | [→](src/database/README.md) |

### Gráficos, Física e Simulação

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **collision** | Bullet3: broadphase/narrowphase, corpos estáticos/dinâmicos, restitution | [→](src/collision/README.md) |
| **sfml_bullet** | SFML + Bullet3, simulação 2D, conversão metros↔pixels, Y-flip | [→](src/sfml_bullet/README.md) |
| **sfml_bullet2** | Ragdoll (7 segmentos), cordas, `btHingeConstraint`, `btConeTwistConstraint` | [→](src/sfml_bullet2/README.md) |
| **qt_sfml** | Qt + SFML integrado, `winId()` trick, `QTimer` como game loop | [→](src/qt_sfml/README.md) |
| **qt_sfml2** | Qt + SFML + Bullet3, criação dinâmica de corpos físicos | [→](src/qt_sfml2/README.md) |
| **flight-simulator** | JSBSim FDM, autopiloto PID, loop de controle 120 Hz, CSV logging | [→](src/flight-simulator/README.md) |
| **calculus** | Eigen3: LU/QR/SVD/autovalores, mínimos quadrados, PCA, número de condição | [→](src/calculus/README.md) |

### Python Interop

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **python** | PyBind11 vs CPython API, GIL, OpenMP em extensões Python | [→](src/python/README.md) |
| **embed-python** | C++ como host do interpretador Python, RAII guard, gerenciamento de `PyObject*` | [→](src/embed-python/README.md) |

### Machine Learning e Visão

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **yolo** | YOLOv10 + ONNX Runtime, pré/pós-processamento, webcam, benchmark p95/p99 | [→](src/yolo/README.md) |

### Ferramentas e Geração de Código

| Subprojeto | Conceitos principais | README |
|---|---|---|
| **parser** | Flex + Bison, análise léxica/sintática, gramática BNF, precedência de operadores | [→](src/parser/README.md) |
| **sandbox** | Espaço livre para experimentos — atualmente: nlohmann/json | [→](src/sandbox/README.md) |

---

## Dependências

Todas gerenciadas pelo Conan 2 e declaradas em `conanfile.py`:

| Biblioteca | Versão | Usada em |
|---|---|---|
| `fmt` | 10.2.1 | Todos os subprojetos (formatação) |
| `spdlog` | 1.14.1 | `flight-simulator` (logging estruturado) |
| `nlohmann_json` | 3.11.3 | `http-client`, `sandbox` |
| `eigen` | 3.4.0 | `calculus` (álgebra linear) |
| `bullet3` | 3.25 | `collision`, `sfml_bullet`, `sfml_bullet2`, `qt_sfml2` |
| `sfml` | 2.6.1 | `sfml_bullet*`, `qt_sfml*`, `mandelbrot*` |
| `libcurl` | 8.6.0 | `http-client` |
| `asio` | 1.28.0 | `network` |
| `soci` | 4.0.3 | `database` (SQLite + PostgreSQL) |
| `sqlite3` | 3.45.0 | `database` (backend) |
| `pybind11` | 3.0.1 | `python` |
| `aws-sdk-cpp` | 1.11.692 | `aws` (componente S3) |
| `amqp-cpp` | 4.3.27 | `rabbitmq` |
| `jsbsim` | 1.1.11 | `flight-simulator` |
| `onnxruntime` | 1.17.3 | `yolo` |
| `opencv` | 4.9.0 | `yolo` |
| `gtest` | 1.14.0 | `tests/` |
| `protobuf` | 3.21.12 | `proto/` |
| OpenMP | — | `mandelbrot_openmp`, `python` |
| Qt 6 | — | `mandelbrot*`, `qt_sfml*` (sistema) |

> Qt 6 é instalado via sistema (`sudo apt install qt6-base-dev`), não via Conan.

---

## Adicionando uma nova dependência

**1.** Declare em `conanfile.py`:

```python
def requirements(self):
    self.requires("boost/1.84.0")
```

**2.** Adicione no `meson.build` do subprojeto:

```meson
boost_dep = dependency('boost')

executable('meu_projeto',
    'src/meu_projeto/main.cpp',
    dependencies: [boost_dep, fmt_dep],
)
```

**3.** Reconfigure:

```bash
make configure
make build
```

---

## Adicionando um novo subprojeto

**1.** Crie o diretório e os arquivos:

```bash
mkdir src/meu_projeto
touch src/meu_projeto/main.cpp
touch src/meu_projeto/meson.build
touch src/meu_projeto/README.md
```

**2.** Escreva o `meson.build`:

```meson
executable('meu_projeto',
    'main.cpp',
    dependencies: [fmt_dep],
    install: true,
    install_dir: get_option('prefix') / 'bin',
)
```

**3.** Registre no `meson.build` da raiz (na seção de subdiretórios):

```meson
subdir('src/meu_projeto')
```

**4.** Compile:

```bash
make build
./dist/bin/meu_projeto
```

---

## Testes

```bash
make test

# Com saída verbosa:
cd build && meson test -v

# Teste específico:
cd build && meson test -v nome_do_teste
```

Os testes usam **Google Test** e ficam em `tests/`.

---

## Documentação

```bash
# Gera HTML com Doxygen
cd build && ninja docs

# Abre no browser:
xdg-open docs/html/index.html
```

---

## Release e CI/CD

O workflow `.github/workflows/release.yml` produz binários para Linux, macOS e Windows quando uma tag `v*` é publicada:

```bash
# Criar release
make release VERSION=1.2.3
# → cria tag v1.2.3 → dispara CI → publica artefatos na GitHub Release
```

| Plataforma | Artefato |
|---|---|
| Linux | `academy-linux.AppImage` |
| macOS | `academy-macos.dmg` |
| Windows | `academy-windows-setup.exe` |

### Testar CI localmente com `act`

```bash
# Instalar act (roda GitHub Actions via Docker)
curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Simular job Linux
act push --job build --matrix os:ubuntu-22.04
```

---

## Licença

MIT — veja [LICENSE](LICENSE) para detalhes.
