# bits — Manipulação de Bits

## O que é este projeto?

Um curso completo sobre o **bit**: os quatro operadores (`&`, `|`, `^`, `~`), as três regras
que produzem comportamento indefinido, o cabeçalho `<bit>` do C++20, `std::bitset`, os campos
de bits nativos, e como serializar bytes de um jeito que a outra ponta consiga ler.

Tudo o que este README afirma é **impresso pelo programa**. Compile e rode:

```bash
make build          # configura, compila e instala em ./dist/
./dist/bin/bits     # roda os 5 demos
```

> `make build` faz três coisas que um `meson compile` solto não faz: usa o diretório de build
> `build/`, carrega o ambiente do Conan (`source build/conanbuild.sh`, sem o qual o meson não
> acha a `fmt`) e instala os binários em `dist/bin/`. É por isso que o executável está em
> `./dist/bin/bits`, e não em `build/`.

> ⚠ Compila com **C++23**, porque a seção 5.3 usa `std::byteswap`. O cabeçalho `<bit>` em si é
> C++20. O `meson.build` deste diretório aplica o `override_options: ['cpp_std=c++23']` só
> neste alvo — o resto do repositório continua em C++20.

---

## Antes de começar

Esta seção define **todo** o vocabulário usado no restante do README. Se você já sabe ler
`0x0F` em binário, pule para "Por que estudar isto?".

### Bit, byte, e o valor de cada posição

Um **bit** vale 0 ou 1. Um **byte** são 8 bits. Um número inteiro na memória é só uma fila de
bits, e cada posição vale uma potência de dois:

```
posição (índice)   7    6    5    4    3    2    1    0
valor da posição  128   64   32   16    8    4    2    1
exemplo            0    0    0    0    1    0    1    0   →  8 + 2 = 10
```

Duas regras que decorrem disso e que você vai usar o tempo todo:

- **o bit de índice `i` vale 2^i**;
- **a máscara que isola o bit `i` é `1u << i`** — o número 1 empurrado `i` casas para a
  esquerda. `1u << 3` é `00001000`, que vale 8.

### Hexadecimal: por que ele existe

Escrever `00001111` cansa. Como **4 bits cabem exatamente em 1 dígito hexadecimal**, o hex é
uma taquigrafia perfeita para binário — cada dígito hex é um grupo de 4 bits, sem conta
nenhuma:

| Hexadecimal | Binário | Decimal | Como ler |
|---|---|---|---|
| `0x0F` | `0000 1111` | 15 | os 4 bits de baixo ligados |
| `0xF0` | `1111 0000` | 240 | os 4 bits de cima ligados |
| `0xFF` | `1111 1111` | 255 | todos os 8 bits — o maior `uint8_t` |
| `0x80` | `1000 0000` | 128 | só o bit mais significativo |

A tabela de conversão de um dígito: `0`→`0000`, `1`→`0001`, `2`→`0010`, `4`→`0100`,
`7`→`0111`, `8`→`1000`, `A`→`1010`, `C`→`1100`, `E`→`1110`, `F`→`1111`.

Em C++ você escreve o mesmo número de três formas: `0b0000'1111` (binário, C++14, com o
separador de dígitos `'`), `0x0F` (hex) e `15` (decimal). São o **mesmo** valor — a base é só
notação para o programador; o hardware não sabe a diferença.

### Como imprimir bits

Toda a saída deste subprojeto está em binário. Você reproduz qualquer uma delas de duas
formas:

```cpp
fmt::print("{:08b}\n", x);                        // 8 dígitos, com zeros à esquerda
fmt::print("{}\n", std::bitset<8>{x}.to_string()); // o mesmo, via bitset
```

O `demo_operadores.hpp` usa a segunda, num helper de 3 linhas que serve para qualquer tipo:

```cpp
template <typename T>
std::string bin(T v) {
    return std::bitset<sizeof(T) * 8>{static_cast<unsigned long long>(v)}.to_string();
}
```

### O glossário deste README

