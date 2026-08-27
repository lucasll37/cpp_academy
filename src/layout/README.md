# layout — Layout de Objetos na Memória

## O que é este projeto?

Como os bytes de um objeto são realmente arrumados: **alinhamento**, **padding**, as
**categorias de tipo** que autorizam cada operação (`memcpy`, `offsetof`, chaves) e o que
`virtual` acrescenta ao objeto.

Toda a regra cabe em três linhas:

1. todo tipo tem `alignof(T)`: seu endereço precisa ser múltiplo dele;
2. o alinhamento de uma struct é o **maior** entre os de seus membros;
3. `sizeof` é sempre múltiplo de `alignof`, para que arrays funcionem.

O resto — inclusive por que `sizeof` quase nunca é a soma dos membros — decorre disso.

```bash
make build && ./dist/bin/layout
```

## Antes de começar

Este README não pressupõe nada além de saber declarar uma `struct`. Todo termo usado adiante
está definido aqui, na ordem em que aparece. Se algum já for familiar, pule.

### O vocabulário mínimo

**Byte e endereço.** A memória do processo é um array gigante de bytes numerados a partir de
zero. O número de um byte é o seu **endereço**. `&x` devolve o endereço do primeiro byte de
`x`; `sizeof(x)` diz quantos bytes ele ocupa a partir dali.

**Alinhamento.** Cada tipo `T` tem um número `alignof(T)` — quase sempre 1, 2, 4, 8 ou 16 — e o
endereço de todo objeto do tipo `T` **precisa ser múltiplo dele**. Um `double` (alinhamento 8)
só pode morar nos endereços 0, 8, 16, 24…

**Por que o hardware exige isso.** A CPU não lê "um byte no endereço 4". Ela lê **blocos**: uma
palavra de 8 bytes por vez do controlador de memória, e uma **linha de cache** de 64 bytes por
vez da RAM, sempre em fronteiras múltiplas do próprio tamanho. Um `double` que começasse no
endereço 4 ficaria **metade num bloco e metade no seguinte** — a CPU teria que fazer duas
leituras e colar os pedaços. No x86-64 isso é só lento; no ARM antigo e em muitas instruções
SIMD é uma exceção de hardware (`SIGBUS`) e o programa morre. O alinhamento é a garantia de que
esse caso nunca acontece. Por isso a regra 1 lá em cima não é burocracia do padrão: é o
formato físico do barramento de memória.

**Padding.** Os bytes de enchimento que o compilador insere **entre** membros (para alinhar o
próximo) e **no fim** da struct (para alinhar o próximo elemento de um array). Eles existem no
`sizeof` e **não fazem parte do valor** do objeto: o conteúdo deles é indeterminado.

**`offsetof(Tipo, membro)`.** Uma macro de `<cstddef>` que devolve, em bytes, **a que distância
do início da struct o membro começa**. `offsetof(Ruim, d) == 20` significa: o membro `d` mora
nos bytes 20..23 do objeto. É a ferramenta que torna todo este README verificável em vez de
narrado — cada diagrama de layout adiante sai dela.

**UB — *undefined behavior*, comportamento indefinido.** A situação em que o padrão da
linguagem **não impõe nada**: o programa pode funcionar, travar, ou fazer algo pior. E, o mais
importante na prática: o otimizador tem **permissão para assumir que UB nunca acontece** e
apagar o seu código com base nisso. UB não é "erro em runtime"; é a ausência total de contrato.
Aparece sete vezes neste README, sempre com esse sentido — o subprojeto
[`src/undefined_behavior/`](../undefined_behavior/) é dedicado só a ele.

**Linha de cache e *false sharing*.** A CPU não sincroniza bytes entre núcleos: sincroniza
**linhas de cache de 64 bytes**. Se a thread A escreve na variável `produtor` e a thread B
escreve em `consumidor`, e as duas caem na **mesma** linha, cada escrita de A invalida a cópia
que o núcleo de B tinha em cache, e vice-versa. Os dados são independentes — o hardware não
tem como saber disso, ele raciocina por linha. O resultado é contenção pura, sem nenhum dado
compartilhado de verdade. Isso é o **false sharing** (compartilhamento falso), e é o motivo do
`alignas(64)` do demo 2.

**SIMD e AVX.** *Single Instruction, Multiple Data*: instruções que operam sobre vários valores
de uma vez. **AVX** é a família dessas instruções no x86-64 que trabalha com registradores de
**32 bytes** (8 `float` de uma vez). Daí o `alignas(32)` do demo 2: as instruções AVX de carga
alinhada exigem endereços múltiplos de 32.

**EBO — *Empty Base Optimization*, otimização de base vazia.** A permissão que o padrão dá ao
compilador de fazer um subobjeto de **base** sem nenhum dado ocupar **zero** byte. Vale para
base, não para membro — e é essa assimetria que o demo 4 explora.

