# file_io — Streams, arquivos, logging e `std::filesystem`

## O que é este projeto?

Como um programa C++ lê e escreve bytes: no teclado, no terminal, num arquivo de texto, num
arquivo binário, numa string em memória, e no sistema de arquivos inteiro.

```bash
make configure      # só na primeira vez (conan + meson)
make build
./dist/bin/file_io
```

O binário chama-se **`file_io`** (`executable('file_io', …)` em `src/file_io/meson.build`),
instalado em `dist/bin/file_io`. O subprojeto **já está ativo**: `src/meson.build` linha 68
tem `subdir('file_io')` — não há nada para descomentar.

Tudo que a demo cria fica em `/tmp/` (nada no diretório atual): `cpp_academy_texto.txt`,
`_rw.txt`, `_bin.dat`, `_rdbuf.txt`, `_logger.log`, `_tee.txt`. O `_logger.log` abre em modo
*append* e **cresce a cada execução**. Limpar: `rm -f /tmp/cpp_academy_*`.

---

## Antes de começar: os conceitos que você precisa

Nada aqui pressupõe mais que C++ básico (sintaxe, funções, classes).

### 1. O que é uma *stream*

Uma **stream** é uma **sequência de bytes com um cursor**. Você não vê o meio de
armazenamento; vê uma fita por onde os bytes passam e uma posição atual nela.

```
        cursor
          ↓
  [ H ][ e ][ l ][ l ][ o ][ \n ][ … ]
```

A ideia central: **a origem dos bytes não importa para quem os consome**. Teclado, arquivo e
`std::string` na RAM expõem a mesma interface (`>>`, `<<`, `getline`, `read`, `write`) porque
derivam das mesmas classes base. É por isso que você troca `std::cin` por um
`std::istringstream` num teste sem mudar uma letra do código que lê — exatamente o que
`demo_iostream.hpp` faz, para os demos não travarem esperando o teclado.

### 2. Tipos ≠ objetos globais

Aqui a maioria dos tutoriais confunde o leitor: `std::cout` **não é uma classe**, é uma
variável global já construída. A árvore de herança contém só tipos:

```
ios_base
 └── basic_ios<CharT>            ← estado (good/fail/bad/eof), fill, locale
       ├── basic_istream<CharT>  ← >>, getline, read, seekg, tellg
       │     ├── basic_ifstream       → std::ifstream
       │     └── basic_istringstream  → std::istringstream
       ├── basic_ostream<CharT>  ← <<, write, seekp, tellp, flush
       │     ├── basic_ofstream       → std::ofstream
       │     └── basic_ostringstream  → std::ostringstream
       └── basic_iostream<CharT> ← herda de basic_istream E basic_ostream
             │                     (herança múltipla; basic_ios é base virtual)
             ├── basic_fstream        → std::fstream
             └── basic_stringstream   → std::stringstream
```

É a herança múltipla de `basic_iostream` que explica por que um `std::fstream` aceita `>>` e
`<<`: ele é as duas coisas. `basic_ios` aparece uma só vez no objeto porque as heranças
intermediárias são **virtuais** ([`../oop/`](../oop/)).

Os quatro objetos globais, construídos antes do seu `main`, não são tipos novos:

| Objeto | Tipo | fd | Buffer |
|---|---|---|---|
| `std::cin` | `std::istream` | 0 | bufferizado; `cin.tie()` aponta para `cout` |
| `std::cout` | `std::ostream` | 1 | bufferizado |
| `std::cerr` | `std::ostream` | 2 | bufferizado **com `unitbuf`** → flush a cada `<<` |
| `std::clog` | `std::ostream` | 2 | bufferizado, **sem** `unitbuf` |

Medido (`flags() & std::ios::unitbuf`, GCC/libstdc++, C++20):
`cout unitbuf: 0 · cerr unitbuf: 1 · clog unitbuf: 0 · cin.tie()==&cout: 1`.

