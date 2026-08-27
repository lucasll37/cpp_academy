# strings — Texto em C++, do byte ao regex

## O que é este projeto?

Cinco demos sobre **texto em C++**: o que existe dentro de um `std::string`, quando usar
`std::string_view`, como formatar com verificação em tempo de compilação, como converter
texto↔número sem alocar e sem depender do idioma da máquina, e por que `std::regex` quase nunca é
a resposta. Tudo o que este README afirma sobre custo foi **medido pelo binário**.

```bash
make build            # compila o repositório e instala em ./dist
./dist/bin/strings    # roda os cinco demos, em ordem
```

O alvo se chama `strings` (`executable('strings', ...)` em [`meson.build`](meson.build)); o
binário instalado é `./dist/bin/strings`.

> ⚠ Este alvo compila com **C++23**, e não com o C++20 do resto do repositório, por causa de uma
> linha só: `std::string::contains`, na seção 1.4 de [`demo_string.hpp`](demo_string.hpp). A
> exceção está no `meson.build`, em `override_options: ['cpp_std=c++23']`.
>
> ⚠ Os demos usam `fmt::print` em vez de `std::format`: a **fmt** é a implementação de referência
> que originou a proposta do `std::format`, já está no `conanfile.py`, e a API é a mesma. Onde se
> lê `fmt::format`, leia `std::format`.

---

## Antes de começar: o vocabulário do texto

Esta seção define **todo** termo que o resto do README usa. Não pressupõe nada além de saber
declarar uma variável e chamar uma função.

**Byte, caractere e codificação.** Um **byte** é um número de 0 a 255; um **caractere** é o que um
humano enxerga (`a`, `é`, `語`). Os dois foram sinônimos enquanto o mundo era ASCII (128 símbolos,
um byte cada). **Codificação** é a regra que transforma caracteres em bytes; a dominante é
**UTF-8**, em que ASCII ocupa 1 byte e o resto ocupa de 2 a 4 — `c a f é` vira
`63 61 66 C3 A9`: 4 caracteres, **5 bytes**. `std::string` é uma sequência de **bytes**, não de
caracteres, e não sabe o que é UTF-8: por isso `"café".size()` é 5, `s[3]` cai no **meio** de um
caractere e `std::reverse` sobre UTF-8 produz lixo. Não é bug, é o contrato (demo 1.6).

**C-string e o terminador `'\0'`.** Em C não existe tipo de texto: existe um ponteiro para o
primeiro byte e a convenção de que a sequência termina no primeiro byte **zero** — uma
**C-string** (*null-terminated*).

```
const char* p = "abc";   p ──▶ ┌───┬───┬───┬────┐
                               │'a'│'b'│'c'│ \0 │ ← delimita o fim; não conta no comprimento
                               └───┴───┴───┴────┘
```

Daí: `strlen(p)` **percorre** os bytes até achar o `\0` (é O(n)), e `printf("%s", p)` lê **até** o
`\0` — se ele não existir, a leitura passa do fim do buffer. `std::string::c_str()` garante o
`\0`; **`std::string_view::data()` não garante nada** (demo 2.2).

**Pilha, heap e "alocar".** Variáveis locais moram na **pilha**: o compilador reserva na entrada
da função e devolve na saída, de graça. Quando o tamanho só é conhecido em execução, o programa
pede memória ao **heap** com `new`/`malloc`; cada pedido é uma **alocação** — dezenas a centenas
de nanossegundos, fragmenta a memória e, num laço quente, domina o tempo. Os demos 1 e 2
**contam** alocações substituindo o `operator new` global por uma versão instrumentada (fim de
[`demo_string.hpp`](demo_string.hpp)): os números daqui são contagens exatas, não estimativas.

**O que existe dentro de um `std::string`, e o que é SSO.** **SSO** é *Small String Optimization*,
**otimização de string curta**: como a maioria das strings de um programa é curta (nomes, chaves,
identificadores), o objeto carrega um **buffer interno** e usa esse espaço enquanto o texto
couber. Na libstdc++ (GCC) o objeto tem **32 bytes**:

```
 CURTA (≤15)  ┌────────────┬────────────┬─────────────────────────────┐
              │ ponteiro ──┼────────────┼─▶ 'a','b','c','\0', …       │  → 0 alocações
              │ (8 bytes)  │ tamanho: 3 │ união de 16 B: o TEXTO aqui │
              └────────────┴────────────┴─────────────────────────────┘
                aponta 16 bytes à frente — para dentro do próprio objeto

 LONGA (≥16)  ┌────────────┬────────────┬─────────────────────────────┐
              │ ponteiro ─┐│ tam.: 100  │ união de 16 B: capac. = 100 │  → 1 alocação
              └───────────┼┴────────────┴─────────────────────────────┘
                          └──▶ heap: ['y','y', …, '\0']
```

A união é o truque: os mesmos 16 bytes são **ou** a capacidade (texto no heap) **ou** o texto em
si. Daí o limite de **15** caracteres — o 16º byte é do `'\0'`, que a `std::string` mantém para
que `c_str()` seja O(1). O demo 1.1 confirma isso contando alocações **e** mostrando que `.data()`
de uma string curta fica 16 bytes depois do endereço do objeto.

**`size`, `capacity`, `reserve`, `npos`.** `size()` é quantos bytes o texto tem agora;
`capacity()`, quantos cabem **sem realocar** (sempre ≥ `size()`); `reserve(n)` pede capacidade de
uma vez, evitando realocações intermediárias — quando `size` passa de `capacity`, a libstdc++
**dobra** o buffer, copia tudo e libera o antigo (a política do `std::vector`). E **`npos`** é o
que toda busca devolve quando **não** acha: `static_cast<std::size_t>(-1)`, o **maior** valor
possível — não `-1`, e muito menos `0`.

```cpp
if (s.find("x") > 0)                  // ✗ SEMPRE verdadeiro quando não acha
if (s.find("x") != std::string::npos) // ✓ a única comparação correta
```