**SSO — *Small String Optimization*, otimização de string curta.** `std::string` guarda strings
curtas (na libstdc++, até 15 caracteres) **dentro do próprio objeto**, sem ir ao heap. Só acima
disso ela aloca. É por isso que um bug de `memcpy` sobre `std::string` pode passar despercebido
nos testes: com string curta não há ponteiro para o heap para duplicar.

**vptr e vtable.** Ver o demo 5, onde os dois são construídos do zero — a explicação curta é
que cada classe com `virtual` ganha **uma** tabela de ponteiros de função no binário (a
*vtable*), e cada **objeto** ganha um ponteiro de 8 bytes para a tabela da sua classe (o
*vptr*).

**ABI — *Application Binary Interface*, o contrato binário.** O acordo, ao nível de bytes e
registradores, entre pedaços de código **já compilados**: como os símbolos são nomeados, qual é
o `sizeof` e o layout de cada classe exposta, em que ordem estão as entradas da vtable. Layout
de objeto **é** parte da ABI, e por isso mudá-lo quebra binários já distribuídos. O assunto tem
subprojeto próprio: [`src/linkage/`](../linkage/).

**Aliasing e *strict aliasing*.** Dois ponteiros de tipos diferentes *aliasam* quando apontam
para os mesmos bytes. A regra de *strict aliasing* diz que o compilador **pode assumir que isso
não acontece** entre tipos não relacionados — e otimizar com base nisso. Ler um `float` através
de um `int*` é, portanto, UB, mesmo que os bytes estejam lá. `std::bit_cast` (C++20) é a forma
legal de reinterpretar bytes; ver [`src/casts/`](../casts/) e [`src/bits/`](../bits/).

### O que este subprojeto **não** cobre

Comportamento de despacho virtual (o *que acontece* na chamada) é [`src/oop/`](../oop/); aqui só
medimos o **custo em bytes**. Cache, AoS vs SoA e medição de desempenho são
[`src/performance/`](../performance/).

## Por que estudar isto?

Porque é conhecimento com retorno imediato e mensurável:

| Descoberta medida aqui | Efeito |
|---|---|
| reordenar 4 membros | struct de **24 → 16 bytes** (33% menor) |
| `[[no_unique_address]]` num membro vazio | **16 → 8 bytes** |
| um `virtual ~T()` que ninguém usa | **+8 bytes por objeto**; 76 MB em 10 milhões |
| `sizeof(Deriv1) == sizeof(Base1)` | o membro do derivado coube no *tail padding* da base |

E porque a alternativa a saber isto é a superstição: "memcpy de struct é rápido" (às vezes é
UB), "POD é o que importa" (o termo foi aposentado no C++20), "alignas resolve" (só aumenta,
nunca diminui).

## Estrutura de arquivos

```
src/layout/
├── main.cpp                      ← ponto de entrada, cor steel_blue
├── demo_tamanho.hpp              ← 1 · sizeof, padding, offsetof, memcmp
├── demo_alignas.hpp              ← 2 · alignas, alocação super-alinhada
├── demo_categorias.hpp           ← 3 · trivial / standard-layout / POD / agregado
├── demo_no_unique_address.hpp    ← 4 · EBO e [[no_unique_address]]
├── demo_polimorfico.hpp          ← 5 · vptr, herança múltipla, herança virtual
├── meson.build
└── README.md
```

```bash
make build && ./dist/bin/layout
```

---

## Demo 1: `sizeof`, alinhamento e padding

### A ordem dos membros muda o tamanho

```cpp
struct Ruim { char a; double b; char c; int d; };   // sizeof 24
struct Bom  { double b; int d; char a; char c; };   // sizeof 16
```

Os **mesmos** membros, 14 bytes de dado útil, 8 bytes de diferença. Num vetor de 1 milhão:
22,9 MB contra 15,3 MB.

```
Ruim: [a][×7 pad][b b b b b b b b][c][×3 pad][d d d d]
Bom:  [b b b b b b b b][d d d d][a][c][×2 pad]
```

Os diagramas acima não são desenho livre: cada posição sai de um `offsetof`, e o programa os
imprime. Confira você mesmo:

```cpp
#include <cstddef>
static_assert(offsetof(Ruim, a) ==  0);
static_assert(offsetof(Ruim, b) ==  8);   // 7 bytes de padding depois de `a`
static_assert(offsetof(Ruim, c) == 16);
static_assert(offsetof(Ruim, d) == 20);   // 3 bytes de padding depois de `c`

static_assert(offsetof(Bom, b) == 0);
static_assert(offsetof(Bom, d) == 8);
static_assert(offsetof(Bom, a) == 12);
static_assert(offsetof(Bom, c) == 13);    // e 2 de padding no fim
```

`offsetof(T, m)` devolve a distância em bytes do início de `T` até o membro `m`. Some `sizeof`
de cada membro aos offsets acima e os buracos aparecem sozinhos.

> **Regra prática:** declare os membros em ordem **decrescente de alinhamento**. É gratuito, e
> o compilador **não pode** fazer por você — a ordem de declaração é garantida pelo padrão
> (para membros com o mesmo *access specifier*), justamente para permitir layout previsível.