> ⚠ Dizer que "`cerr` é *unbuffered*" é impreciso e está em quase todo tutorial. `cerr`
> **tem** buffer; o que ele tem é a flag `unitbuf`, que dispara flush ao fim de cada operação
> de saída. A consequência é a que importa: a mensagem chega antes de um `abort()`, ao custo
> de uma syscall `write` por `<<`. `clog` existe para o caso oposto — mesmo destino (fd 2),
> volume alto, sem pagar syscall por linha.

### 3. Buffer, e por que "não aparece nada"

Um **buffer** é uma área intermediária na RAM. `std::cout << "oi"` não escreve no terminal:
escreve num array. Só quando ele enche, ou alguém pede **flush**, é que um `write()` leva
tudo de uma vez. Existe porque uma syscall custa microssegundos e escrever num array custa
nanossegundos.

```
seu código        buffer (RAM)               kernel          terminal/disco
cout << "oi"  →  [o][i][ ][ ] ─── flush ──→ write(1,…)  →    aparece
```

Esvazia quando: enche; você pede (`std::flush`, `std::endl`, `.flush()`); a stream é
destruída; o programa termina normalmente; a stream tem `unitbuf`; ou alguém lê de uma stream
**atada** a ela — `cin` está atado a `cout`, e por isso o *prompt* impresso antes de um
`cin >>` aparece na hora mesmo sem `endl`.

> ⚠ **"Meu programa não imprime nada antes de crashar" quase nunca é código que não rodou —
> é buffer não esvaziado.** Um `SIGSEGV` mata o processo sem destrutores e sem flush final: o
> conteúdo do buffer de `cout` evapora. Depurar com `cerr` resolve.

### 4. `streambuf` — quem realmente move os bytes

Uma stream faz duas coisas, em objetos diferentes:

```
   std::ostream            std::streambuf            destino real
 ┌──────────────┐        ┌────────────────┐        ┌──────────────┐
 │ formatação   │  ───→  │ o buffer +     │ ─────→ │ fd 1, arquivo│
 │ <<, setw,    │        │ como esvaziá-lo│ write()│ string, rede │
 │ hex, fixed   │        │ overflow/xsputn│        │              │
 └──────────────┘        └────────────────┘        └──────────────┘
    casca fina
```

`stream.rdbuf()` devolve esse buffer; `stream.rdbuf(outro)` **troca o destino sem trocar a
stream**. Essa frase explica todo o `demo_logging.hpp`: em
`std::cout.rdbuf(arquivo.rdbuf())`, `std::cout` continua o mesmo objeto global com as mesmas
flags, mas os bytes passam a sair no arquivo.

`std::streambuf` é polimórfica (64 bytes na libstdc++ x86-64, medido com `sizeof`). Virtuais
que interessam: **`overflow(int c)`** — chamado quando o buffer encheu, recebe **um**
caractere; **`xsputn(const char*, streamsize n)`** — escrita em bloco, recebe **n** bytes;
`underflow()`/`uflow()` para leitura; `sync()` para flush. Sobrescrever só `overflow` já
funciona (o `xsputn` padrão o chama byte a byte); sobrescrever `xsputn` é o que deixa rápido.
O `TeeBuf` do demo faz os dois.

### 5. Os quatro bits de estado

| Bit | Liga quando | Recuperável |
|---|---|---|
| `goodbit` | nenhum dos outros ligado | — |
| `eofbit` | uma leitura tentou passar do fim e o encontrou | sim (`clear()`) |
| `failbit` | falha lógica: leu `abc` esperando `int`, arquivo não abriu | sim (`clear()`+`ignore()`) |
| `badbit` | erro irrecuperável do meio (disco, dispositivo) | não, na prática |

`if (!f)` equivale a `f.fail()` (failbit **ou** badbit) e `while (f >> x)` usa essa mesma
conversão a cada iteração. Repare: `if (!f)` **não** olha `eofbit` — uma stream que chegou ao
fim mas cuja última leitura deu certo continua "boa". É a raiz da armadilha 1.

```cpp
int n;
if (!(std::cin >> n)) {
    std::cin.clear();                                                   // apaga os bits
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // joga o lixo fora
}
```