| Termo | O que significa |
|---|---|
| **UB** (*undefined behavior*, comportamento indefinido) | o padrão da linguagem não exige **nada** do programa. O compilador tem o direito de assumir que a situação nunca acontece e otimizar em cima disso — por isso o sintoma costuma aparecer só com `-O2`, só na outra máquina, e meses depois. Ver [`../undefined_behavior/`](../undefined_behavior/). |
| ***type punning*** | ler os mesmos bits como se fossem de outro tipo — por exemplo, olhar um `float` como se fosse um `uint32_t` para inspecionar expoente e mantissa. |
| ***strict aliasing*** | a regra que permite ao compilador assumir que um `float*` e um `int*` **nunca** apontam para o mesmo objeto. É o que torna `*(int*)&f` UB, e não apenas feio: o compilador pode reordenar a leitura e a escrita porque "provou" que não se cruzam. |
| ***data race*** | dois acessos concorrentes à mesma posição de memória, pelo menos um deles de escrita, sem sincronização. O C++11 define isso como UB. Ver [`../concurrency2/`](../concurrency2/). |
| **unidade de alocação** | o pedaço de memória (tipicamente uma palavra de 32 ou 64 bits) que o compilador realmente lê e escreve para chegar num campo de bits. Você pede 4 bits; a CPU move a palavra inteira. |
| **distância de Hamming** | quantos bits diferem entre dois valores. `popcount(a ^ b)` calcula em uma instrução — o `^` marca com 1 exatamente as posições diferentes. |
| **CVE** | *Common Vulnerabilities and Exposures*, o registro público de falhas de segurança. "Já foi CVE" quer dizer: alguém explorou isso de verdade, em software de verdade. |
| ***proxy*** | um objeto que finge ser uma referência. `vector<bool>::operator[]` não pode devolver um `bool&` (não existe endereço de um bit), então devolve um objeto intermediário que sobrecarrega `=` e a conversão para `bool`. |

### O exemplo mínimo

Este programa completo compila e roda. É o vocabulário inteiro do Demo 1 em 17 linhas:

```cpp
#include <bitset>
#include <cstdint>
#include <fmt/core.h>

int main() {
    std::uint8_t flags = 0;                      // 00000000
    constexpr std::uint8_t LEITURA = 1u << 0;    // 00000001 — bit 0
    constexpr std::uint8_t ESCRITA = 1u << 1;    // 00000010 — bit 1

    flags |= LEITURA;                                     // LIGA o bit 0
    flags |= ESCRITA;                                     // LIGA o bit 1
    flags = static_cast<std::uint8_t>(flags & ~ESCRITA);  // DESLIGA o bit 1

    fmt::print("flags   = {}\n", std::bitset<8>{flags}.to_string());
    fmt::print("leitura = {}\n", (flags & LEITURA) != 0);   // TESTA o bit 0
    fmt::print("escrita = {}\n", (flags & ESCRITA) != 0);   // TESTA o bit 1
}
```

Saída, verificada:

```
flags   = 00000001
leitura = true
escrita = false
```

Repare no `static_cast<std::uint8_t>` da linha do "desligar". Ele não é firula: sem ele o
compilador avisa com `-Wconversion`, e o motivo é a **promoção inteira** — o assunto da
primeira armadilha logo abaixo.

---

## Por que estudar isto?

Porque `&`, `|`, `<<` parecem os operadores mais simples da linguagem e escondem três
armadilhas que o compilador, por padrão, não denuncia:

```cpp
uint8_t x = 0x0F;
~x                    // NÃO é 0xF0 — é -16, um int    (promoção inteira)
uint64_t m = 1 << 40; // ✗ UB: o literal `1` é int      (shift fora de faixa)
int f : 1; f = 1;     // f vale -1                       (bit de sinal)
```

Nenhuma das três dá erro de compilação. As duas primeiras dão o valor errado; a terceira é
UB — o programa pode fazer qualquer coisa, inclusive funcionar até o dia em que você trocar
de compilador.

---

## Estrutura de arquivos

```
src/bits/
├── main.cpp                ← ponto de entrada, executa os 5 demos, cor slate_gray
├── demo_operadores.hpp     ← 1 · máscaras, promoção inteira, shift UB, campos portáveis
├── demo_bit.hpp            ← 2 · bit_cast, popcount, bit_ceil, rotl
├── demo_bitset.hpp         ← 3 · a interface, ordem dos índices, crivo de Eratóstenes
├── demo_bitfields.hpp      ← 4 · sintaxe nativa, layout medido, e por que evitar
├── demo_endianness.hpp     ← 5 · std::endian, byteswap, serialização correta
├── meson.build
└── README.md
```

---

## Demo 1: Operadores

### As quatro operações com máscara

Toda manipulação de bits se reduz a estas quatro linhas. `M` é a máscara — o valor com 1
exatamente nas posições que interessam:

| Operação | Idioma | Por que funciona |
|---|---|---|
| testar | `(x & M) != 0` | `&` só deixa passar os bits que estão ligados nos **dois** |
| ligar | `x \|= M` | `\|` liga os bits de `M` e preserva o resto |
| desligar | `x &= ~M` | `~M` tem 0 onde `M` tinha 1; o `&` zera só essas posições |
| alternar | `x ^= M` | `^` inverte onde `M` tem 1 (1^1=0, 0^1=1) e preserva onde tem 0 |

### ⚠ Promoção inteira

Todo operando de tipo **menor que `int`** (`char`, `short`, `uint8_t`, `uint16_t`) é
**promovido a `int`** antes de qualquer operação aritmética ou de bit. O resultado é um `int`
de 32 bits, com sinal:

```
uint8_t x = 0x0F;
~x como int       : -16 (0xFFFFFFF0)      ← não é 0xF0
uint8_t(0xFF) << 4 = 0xFF0 (12 bits!)     ← não trunca em 8 bits
```

O bug real: `if (flags == ~MASCARA)` compara um `uint8_t` (0 a 255) com um `int`
**negativo** — a igualdade nunca é verdadeira, e o compilador não reclama.

Defesas: use `unsigned`/`uint32_t` para máscaras, faça o `static_cast` de volta ao atribuir, e
compile com `-Wconversion` para o compilador cobrar isso de você.

### ⚠ Shift fora de faixa

`x << n` e `x >> n` são **UB** se `n < 0` ou se `n >=` o número de bits de `x` **após a
promoção**:

```cpp
uint32_t x = 1;  x << 32;    // ✗ UB
uint8_t  b = 1;  b << 8;     // ✓ ok! b virou int (32 bits) — a mesma promoção, salvando
uint64_t m = 1 << 40;        // ✗ UB — o literal `1` é int, e int tem 32 bits
uint64_t m = 1ull << 40;     // ✓ o literal já é de 64 bits
```

A terceira linha é o erro mais comum da lista, porque **parece** correta: o destino é
`uint64_t`, mas a conta acontece antes da atribuição, no tipo do literal. O UBSan pega em
runtime, com a mensagem `runtime error: shift exponent 32 is too large`.

> Uma regra que **mudou** no C++20: `-1 << 1` era UB até o C++17; agora é definido, porque
> o C++20 fixou complemento de dois como representação obrigatória dos inteiros com sinal.

### `>>` em signed vs unsigned

O deslocamento à direita preenche o buraco da esquerda com algo — e esse "algo" depende de o
tipo ter sinal ou não:

```
int32_t(-16) >> 2 = -4           ← preencheu com o bit de SINAL (1s)
uint32_t     >> 2 = 1073741820   ← preencheu com zeros

-7 >> 1 = -4    (arredonda para -∞)
-7 /  2 = -3    (trunca para zero)
```

> Para manipulação de **bits**, use sempre `unsigned`. `signed` é para **aritmética**.
> Guarde esta frase: ela volta a importar na serialização do Demo 5.

### Campos portáveis, à mão

A seção 1.5 do demo mostra a alternativa portável aos campos de bits nativos do Demo 4: um
`constexpr CampoBits{offset, largura}` que gera a máscara e faz `extrair`/`inserir` com shift.
O layout passa a ser definido pelo **seu** código, e os offsets são verificados por
`static_assert` — um erro de posição vira erro de build, não bug de produção.

---

## Demo 2: `<bit>`

### `bit_cast` — o fim do type punning informal

Você tem um `float` e quer ver os bits dele como `uint32_t`. As quatro formas historicamente
usadas, e o veredito de cada uma:

| Forma | Veredito |
|---|---|
| `*(int*)&f` | ✗ viola *strict aliasing* — UB |
| `union { float f; int i; }` | ✗ ler o membro inativo é UB em C++ (é legal em C) |
| `memcpy(&i, &f, 4)` | ✓ correto, mas verboso e **não** é `constexpr` |
| `std::bit_cast<int>(f)` | ✓ correto, conciso, **e** `constexpr` |

```cpp
constexpr auto b = std::bit_cast<uint32_t>(2.0f);
static_assert(b == 0x40000000u);        // ✓ verificável em compilação
```

Os dois requisitos são checados em compilação: `sizeof(To) == sizeof(From)` e ambos os tipos
trivialmente copiáveis. Não há como errar em runtime.