### Por que existe padding no fim

`struct { int a; char b; }` soma 5 bytes e ocupa 8. Por causa dos **arrays**: cada elemento
precisa começar num endereço múltiplo de `alignof`. Se `sizeof` fosse 5, `v[1]` cairia
desalinhado.

### ⚠ `memcmp` e o lixo do padding

Duas instâncias com os **mesmos valores de membro**, uma com o padding marcado com `0xAA`:

```
x: 07 00 00 00 7A AA AA AA
y: 07 00 00 00 7A 00 00 00
                  ▲▲ ▲▲ ▲▲  ← padding

x.a == y.a && x.b == y.b     → true
memcmp(&x, &y, sizeof)  == 0 → false
```

**O padding não faz parte do valor do objeto**, e seu conteúdo é indeterminado. Portanto:

- nunca use `memcmp` para comparar structs — use `operator==` (ou `= default`);
- nunca transmita nem persista uma struct com padding sem zerá-la antes: o padding **vaza
  memória do processo**;
- se precisa de bytes, serialize campo a campo.

### Bit-fields: quando o campo é menor que um byte

> Esta subseção e a de *Unions* (demo 3) são as duas únicas do README que **não** têm saída
> correspondente no programa — são layout de objeto tanto quanto o resto, e ficariam faltando.
> Todo `sizeof` citado aqui é do x86-64 e você pode conferir com um `static_assert`.

Um membro pode declarar **quantos bits** quer, com `: n` depois do nome:

```cpp
struct Flags {
    unsigned tipo    :  3;   // 0..7
    unsigned ativo   :  1;   // 0 ou 1
    unsigned reserva : 28;
};                           // sizeof 4 — os 32 bits couberam num unsigned
```

Sem bit-fields, os mesmos três campos custariam 12 bytes. É a forma padrão de empacotar flags e
campos pequenos — e vem com três armadilhas que o resto do capítulo não tem:

> ⚠ **Não existe `&flags.tipo`.** Um endereço aponta para um *byte*; um campo de 3 bits não
> começa numa fronteira de byte. Tomar o endereço de um bit-field é **proibido**, e não há
> referência nem ponteiro para ele. (Um bit-field de tamanho ≥ 1 byte e alinhado ainda assim é
> proibido — a regra é sintática.)
>
> ⚠ **`offsetof` não se aplica** a bit-field, pela mesma razão.
>
> ⚠ **A ordem e o empacotamento dos bits são definidos pela implementação.** O padrão não diz
> se `tipo` ocupa os bits mais significativos ou os menos, nem se um campo pode atravessar a
> fronteira da unidade de alocação. GCC no x86-64 e MSVC divergem. Portanto: **bit-field nunca
> é formato de arquivo nem de protocolo de rede.** Para bytes exatos na rede, monte o valor com
> deslocamento e máscara à mão — é o assunto de
> [`src/bits/demo_bitfields.hpp`](../bits/demo_bitfields.hpp).

Bit-field é ótimo para economizar memória **dentro** do seu processo, e péssimo para qualquer
coisa que atravesse a fronteira dele.

### Structs vazias

`sizeof(struct{})` é **1**, não 0 — senão dois objetos distintos teriam o mesmo endereço, e um
`Vazia v[3]` teria os três no mesmo lugar. Métodos não ocupam espaço no objeto: são funções
comuns com um `this` implícito.

A exceção que salva o dia: `struct HerdaVazia : Vazia { int x; }` tem `sizeof` **4**, não 8. A
base vazia foi otimizada para **zero** byte. Isso é a **EBO** (*Empty Base Optimization*) —
assunto do demo 4.

### Tipos da biblioteca padrão, medidos

| tipo | `sizeof` | `alignof` |
|---|---|---|
| `double` | 8 | 8 |
| `long double` | **16** | 16 |
| `void*` | 8 | 8 |
| `std::string` | 32 | 8 |
| `std::vector<int>` | 24 | 8 |
| `std::max_align_t` | 32 | **16** |

`long double` no x86-64 SysV: 80 bits de dado útil, `sizeof` 16 — quase metade é padding. No
MSVC ele é 8 bytes.

Repare em `std::max_align_t`: `sizeof` **32** e `alignof` **16**. O que importa dele é o
`alignof`, não o `sizeof` — é **16** o alinhamento que `malloc` e o `operator new` comum
garantem, e é contra esse 16 que se define "over-aligned" no demo 2. (Na libstdc++ ele é uma
união de `long double` com um ponteiro de função, daí o `sizeof` maior; o número que a
linguagem promete é só o alinhamento.)

---

## Demo 2: `alignas`

### Em tipo e em membro

```cpp
struct alignas(32) Vetor8f { float v[8]; };   // 8 floats = 32 bytes, um registrador AVX

struct Espacada {
    alignas(64) int produtor;                 // cada um na sua linha de cache
    alignas(64) int consumidor;
};
```

O `alignas(32)` do primeiro existe porque um registrador **AVX** tem 32 bytes e as instruções
de carga alinhada exigem endereço múltiplo de 32.