Sem `clear()`, tudo depois falha em silêncio. Sem `ignore()`, a próxima leitura reencontra o
mesmo `abc` — laço infinito.

### 6. *File descriptor* e RAII

Um **fd** é um `int` que o kernel usa como índice na tabela de arquivos abertos do processo.
Todo processo começa com `0 = stdin`, `1 = stdout`, `2 = stderr`. O shell manipula esses
inteiros **antes** do seu programa começar — por isso `./prog > out.txt` funciona sem o
programa saber de nada, e `std::cout` não sabe se o fd 1 é terminal, arquivo ou pipe.

**RAII** (*Resource Acquisition Is Initialization*): o construtor adquire, o destrutor
libera, e o C++ garante que o destrutor roda em **todos** os caminhos de saída — `return` no
meio, `break`, exceção. Em I/O, você quase nunca chama `close()`:

```cpp
{ std::ofstream f("saida.txt"); f << "linha\n"; }   // destrutor esvazia e fecha
```

---

## Por que estudar isto?

| Sem entender streams | Entendendo streams |
|---|---|
| "não imprime nada quando crasha" | é buffer; logue em `cerr` |
| laço que processa a última linha duas vezes | `while (std::getline(f,l))`, e você sabe por quê |
| `cin >> n` e o `getline` seguinte vem vazio | `ignore()` até o `'\n'` |
| `"pasta" + "/" + nome` (quebra no Windows) | `fs::path base / nome` |
| testar código que lê de `cin` exige teclado | trocar por `istringstream` |
| escrever no arquivo errado sem perceber | `if (!arquivo)` logo após abrir |

**Por que este subprojeto usa `fmt::print` e não `std::cout`?** Porque C++ tem três
mecanismos de saída concorrentes: `printf` é curto mas não tem segurança de tipo (`%d` com um
`double` é UB); `<iostream>` é seguro e extensível via `operator<<`, mas verboso e com
manipuladores de **estado pegajoso**; `std::format`/`fmt` é declarativo **e** seguro, checado
em compilação. Regra do repositório: **`fmt::print` para imprimir; streams quando o assunto é
a stream** — redirecionar, ler arquivo, fazer parsing, capturar saída em teste. Convivem
porque `fmt::print` escreve no mesmo fd 1. Formatação a fundo: [`../strings/`](../strings/).

---

## O exemplo mínimo completo

```cpp
#include <fstream>
#include <iostream>
#include <string>

int main() {
    {
        std::ofstream saida("dados.txt");                    // abre truncando
        if (!saida) { std::cerr << "falhou\n"; return 1; }   // SEMPRE cheque
        saida << "primeira linha\n";
        saida << "segunda linha: " << 42 << '\n';
    }                                                        // destrutor: esvazia e fecha

    std::ifstream entrada("dados.txt");
    if (!entrada) { std::cerr << "falhou\n"; return 1; }
    std::string linha;
    while (std::getline(entrada, linha))                     // a forma CORRETA do laço
        std::cout << "> " << linha << '\n';
}
```

```
> primeira linha
> segunda linha: 42
```

Todo o resto do subprojeto é variação disto: trocar o destino, o formato, ou o assunto.

---

## ⚠ As três armadilhas de I/O em C++

Números medidos com GCC/libstdc++ e C++20, não deduzidos.

### 1. `while (!arquivo.eof())`

`eofbit` só liga **depois** de uma leitura que encontrou o fim. Testar antes de ler entra no
corpo uma vez a mais:

```cpp
std::istringstream f("10 20 30\n");
int x = 0, soma = 0;
while (!f.eof()) { f >> x; soma += x; }   // ERRADO → soma = 90 (30 contado 2×)
while (f >> x)   { soma += x; }           // CERTO  → soma = 60 ✓
```

Por que 90: depois de ler `30` o cursor está antes do `\n`, o fim ainda não foi alcançado,
`eof()` é falso, o laço entra de novo, `>>` falha — e **`>>` deixa `x` inalterado quando
falha** (C++11), então 30 é somado outra vez. Com `getline` o sintoma muda e a causa é a
mesma — uma iteração extra com a string vazia:

```
arquivo "A\nB\nC\n"
while (!f.eof()) { getline(f,l); … }  →  [A] [B] [C] []   ← linha fantasma
while (getline(f,l))          { … }   →  [A] [B] [C]      ✓
```

**Regra:** o teste do laço é o **resultado** da leitura, nunca um estado consultado antes.

### 2. `>>` seguido de `getline`

```cpp
std::cin >> idade;              // lê "42", PARA no '\n' e o DEIXA no buffer
std::getline(std::cin, nome);   // acha o '\n' na hora → string vazia
```

Medido com a entrada `"42\nLucas Silva\n"`: `n=42  nome=''`. `>>` pula espaços **antes** do
valor, nunca o que vem depois. A correção, com `<limits>`:

```cpp
std::cin >> idade;
std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
std::getline(std::cin, nome);   // n=42  nome='Lucas Silva'  ✓
```

`ignore(n, delim)` descarta até `n` caracteres, parando **depois** de consumir `delim`.

### 3. Não checar se o arquivo abriu

```cpp
std::ofstream o("/diretorio/inexistente/x.txt");
o << "some texto";        // não lança, não avisa, não escreve
```

Medido: `o.fail() == 1`, nenhuma exceção, código de saída 0. **O programa reporta sucesso e
os dados somem.** Streams sinalizam erro por bit de estado, não por exceção — por padrão:

```cpp
std::ofstream o(caminho);  if (!o) { /* trate */ }        // (a) checar

std::ifstream f;                                          // (b) pedir exceções
f.exceptions(std::ios::failbit | std::ios::badbit);
try { f.open("/tmp/nao_existe.txt"); }
catch (const std::ios_base::failure& e) { /* … */ }
```

A mensagem real que a demo imprime em (b) é `basic_ios::clear: iostream error` — genérica e
pouco útil, o que é um argumento a favor de (a) ou de `std::error_code`
([`../errors/`](../errors/)).

---

## Estrutura de arquivos

```
src/file_io/
├── main.cpp               ← ponto de entrada; chama os 5 demos em ordem
├── meson.build            ← executable('file_io', …), depende de fmt
├── demo_iostream.hpp      ← 1 · cout, manipuladores, cerr/clog, estados de cin
├── demo_fstream.hpp       ← 2 · ofstream/ifstream/fstream, binário, seek, erros
├── demo_sstream.hpp       ← 3 · ostringstream/istringstream/stringstream, view()
├── demo_logging.hpp       ← 4 · rdbuf, RedirectGuard (RAII), TeeBuf, Logger
├── demo_filesystem.hpp    ← 5 · path, criar/copiar, iterar, metadados, error_code
└── README.md              ← este arquivo
```

---

## Os demos, um a um

### Demo 1 — `cout` e os manipuladores

```cpp
std::cout << std::hex << 255 << '\n';                                 // ff
std::cout << std::showbase << std::hex << 255 << '\n';                // 0xff
std::cout << std::fixed << std::setprecision(2) << 3.14159 << '\n';   // 3.14
std::cout << std::setfill('.') << std::left << std::setw(20) << "esquerda" << "|\n";
```

Saída real: `ff`, `377`, `0xff`, `0377`, `3.14`, `3.1416e+00`, e
`esquerda............|` / `.............direita|` / `-.................42|` — a última é
`std::internal`, que põe o preenchimento **entre o sinal e o número**.

> ⚠ **Manipuladores são estado pegajoso.** `std::hex` vale até alguém desfazer, não só para o
> próximo `<<`; o demo faz `std::cout << std::dec;` logo depois e usa
> `std::cout.unsetf(std::ios::floatfield)` para voltar ao padrão. Exceções: `setw` vale **só
> para a próxima** saída; `setfill` é pegajoso. É a razão prática de preferir
> `fmt::print("{:x}", n)`, onde a formatação não vaza. E `std::endl` é `'\n'` **mais** flush:
> num laço de milhares de linhas, milhares de syscalls — use `'\n'`.

