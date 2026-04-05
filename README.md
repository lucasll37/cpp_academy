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

## Deploy cross-platform (Gitflow)

O binário alvo é `dist/bin/qt_academy`. O CI gera automaticamente os pacotes para cada plataforma quando uma tag `v*` é publicada.

### Fluxo de release

```bash
# Na branch main, após o merge da release branch:
make release VERSION=1.2.3
# Cria a tag v1.2.3 e faz push — o GitHub Actions dispara automaticamente.
```

O workflow `.github/workflows/release.yml` produz:

| Plataforma | Artefato |
|------------|----------|
| Linux | `academy-linux.AppImage` |
| macOS | `academy-macos.dmg` |
| Windows | `academy-windows-setup.exe` |

Os artefatos são publicados na GitHub Release correspondente à tag.

---

## Testando o deploy

### 1. Local — build e run

```bash
make configure
make build
make run        # abre a janela Qt+SFML
```

### 2. Local — simular o CI com `act`

[`act`](https://github.com/nektos/act) roda o workflow via Docker sem precisar de push.

```bash
# Instalar act
curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Simular o job Linux
act push --job build \
  --matrix os:ubuntu-22.04 \
  --eventpath <(echo '{"ref":"refs/tags/v0.0.1"}')
```

### 3. CI real — tag de pré-release

```bash
make release VERSION=0.0.1-rc1
```

Acompanhe em `github.com/<seu-repo>/actions`. Se os três jobs passarem, a Release aparece em `github.com/<seu-repo>/releases` com os artefatos.

### O que observar em cada job

| Job | Ponto de atenção |
|-----|-----------------|
| Linux | AppImage criado? `linuxdeploy` bundlou Qt corretamente? |
| macOS | `macdeployqt` encontrou todos os frameworks? `.dmg` monta? |
| Windows | `windeployqt` copiou as DLLs? Installer NSIS executa? |

**Ordem recomendada:** local → `act` (Linux) → tag `v0.0.1-rc1` → se tudo OK, `make release VERSION=1.0.0` na `main`.

---

## Licença

MIT — veja [LICENSE](LICENSE) para detalhes.