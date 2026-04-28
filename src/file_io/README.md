# `file_io` — Streams, Arquivos, Logging e `std::filesystem`

Subprojeto do `cpp_academy` dedicado à manipulação completa de I/O em C++ moderno.

---

## Hierarquia de Streams

```
ios_base
 └── basic_ios<CharT>
       ├── basic_istream<CharT>       ← std::istream
       │     ├── std::cin              (stdin  — teclado)
       │     └── std::ifstream         (arquivo)
       ├── basic_ostream<CharT>       ← std::ostream
       │     ├── std::cout             (stdout — terminal, BUFFERED)
       │     ├── std::cerr             (stderr — UNBUFFERED, imediato)
       │     ├── std::clog             (stderr — BUFFERED)
       │     └── std::ofstream         (arquivo)
       └── basic_iostream<CharT>      ← std::iostream
             ├── std::fstream          (arquivo — leitura e escrita)
             └── std::stringstream     (string em memória)
```

---

## Demos

### `demo_iostream.hpp` — cout / cerr / clog / cin

| Tópico | O que cobre |
|---|---|
| `std::cout` | Bases (dec/hex/oct), ponto flutuante (fixed/scientific), alinhamento, fill, boolalpha |
| `std::cerr` | Unbuffered — garante flush mesmo em crash |
| `std::clog` | Buffered — mais eficiente para logs volumosos |
| `std::cin` | Leitura por token, `getline`, estados fail/bad/eof, `ignore()`, `clear()` |
| Manipuladores | `setw`, `setprecision`, `setfill`, `left`, `right`, `internal`, `showbase`, `boolalpha` |

---

### `demo_fstream.hpp` — Arquivos texto e binários

| Tópico | O que cobre |
|---|---|
| `std::ofstream` | Escrita texto, truncate vs append (`ios::app`) |
| `std::ifstream` | `getline`, operador `>>`, leitura total, `rdbuf()` |
| `std::fstream` | `seekg` / `seekp`, `tellg` / `tellp`, acesso aleatório |
| Modo binário | `write()/read()` de structs, `gcount()`, seek por offset |
| Estados | `is_open()`, `good()`, `fail()`, `bad()`, `eof()`, exceções |

**Modos de abertura:**

```cpp
std::ofstream f("arq.txt");                         // out | trunc
std::ofstream f("arq.txt", std::ios::app);          // append
std::ifstream f("arq.txt");                         // in
std::fstream  f("arq.txt", std::ios::in|std::ios::out); // lê e escreve
std::ofstream f("arq.bin", std::ios::binary);       // binário
```

---

### `demo_sstream.hpp` — String streams

| Classe | Uso |
|---|---|
| `std::ostringstream` | Construção eficiente de strings, formatação de tabelas ASCII, `str()` / `view()` |
| `std::istringstream` | Parser de config `key=value`, tokenização CSV, conversão string→tipo |
| `std::stringstream` | Buffer bidirecional, serialização/deserialização simples |

**Padrão comum: serialização com sstream**

```cpp
std::ostringstream oss;
oss << ponto.x << "," << ponto.y;
std::string serial = oss.str();

std::istringstream iss(serial);
char sep; int x, y;
iss >> x >> sep >> y;
```

---

### `demo_logging.hpp` — Logging e Redirecionamento

#### `rdbuf()` — troca o buffer de streams

```cpp
std::ofstream arquivo("log.txt");
std::streambuf* original = std::cout.rdbuf(arquivo.rdbuf()); // redireciona
std::cout << "vai para o arquivo";
std::cout.rdbuf(original);                                    // restaura
```

#### `RedirectGuard` — RAII para redirecionamento seguro

```cpp
{
    RedirectGuard guard(std::cerr, arquivo_de_erros);
    std::cerr << "vai para o arquivo";
} // restaura automaticamente
```

#### `TeeStream` — escrever em dois destinos ao mesmo tempo

Equivalente ao `tee` do Unix:

```bash
./prog 2>&1 | tee log.txt
```

Em C++ com `TeeBuf` (herda `streambuf`, sobrescreve `overflow` e `xsputn`).

#### `Logger` — níveis, timestamps, cor, thread-safe

```
[08:42:01.123] [TRACE] inicializando...
[08:42:01.124] [DEBUG] variável x = 42
[08:42:01.124] [INFO ] conexão estabelecida
[08:42:01.125] [WARN ] buffer 80% cheio
[08:42:01.125] [ERROR] falha ao abrir arquivo   ← vai para stderr
[08:42:01.125] [FATAL] estado inválido          ← vai para stderr
```

---

### `demo_filesystem.hpp` — `std::filesystem` (C++17)

| API | O que faz |
|---|---|
| `fs::path` | Composição (`/`), decomposição (`stem`, `extension`, `parent`), normalização |
| `fs::create_directories` | Cria hierarquia completa de diretórios |
| `fs::copy_file` | Copia com opções (`overwrite_existing`, etc.) |
| `fs::rename` | Move / renomeia |
| `fs::exists / is_regular_file / is_directory` | Verificações de tipo |
| `fs::directory_iterator` | Iteração em nível único |
| `fs::recursive_directory_iterator` | Iteração em toda a árvore |
| `fs::file_size` | Tamanho em bytes |
| `fs::last_write_time` | Timestamp de modificação |
| `fs::permissions` | Leitura e modificação de permissões POSIX |
| `fs::space` | Capacidade / espaço livre do filesystem |
| `std::error_code` | Versões sem exceção de todas as funções |

---

## Redirecionamento Shell — Referência Rápida

```bash
# File descriptors:
#   0 = stdin    1 = stdout    2 = stderr

./prog > out.txt             # stdout → arquivo (trunca)
./prog >> out.txt            # stdout → arquivo (append)
./prog 2> err.txt            # stderr → arquivo
./prog > out.txt 2> err.txt  # stdout e stderr separados
./prog > out.txt 2>&1        # stderr segue stdout (ambos no arquivo)
./prog 2>&1 | tee log.txt    # terminal + arquivo simultaneamente
./prog < input.txt           # stdin lê de arquivo
./prog < in.txt > out.txt    # pipeline completo

# Descartar saída:
./prog > /dev/null           # descarta stdout
./prog 2> /dev/null          # descarta stderr
./prog > /dev/null 2>&1      # descarta tudo
```

> **Atenção:** a ordem importa. `./prog 2>&1 > out.txt` é diferente de `./prog > out.txt 2>&1`.
> No primeiro caso, stderr vai para o terminal (onde stdout apontava *antes* do redirect).

---

## Compilar e Executar

```bash
# Ativar o subprojeto em src/meson.build:
# subdir('file_io')

make native/configure
make native/build
./build/src/file_io/file_io

# Redirecionar saída padrão e erros separadamente:
./build/src/file_io/file_io 1>stdout.txt 2>stderr.txt

# Ver tudo no terminal e salvar:
./build/src/file_io/file_io 2>&1 | tee full.log
```

---

## Dependências

Apenas `fmt` (já presente no projeto) e a STL padrão C++17.
Nenhum Conan extra necessário.

---

## Arquivos

```
src/file_io/
├── main.cpp             ← ponto de entrada, chama todos os demos
├── meson.build          ← build do executável
├── demo_iostream.hpp    ← cout/cerr/clog/cin + manipuladores
├── demo_fstream.hpp     ← ofstream/ifstream/fstream texto e binário
├── demo_sstream.hpp     ← ostringstream/istringstream/stringstream
├── demo_logging.hpp     ← rdbuf, RedirectGuard, TeeStream, Logger
├── demo_filesystem.hpp  ← std::filesystem completo
└── README.md
```