### Demo 2 — arquivos texto e binário

| Modo | Efeito |
|---|---|
| `ios::in` / `ios::out` | leitura / escrita (`out` sozinho implica `trunc`) |
| `ios::app` | toda escrita vai para o fim, ignorando `seekp` |
| `ios::ate` | posiciona no fim ao abrir, mas `seekp` continua valendo |
| `ios::trunc` | zera o arquivo ao abrir |
| `ios::binary` | sem tradução de fim de linha |

```cpp
std::ofstream f("a.txt");                               // out | trunc
std::ofstream f("a.txt", std::ios::app);                // acrescenta
std::fstream  f("a.txt", std::ios::in | std::ios::out); // lê e escreve, sem truncar
```

**Dois cursores independentes**, e as letras não são arbitrárias: `g` de *get* (leitura), `p`
de *put* (escrita) — `tellg`/`seekg` e `tellp`/`seekp`, com origens `ios::beg`, `ios::cur`,
`ios::end`. A demo grava `ITEM_0..ITEM_9`, lê a 1ª linha (`tellg()` devolve **7** = 6 letras
+ `\n`), faz `seekp(0, beg)` e escreve `MODIF` — resultado `MODIF0`, porque escrever
**sobrescreve**, não insere. Não existe "inserir no meio" de um arquivo: reescreve-se.

```cpp
struct Registro { uint32_t id; float valor; char nome[16]; };   // sizeof == 24
std::ofstream o("d.dat", std::ios::binary);
o.write(reinterpret_cast<const char*>(&r), sizeof(Registro));
std::ifstream i("d.dat", std::ios::binary);
while (i.read(reinterpret_cast<char*>(&r), sizeof(Registro))) { /* … */ }
```

Saída medida: `3 registros escritos (24 bytes cada, total=72 bytes)` — 4+4+16 = 24 exatos,
sem *padding*, porque `alignof(Registro)` é 4 e o tamanho já é múltiplo de 4
([`../layout/`](../layout/)). Com registros de tamanho fixo, acesso aleatório sai de graça: o
*k*-ésimo está no byte `k * sizeof(Registro)`.

> ⚠ **Três limites do binário cru:** (1) portabilidade zero — depende de endianness, `sizeof`
> e padding do compilador; (2) só tipos triviais — `write` de uma struct com `std::string`
> grava o **ponteiro**, não o texto; (3) no Windows, sem `ios::binary` o modo texto converte
> `\n` em `\r\n` e corrompe dados binários em silêncio (no Linux não há diferença, o que
> torna o bug invisível até rodar em outro sistema).

> ⚠ A demo imprime `gcount()=0`. Não é bug: `gcount()` conta os bytes da **última** operação,
> e a última foi a que falhou e encerrou o laço. Chame `gcount()` **dentro** do laço.

### Demo 3 — string streams: formatador e parser

Uma `stringstream` é uma stream cujo destino é um `std::string` na memória — interface
idêntica à de arquivo, mesma classe base.

```cpp
std::ostringstream oss;                                   // montar
oss << "porta=" << 5432 << " pi=" << std::fixed << std::setprecision(4) << 3.14159;
std::string s = oss.str();                                // "porta=5432 pi=3.1416"

std::istringstream iss("10 20 30");                       // desmontar
int x, soma = 0; while (iss >> x) soma += x;              // 60

oss.str("");   // reutilizar: esvazia o conteúdo …
oss.clear();   // … e limpa os BITS DE ESTADO; sem isto um failbit anterior persiste
```

Os cinco usos que o header documenta: construir strings compostas sem `+` repetido; parsing
de `chave=valor` e CSV; converter tipo ↔ string; tokenizar com `getline(iss, tok, ',')` (o 3º
parâmetro troca o delimitador); e testar código que lê de `cin`, sem teclado.

`str()` devolve **cópia** (aloca); `view()` (C++20) devolve `std::string_view` sem alocar.