**Locale.** Conjunto **global** de convenções de formatação que a biblioteca C consulta: separador
decimal (`.` ou `,`), moeda, ordem de datas, ordenação. Vale para o **processo inteiro**, e
qualquer biblioteca pode trocá-lo com `setlocale(LC_ALL, "")` na inicialização, sem avisar você.
Respeitam o locale: `printf`, `iostream`, `std::stod`, `std::to_string`; **não** respeitam, por
decisão de projeto: `std::from_chars` e `std::to_chars` (demo 4.5).

**`char_traits`.** `std::string` não é uma classe, é apelido de
`std::basic_string<char, std::char_traits<char>, std::allocator<char>>`. O segundo parâmetro é a
classe de **políticas** que diz como comparar, copiar e medir os caracteres (`eq`, `lt`,
`compare`, `length`, `find`); trocá-la dá uma string com regras próprias — por exemplo insensível
a maiúsculas — sem custo em execução (demo 1.5).

**`consteval`, ou "validado na compilação".** `constexpr` marca algo que **pode** ser avaliado em
compilação; **`consteval`** (C++20) marca uma função que **só** existe em compilação. Como um
`throw` durante avaliação constante é **erro de build**, uma função `consteval` que analisa a
string de formato e lança quando ela é inválida transforma erro de formatação em erro de
compilação — é assim que `fmt::format("{:d}", "texto")` deixa de compilar. Detalhado em
[`src/compile_time/demo_consteval.hpp`](../compile_time/demo_consteval.hpp).

**Round-trip.** Converter um valor para texto e de volta obtendo **exatamente** o mesmo valor.
Para `double` não é automático: poucos dígitos perdem precisão, dígitos demais produzem
`0.10000000000000001`. `std::to_chars` garante a **menor** string que faz round-trip (demo 4.4).

**Backtracking e ReDoS.** Um motor de regex por **backtracking** tenta uma alternativa e, se ela
falhar adiante, **volta** e tenta a próxima, recursivamente — é o que `std::regex`, PCRE e o motor
do JavaScript fazem. `(a+)+$` sobre `"aaaaaaaaaaaaaaaaaaaaX"` gera explosão **exponencial** de
combinações; quando o atacante controla o padrão ou o texto, isso vira **ReDoS** (*Regular
expression Denial of Service*): uma requisição de 30 bytes ocupa um núcleo por minutos. Motores de
autômato (RE2) não têm o problema (demo 5.5).

### O exemplo mínimo, completo

```cpp
#include <fmt/core.h>
#include <string>
#include <string_view>

int main() {
    std::string s = "caminho";     // 7 bytes: cabem dentro do próprio objeto (SSO)
    s += "/arquivo.txt";           // 19 bytes: não cabem mais — houve uma alocação
    fmt::print("size = {}   capacity = {}\n", s.size(), s.capacity());

    const std::size_t barra = s.find('/');
    if (barra != std::string::npos)                          // ⚠ nunca `> 0`
        fmt::print("depois da '/': '{}'\n", s.substr(barra + 1));

    const std::string_view janela{s};                        // ponteiro + tamanho, sem copiar
    fmt::print("view dos 7 primeiros: '{}'\n", janela.substr(0, 7));
}
```

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -Wall -Wextra -Wpedantic -I$FMTP/include exemplo.cpp \
    -o /tmp/ex -L$FMTP/lib -lfmtd && LD_LIBRARY_PATH=$FMTP/lib /tmp/ex
```

Saída **verificada**: `size = 19   capacity = 30`, `depois da '/': 'arquivo.txt'`,
`view dos 7 primeiros: 'caminho'`. A `capacity` é 30 e não 19 porque, ao estourar os 15 bytes do
buffer interno, a libstdc++ **dobrou** — esse número já é a seção 1.2 inteira, resumida. E `size`
conta **bytes**, o que prepara a seção 1.6.

---

## Por que estudar isto?

Texto é onde o desempenho vaza sem ninguém perceber, e onde a correção falha em silêncio.

| Sem o desta pasta | Com |
|---|---|
| `void f(const std::string&)` — todo chamador com `const char*` **aloca** para chamar | `void f(std::string_view)`: zero alocação, mesma legibilidade |
| montar texto com `a + b + c + d` num laço | `reserve` + `append`: de 2–3 alocações para **1** (medido em 1.3) |
| `printf("%d", 1.5)` compila e imprime lixo | `fmt::format("{:d}", 1.5)` **não compila** |
| `std::stod("3.14")` devolve `3.0` na máquina do cliente | `std::from_chars` ignora o locale, sempre |
| `to_string(0.1)` → `"0.100000"`, e a volta perde o valor | `std::to_chars` garante o *round-trip* mínimo |
| `std::regex` num laço quente | `find` + `from_chars`: **~150×** mais rápido no demo 5.5 |
| `printf("%s", view.data())` lê além do fim | `printf("%.*s", (int)v.size(), v.data())` |

---

## Estrutura de arquivos

```
strings/
├── README.md               ← este arquivo
├── meson.build             ← o alvo `strings`; override para cpp_std=c++23
├── main.cpp                ← única unidade de tradução; chama os cinco demos em ordem
├── demo_string.hpp         ← 1 · SSO medido, capacity, concatenação, API, char_traits,
│                             encoding — e o `operator new` instrumentado
├── demo_string_view.hpp    ← 2 · o custo que elimina, o terminador ausente, tempo de vida
├── demo_format.hpp         ← 3 · validação em compilação, mini-linguagem, format_to,
│                             formatter próprio, custo (com a ressalva metodológica)
├── demo_charconv.hpp       ← 4 · from_chars/to_chars, round-trip, locale, custo
└── demo_regex.hpp          ← 5 · match vs search, capturas, replace, gramáticas, custo
```

`main.cpp` é a **única** unidade de tradução, e isso é obrigatório: `demo_string.hpp` **substitui
o `operator new` global**, substituição que precisa ser única no programa (ver
[`src/linkage/`](../linkage/) sobre a ODR).

**Como ler os números daqui:** CPU AMD Ryzen 9 5900X, g++ 13.3.0, `buildtype=debug` (**`-O0`**,
sem otimização) e `libfmtd`, a variante de **debug** da fmt. Contagens de alocação são estruturais
e não mudam com `-O2`; tempos em milissegundos variam ~30% entre execuções e mudam com as flags —
prefira as **razões**, e rode você mesmo.

---

## Demo 1 — `std::string` por dentro

### 1.1 SSO, medido

```
sizeof(std::string) = 32 bytes
 1 chars → 0 alocação(ões) ← cabe no SSO     16 chars → 1 alocação(ões)
 8 chars → 0 alocação(ões) ← cabe no SSO     17 chars → 1 alocação(ões)