O `alignas(64)` do segundo existe porque **a CPU não sincroniza bytes entre núcleos: sincroniza
linhas de cache de 64 bytes**. Sem o atributo, `produtor` e `consumidor` caberiam nos mesmos 8
bytes e portanto na mesma linha; cada escrita da thread produtora invalidaria a cópia em cache
do núcleo consumidor, e vice-versa — apesar de os dois campos serem **independentes**. Isso é o
*false sharing*: contenção de hardware sem compartilhamento de dado nenhum. O `alignas(64)`
empurra cada campo para a sua própria linha e a contenção some.

Medido: `Compacta` tem `sizeof` 8; `Espacada` tem `sizeof` **128** — **16× mais memória** para
comprar essa independência. Se compensa é medição, nunca fé: ver
[`src/performance/`](../performance/) e [`src/concurrency2/`](../concurrency2/).

### ⚠ `alignas` só aumenta

```cpp
struct alignas(1) Tentativa { double d; };
// error: alignment of 'Tentativa' is less than the minimum alignment (8) of its members
```

É proposital: ler um `double` desalinhado é UB, e em ARM antigo é `SIGBUS`. Para empacotar de
verdade existem extensões **não padrão** (`#pragma pack`, `__attribute__((packed))`) — que
criam um perigo silencioso: um ponteiro para membro de struct empacotada é **desalinhado**, e
passá-lo adiante é UB (`-Waddress-of-packed-member`). Se você precisa de bytes exatos,
serialize campo a campo.

### Alocação super-alinhada

`malloc` garante `alignof(std::max_align_t)` = **16**. Um tipo que peça mais é *over-aligned*.

Antes do C++17, `new Vetor8f` **ignorava** o `alignas(32)` — UB silencioso ao usar AVX. O
C++17 (P0035) acrescentou as sobrecargas com `std::align_val_t`, e o compilador as chama
sozinho. Medido: **8 de 8** alocações vieram alinhadas a 32.

Três formas de alinhar à mão:

```cpp
std::aligned_alloc(32, n)                    // C11 — n múltiplo de 32 (ver ⚠)
::operator new(n, std::align_val_t{32})      // C++17, o caminho portável
std::align(32, n, ptr, espaco)               // ajusta um bloco JÁ existente
```

> ⚠ `aligned_alloc` se libera com `free`; `operator new(align_val_t)` com o `operator delete`
> correspondente. Misturar é UB.
>
> ⚠ **`aligned_alloc` não existe no MSVC.** Lá o par é `_aligned_malloc` / `_aligned_free` — e
> `free` sobre memória do `_aligned_malloc` é UB. Como este README cita o MSVC em outros dois
> pontos, vale a regra prática: **em código portável use `::operator new(n,
> std::align_val_t{A})`**, que é padrão desde o C++17 e funciona nos três compiladores.
>
> ⚠ O C11 original exigia que `n` fosse múltiplo do alinhamento. A exigência foi **relaxada**
> depois (DR 460 / C17), e as libcs atuais aceitam qualquer `n`. Ainda assim, passar `n`
> múltiplo do alinhamento é o hábito seguro: é o que funciona em toda implementação.

### `std::assume_aligned` (C++20)

O tipo carrega o alinhamento; um `float*` cru não carrega nada. `std::assume_aligned<32>(p)`
**promete** ao compilador que `p` está alinhado, liberando vetorização sem prólogo escalar.

> ⚠ É uma promessa, não uma verificação. Mentir é UB do pior tipo: funciona no x86 e falha no
> ARM. O UBSan pega com `-fsanitize=alignment`.

---

## Demo 3: As categorias de tipo

Quatro palavras parecidas, quatro autorizações diferentes:

| tipo | triv. copyable | triv. ctor | std. layout | agregado |
|---|---|---|---|---|
| `int` | sim | sim | sim | não |
| `struct { int; double; }` | sim | sim | sim | sim |
| com construtor escrito | **sim** | não | sim | não |
| com `virtual` | não | não | não | não |
| membros em `public` **e** `private` | sim | sim | **não** | não |
| dados em 2 níveis de herança | sim | sim | **não** | sim |
| com membro `std::string` | não | não | **sim** | **sim** |
| `std::string` | não | não | sim | não |

A coluna **triv. ctor** é `is_trivially_default_constructible`: escrever `T x;` não executa
código nenhum, a memória fica com o lixo que já estava lá. É o que a linha "com construtor
escrito" perde — e repare que ela **continua** trivially copyable.

### O que define cada categoria

A tabela acima mostra as respostas. Estas quatro regras deixam você **prever** a resposta sem
rodar nada — cada uma explica exatamente uma coluna:

- **trivially copyable** — todos os construtores de cópia e de movimento e todos os operadores
  de atribuição de cópia e movimento do tipo são **triviais** (gerados pelo compilador, ou
  `= default` na primeira declaração) ou **deletados**, existe pelo menos um deles não
  deletado, e o destrutor é trivial e **não-virtual**. Nada disso fala de *construtor padrão*:
  é por isso que "com construtor escrito" ainda é trivially copyable. Um `virtual` mata a
  categoria porque o vptr precisa ser preenchido por código.
  *Explica a coluna* **triv. copyable**.