O demo usa isso para dissecar um `float`: para `1.0f`, sinal 0, expoente 127 (com o viés de
127, expoente real 0) e mantissa zerada — e para mostrar por que `nan != nan` é a única forma
portátil de detectar NaN.

### Contagem

```
v = 00001011011000000000000000000000
  popcount    = 5    bits 1 no total
  countl_zero = 4    zeros à esquerda  (l de left)
  countr_zero = 21   zeros à direita   (r de right)
```

| Função | Uso prático |
|---|---|
| `popcount` | distância de Hamming, cardinalidade de um conjunto guardado em bitmask, paridade |
| `countl_zero` | posição do bit mais significativo → `⌊log2⌋` |
| `countr_zero` | alinhamento de um ponteiro, próximo slot livre num alocador de bitmap |

### Potências de dois

| Valor | `has_single_bit` | `bit_width` | `bit_floor` | `bit_ceil` |
|---|---|---|---|---|
| 0 | não | 0 | 0 | 1 |
| 7 | não | 3 | 4 | 8 |
| 8 | sim | 4 | 8 | 8 |
| 100 | não | 7 | 64 | 128 |
| 1000 | não | 10 | 512 | 1024 |

O uso canônico é dimensionar buffer: pediram 1000 elementos, aloque `bit_ceil(1000)` = 1024,
porque com uma potência de dois o índice circular vira `i & (n - 1)` em vez de `i % n`.

Antes do C++20, `has_single_bit(v)` era `v && !(v & (v - 1))` — correto, ilegível, e errado
para `v == 0` se você esquecesse o primeiro termo.

> ⚠ `bit_ceil(v)` é **UB** se o resultado não couber no tipo: `bit_ceil(uint32_t{0x80000001})`
> precisaria de 2^32. Verifique antes que `v <= bit_floor(max/2)`.

### Rotação

```
v          = 10010011
v << 3     = 10011000    (shift: entram zeros, os bits que saem se perdem)
rotl(v, 3) = 10011100    (rotação: os bits VOLTAM pelo outro lado)
rotl(v, 11)= 10011100    (11 % 8 = 3 — sem UB)
rotl(v, -3)= 01110010    (negativo inverte o sentido: vira rotr)
```

Diferente do shift, `rotl`/`rotr` **não têm UB** para nenhuma contagem: ela é reduzida módulo
a largura do tipo. Antes do C++20 escrever isso à mão era `(v << n) | (v >> (32 - n))` — que
é UB quando `n == 0`, porque vira um shift de 32.

### O código gerado

| Função | Instrução x86-64 | Extensão |
|---|---|---|
| `std::popcount` | `POPCNT` | SSE4.2 |
| `std::countl_zero` | `LZCNT` ou `BSR` | BMI1 |
| `std::countr_zero` | `TZCNT` ou `BSF` | BMI1 |
| `std::rotl`/`rotr` | `ROL`/`ROR` | base |
| `std::bit_cast` | nenhuma | — |