14 chars → 0 alocação(ões) ← cabe no SSO     32 chars → 1 alocação(ões)
15 chars → 0 alocação(ões) ← cabe no SSO     → o limite na libstdc++ é 15 caracteres
```

E a prova estrutural que fecha o diagrama de *Antes de começar*: numa string curta, `.data()` fica
**16 bytes** depois do endereço do objeto — dentro dele. Três consequências: strings curtas são
**quase de graça** (não evite `std::string` em nomes e chaves); ⚠ o **move** de uma string curta
**copia** os bytes, e só é O(1) acima do limite; ⚠ `sizeof(std::string)` é 32, então um
`vector<std::string>` de 1 milhão de elementos gasta **32 MB só de cabeçalhos** — ver
[`src/layout/`](../layout/).

> O limite 15 é da **libstdc++**; a libc++ (Clang) usa outro arranjo e chega a 22. Nunca escreva
> código que dependa do número; escreva código que se beneficie de o SSO existir.

### 1.2 `capacity` e crescimento

```
string vazia: size=0 capacity=15     ← o SSO já está disponível
crescendo char a char: 15→30 30→60 60→120 120→240   →  size=200 capacity=240
200 caracteres, um a um:  sem reserve → 4 alocações  ·  com reserve → 1 alocação
```

⚠ `clear()` zera o `size` e **mantém** a memória (`capacity` continua 240) — ótimo para
reaproveitar um buffer, ruim se você esperava liberar. `shrink_to_fit()` é apenas um **pedido** não
vinculante, ao qual a libstdc++ atende (volta a 15); a forma garantida de liberar é
`std::string{}.swap(s);`.

### 1.3 Concatenação: `+` vs `+=` vs `append`

```
quatro trechos, 96 bytes no total:
a + b + c + d        → 2 aloc, 149 bytes        reserve + +=      → 1 aloc, 97 bytes
r += a; r += b; ...  → 3 aloc, 213 bytes        reserve + append  → 1 aloc, 97 bytes
```

`a + b + c + d` é `((a + b) + c) + d`, e cada `+` produz uma `string` **temporária** — foram 2 e
não 3 alocações porque a libstdc++ reaproveita o buffer quando o operando da esquerda já é um
*rvalue*. Repare no contraintuitivo: **`+=` sem `reserve` alocou mais que `+`**, porque cada
estouro de capacidade dobra o buffer.

> Numa cadeia com N trechos: **`reserve(total)` e depois `append`** — uma alocação, 97 bytes (96 do
> texto + 1 do `'\0'`). Para texto formatado, `fmt::format` já faz isso; demo 3.

### 1.4 A API que você vai usar todo dia

| operação | devolve | quando não acha |
|---|---|---|
| `s.find(x)` / `s.rfind(x)` | posição da 1ª / última ocorrência | **`npos`** |
| `s.find_first_of("abc")` | posição do 1º byte **no** conjunto | **`npos`** |
| `s.find_first_not_of(" \t")` | posição do 1º byte **fora** do conjunto | **`npos`** |
| `s.substr(pos, n)` | uma `std::string` **nova** (aloca) | lança `out_of_range` se `pos > size()` |
| `s.replace(pos, n, txt)` | referência a `s`, já modificada | — |
| `s.starts_with(x)` / `ends_with(x)` | `bool` — **C++20** | `false` |
| `s.contains(x)` | `bool` — **C++23**, o motivo do override no `meson.build` | `false` |

```
saída real sobre '/caminho/para/arquivo.tar.gz':
find('/') = 0    rfind('/') = 13    find_last_of(".") = 25    contains("para") = true
starts_with("/caminho") = true    ends_with(".gz") = true    substr → 'arquivo.tar.gz'
replace(0,8,"/novo") → '/novo/para/arquivo.tar.gz'
split por '/': [''] ['caminho'] ['para'] ['arquivo.tar.gz']
```

Antes do C++20, `starts_with` era `s.compare(0, p.size(), p) == 0` ou, pior,
`s.substr(0, p.size()) == p` — que **aloca** só para comparar. ⚠ **A STL não tem `trim` nem
`split`**: o demo escreve os dois em cinco linhas cada, sobre `string_view` (sem alocar). Desde o
C++20 há `std::views::split` — [`src/ranges/`](../ranges/).

### 1.5 `char_traits`: o parâmetro que ninguém olha

```cpp
struct TraitsSemCaso : std::char_traits<char> {      // compare, find… seguem a mesma ideia
    static bool eq(char a, char b) { return std::tolower(a) == std::tolower(b); }
    static bool lt(char a, char b) { return std::tolower(a) <  std::tolower(b); }
};
using StringSemCaso = std::basic_string<char, TraitsSemCaso>;
```

A comparação passa a ser insensível a maiúsculas, decidida em **compilação**, sem custo em
execução e sem alocar. ⚠ Mas **`StringSemCaso` não é `std::string`**: é outro tipo, não converte
implicitamente, não entra numa API que pede `const std::string&`, e cada função sua viraria
template — na prática, quase sempre é melhor **normalizar** (guardar em minúsculas). O valor de
conhecer isto é ver que a política de comparação é um **parâmetro**, a mesma ideia do comparador
de `std::map`.

### 1.6 O que `std::string` não sabe

```
"cafe".size() = 4  (4 letras, 4 bytes)      iterando "café": 63 61 66 C3 A9
"café".size() = 5  (4 letras, 5 BYTES)                              └──┬──┘  UM caractere
```

⚠ `s[i]` pode cair no meio de um caractere; `substr` pode cortá-lo ao meio; `std::reverse` produz
bytes inválidos; comparar sem **normalização Unicode** dá falso negativo (`é` pode ser um ponto de
código ou `e` + acento combinante). O padrão quase nada oferece:
`u8string`/`u16string`/`u32string` só marcam a intenção, e `<codecvt>` foi **depreciado** no
C++17. Para trabalho sério: **ICU**, **utf8cpp** ou **Boost.Locale**.

---

## Demo 2 — `std::string_view`

### 2.1 O problema que ele resolve

`std::string_view` é **um ponteiro e um tamanho** (16 bytes) que **não possui** os caracteres.
Trocar `const std::string&` por `std::string_view` num parâmetro de leitura elimina a `std::string`
temporária que todo chamador com `const char*` era obrigado a construir:

```
sizeof(std::string) = 32      sizeof(std::string_view) = 16
contar_vogais_string(literal) → constrói uma std::string TEMPORÁRIA (aloca 45 bytes)
contar_vogais_view(literal)   → nenhuma alocação
```

E `v.substr(4, 5)` só ajusta ponteiro e tamanho, enquanto `str.substr(4, 5)` devolve uma
`std::string` **nova**.

### 2.2 ⚠ Ele não é *null-terminated*

A diferença que mais causa bug, porque o código compila e passa nos testes. Com
`texto = "abcdefghij"` e `parcial = std::string_view{texto}.substr(0, 3)`, o `parcial.data()`
aponta para o `'a'` do texto **original**, e os bytes seguintes são `'d'`, `'e'`, `'f'`… e **não**
um `'\0'`:

```
parcial = 'abc' (size=3)      printf("%s", parcial.data()) leria: 'abcdefghij'
```

Leu a string inteira. Se a origem não tivesse `'\0'` — um buffer de rede, por exemplo — seria
leitura fora de limites, e o AddressSanitizer pegaria. As três saídas corretas:

| forma | custo | quando |
|---|---|---|
| `std::string{v}.c_str()` | 1 alocação | o caminho seguro e óbvio |
| `printf("%.*s", (int)v.size(), v.data())` | zero | a API aceita `(ptr, len)` |
| garantir o `'\0'` **na origem** | zero | você controla quem produziu o buffer |

> **Regra:** `.data()` de um `string_view` **nunca** vai direto para uma função que espera
> C-string.

### 2.3 ⚠ Tempo de vida: as três regras

```cpp
std::string_view v = produz();              // (a) ✗ o temporário morreu no ';'
std::string_view f() { std::string local = "..."; return local; }  // (b) ✗ pendurado
struct Config { std::string_view nome; };   // (c) ⚠ quem garante que a origem sobrevive?
```

| uso | seguro? |
|---|---|
| **parâmetro** de função | **sempre** — é o uso dominante |
| **retorno** | só se a origem sobreviver à chamada (ex.: aponta para um membro do objeto) |
| **membro** de classe | exige documentar o dono; na dúvida, guarde `std::string` |

O caso (b) o GCC pega com `-Wreturn-local-addr`. O demo mostra o retorno correto
(`Documento::titulo()` devolve um view para o membro `conteudo_`, cuja vida é a do `Documento`) e
o caso que **parece** errado e não é: `tamanho(produz())` é seguro, porque o temporário vive até o
fim da **expressão completa**.

### 2.4 Conversões, e a que custa `strlen`

| conversão | como | custo |
|---|---|---|
| `string` → `string_view` | **implícita** | zero |
| `string_view` → `string` | **explícita** (`std::string t{v};`) | 1 alocação |
| `const char*` → `string_view` | implícita | ⚠ chama `strlen`, O(n) |

`std::string t = v;` **não compila**, e é intencional: a conversão cara tem que ser visível. Já a
de literal é implícita e silenciosa — num laço quente, é `strlen` por iteração. A saída é o sufixo
`sv` (C++17), cujo `size()` é `constexpr`: `constexpr auto s = "…"sv;`. ⚠ E a lição de encoding
volta pela porta dos fundos: a frase usada no demo tem **31 caracteres** e `s.size()` é **33**
(`ç` e `ã` ocupam 2 bytes cada) — o fonte fixa isso num `static_assert(literal_sv.size() == 33)`,
ou seja, o engano seria pego na **compilação**.

### 2.5 Escolhendo a assinatura

| assinatura | quando |
|---|---|
| `std::string_view` | a função só **lê** e não guarda — **o default** |
| `const std::string&` | precisa de `.c_str()`, ou repassa para uma API que já pede isso |
| `std::string` por valor (*sink*) | vai **guardar** uma cópia — [`demo_move_only.hpp`](../value_semantics/demo_move_only.hpp) |
| `std::string&` | vai **modificar** a do chamador |

---

## Demo 3 — `std::format`

### 3.1 O problema, antes da API

| | `printf` | `iostream` | `std::format` |
|---|---|---|---|
| segurança de tipo | **nenhuma** | sim | sim |
| erro de formato | em runtime, ou lixo | não se aplica | **erro de compilação** |
| concisão | boa | péssima | boa |
| estado global | não | **sim** (`std::hex` gruda) | não |
| tipos do usuário | impossível | `operator<<` | `formatter<T>` |

`printf("%d", 1.5)` compila e imprime lixo; `printf("%s", std::string{})` compila e é
comportamento indefinido; `printf("%d %d", 1)` compila e lê a pilha. Já `fmt::format("{:d}",
"texto")`, `fmt::format("{} {}", 1)` e `fmt::format("{", 1)` **não compilam** — pelo `consteval`
de *Antes de começar*: a string de formato é analisada em compilação, e o `throw` do analisador
vira erro de build. Com `<fmt/ranges.h>` (no `std::format`, isso chegou no C++23) os tipos da STL
já são formatáveis: `vector<int>{1,2,3}` → `[1, 2, 3]`.

### 3.2 A mini-linguagem, especificador por especificador

```
{ índice : preenchimento alinhamento sinal # 0 largura .precisão tipo }
             (qualquer     (< > ^)   (+ - ) │ │ mínima   casas   d b o x X e f g s
              caractere)                    │ └─ preencher com zeros
                                            └─── prefixo 0b / 0 / 0x
```

Cada linha é a **saída literal** do demo; `|` marca o fim do campo.

| especificador | argumento | saída |
|---|---|---|
| `{:<10}` / `{:>10}` / `{:^10}` | `"esq"` / `"dir"` / `"centro"` | `esq       \|` / `       dir\|` / `  centro  \|` |
| `{:*^10}` | `"abc"` | `***abc****\|` |
| `{:d}` / `{:+}` | `42` | `42` / `+42` |
| `{:08.3f}` | `3.14159` | `0003.142` |
| `{:.2e}` / `{:g}` | `123456.789` | `1.23e+05` / `123457` |
| `{:b}` / `{:#b}` / `{:o}` | `42` | `101010` / `0b101010` / `52` |
| `{:#x}` / `{:#X}` | `255` | `0xff` / `0XFF` |
| `{0} {1} {0}` | `"A", "B"` | `A B A` |
| `{:{}.{}f}` | `3.14159265, 12, 3` | `       3.142\|` |

As duas últimas são o que `printf` não faz bem: **reusar** um argumento sem passá-lo duas vezes, e
receber largura/precisão **dinâmicas** sem o `*` posicional.

⚠ **Não existe especificador de porcentagem.** `{:.1%}` **não compila** para `double` (nem no fmt
10, nem no `std::format` do C++20) — e essa foi uma correção real neste subprojeto, pega na
compilação, exatamente como a seção 3.1 promete. Multiplique por 100 e imprima o `%` como texto:
`fmt::print("{:.1f}%", 0.856 * 100)` → `85.6%`.

### 3.3 `format_to` — formatar sem alocar

`format` devolve uma `std::string` **nova** (aloca); `format_to` escreve em qualquer **iterador de
saída**; `format_to_n` faz o mesmo com limite de bytes; `print` vai direto ao `stdout`.

```cpp
char buffer[64];
auto r = fmt::format_to_n(buffer, sizeof(buffer) - 1, "id={} nome={}", 42, "teste");
*r.out = '\0';                                   // ⚠ format_to_n NÃO escreve o terminador

std::string acumulado;
acumulado.reserve(128);                          // → UMA alocação, não três
for (int i = 1; i <= 3; ++i)
    fmt::format_to(std::back_inserter(acumulado), "[item {}] ", i);
```

⚠ `format_to` só é livre de alocação se o destino for: um `back_inserter` num vetor sem `reserve`
realoca. `fmt::memory_buffer` é um buffer com 500 bytes inline — o mesmo princípio do SSO — que só
aloca acima disso. E é por isso que este repositório usa `fmt::print` em vez de
`std::cout << fmt::format(...)`: o segundo constrói uma string intermediária à toa.

### 3.4 Um `formatter` para o seu tipo

```cpp
template <>
struct fmt::formatter<Temperatura> {
    char unidade = 'C';
    // parse lê os especificadores entre ':' e '}'. É constexpr — lançar aqui é ERRO DE BUILD.
    constexpr auto parse(format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && (*it == 'C' || *it == 'F' || *it == 'K')) unidade = *it++;
        if (it != ctx.end() && *it != '}') throw format_error("aceita apenas C, F ou K");
        return it;
    }
    auto format(const Temperatura& t, format_context& ctx) const {   // escreve na saída
        if (unidade == 'F') return fmt::format_to(ctx.out(), "{:.1f}°F", t.celsius*9/5 + 32);
        if (unidade == 'K') return fmt::format_to(ctx.out(), "{:.2f}K",  t.celsius + 273.15);
        return fmt::format_to(ctx.out(), "{:.1f}°C", t.celsius);
    }
};
```

Para `Temperatura{25.0}`: `'{}'` → `25.0°C`, `'{:F}'` → `77.0°F`, `'{:K}'` → `298.15K`, e `'{:X}'`
→ **não compila** (o `throw` do `parse` acontece em avaliação constante). **Atalho que vale ouro:**
herdar de um `formatter` existente reaproveita a mini-linguagem inteira dele —
`struct fmt::formatter<Metros> : fmt::formatter<double>`, com um `format` que repassa `m.valor`,
faz `{:8.2f}` funcionar em `Metros` de graça.

### 3.5 O custo — e a ressalva que vale mais que o número

```
100 000 formatações da mesma linha (faixa de três execuções, build de debug):
snprintf : 19–34 ms · fmt::format_to_n : 86–129 ms · ostringstream : 58–95 ms
fmt::format : 102–147 ms
```

⚠ **Estes números não medem o que parecem medir.** O repositório linka `libfmtd`, a variante de
**debug** da fmt, e o custo está **dentro da biblioteca**, já compilada — recompilar *este* arquivo
com `-O2` quase não muda o quadro. O `snprintf` vem da glibc, sempre otimizada: comparar os dois é
comparar debug com release. A **única** comparação controlada é entre as duas linhas da fmt, que
diferem só no destino — a diferença entre `format_to_n` e `format` é o custo de **alocar** uma
`std::string` por iteração, e em código quente vale `format_to_n`.

Daí a lição metodológica, que vale mais que os números: (1) saiba em que modo a **biblioteca** foi
compilada, não só o seu código; (2) compare pares que diferem em **uma** variável só; (3)
desconfie de todo benchmark cujo autor não diz as flags — **inclusive deste**. E o argumento que
não depende de medição nenhuma: `snprintf` não tem segurança de tipo e o `ostringstream` carrega
estado global pegajoso; `format` dá as duas coisas.

---

## Demo 4 — `<charconv>`

### 4.1 As cinco formas de converter texto em número

| forma | detecta erro? | aloca? | lança? | olha o locale? |
|---|---|---|---|---|
| `atoi` | **não** (`"abc"` → 0, igual a `"0"`) | não | não | sim |
| `strtol` | via `errno` **global** + ponteiro de saída | não | não | sim |
| `std::stoi` | sim | sim (string temporária interna) | **sim** | sim |
| `istringstream` | via flags do stream | **sim** | não | **sim** |
| **`std::from_chars`** | **sim, no retorno** | **não** | **não** | **não** |

### 4.2 `from_chars`: ele nunca lança

O engano mais comum é achar que `from_chars` sinaliza erro por exceção. **Não.** Ele devolve uma
struct, `std::from_chars_result`:

```cpp
#include <charconv>
#include <system_error>

int valor = 0;
std::string_view s = "42abc";
auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), valor);
//    │     └── std::errc: vale std::errc{} em sucesso
//    └──────── const char*: ONDE a conversão parou

if      (ec == std::errc::invalid_argument)    { /* nada parseável */ }
else if (ec == std::errc::result_out_of_range) { /* não cabe no tipo */ }
else if (ptr != s.data() + s.size())           { /* ok, mas SOBROU lixo */ }
else                                           { /* consumiu tudo */ }
```

⚠ `std::errc::invalid_argument` é um **valor de enumeração**, não a exceção
`std::invalid_argument` que o `std::stoi` lança. `from_chars` **não lança nunca**, e não aloca
nunca — é o que o torna usável em código `noexcept` e em laço quente.

```
'42'    → 42 ✓                    'abc'  → invalid_argument
'-17'   → -17 ✓                   ''     → invalid_argument
'3.14'  → 3 — mas SOBROU '.14'    ' 42'  → invalid_argument (não pula espaços)
'42abc' → 42 — mas SOBROU 'abc'   '99999999999999' → result_out_of_range
```

O `ptr` é o que permite detectar lixo sobrando — `std::stoi("42abc")` devolve 42 e não reclama. E
é o mesmo `ptr` que realimenta o laço de um parser, sem alocar uma única string:

```cpp
const char* p = csv.data(); const char* fim = csv.data() + csv.size();  // "10,20,30,40,50"
while (p < fim) {
    int v = 0;
    auto [novo_p, ec] = std::from_chars(p, fim, v);
    if (ec != std::errc{}) break;
    valores.push_back(v);                          // std::vector<int>, com reserve prévio
    p = (novo_p < fim && *novo_p == ',') ? novo_p + 1 : fim;
}                                                  // → 5 valores, soma 150
```

Bases são um parâmetro, não um prefixo mágico: `std::from_chars(p, fim, x, 16)` lê `"ff"` como
255. E o que ele **não** faz é de propósito — não pula espaços, não aceita `0x` nem `+`, não
aloca, não lança, não olha o locale: a função faz **uma** coisa, do jeito mais rápido possível, e
deixa as políticas para quem chama.

### 4.3 `to_chars` e o *round-trip*

`to_chars` **sem** precisão produz a **menor** string que faz `from_chars` devolver exatamente o
mesmo `double` (algoritmos Ryū/Grisu). Nem `printf("%g")` nem os iostreams dão essa garantia — o
demo converte e reconverte, verificando:

```
0.1 → '0.1' ✓      0.3 → '0.3' ✓      1e+300 → '1e+300' ✓
0.3333333333333333 → '0.3333333333333333' ✓      123456789.12345679 → idem ✓
```

`printf("%g")` usa 6 dígitos significativos e **perde** precisão; `printf("%.17g")` sempre imprime
17 dígitos, feio e longo. Isso importa em JSON, telemetria e qualquer formato em que o número
precise voltar **idêntico**.

### 4.4 ⚠ A armadilha do locale

Com o locale global em `pt_BR` ou `de_DE`, `std::ostringstream oss; oss << 3.14;` produz `"3,14"`
(**vírgula**) e `std::stod("3.14")` devolve **3.0** — parou no ponto, e não reclamou. O bug
clássico: (1) o programa grava um CSV numa máquina `en_US` → `"3.14"`; (2) outra máquina, `pt_BR`,
lê o arquivo; (3) `stod` devolve 3.0 silenciosamente; (4) o dado fica errado para sempre. E basta
uma biblioteca de terceiros chamar `setlocale(LC_ALL, "")` na inicialização para o comportamento
do **seu** código mudar. `from_chars`/`to_chars` são **imunes**: não têm sequer um parâmetro de
locale.

> **Regra:** para **dados** (arquivos, rede, protocolos) → `<charconv>`. Locale é para
> **apresentação** — e aí `std::format` com `{:L}` faz isso explicitamente, num ponto só.

### 4.5 O custo, medido — e o número que contraria a expectativa

```
200 000 conversões texto → int (build de debug):
from_chars : 8–10 ms      std::stoi : 5–8 ms  ← mais rápido!      istringstream : 20–30 ms
```

⚠ **`from_chars` perdeu para `stoi`, e não é erro do teste** — é a build: `std::from_chars` para
inteiros é **código de cabeçalho**, compilado junto com este demo (portanto com **`-O0`**, sem
otimização nenhuma no laço de dígitos), enquanto `std::stoi` chama `strtol`, que mora na **glibc já
compilada com otimização máxima**. Só o `istringstream` perde em qualquer build, porque constrói um
objeto de stream inteiro — com locale, *sentry* e flags — a cada conversão. O resultado mede a
**build**, não as funções: em release o quadro se inverte. É a lição de 3.5 com o sinal trocado, e
por isso os dois benchmarks estão aqui — num deles a medição confirma o esperado, no outro ela o
contradiz.

O argumento **estrutural** a favor de `from_chars` não precisa de cronômetro: não aloca, não lança,
não consulta locale e diz **onde parou**. E `stoi` tem um custo que o cronômetro nem vê — ele
**lança** em entrada inválida, e cada erro custa um *unwinding* completo, medido em
[`src/errors/demo_exceptions.hpp`](../errors/demo_exceptions.hpp) como ~1000× o caminho feliz.

---

## Demo 5 — `std::regex`

### 5.1 `match` vs `search` — o erro nº 1

```
texto: 'pedido 42 entregue'   padrão: \d+
regex_match  → false   (exige que a string INTEIRA case)     regex_match("42", \d+) → true
regex_search → true    (procura em qualquer posição)
```

`std::regex_match` casa a string **inteira**; `std::regex_search`, a **primeira** ocorrência em
qualquer posição; `std::sregex_iterator`, **todas**. ⚠ Em Python, `re.match` casa no **início**;
em C++, `regex_match` exige a string **toda**.

### 5.2 Capturas, `smatch` e `sregex_iterator`

Um grupo de captura é um trecho do padrão entre parênteses. O resultado vem num **`std::smatch`**:
`m[0]` é o casamento inteiro; `m[1]`, `m[2]`… são os grupos, na ordem dos parênteses de abertura;
`m.prefix()` e `m.suffix()` são o texto antes e depois.

```cpp
const std::regex data{R"((\d{4})-(\d{2})-(\d{2}))"};
std::smatch m;
if (std::regex_search(entrada, m, data)) {   // entrada = "evento em 2026-08-25 às 14h"
    m[0].str();        // "2026-08-25"   m.position(0)==10, m.length(0)==10
    m[1].str();        // "2026"         m.prefix().str() == "evento em "
}
```

⚠ **`smatch` guarda iteradores para a string original**, não cópias. Se ela morrer, o `smatch`
fica pendurado — e o padrão levou isso a sério: a sobrecarga que recebe `const std::string&&` foi
**deletada**, então `std::regex_search(obterTexto(), m, re)` (com um temporário) **não compila**.

**Todas** as ocorrências saem de `std::sregex_iterator`, um iterador de entrada que avança de
casamento em casamento; o construtor sem argumentos é o "fim":

```cpp
const std::string log = "erro 404, erro 500, erro 403";
const std::regex  erro{R"(erro (\d{3}))"};
for (auto it = std::sregex_iterator{log.begin(), log.end(), erro};
     it != std::sregex_iterator{}; ++it)
    fmt::print("código {} na posição {}\n", (*it)[1].str(), it->position());
// código 404 na posição 0 · código 500 na posição 10 · código 403 na posição 20
```

### 5.3 `regex_replace` e o formato de substituição

O terceiro argumento é um **formato**, não texto literal: `$1`…`$9` são os grupos, `$&` é o
casamento inteiro, `` $` `` e `$'` são o texto antes e depois, e `$$` é um `$` literal. Saída real
sobre `'contato: ada@exemplo.com e grace@teste.org'` com `R"((\w+)@([\w.]+))"`:

```
"$1@***"    → 'contato: ada@*** e grace@***'
"$2 <- $1"  → 'contato: exemplo.com <- ada e teste.org <- grace'
"[$&]"      → 'contato: [ada@exemplo.com] e [grace@teste.org]'
"$1;" com std::regex_constants::format_no_copy → 'ada;grace;'
```

`format_no_copy` descarta o texto ao redor e devolve **só** as substituições — a forma mais curta
de extrair todos os campos de uma vez.

### 5.4 Gramáticas e flags

São seis gramáticas: **`ECMAScript`** é o padrão e a mais completa (`\d`, `\w`, lookahead, grupos
não capturantes); `basic`/`extended` são POSIX BRE/ERE; `awk`/`grep`/`egrep` existem para
compatibilidade com as ferramentas de mesmo nome. As flags de construção úteis: `icase` (ignora
maiúsculas), `multiline` (`^` e `$` casam em cada linha, C++17), `nosubs` (não guarda grupos →
mais rápido) e `optimize` (constrói mais devagar para casar mais rápido). Com
`std::regex{"erro", std::regex_constants::icase}`, `regex_search("Erro Grave", …)` passa de
`false` para `true`.

⚠ **Sempre use *raw string*.** Sem ela o compilador processa os escapes **antes** da regex, e `\d`
vira `\\d`: `std::regex re{"\\d+"}` contra `std::regex re{R"(\d+)"}`. ⚠ E **construir um
`std::regex` é caro** — ele compila a expressão; nunca dentro de um laço:
`static const std::regex re{padrao};` fora dele.

### 5.5 ⚠ O custo, medido — e ele é brutal

```
extraindo 'codigo=(\d+)' de uma linha de log (três execuções, build de debug):
find + from_chars    :      1 ms   (20 000 iterações)
regex fora do laço   : 155–211 ms  (20 000 iterações)
regex DENTRO do laço : 274–310 ms  ( 2 000 iterações — 10× MENOS trabalho!)
```

**Construir a regex no laço domina tudo**: a terceira linha fez 10× menos trabalho e ainda assim
levou **mais** tempo que a segunda — cerca de **20× mais lento por iteração**. E mesmo
pré-construída, a regex perde para `find` + `from_chars` por cerca de **150×** aqui: para um
padrão **fixo** e simples, ela não se paga. A razão encolhe em release, mas a ordem de grandeza
não muda — a implementação de `std::regex` da libstdc++ é notoriamente lenta, e isso é consenso,
não opinião deste README.

**Quando `std::regex` é a escolha certa:** o padrão vem do **usuário** (configuração, campo de
busca) e não há como escrever um parser específico; a expressão é complexa e roda uma vez; ou
legibilidade importa mais que velocidade. **Quando não usar:** laço quente com padrão fixo
(`find`/`starts_with`/`from_chars`); formato conhecido (escreva o parser —
[`src/parser/`](../parser/), com Flex e Bison); validação simples (`std::all_of` com um
predicado); desempenho real (**RE2**, **PCRE2**, **CTRE** — este compila a regex **em tempo de
compilação**, com templates).

⚠ **E há um risco de segurança.** Como definido em *Antes de começar*, `std::regex` usa
**backtracking**: `(a+)+$` contra uma entrada longa leva tempo **exponencial** — é o ataque
**ReDoS**. Nunca aplique regex de terceiros a entrada de terceiros; se o padrão vem de fora, use
um motor sem backtracking (RE2) ou imponha limite de tempo.

---

## Como validar

**1. Rodar e conferir as linhas que provam cada afirmação.**

```bash
make build && ./dist/bin/strings ; echo "código de saída: $?"     # esperado: 0
```

| linha da saída | confirma |
|---|---|
| `sizeof(std::string) = 32 bytes` | o diagrama do objeto em *Antes de começar* |
| `15 chars → 0 alocação(ões)` seguido de `16 chars → 1` | o limite exato do SSO |
| `distância … curta (3 chars): 16 bytes` | que a string curta mora **dentro** do objeto |
| `sem reserve → 4` / `com reserve → 1` | o valor do `reserve` (1.2) |
| `reserve + append → 1 aloc, 97 bytes` | a regra de concatenação (1.3) |
| `contains("para") = true  (C++23)` | que o override `cpp_std=c++23` está ativo |
| `"café".size() = 5` e `"..."sv.size() = 33` | `size()` conta bytes (1.6 e 2.4) |
| `' 42' → invalid_argument` | que `from_chars` não pula espaços (4.2) |
| cinco linhas `✓ round-trip exato` | a garantia do `to_chars` (4.3) |
| `find + from_chars : 1 ms` vs `regex fora do laço : ~150 ms` | o custo do regex (5.5) |

**2. Compilar à mão, com todos os avisos.**

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -O0 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/strings/main.cpp -o /tmp/st -L$FMTP/lib -lfmtd
LD_LIBRARY_PATH=$FMTP/lib /tmp/st | head -40
```

Esperado: **zero avisos** e o demo 1 na tela. Trocar para `-std=c++20` **falha** com
`'std::basic_string_view<char>' has no member named 'contains'` — a forma mais rápida de conferir
que o override do `meson.build` não é decorativo. ⚠ Com `-O2` aparecem **9 avisos
`-Wmismatched-new-delete`**, e são **falsos positivos** esperados: ao inlinar o `operator delete`
substituído, o GCC vê um `free()` sobre um ponteiro que "veio de `operator new`" e não conclui que
esse `new` é justamente o que chamou `malloc`. O vizinho
[`demo_implementacao.hpp`](../type_erasure/demo_implementacao.hpp) silencia o mesmo aviso com um
`#pragma GCC diagnostic`; aqui ele não é silenciado, e no build padrão (`-O0`) não aparece.

**3. Sob os sanitizers** — é assim que se prova que nenhum `string_view` ficou pendurado e que
nenhuma leitura passou do fim de um buffer:

```bash
g++ -std=c++23 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/strings/main.cpp -o /tmp/stsan -L$FMTP/lib -lfmtd
LD_LIBRARY_PATH=$FMTP/lib /tmp/stsan > /dev/null ; echo "código de saída: $?"
```

Esperado: saída **0**, sem nenhuma linha `runtime error:` ou `ERROR: AddressSanitizer`. E, para
números que digam algo sobre as bibliotecas em vez de sobre a build, refaça as medições em
release: `conan install . -s build_type=Release --build=missing`, compilando com `-O2 -DNDEBUG` e
linkando `-lfmt`.

---

## Referências

**Neste repositório:** [`src/ranges/`](../ranges/) (`views::split`, a versão da STL do utilitário
que 1.4 escreve à mão) ·
[`src/compile_time/demo_consteval.hpp`](../compile_time/demo_consteval.hpp) (o `consteval` que
sustenta o demo 3) · [`src/vocabulary_types/`](../vocabulary_types/) (`string_view` no conjunto
maior de tipos de vocabulário) ·
[`src/value_semantics/demo_move_only.hpp`](../value_semantics/demo_move_only.hpp) (quando passar
`std::string` por valor, citado em 2.5) · [`src/layout/`](../layout/) (por que
`sizeof(std::string)` é 32, e o que isso custa num vetor grande) ·
[`src/errors/demo_exceptions.hpp`](../errors/demo_exceptions.hpp) (o custo do `throw` que torna
`stoi` inadequado a um parser, 4.5) · [`src/parser/`](../parser/) (Flex e Bison, a alternativa
séria à regex) · [`src/performance/`](../performance/) (como medir sem se enganar) ·
[`src/linkage/`](../linkage/) (ODR: por que o `operator new` instrumentado mora num cabeçalho
incluído uma vez só) · [`src/type_erasure/`](../type_erasure/) (o `char_traits` de 1.5 é política
resolvida em compilação; ali está a mesma ideia resolvida em execução).

**Fora daqui:** cppreference (*basic_string*, *basic_string_view*, *format*, *charconv*,
*basic_regex*) · **P0220R1**, a proposta que levou `std::string_view` ao C++17 (wg21.link/p0220) ·
**P0645R10**, *Text Formatting*, a proposta do `std::format`, de Victor Zverovich, a partir da
biblioteca **fmt** (wg21.link/p0645) · **P0067R5**, *Elementary string conversions*, a proposta do
`<charconv>` (wg21.link/p0067) · Ulf Adams, *Ryū: fast float-to-string conversion* (PLDI 2018), o
algoritmo por trás do *round-trip* do `to_chars` · **RE2** (github.com/google/re2), motor sem
backtracking com tempo linear garantido · **CTRE**
(github.com/hanickadot/compile-time-regular-expressions), regex compilada em tempo de compilação ·
*UTF-8 Everywhere* (utf8everywhere.org), por que `std::string` + UTF-8 é a escolha certa em quase
todo lugar.