- **standard-layout** — nenhum membro virtual, nenhuma base virtual; **todos** os membros de
  dado não-estáticos com o **mesmo** especificador de acesso (todos `public`, ou todos
  `private`); os membros de dado concentrados em **um único** nível da hierarquia (a base pode
  ter os dados, ou o derivado, nunca os dois); e nenhuma base do mesmo tipo do primeiro membro.
  É a categoria que garante compatibilidade de layout com C.
  *Explica as linhas* "membros em `public` **e** `private`" *e* "dados em 2 níveis de herança".

- **agregado** — nenhum construtor **declarado pelo usuário** (nem herdado), nenhum membro de
  dado `private` ou `protected`, nenhuma função virtual e nenhuma base virtual, nenhum
  inicializador padrão de membro *no C++11* (permitido desde o C++14) e bases públicas
  permitidas desde o C++17. Note que agregado **não** exige nada sobre trivialidade: por isso
  a linha "com membro `std::string`" é agregado apesar de não ser trivial em nada.
  *Explica a coluna* **agregado**.

- **trivially destructible** — o destrutor é implícito ou `= default`, não é virtual, e todos os
  membros e bases também são trivialmente destrutíveis. É a categoria que não aparece na tabela
  acima porque nenhuma linha a perderia por conta própria — mas é ela que a tabela de
  autorizações abaixo cita.

Um jeito de gravar: **trivially copyable** fala do que o compilador precisa *executar* para
copiar (nada); **standard-layout** fala de onde o compilador pode *colocar* os bytes (num lugar
previsível); **agregado** fala de o que a *sintaxe de chaves* pode preencher.

### O que cada uma autoriza

| categoria | autoriza |
|---|---|
| **trivially copyable** | `memcpy` do objeto inteiro — é o que deixa `vector<T>` usar `memmove` no realloc |
| **standard-layout** | `offsetof` legal; interop com C; `&s == &s.primeiro_membro` |
| **agregado** | inicialização por chaves e *designated initializers* |
| **trivially destructible** | `vector<T>::clear()` só zera o tamanho, sem percorrer |

### ⚠ `memcpy` sobre o tipo errado

`memcpy` num `std::string` copia o **ponteiro interno**: os dois objetos apontam para o mesmo
buffer e os dois destrutores o liberam — *double free* (liberar o mesmo bloco duas vezes:
corrompe o alocador, e o crash costuma acontecer bem longe do erro).

E agora a parte traiçoeira: com **SSO** (*Small String Optimization* — strings curtas moram
dentro do próprio objeto `std::string`, sem heap; na libstdc++ até 15 caracteres) **não existe
ponteiro para o heap para duplicar**, e o `memcpy` errado "funciona". Os seus testes usam
`"ok"`, `"teste"`, `"foo"`. A produção usa um caminho de arquivo de 60 caracteres. É por isso
que este bug some nos testes e aparece no cliente.

Sobre um tipo com `virtual`, o `memcpy` copia o **vptr** — o destino passa a despachar para os
métodos da classe errada.

A defesa:

```cpp
template <typename T>
void copia_bruta(T& d, const T& o) {
    static_assert(std::is_trivially_copyable_v<T>, "memcpy exige trivially copyable");
    std::memcpy(&d, &o, sizeof(T));
}
```

Em C++20, `std::bit_cast<To>(from)` faz isso com verificação em compilação e **sem UB de
aliasing** — ou seja, sem violar a regra de *strict aliasing*, que permite ao compilador supor
que um `float*` e um `int*` nunca apontam para os mesmos bytes. Reinterpretar bytes com um
`reinterpret_cast` de ponteiro viola essa suposição e o otimizador pode reordenar as escritas;
`bit_cast` **copia** os bytes para um objeto novo do tipo destino, o que é sempre legal. Ver
[`src/casts/`](../casts/) e [`src/bits/`](../bits/).

### POD foi aposentado

`std::is_pod` está **depreciado desde o C++20** (P0767), porque juntar trivial + standard-layout
numa palavra só escondia qual das duas você precisava:

```
quero memcpy?          → is_trivially_copyable
quero offsetof?        → is_standard_layout
quero `T x;` de graça? → is_trivially_default_constructible
quero chaves?          → is_aggregate
```

Um tipo com construtor escrito à mão **não é POD** e ainda assim `memcpy` sobre ele é legal.

### Unions e a *common initial sequence*

Numa `union`, ler um membro que **não** é o ativo (o último escrito) é UB — é o famoso
"type punning por union", que funciona no GCC como extensão e **não** é padrão. O padrão abre
exatamente **uma** exceção, e ela é layout puro:

> Se uma union contém vários **structs standard-layout** que começam com uma **sequência
> inicial comum** — os primeiros N membros com tipos *layout-compatible*, na mesma ordem —,
> então é legal inspecionar esses N primeiros membros **através de qualquer um deles**, seja
> qual for o ativo.