> ⚠ `view()` é invalidada assim que você escreve mais na stream — o buffer pode realocar e o
> `string_view` fica pendurado em memória liberada. Use e descarte no mesmo escopo. E para
> **apenas formatar**, `fmt::format`/`std::format` é mais curto, mais rápido e sem estado
> pegajoso; `ostringstream` só ganha quando você precisa da **interface de stream** — por
> exemplo reaproveitar um `operator<<` que já existe para o seu tipo.

### Demo 4 — redirecionamento e logging

**Degrau 1 — trocar o `rdbuf` na mão:**

```cpp
std::ofstream arquivo("/tmp/log.txt");
std::streambuf* original = std::cout.rdbuf();   // guarda o destino atual
std::cout.rdbuf(arquivo.rdbuf());               // troca
std::cout << "isto vai para o arquivo\n";
std::cout.rdbuf(original);                      // restaura
```

Com um `ostringstream` no lugar do arquivo, o mesmo truque **captura a saída em memória** — é
assim que se testa uma função que imprime, sem tocar nela.

**Degrau 2 — por que a restauração manual quebra.** A linha que restaura fica *depois* do
trabalho: um `return`, `break` ou exceção no meio pula ela. Pior — o `ofstream` é destruído
ao sair do escopo e `std::cout` fica segurando um `streambuf` morto, e toda saída seguinte do
programa é comportamento indefinido.

**Degrau 3 — `RedirectGuard`, a versão RAII.** A restauração vira destrutor, e o destrutor
roda em todos os caminhos:

```cpp
class RedirectGuard {
public:
    RedirectGuard(std::ostream& s, std::ostream& destino)
        : stream_(s), buf_original_(s.rdbuf(destino.rdbuf())) {}  // troca ao construir
    ~RedirectGuard() { stream_.rdbuf(buf_original_); }            // restaura sempre
    RedirectGuard(const RedirectGuard&)            = delete;      // não copiável:
    RedirectGuard& operator=(const RedirectGuard&) = delete;      //   restauraria 2×
private:
    std::ostream& stream_; std::ofstream arquivo_; std::streambuf* buf_original_;
};

{ RedirectGuard g(std::cerr, "/tmp/erros.txt"); std::cerr << "vai para o arquivo\n"; }
```

**Degrau 4 — `TeeBuf`, dois destinos ao mesmo tempo.** Com `streambuf` já definido, é o `tee`
do Unix em quinze linhas: uma subclasse que guarda dois buffers e repassa para os dois.

```cpp
class TeeBuf : public std::streambuf {
    std::streambuf *a_, *b_;
protected:
    int overflow(int c) override {                  // caminho de 1 caractere
        if (c == EOF) return !EOF;
        if (a_->sputc(char(c)) == EOF) return EOF;
        if (b_->sputc(char(c)) == EOF) return EOF;
        return c;
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {   // bloco
        a_->sputn(s, n); b_->sputn(s, n); return n;
    }
public:
    TeeBuf(std::streambuf* a, std::streambuf* b) : a_(a), b_(b) {}
};
```

> ⚠ **Comportamento real da demo, e é instrutivo:** logo depois de escrever pelo `TeeBuf` o
> programa reabre o arquivo e imprime — e **imprime vazio**. O arquivo não está vazio: o
> `ofstream` de destino ainda está aberto e o texto ainda está no buffer dele. Rode
> `cat /tmp/cpp_academy_tee.txt` **depois** do programa terminar e as duas linhas estão lá
> (98 bytes). Um `arquivo.flush()` antes da releitura resolveria. É a Seção 3 na prática.

**Os quatro níveis de redirecionamento** (a tabela mora em `demo_logging.hpp`):

| Nível | Mecanismo | Afeta `cout`/`cerr` | Afeta `printf` e libs C | Reversível |
|---|---|---|---|---|
| C++ | `stream.rdbuf(outro)` | sim | **não** | sim (guarde o ponteiro) |
| C | `freopen("f","w",stdout)` | sim | sim | difícil |
| kernel | `dup2(fd_novo, 1)` | sim | sim | sim (`dup` antes) |
| shell | `./prog > out.txt` | sim | sim | n/a (externo) |

