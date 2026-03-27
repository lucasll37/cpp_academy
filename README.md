# Academy

Projeto C++ moderno com **Meson** como sistema de build e **Conan** como gerenciador de dependências.

---

## Requisitos

| Ferramenta | Versão mínima |
|------------|---------------|
| GCC / Clang | C++17 |
| Meson | 1.0+ |
| Ninja | 1.10+ |
| Conan | 2.0+ |

---

## Primeiros passos

### 1. Clonar o repositório

```bash
git clone <url-do-repositório>
cd academy
```

### 2. Configurar o projeto

Instala as dependências via Conan e configura o Meson:

```bash
make configure
```

### 3. Compilar

```bash
make build
```

### 4. Executar

```bash
make run
```

---

## Comandos disponíveis

```bash
make configure   # Instala dependências e configura o build
make build       # Compila todos os alvos
make install     # Instala biblioteca e executável em ./dist/
make package     # Empacota via Conan (Debug + Release)
make run         # Executa o binário principal
make clean       # Remove build/, dist/ e cache do Conan
make help        # Lista todos os comandos disponíveis
```

---

## Estrutura do projeto

```
academy/
├── include/          # Headers públicos da biblioteca
├── src/              # Código-fonte (.cpp) e meson.build local
├── tests/            # Testes unitários (GTest)
├── scripts/          # Scripts auxiliares
├── proto/            # Definições Protobuf (futuro)
├── meson.build       # Build principal
├── meson_options.txt # Opções configuráveis do Meson
├── conanfile.py      # Receita e dependências Conan
└── Makefile          # Interface de conveniência
```

---

## Dependências

| Pacote | Versão | Uso |
|--------|--------|-----|
| [GTest](https://github.com/google/googletest) | 1.14.0 | Testes unitários |

---

## Executando os testes

> Os testes utilizam o [Google Test](https://github.com/google/googletest).

```bash
# Após make configure && make build:
cd build
meson test

# Com saída verbose:
meson test -v
```

---

## Adicionando dependências

Declare no `conanfile.py`:

```python
def requirements(self):
    self.requires("gtest/1.14.0")
    self.requires("fmt/10.2.1")       # exemplo
    self.requires("spdlog/1.13.0")    # exemplo
```

Em seguida, adicione a dependência no `meson.build`:

```meson
fmt_dep = dependency('fmt')
```

E execute `make configure` novamente.

---

## Opções de build

As opções são declaradas em `meson_options.txt` e passadas via `meson setup`:

```bash
meson setup build -Dtest_option=true
```

---

## Licença

MIT — veja [LICENSE](LICENSE) para detalhes.