```cpp
struct Circulo   { int tag; double raio; };
struct Retangulo { int tag; double largura, altura; };   // ← mesma sequência inicial: int tag

union Forma {
    Circulo   c;
    Retangulo r;
};

// Legal, mesmo que o membro ativo seja `r`:
//   os DOIS começam com `int tag`, e ambos são standard-layout.
int qual_e(const Forma& f) { return f.c.tag; }
```

Por que funciona: standard-layout garante que os membros ficam na ordem declarada e que o
primeiro membro começa no offset 0 — logo `tag` está fisicamente no mesmo lugar nos dois casos,
e o padrão promete isso em vez de deixar por conta do compilador.

> ⚠ Basta **um** dos structs não ser standard-layout (um `private` a mais, um `virtual`) e a
> garantia some, silenciosamente.
>
> ⚠ Fora dessa exceção, use **`std::variant`**: ele guarda a tag por você, verifica em
> `std::get` e chama o destrutor certo. União crua com tipos não-triviais exige que **você**
> chame destrutor e construtor por posicionamento à mão.

---

## Demo 4: EBO e `[[no_unique_address]]`

### O 1 byte que vira 8

```cpp
struct { Vazio p; double d; };                        // sizeof 16  ← 8 desperdiçados
struct : Vazio { double d; };                         // sizeof  8  (EBO)
struct { [[no_unique_address]] Vazio p; double d; };  // sizeof  8  (C++20)
```

> **Convenção deste README (e da saída do programa):** nos diagramas apertados adiante,
> `[[nua]]` é uma **abreviação nossa** para `[[no_unique_address]]`, feita só para a linha
> caber. **`[[nua]]` não é sintaxe válida de C++** — não tente compilar. O atributo real, por
> extenso, é `[[no_unique_address]]`.

O membro vazio precisa de endereço próprio, então ocupa 1 byte, e `double` só pode começar no
offset 8. Em 10 milhões de elementos: **152,6 MB contra 76,3 MB**.

### Por que `vector` herda do alocador

`sizeof(std::allocator<int>)` = 1 (é vazio) e `sizeof(std::vector<int>)` = 24 — três ponteiros
e **nada mais**. Se o alocador fosse membro, seriam 32.

```cpp
struct _Vector_impl : public _Tp_alloc_type { ... };   // herança, só pelo layout
```

Herdar de algo que não é interface incomoda, mas antes do C++20 era a **única** forma —
`boost::compressed_pair` existia para embrulhar essa feiura.

O mesmo no `unique_ptr`, medido:

```
unique_ptr<int>                       =  8 bytes
unique_ptr<int, lambda SEM captura>   =  8 bytes  (EBO)
unique_ptr<int, void(*)(int*)>        = 16 bytes  ← dobrou
```

> ⚠ Passar `&minha_funcao` como deleter **dobra** o `unique_ptr`. Uma lambda sem captura é
> gratuita.

### ⚠ Dois membros vazios

Dois subobjetos do **mesmo tipo** não podem compartilhar endereço — a regra vale mesmo com o
atributo:

```
struct { [[nua]] Vazio a; [[nua]] Vazio b;      } → sizeof 2
struct { [[nua]] Vazio a; [[nua]] Comparador b; } → sizeof 1
```

Mas isso **não** significa que o custo apareça na struct que os contém. Basta haver outro
membro para absorvê-los:

```
struct { [[nua]] Vazio a, b; int dado; } → sizeof 4
  offset de a    = 0
  offset de b    = 1
  offset de dado = 0      ← os vazios moram DENTRO do int
```

> ⚠ `[[no_unique_address]]` é uma **autorização**, não uma garantia. O MSVC a ignora por
> compatibilidade de ABI e oferece `[[msvc::no_unique_address]]`. Se o layout importa, trave
> com `static_assert(sizeof(T) == N)`.

### A regra de projeto

Toda política passada por template — comparador, alocador, deleter, hasher, *mutex policy* —
merece `[[no_unique_address]]`. É uma linha, e é gratuita quando a política é vazia, que é o
caso comum. Medido: `std::less<int>` custou **0 bytes**; um comparador com um `int` de estado
custou 8, e aí é justo pagar.

---

## Demo 5: O que `virtual` acrescenta

### Como o despacho virtual é implementado — e por isso custa 8 bytes

Antes de medir, o mecanismo, porque sem ele o número parece mágico. Quando o compilador vê uma
classe com pelo menos um método `virtual`, ele monta **duas** coisas:

1. **A vtable** (*virtual table*) — **um** array estático de ponteiros de função, gravado no
   binário, **uma por classe** (não por objeto). A posição de cada método nesse array é fixada
   em compilação: se `f()` é a entrada 0 e `g()` é a entrada 1 na base, elas continuam sendo 0
   e 1 em todo derivado — o derivado só troca *para onde* cada entrada aponta.