Se uma biblioteca C dentro do processo escreve com `printf`, trocar o `rdbuf` de `std::cout`
**não captura nada** dela — só `freopen`, `dup2` ou o shell alcançam. A demo não executa
`freopen` de propósito: é global e difícil de desfazer.

**O `Logger`** junta as peças: seis níveis (`TRACE < DEBUG < INFO < WARN < ERROR < FATAL`),
timestamp com milissegundos, cores ANSI opcionais, `std::mutex`, duas saídas simultâneas.

```
[14:32:04.067] [INFO ] conexão estabelecida com sucesso
[14:32:04.067] [ERROR] falha ao abrir arquivo de configuração   ← este vai para cerr
```

Decisões que valem estudo: `ERROR`/`FATAL` vão para `cerr` e o resto para `cout`, e assim
`2> erros.txt` separa o que importa; o arquivo recebe o texto **sem** códigos ANSI;
`arquivo_.flush()` a cada linha, porque o log tem de sobreviver ao crash que você investiga
(a mesma escolha do `unitbuf` de `cerr`); e um `std::lock_guard` no mesmo mutex protege
terminal e arquivo juntos, senão duas threads intercalam metade de cada linha
([`../concurrency/`](../concurrency/)).

> ⚠ O arquivo abre com `std::ios::app` e a demo imprime o arquivo inteiro:
> `/tmp/cpp_academy_logger.log` **acumula** todas as execuções e na décima rodada a saída
> fica longa. Não é bug, é o que `app` faz — `rm` no arquivo zera.

> ⚠ **No shell, a ordem importa.** `2>&1` significa "faça o fd 2 apontar para onde o fd 1
> aponta **agora**". Em `./prog 2>&1 > out.txt` o `2>&1` acontece antes, quando o fd 1 ainda
> é o terminal: stderr fica no terminal e só stdout vai ao arquivo. O que se quer é
> `./prog > out.txt 2>&1`.

### Demo 5 — `std::filesystem` (C++17)

Concatenar caminhos com string quebra na primeira barra duplicada, no primeiro separador do
Windows, no primeiro nome com espaço. `namespace fs = std::filesystem;`

```cpp
fs::path log = fs::path("/tmp/cpp_academy") / "logs" / "2024" / "app.log";
fs::path p   = "/tmp/cpp_academy/subdir/arquivo.txt";
p.parent_path();   // /tmp/cpp_academy/subdir      p.filename();  // arquivo.txt
p.stem();          // arquivo (sem extensão)       p.extension(); // .txt (COM o ponto)
fs::path b = p; b.replace_extension(".bak");                  // arquivo.bak
fs::path("/tmp/a/../a/./logs").lexically_normal();            // /tmp/a/logs

fs::create_directories(base / "subdir1" / "nested");   // cria a hierarquia inteira
fs::copy_file(a, b, fs::copy_options::overwrite_existing);
fs::rename(a, b);   fs::exists(p);   fs::is_directory(p);   fs::file_size(p);
uintmax_t n = fs::remove_all(base);                    // apaga a árvore, devolve a contagem

for (const auto& e : fs::directory_iterator(base)) { }            // só o nível de cima
for (const auto& e : fs::recursive_directory_iterator(base))      // tudo abaixo
    if (e.is_regular_file() && e.path().extension() == ".txt")
        std::cout << e.path() << '\n';

auto mtime = fs::last_write_time(p);                   // fs::file_time_type
auto perms = fs::status(p).permissions();
bool pode_ler = (perms & fs::perms::owner_read) != fs::perms::none;
fs::permissions(p, fs::perms::owner_exec, fs::perm_options::add);
auto s = fs::space("/tmp");                            // s.capacity, s.free, s.available
```