Mas só se o compilador puder emitir a extensão: sem `-msse4.2` ou `-march=native`, o GCC
gera a rotina genérica em software (o *Hamming weight*, ~12 instruções). O experimento está
em [Como validar](#como-validar).

---

## Demo 3: `std::bitset`

```cpp
b.test(3)  vs  (flags >> 3) & 1
b.set(3)   vs  flags |= (1u << 3)
b.reset(3) vs  flags &= ~(1u << 3)
b.count()  vs  __builtin_popcount(flags)
```

Mais legível e igualmente rápido — compila para as mesmas instruções.

> ⚠ `test(i)` verifica os limites e lança `std::out_of_range`; `b[i]` **não** verifica — fora
> da faixa é UB. É a mesma dupla de `vector::at` e `vector::operator[]`.

### ⚠ A ordem dos índices

O índice 0 é o bit **menos** significativo — o mais à **direita** na string:

```
b.set(0) → to_string() = "00000001"
b.set(7) → to_string() = "10000000"
```

Regra: `índice i → valor 2^i` · `string[j] → índice (N-1-j)`. A string lê da esquerda para a
direita como um número; os índices contam da direita para a esquerda, como potências.

### bitset vs as alternativas

| Opção | Tamanho | Legível | Seguro | Dinâmico |
|---|---|---|---|---|
| `uint32_t` + máscara | fixo | não | não | não |
| `enum class` flags | fixo | **sim** | **sim** | não |
| `std::bitset<N>` | fixo | **sim** | sim | não |
| `vector<bool>` | **dinâmico** | sim | não\* | **sim** |
| `vector<char>` | dinâmico | sim | sim | sim |

\* devolve *proxy* — ver [`../stl/demo_pitfalls.hpp`](../stl/demo_pitfalls.hpp).

```
sizeof: bitset<8>=8  bitset<64>=8  bitset<65>=16  bitset<100>=16
```

O bitset arredonda para múltiplos da palavra — `bitset<65>` já custa 16 bytes.

No crivo de Eratóstenes da seção 3.5, `bitset<1001>` ocupa 128 bytes contra 1001 de um
`bool[1001]`: 8× menos memória, e o conjunto inteiro cabe no cache L1 — que na prática importa
mais que o tamanho em si. O mesmo crivo aparece rodando em tempo de **compilação** em
[`../compile_time/demo_constexpr.hpp`](../compile_time/demo_constexpr.hpp).

---

## Demo 4: Campos de Bits

A sintaxe nativa parece a solução ideal para protocolos binários. **Não é.**

```cpp
struct CabecalhoIP { unsigned versao : 4; unsigned ihl : 4; /* ... */ };
h.versao = 4;                      // com campo
b = (b & 0x0F) | (4 << 4);         // sem campo — o que o compilador faz por baixo
```

### ⚠ Quase tudo é *implementation-defined*

- a **ordem** dos campos dentro da palavra (o primeiro pode ir no bit mais alto ou no mais baixo);
- se um campo pode **atravessar** a fronteira da unidade de alocação;
- o alinhamento e o tamanho total.

Neste compilador (GCC/x86-64, little-endian) o primeiro campo ficou nos bits **menos**
significativos — mas isso é uma observação, não uma garantia.

> Um struct com campos de bits **não é portável** entre compiladores nem entre arquiteturas.
> Escrevê-lo direto num socket é um bug esperando o dia em que a outra ponta compilar com
> outra coisa.

### ⚠ Não têm endereço

```cpp
fmt::print("{}", h.versao);    // ✗ error: cannot bind bit-field to 'unsigned char&'
fmt::print("{}", +h.versao);   // ✓ o `+` unário promove a um int temporário
&x.campo                       // ✗ não compila
unsigned& r = x.campo;         // ✗ não compila
```

Um campo de bits não ocupa um endereço próprio — ele mora *dentro* de um byte compartilhado.
Este demo precisou do `+` em cada campo, e isso **é** a lição.

### ⚠ O campo signed de 1 bit

```
struct { int com_sinal : 1; };
f.com_sinal = 1;    →  vale -1     ← o único bit É o bit de sinal
```

O GCC avisa: *"overflow in conversion... changes value from '1' to '-1'"*. O bug clássico é
`if (f.ativo == 1)`, que é **falso**, enquanto `if (f.ativo)` é verdadeiro (−1 é *truthy*).

> Campos de bits são **sempre** `unsigned` ou `bool`. Note que `char x : 1` tem o mesmo
> problema, porque a assinatura de `char` também é definida pela implementação.

### Custos não óbvios

- ler/escrever um campo vira shift + máscara — mais instruções que acessar um byte inteiro;
- **dois campos na mesma palavra não podem ser escritos por threads diferentes ao mesmo
  tempo.** Cada escrita lê a palavra inteira, altera alguns bits e a grava de volta
  (*read-modify-write*); uma escrita apaga a outra. Isso é um **data race** — acesso
  concorrente à mesma posição de memória com pelo menos uma escrita e sem sincronização —, o
  que o C++11 define explicitamente como UB. Ver [`../concurrency2/`](../concurrency2/).

### Campos anônimos e `: 0`

```cpp
unsigned : 5;   // 5 bits de PREENCHIMENTO, sem nome
unsigned : 0;   // força o próximo campo a começar numa nova unidade de alocação
```

Servem para **mapear** um layout que já existe (um registrador de hardware com bits
reservados), documentando os buracos em vez de escondê-los. No demo, o `: 0` levou o `sizeof`
de 4 para 8 bytes.

| Objetivo | Ferramenta |
|---|---|
| economizar memória **interna** | campos de bits |
| protocolo binário / arquivo | máscaras + shift (seção 1.5) |
| registrador de hardware | máscaras + `volatile` |
| flags nomeadas | `enum class` + operadores |
| muitos bits homogêneos | `std::bitset` |

---

## Demo 5: Endianness

**Endianness** é a ordem em que os bytes de um número são guardados na memória. Little-endian
põe o byte **menos** significativo no **menor** endereço:

```
uint32_t 0x12345678 → 78 56 34 12      ← little-endian (x86-64, ARM comum)
```

Note que o endianness afeta a ordem dos **bytes**, nunca a dos **bits** dentro de um byte.

Desde o C++20, `std::endian::native` responde isso em tempo de **compilação** — dá para usar
em `if constexpr` e podar o ramo errado. Antes, o truque era `*(char*)&x == 1`, que é UB por
*strict aliasing*.

### ⚠ O erro de serializar o struct inteiro

```
struct Pacote { uint16_t tipo; uint32_t tamanho; uint8_t versao; };
  sizeof = 12 bytes (os campos somam 7 — o resto é PADDING)

write(fd, &p, sizeof(p)) escreve:
  02 01 00 00 06 05 04 03 07 00 00 00
```

Três problemas silenciosos: **padding** (bytes de valor indeterminado — enviá-los vaza memória
do processo, e já foi CVE) · **endianness** (a ordem depende da arquitetura de quem escreveu)
· **layout** (outro compilador pode ordenar ou alinhar diferente).

Nenhum aparece em teste, porque quem escreve e quem lê é o mesmo binário. Aparece no dia em
que a outra ponta muda. O mecanismo do padding está em [`../layout/`](../layout/).

### A forma correta: escrever e ler campo a campo

O par de funções abaixo é o subprojeto inteiro em 14 linhas. Escrever:

```cpp
// Escreve em BIG-ENDIAN (network byte order), independente da máquina.
template <typename T>
void escrever(T v) {
    static_assert(std::is_unsigned_v<T>, "serialize só tipos sem sinal");
    for (int i = sizeof(T) - 1; i >= 0; --i)                 // do byte mais significativo…
        saida_.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
}
```

E ler — o inverso exato, montando o valor byte a byte:

```cpp
template <typename T>
T ler() {
    static_assert(std::is_unsigned_v<T>);
    T v = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i)
        v = static_cast<T>((v << 8) | dados_[pos_++]);       // …e de volta, na mesma ordem
    return v;
}
```

```
serializado campo a campo, big-endian:
  01 02 03 04 05 06 07        ← 7 bytes, contra 12 do struct cru
round-trip: tipo=0x0102 tamanho=0x03040506 versao=0x07 → ✓ idêntico
```

Funciona porque **deslocamento opera sobre o valor numérico**, não sobre os bytes: `v >> 24`
pega o byte mais significativo em qualquer arquitetura. Endianness só existe quando você olha
os **bytes**.

> ⚠ Repare no `static_assert(std::is_unsigned_v<T>)` nas duas funções — ele não está ali por
> capricho. Com um tipo **com sinal**, `v >> (i * 8)` propaga o bit de sinal (a lição da seção
> 1.4): serializar `int16_t{-1}` daria `FF FF`, mas serializar `int32_t{-1}` truncado para 2
> bytes também daria `FF FF`, e o `<<` da leitura sobre um tipo com sinal pode estourar o bit
> de sinal. A regra é a mesma do Demo 1: **bits ⇒ `unsigned`**. Se o seu formato tem campos
> com sinal, converta para o `uint` do mesmo tamanho com `std::bit_cast` antes de escrever, e
> de volta depois de ler.

> É o mecanismo por trás de Protobuf, MessagePack e Cap'n Proto. Para o caso real, com socket
> de verdade, ver [`../network/`](../network/).

E quando você **precisa** só inverter a ordem — porque o formato é little-endian e a máquina é
big-endian, ou vice-versa:

| Forma | Nota |
|---|---|
| `std::byteswap(v)` | C++23, portável, uma instrução (`BSWAP`) |
| `__builtin_bswapNN(v)` | GCC/Clang, uma instrução |
| `htonl`/`ntohl` | POSIX, só 16/32 bits, só para rede |
| shifts à mão | portável e feio; o compilador geralmente reconhece o idioma |

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Manipular bits | tipos `unsigned` — sempre |
| Máscara com literal | `1u` ou `1ull`, nunca `1` |
| Aplicar `~` num tipo pequeno | `static_cast` de volta, e `-Wconversion` |
| Contar/achar bits | `<bit>` — nunca laço à mão |
| Reinterpretar bits | `std::bit_cast` — nunca cast de ponteiro nem union |
| Rotacionar | `std::rotl`/`rotr` |
| Conjunto fixo de bits | `std::bitset<N>` |
| Flags nomeadas | `enum class` + operadores |
| Campo de bits | `unsigned` ou `bool`, e `+campo` para imprimir |
| Struct que vai para rede/arquivo | serialize campo a campo — **nunca** `memcpy` |
| Trocar ordem de bytes | `std::byteswap` (C++23) |

---

## Como validar

### 1. Rodar os demos

```bash
make build
./dist/bin/bits
```

A última linha tem de ser:

```
✓ Todos os demos de bits concluídos!
```

Três linhas confirmam as lições centrais — procure por elas na saída:

| Linha esperada | O que ela prova |
|---|---|
| `~x como int       : -16 (0xFFFFFFF0)` | a promoção inteira transformou um `uint8_t` num `int` negativo |
| `com_sinal = -1` | um campo `int : 1` guarda o bit de sinal, não o valor 1 |
| `round-trip: … → ✓ idêntico` | a serialização campo a campo fecha o círculo (`escrever` + `ler`) |

### 2. Ver a instrução `POPCNT` aparecer — e sumir

Este é um experimento com **dois** resultados, e os dois ensinam:

```bash
echo 'int f(unsigned x){return __builtin_popcount(x);}' \
  | g++ -O2 -msse4.2 -S -x c++ - -o - | grep -i popcnt
```

Saída esperada — uma instrução, uma linha:

```
	popcntl	%edi, %eax
```

Agora **sem** a flag da extensão:

```bash
echo 'int f(unsigned x){return __builtin_popcount(x);}' \
  | g++ -O2 -S -x c++ - -o - | grep -ci popcnt
```

Saída esperada: `0`. O grep vazio aqui **não** é falha do experimento — é o resultado: sem
`-msse4.2` o alvo padrão do GCC não garante SSE4.2, e ele emite a rotina genérica em software
(a sequência de shifts e máscaras do *Hamming weight*). Tire o `grep` para ver o assembly
inteiro dos dois casos lado a lado.

### 3. Confirmar o endianness da sua máquina

A linha `esta máquina é LITTLE-endian` da seção 5.1 tem de bater com:

```bash
lscpu | grep -i "byte order"
```

---

## Referências

### Neste repositório

- [`../undefined_behavior/`](../undefined_behavior/) — o que é UB, e por que ele só aparece
  com `-O2`. Leitura obrigatória para as seções 1.2 e 1.3.
- [`../concurrency2/`](../concurrency2/) — data race, `happens-before` e o modelo de memória
  do C++, que definem o problema da seção 4.5.
- [`../layout/`](../layout/) — `sizeof`, alinhamento e padding: por que o `Pacote` da seção 5.4
  ocupa 12 bytes para 7 de dado.
- [`../network/`](../network/) — serialização atravessando um socket de verdade.
- [`../stl/demo_pitfalls.hpp`](../stl/demo_pitfalls.hpp) — o proxy de `vector<bool>`.
- [`../compile_time/demo_constexpr.hpp`](../compile_time/demo_constexpr.hpp) — o mesmo crivo,
  em tempo de compilação.
- [`../inline_asm/`](../inline_asm/) e [`../asm_course/`](../asm_course/) — o lado do assembly
  das instruções da seção 2.5.

### Fora

- [cppreference: `<bit>`](https://en.cppreference.com/w/cpp/header/bit) — `bit_cast`,
  `popcount`, `rotl`, `endian` e o resto do cabeçalho.
- [cppreference: `std::bitset`](https://en.cppreference.com/w/cpp/utility/bitset)
- [cppreference: campos de bits](https://en.cppreference.com/w/cpp/language/bit_field) — a
  lista oficial do que é *implementation-defined*.
- [P0476 — `bit_cast`](https://wg21.link/p0476) e [P0553 — operações de bit](https://wg21.link/p0553):
  as propostas que trouxeram o `<bit>`, com a justificativa de cada função.
- **Hacker's Delight**, Henry S. Warren Jr. — a fonte clássica dos idiomas de bit. Quase tudo
  que virou função do `<bit>` no C++20 está lá, escrito à mão, com a prova de por que funciona.