2. **O vptr** (*virtual pointer*) — **um** ponteiro de 8 bytes **dentro de cada objeto**,
   preenchido pelo construtor, apontando para a vtable da classe real daquele objeto.

Com isso, `p->f()` não é uma chamada direta; ela vira, em essência:

```
p->f(arg)     ⟶     (*(p->vptr[índice_de_f]))(p, arg)
                       ▲                       ▲
                       lê o ponteiro na tabela  passa o `this`
```

Duas consequências caem de graça:

- O objeto carrega **um ponteiro**, não uma cópia da tabela. Por isso **dez métodos virtuais
  custam os mesmos 8 bytes de um só**: quem cresce é a vtable, e ela está no binário, uma vez
  para a classe inteira.
- A chamada custa **uma leitura de memória a mais** (o vptr) e um salto indireto, que o
  preditor de saltos pode errar. O custo em tempo é assunto de [`src/oop/`](../oop/) e
  [`src/type_erasure/`](../type_erasure/); aqui medimos só o custo em **bytes**.

### O vptr, medido

```
struct { int a; int b; }                → sizeof  8
struct { int a; int b; virtual ~T(); }  → sizeof 16
```

Um **único** método virtual custa 8 bytes por objeto — o vptr, mais o padding que ele impõe
(`alignof` sobe de 4 para 8).

```
80 E7 47 87 CA 55 00 00 | 11 11 11 11 22 22 22 22
└─── vptr (8 bytes) ───┘  └─── a ────┘└─── b ────┘
```

A posição no início é convenção do **Itanium ABI** (Linux, macOS), não exigência do padrão.

> **Prática:** num `vector<Particula>` de 10 milhões, um `virtual ~Particula()` que ninguém usa
> custa **76 MB**. Se a classe não é destruída por ponteiro-para-base, não declare o destrutor
> virtual — torne-o `protected` e não-virtual.

### Herança simples, e uma surpresa

`sizeof(Deriv1) == sizeof(Base1) == 16`, apesar de `Deriv1` ter um `int y` a mais:

```
Base1:  [vptr 8][x 4][padding 4]
Deriv1: [vptr 8][x 4][   y   4]     ← y foi para DENTRO do padding de cauda
```

O compilador só pode fazer isso porque `Base1` **não é standard-layout** (tem `virtual`). Se
fosse, o padrão proibiria — e é por isso que `memcpy` de um objeto para o subobjeto-base de
outro pode corromper o derivado.

### ⚠ Herança múltipla: o `this` que se desloca

As classes que produzem o layout abaixo — são estas três, e nada mais:

```cpp
struct A  { int a = 1; virtual ~A() = default; virtual void fa() {} };
struct B  { int b = 2; virtual ~B() = default; virtual void fb() {} };
struct AB : A, B { int c = 3; };
```

Cada base tem um `int` (4 bytes) e um vptr (8), com padding: `sizeof(A) == sizeof(B) == 16`.
`AB` empilha as duas e acrescenta o seu `int c`: `sizeof(AB) == 32`. Medido:

```
&obj       offset  0
(A*)&obj   offset  0
(B*)&obj   offset 16   ← DESLOCADO

Layout: [vptr-A][a][pad][vptr-B][b][pad][c][pad]
```

Há **dois vptr**: a parte-B precisa da sua própria vtable, e um `B*` tem que apontar para o
começo dela. Três consequências:

1. `static_cast<B*>(&obj)` **não é um no-op** — o compilador soma 16. Um `reinterpret_cast` no
   lugar produziria um ponteiro errado, sem aviso.
2. Comparar ponteiros engana: `(void*)pa == (void*)pb` é **false**, mas `pa == &obj &&
   pb == &obj` é **true**. Use `dynamic_cast<void*>`, que devolve o início do objeto mais
   derivado.
3. `delete pb` precisa **ajustar** o ponteiro antes de liberar: `pb` aponta para o offset 16, e
   quem aloca/libera precisa do offset 0. Quem faz o ajuste é um **thunk** — um trecho minúsculo
   de código que o compilador gera e coloca na vtable de B, no lugar do destrutor real: ele
   subtrai 16 do `this` e só então salta para o destrutor verdadeiro de `AB`. Sem destrutor
   virtual em B, esse thunk não existiria e o `delete` liberaria um endereço no meio do bloco —
   UB.

### Herança virtual — o diamante

```
sizeof(NaoDiamante) = 40   (a base V aparece DUAS vezes)
sizeof(Diamante)    = 48   (a base V é COMPARTILHADA)
```

O virtual custou **mais** bytes apesar de ter **menos** dados. O mecanismo explica: `E1` e `E2`
não sabem em compilação onde `V` vai estar (a classe mais derivada decide), então cada uma
ganha um ponteiro extra — o *virtual base offset* — consultado em runtime, e o `V`
compartilhado vai para o fim do objeto.

> **Quando vale:** interfaces puras, sem dados. Com dados na base virtual, reconsidere o
> desenho — composição costuma ser mais simples e mais rápida.

### Ferramentas para inspecionar sem adivinhar