`lexically_normal()` é **puramente textual**: não toca no disco nem resolve links simbólicos
— para isso existe `fs::canonical()`, que exige que o caminho exista. E `directory_entry`
guarda em cache o que já perguntou ao sistema de arquivos: `e.is_regular_file()` é bem mais
barato que `fs::is_regular_file(e.path())`, que refaz o `stat`.

> ⚠ `fs::file_time_type` **não** é `system_clock::time_point` antes do C++20; converter exige
> a ginástica que `fmt_time()` faz no header — medir a diferença entre "agora" nos dois
> relógios e aplicá-la. Em C++20 há `std::chrono::clock_cast` ([`../chrono/`](../chrono/)).

**Erros: exceção ou `error_code`.** Toda função de `<filesystem>` tem as duas versões — a que
lança `fs::filesystem_error` (com `e.path1()` e `e.code()`) e a sobrecarga que recebe um
`std::error_code&` no fim e **nunca** lança. Use a segunda quando "não existe" é resultado
esperado, a primeira quando a falha é excepcional ([`../errors/`](../errors/)).

> ⚠ Toda operação de `<filesystem>` é sujeita a **TOCTOU** (*time of check, time of use*):
> entre o seu `fs::exists(p)` e o seu `fopen(p)`, outro processo pode ter apagado o arquivo.
> Nenhuma versão de `exists()` resolve isso. O padrão robusto é **tentar a operação e tratar
> a falha**, não perguntar antes.
## Como validar

```bash
make configure && make build && ./dist/bin/file_io
```

Compilando **só** este subprojeto, isolado, com todos os avisos (exige um `make configure`
prévio, para o `.pc` do `fmt` existir em `build/`):

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic \
    $(PKG_CONFIG_PATH=$PWD/build pkg-config --cflags fmt) \
    src/file_io/main.cpp -o /tmp/file_io_test \
    $(PKG_CONFIG_PATH=$PWD/build pkg-config --libs fmt)
/tmp/file_io_test
```

Compila **sem um único aviso** — verificado com GCC e C++20. Espera-se ver cinco cabeçalhos
em moldura (`1. iostream`, `2. fstream`, `3. sstream`, `4. logging`, `5. std::filesystem`) e,
no fim, `✓ file_io — todos os demos concluídos`.

```bash
./dist/bin/file_io > out.txt 2> err.txt && head -5 err.txt        # o que saiu por cerr/clog
./dist/bin/file_io > /dev/null && cat /tmp/cpp_academy_tee.txt    # o tee, já esvaziado
./dist/bin/file_io > /dev/null && ls -l /tmp/cpp_academy_bin.dat  # 72 bytes = 3 × 24
rm -f /tmp/cpp_academy_*                                          # limpar
```

---

## Referências

Subprojetos irmãos:

- [`../strings/`](../strings/) — `std::format` e `fmt`: a alternativa moderna aos
  manipuladores, sem estado pegajoso
- [`../errors/`](../errors/) — exceções, `std::error_code` e `std::expected`
- [`../chrono/`](../chrono/) — relógios e `time_point`, por trás do timestamp do `Logger` e
  de `fs::last_write_time`
- [`../concurrency/`](../concurrency/) — `std::mutex` e `std::lock_guard` do `Logger`
- [`../layout/`](../layout/) — `sizeof`, alinhamento e *padding*: por que `Registro` ocupa 24
  bytes no arquivo binário
- [`../memory/`](../memory/) — RAII a fundo, o padrão por trás de `RedirectGuard`
- [`../oop/`](../oop/) — herança múltipla e virtual, por trás de `basic_iostream`

Fontes externas canônicas:

- cppreference — [Input/output library](https://en.cppreference.com/w/cpp/io)
- cppreference — [`std::basic_streambuf`](https://en.cppreference.com/w/cpp/io/basic_streambuf)
- cppreference — [Filesystem library](https://en.cppreference.com/w/cpp/filesystem)
- ISO C++ Core Guidelines — [SL.io](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#SS-io)
- `man 2 dup2`, `man 3 freopen`, `man 1 tee` — os níveis abaixo do C++