```bash
clang++ -Xclang -fdump-record-layouts    # layout completo, clang
g++     -fdump-lang-class                # gera arquivos .class
pahole -C NomeDaStruct ./binario         # mostra os BURACOS
static_assert(sizeof(T) == 16);          # trava o contrato
```

O `static_assert` é o mais valioso: transforma uma suposição sobre layout numa falha de
compilação no dia em que alguém acrescentar um membro.

---

## Como validar

**1. O caminho normal — o build do repositório.**

```bash
make build            # configura (conan + meson), compila e instala em ./dist
./dist/bin/layout     # roda
```

`make build` supõe que `make configure` já rodou uma vez (é ele que cria `build/` e o
`conanbuild.sh` sem o qual o meson não acha o `fmt`). Na primeira vez, `make all`.

O que se espera ver: cinco blocos numerados em azul-aço, do `1 · sizeof, alinhamento e padding`
ao `5 · O que virtual acrescenta ao objeto`, com todos os `sizeof`, `alignof` e `offsetof`
impressos a partir do seu compilador — não de constantes no fonte. Se o seu toolchain não for
x86-64 SysV, os números de `long double` e `max_align_t` **vão** diferir, e é esse o ponto.

**2. Compilar à mão, com todos os avisos ligados.**

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)

g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/layout/main.cpp -o /tmp/ly -L$FMTP/lib -lfmtd
LD_LIBRARY_PATH=$FMTP/lib /tmp/ly | head -30
```

Esperado: **nenhuma linha de aviso** na compilação, e o demo 1 na tela.

**3. Sob os sanitizers.**

```bash
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/layout/main.cpp -o /tmp/lysan -L$FMTP/lib -lfmtd
LD_LIBRARY_PATH=$FMTP/lib /tmp/lysan > /dev/null
echo "código de saída: $?"
```

Esperado: código de saída **0** e **nenhuma** linha `runtime error:` ou `ERROR: AddressSanitizer`
em `stderr`. `-fsanitize=undefined` inclui `-fsanitize=alignment`, que é justamente o detector
do erro que este subprojeto ensina a evitar.

**Estado atual (verificado):** sem warnings com `-Wall -Wextra -Wpedantic` em `-O2`, e
**0 achados sob ASan+UBSan**.

Dois `#pragma GCC diagnostic` locais no fonte suprimem `-Winvalid-offsetof` nos pontos em que
usar `offsetof` num tipo **não** standard-layout **é a própria lição** (demo 5, para achar o
`a` depois do vptr) — o aviso está certo, é *conditionally-supported* pelo padrão, o GCC aceita,
e o comentário no fonte explica por quê.

## Referências

### Neste repositório

- [`src/undefined_behavior/`](../undefined_behavior/) — o que é UB, os sanitizers, e o
  otimizador raciocinando em cima dele. **Leia se a sigla UB ainda incomodar.**
- [`src/oop/`](../oop/) — o *comportamento* do despacho virtual, o slicing e a herança múltipla
  do ponto de vista do desenho; aqui só medimos os bytes.
  ([`demo_slicing.hpp`](../oop/demo_slicing.hpp) é a continuação direta do demo 5.2.)
- [`src/bits/`](../bits/) — `bit_cast`, endianness e serialização binária correta; e
  [`demo_bitfields.hpp`](../bits/demo_bitfields.hpp), a alternativa portável ao bit-field.
- [`src/casts/`](../casts/) — `reinterpret_cast`, strict aliasing e por que `bit_cast` é a saída.
- [`src/memory/`](../memory/) — alocação, arena, `std::pmr`, e o alinhamento do lado do alocador.
- [`src/performance/`](../performance/) — cache, false sharing medido, AoS vs SoA.
- [`src/concurrency2/`](../concurrency2/) — o outro lado do `alignas(64)`: por que duas threads
  brigam por uma linha de cache.
- [`src/linkage/`](../linkage/) — ABI: por que mudar o layout de uma classe pública quebra
  binários já compilados.
- [`src/type_erasure/`](../type_erasure/) — o custo do vptr visto do lado do projeto.

### Fora daqui

- *Itanium C++ ABI* (itanium-cxx-abi.github.io/cxx-abi/abi.html) — a especificação de layout,
  vtable e mangling que GCC e Clang seguem no Linux e no macOS.
- **P0035R4** — a proposta que levou ao C++17 as sobrecargas de `operator new` com
  `std::align_val_t` (seção *Alocação super-alinhada*).
- **P0767R1** — a proposta que depreciou `std::is_pod` no C++20 (seção *POD foi aposentado*).
  Ambas em wg21.link/p0035 e wg21.link/p0767.
- cppreference, verbetes *Object layout*, *alignof*, *Standard-layout type*, *Aggregate
  initialization* — a referência do dia a dia.
- `pahole(1)`, do pacote `dwarves` — lê o DWARF do binário e imprime os **buracos** de cada
  struct, com o total desperdiçado. A ferramenta certa para auditar um projeto inteiro.
