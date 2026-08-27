# compile_time — o que o compilador pode calcular por você

## O que é este projeto?

Um curso, do zero, sobre **mover trabalho da execução para a compilação**: as quatro
palavras-chave que controlam isso (`constexpr`, `consteval`, `constinit`, `static_assert`), os
parâmetros de template que são **valores** (NTTP), e o que se ganha — e se paga.

```bash
make build
./dist/bin/compile_time
```

O executável se chama `compile_time` (veja em [`meson.build`](meson.build)) e termina com
`✓ Todos os demos de compile-time concluídos!`.

> ⚠ Este alvo compila em **C++23**, porque `if consteval` exige esse padrão. O `cpp_std` da raiz
> do repositório continua C++20; a exceção é **local**, via `override_options: ['cpp_std=c++23']`
> no [`meson.build`](meson.build).

---

## Antes de começar: o vocabulário inteiro deste subprojeto

Esta é a seção mais importante do documento: ela define **todo** termo que o resto do README usa.
Se você sabe escrever uma função e uma classe em C++, tem o necessário para seguir.

### 1. O que significa "avaliar em compilação"

Normalmente um programa passa por duas fases separadas no tempo:

```
   VOCÊ ESCREVE          COMPILAÇÃO                 EXECUÇÃO
   ────────────          ──────────                 ────────
   quadrado(3)   ──▶   gera instruções    ──▶   a CPU executa
                       de multiplicação          e produz o 9
```

`constexpr` permite mover a conta para a **esquerda**: o compilador *executa* a função ele mesmo,
durante a compilação, e grava só o **resultado** no binário. Programa mínimo, completo e
compilável, que demonstra a ideia inteira:

```cpp
#include <fmt/core.h>

constexpr int quadrado(int n) { return n * n; }

static_assert(quadrado(3) == 9);      // o compilador calcula e CONFERE, aqui

int main() {
    constexpr int nove = quadrado(3); // o 9 já está no binário
    fmt::print("{}\n", nove);
}
```

```bash
g++ -std=c++23 min.cpp -o min $(pkg-config --cflags --libs fmt) && ./min   # imprime 9
g++ -std=c++23 -O2 -S min.cpp -o - | grep quadrado   # nenhuma saída: a função sumiu
```

O que sobra no executável é o número `9`; a função **pode nem existir** no binário final.

### 2. Expressão constante

Uma expressão que o compilador consegue avaliar **inteira** durante a compilação, usando só
coisas que ele já conhece.

| Expressão | É constante? | Por quê |
|---|---|---|
| `3 * 4` | sim | literais |
| `quadrado(3)` | sim | função `constexpr`, argumento literal |
| `quadrado(n)`, com `int n = 3;` | **não** | `n` é variável comum de runtime |
| `std::time(nullptr)` | não | depende do sistema operacional |
| `1 / 0` | não | é *undefined behavior*, proibido aqui |

Onde o C++ **exige** expressão constante (tamanho de array, argumento de template, inicializador
`constexpr`, condição de `static_assert`), uma expressão não-constante é **erro de compilação**.

### 3. Type trait, e os sufixos `_v` e `_t`

Um **type trait** é um `template` que responde a uma pergunta **sobre um tipo**, em compilação.
Não há mágica — um trait inteiro cabe em quatro linhas:

```cpp
template <typename T> struct is_ponteiro     : std::false_type {};  // caso geral: não
template <typename T> struct is_ponteiro<T*> : std::true_type  {};  // se for T*: sim

static_assert(is_ponteiro<int*>::value == true);
static_assert(is_ponteiro<int >::value == false);
```

`std::false_type`/`std::true_type` são structs da biblioteca que carregam um
`static constexpr bool value`; a especialização `<T*>` é escolhida sempre que o tipo casa com o
padrão `algum-tipo*`. Daí vêm as duas convenções de nome que você verá em todo lugar:

| Escrita | Devolve | Equivale a |
|---|---|---|
| `Trait<T>::value` / `Trait_v<T>` | um **bool** de compilação | `is_polymorphic_v<T>` = `is_polymorphic<T>::value` |
| `Trait<T>::type` / `Trait_t<T>` | um **tipo** | `decay_t<T>` = `decay<T>::type` |

Traits em `_v` respondem **sim/não**; traits em `_t` **devolvem um tipo**. É a distinção inteira.

### 4. Concept

Predicado nomeado sobre tipos, do C++20, que o compilador consulta **enquanto escolhe** qual
sobrecarga chamar: `template <std::integral T> void f(T);` só entra na disputa se `T` for
inteiro. A diferença prática para um `static_assert` é o **momento** da falha (Demo 3). Mecânica
completa em [`../concepts/`](../concepts/).

### 5. Detection idiom

Não existe trait pronto para "este tipo tem o método `serializar()`?". O **detection idiom** é a
receita para escrever esse trait você mesmo, usando SFINAE (*substitution failure is not an
error*: se substituir `T` produz código inválido, o compilador **descarta** aquela especialização
em silêncio em vez de dar erro). Está em [`demo_static_assert.hpp`](demo_static_assert.hpp) e é
explicado no Demo 3.

### 6. NTTP — *Non-Type Template Parameter*

Parâmetro de template que é um **valor**, não um tipo. `template <typename T> class A;` recebe um
tipo; `template <std::size_t N> class B;` recebe um número — este é o NTTP. `B<4>` e `B<16>` são
**tipos diferentes**. A sigla não se traduz; é o assunto do Demo 4.

### 7. *Immediate function*

O nome oficial, no padrão, de uma função marcada `consteval`: só pode ser avaliada em compilação,
e toda chamada a ela precisa ser, ela própria, expressão constante. **Contexto imediato** é
qualquer lugar onde essa exigência já está garantida — o corpo de outra `consteval`, por exemplo.

### 8. ABI — o contrato binário

**API** é o contrato de **código-fonte**: se muda, você recompila e resolve. **ABI**
(*Application Binary Interface*) é o contrato **binário** entre pedaços já compilados: tamanho de
cada struct, offset de cada campo, em qual registrador cada argumento viaja. Se a ABI muda e você
**não** recompila tudo, o programa lê o campo errado e não reclama.

```
   quem ENVIA (compilado ontem)      quem RECEBE (recompilado, com um campo novo)
   ┌──┬──┬────┬────────┐             ┌──┬──┬──┬────┬────────┐
   │v │t │ len│  time  │  ──rede──▶  │v │t │NV│ len│  time  │
   └──┴──┴────┴────────┘             └──┴──┴──┴────┴────────┘
    0  1  2    4                      0  1  2  3    5
                                            ↑ "len" agora é lido do offset errado
```

Tratamento completo em [`../linkage/`](../linkage/), Demo 5. No Demo 3 vemos como `static_assert`
transforma esse contrato num **erro de build**.

### 9. O *static initialization order fiasco*

Variáveis globais de **unidades de tradução diferentes** (arquivos `.cpp` diferentes) são
inicializadas em ordem **não especificada**; na prática o GCC segue a ordem dos `.o` no link. Se o
global de um arquivo lê o global de outro, o resultado depende dessa ordem. Não é teoria:

```cpp
// a.cpp                          // b.cpp                        // main.cpp
int criar_base();                 extern int base;                #include <cstdio>
int base = criar_base();          int derivado = base * 2;        extern int base, derivado;
int criar_base() { return 10; }   // ↑ lê um global de OUTRA TU   int main() {
                                                                    std::printf("%d %d\n",
                                                                      base, derivado); }
```

Medido nesta máquina, mudando **apenas a ordem dos arquivos no link**:

```
$ g++ -std=c++23 a.cpp b.cpp main.cpp -o t && ./t   →  10 20   ← o que você esperava
$ g++ -std=c++23 b.cpp a.cpp main.cpp -o t && ./t   →  10 0    ← `base` ainda valia 0
```

Nenhum aviso, nenhum crash. Só um `0` no lugar de `20`. O Demo 2 fecha esse buraco.

### 10. *Meyers singleton*

Uma das duas saídas clássicas para o fiasco: em vez de um global, um `static` **local dentro de
uma função**, inicializado na **primeira chamada** — nunca antes, nunca fora de ordem:
`Config& config() { static Config c = carregar(); return c; }`. Implementado em
[`../solid/ocp/demo_registry.hpp`](../solid/ocp/demo_registry.hpp). A outra saída é `constinit`.

### 11. As seções do binário: `.rodata`, `.data.rel.ro`, `.bss`

Um executável ELF é dividido em **seções**, e onde um dado cai conta como ele foi inicializado:

| Seção | Conteúdo | Quem escreve |
|---|---|---|
| `.rodata` | constantes puras, somente leitura | o **compilador**, no arquivo |
| `.data.rel.ro` | constantes que contêm **endereços** | compilador + **loader**, que corrige |
| `.data` | dados mutáveis com valor inicial | o compilador, no arquivo |
| `.bss` | dados que começam **zerados** | ninguém — o SO zera a página |

**Relocação** é a correção de endereços que o *loader* faz ao mapear o binário: como o programa
pode ser carregado em qualquer endereço-base, todo ponteiro gravado no arquivo precisa ser
ajustado. Por isso uma tabela `constexpr` contendo `string_view` (que são ponteiros!) vai para
`.data.rel.ro` — "somente leitura **depois** da relocação". Achar um dado em `.bss` seria alarme:
significaria que ele começa zerado e alguém o preenche em runtime.

### 12. Crivo de Eratóstenes

O algoritmo clássico dos primos: parta de 2 até N e, para cada número ainda não riscado, risque
todos os seus múltiplos; quem sobrar é primo.

```
2  3  4  5  6  7  8  9 10        →  riscando múltiplos de 2 e depois de 3
2  3  ✗  5  ✗  7  ✗  ✗  ✗        →  primos: 2 3 5 7
```

O Demo 1 roda esse crivo inteiro **na compilação**, com um `std::vector<bool>` de rascunho.

---

## Por que estudar isto?

O ganho óbvio é performance: o que o compilador calcula, o programa não calcula. O ganho maior é
outro — **provas**. Em contexto `constexpr`, **qualquer undefined behavior vira erro de
compilação**. Um teste mostra que funcionou naquela execução; um `static_assert` mostra que **não
existe** execução em que falhe.

| Problema | Sem compile-time | Com compile-time |
|---|---|---|
| Tabela de CRC-32 | laço no `main()`, toda vez que o programa sobe | 1024 bytes já em `.rodata` |
| Porta TCP inválida na config | exceção no cliente, às 3h da manhã | **o build quebra**, na sua máquina |
| Matrizes de dimensões incompatíveis | leitura fora dos limites, silenciosa | erro de compilação, com os números |
| Struct de protocolo que mudou de tamanho | pacotes corrompidos na outra ponta | erro com a mensagem que você escreveu |
| Global que lê outro global | `0` no lugar de `20`, dependendo do link | `constinit` recusa e o build para |
| Busca binária em tabela desordenada | UB silencioso | `static_assert(is_sorted(...))` prova a ordem |

E o custo, real, pago em **tempo de build** — medido no Demo 5.

---

## Estrutura de arquivos

```
compile_time/
├── README.md                 ← este documento
├── meson.build               ← alvo `compile_time`, com cpp_std=c++23 local
├── main.cpp                  ← ponto de entrada: chama os 5 demos em ordem
├── demo_constexpr.hpp        ← 1 · permissão vs garantia, vector/string, classes, limites
├── demo_consteval.hpp        ← 2 · immediate functions, validação, constinit, if consteval
├── demo_static_assert.hpp    ← 3 · mensagens, type traits, detection idiom, layout e ABI
├── demo_nttp.hpp             ← 4 · valores no tipo, matrizes, StringFixa, o custo
└── demo_tabelas.hpp          ← 5 · CRC-32, ordenação e mapa gerados na compilação
```

Todos os `demo_*.hpp` são *header-only*, com uma `run()` em namespace próprio; o
[`main.cpp`](main.cpp) só chama as cinco em sequência.

---

## Demo 1 — `constexpr` ([`demo_constexpr.hpp`](demo_constexpr.hpp))

**O problema.** Você tem uma função que às vezes recebe um literal e às vezes um valor lido do
disco. Antes do C++11, isso eram **duas** funções e uma macro para escolher.

### `constexpr` é permissão, não garantia

A **mesma** função serve aos dois mundos; quem decide é o **chamador**:

```cpp
constexpr int fatorial(int n) {
    int r = 1;
    for (int i = 2; i <= n; ++i) r *= i;   // laço: legal desde C++14
    return r;
}

constexpr int a = fatorial(10);   // avaliado na COMPILAÇÃO → 3628800
int n = 5;
const int b = fatorial(n);        // avaliado na EXECUÇÃO   → 120
```

| Forma de forçar a avaliação | Força? | Observação |
|---|---|---|
| `constexpr int a = f();` | sim | só naquela declaração |
| `static_assert(f() == x);` | sim | e ainda **verifica** o valor |
| `std::array<int, f()>` | sim | o argumento de template exige expressão constante |
| chamada normal | não | o compilador decide (com `-O2`, quase sempre avalia) |

Para garantir que uma função **nunca** rode em runtime, a ferramenta é `consteval` — Demo 2.

### C++20: `vector` e `string` em `constexpr`

Até o C++17 alocar em contexto `constexpr` era proibido, o que excluía todo container dinâmico. O
C++20 liberou, com uma regra: **alocação transitória** — toda alocação feita durante a avaliação
em compilação precisa ser **liberada** antes de a avaliação terminar. Um `vector` serve como
rascunho, mas não sobrevive até o runtime. É o que permite rodar o crivo inteiro na compilação:

```cpp
constexpr int soma_dos_primos_ate(int limite) {
    std::vector<bool> composto(limite + 1, false);   // aloca…
    int soma = 0;
    for (int i = 2; i <= limite; ++i) {
        if (composto[i]) continue;
        soma += i;
        for (int j = i * i; j <= limite; j += i) composto[j] = true;
    }
    return soma;                                     // …e é destruído aqui
}
static_assert(soma_dos_primos_ate(1000) == 76127);   // medido: 76127
```

> ⚠ `constexpr std::vector<int> v = {1,2,3};` **não compila** — a memória do compilador não existe
> no runtime. O erro real do GCC 13 é explícito: *"is not a constant expression because it refers
> to a result of `operator new`"*. Para **devolver** dados de uma função `constexpr`, use
> `std::array`, cujo armazenamento é o próprio objeto.

### `constexpr` em classes, e os limites

Um construtor `constexpr` faz o objeto **inteiro** existir em compilação; e desde o C++14 métodos
`constexpr` **não** são implicitamente `const` — podem modificar o objeto durante a avaliação.

```cpp
constexpr Racional meio{2, 4};                                     // normaliza para 1/2
static_assert(meio.numerador() == 1 && meio.denominador() == 2);
constexpr Racional soma = Racional{1,3} + Racional{1,6};           // = 1/2, na compilação
```

Requisitos: construtor `constexpr`, membros de tipo literal, destrutor trivial (ou `constexpr`,
desde o C++20). E uma avaliação `constexpr` **não pode**: fazer I/O, chamar função
não-`constexpr`, ler variável não-`constexpr`, usar `reinterpret_cast`, deixar alocação viva ao
fim, nem causar UB. Há ainda um teto de passos de avaliação — `-fconstexpr-ops-limit`, padrão
**33 554 432** (= 2²⁵) — que dá `error: 'constexpr' evaluation operation count exceeds limit`.

| Padrão | O que passou a caber numa função `constexpr` |
|---|---|
| C++11 | praticamente só `return expr;` — uma instrução |
| C++14 | laços, variáveis locais, `if`/`switch`, múltiplos `return` |
| C++17 | `if constexpr`, lambdas `constexpr` |
| C++20 | alocação transitória, `try`/`catch`, `virtual`, `vector`, `string` |
| C++23 | `if consteval`, `goto`, variáveis `static`/`thread_local` |

---

## Demo 2 — `consteval` e `constinit` ([`demo_consteval.hpp`](demo_consteval.hpp))

**O problema.** `constexpr` diz "**pode** rodar na compilação". Faltavam duas garantias mais
fortes: "**tem que** rodar na compilação" e "a **inicialização** desta global acontece na
compilação".

| Palavra | Avaliação em compilação | Argumento de runtime |
|---|---|---|
| (nenhuma) | não | ok |
| `constexpr` | **permitida** | ok |
| `consteval` | **obrigatória** | **erro de compilação** |

### `consteval` é contagiosa para cima

Uma `constexpr` **pode** rodar em runtime; se ela repassasse um **parâmetro** a uma `consteval`,
essa chamada aconteceria em runtime — proibido por definição. O compilador recusa já na
**definição**:

```cpp
consteval int dobro(int x) { return x * 2; }

constexpr int f(int x) { return dobro(x);  }   // ✗ f pode rodar em runtime
consteval int g(int x) { return dobro(x);  }   // ✓ contexto imediato propaga
constexpr int h(int x) { return dobro(10); }   // ✓ o argumento é literal, não o parâmetro
```

### A aplicação mais útil: validação em compilação

Um `throw` que **escapa** de uma avaliação constante é erro de compilação — não exceção. Isso
transforma um construtor `consteval` num validador que roda na sua máquina:

```cpp
class Porta {
public:
    consteval explicit Porta(int v) : valor_(v) {
        if (v < 1 || v > 65535) throw "porta fora do intervalo 1..65535";
    }
    constexpr int valor() const { return valor_; }
private:
    int valor_;
};

constexpr Porta http{80};       // ✓
constexpr Porta ruim{70000};    // ✗ NÃO COMPILA
```

O erro real do GCC 13 aponta a linha do `throw`:
`error: expression '<throw-expression>' is not a constant expression`, precedido de
`in 'constexpr' expansion of 'Porta(70000)'`. É exatamente o mecanismo do `std::format` do
C++20: `std::format("{:d}", "texto")` falha na **compilação**, não com exceção em produção.

### `constinit`

| Palavra | Inicialização | Mutável depois |
|---|---|---|
| `const` | qualquer | não |
| `constexpr` | compilação | não |
| `constinit` | **compilação** | **sim** |

`constinit` é a resposta direta ao fiasco da seção 9: obriga a inicialização a caber na
compilação, de modo que **não existe ordem a respeitar** — o valor já está no binário quando o
programa começa. Aplicado ao exemplo medido lá em cima, `constinit int derivado = base * 2;`
(com `base` vindo de outra TU) para o build com duas mensagens:

```
error: 'constinit' variable 'derivado' does not have a constant initializer
error: the value of 'base' is not usable in a constant expression
```

O `0` silencioso virou uma mensagem.

### `if consteval` (C++23)

Às vezes o algoritmo ótimo difere entre os mundos: em runtime você quer o intrínseco do
processador; em compilação, ele não existe.

```cpp
constexpr int contar_bits(unsigned x) {
    if consteval {                              // ramo de COMPILAÇÃO
        int n = 0;
        while (x) { n += int(x & 1u); x >>= 1; }
        return n;
    } else {                                    // ramo de EXECUÇÃO
        return __builtin_popcount(x);
    }
}
```

Mesma função, dois algoritmos, um resultado: o demo mede `contar_bits(0b10110110) == 5` nos dois
caminhos.

### ⚠ As duas armadilhas de `std::is_constant_evaluated()`

Ponto em que quase toda explicação erra — este README incluído, até esta revisão:

> **`std::is_constant_evaluated()` dentro de um `if` NORMAL funciona como se espera**: `true`
> quando a função é avaliada na compilação, `false` quando roda em runtime. Esse é o uso
> **correto** da função. O demo mede: `1` em compilação, `2` em runtime.

**Armadilha 1 — `if constexpr (std::is_constant_evaluated())` é sempre `true`.** A condição de um
`if constexpr` é, ela própria, avaliada em compilação, e o ramo do `else` nunca é gerado. Medido
no demo: `1` em compilação e `1` em runtime, quando o correto seria `2`. O GCC avisa disso com
`-Wtautological-compare`, e o aviso está certo.

**Armadilha 2 — inicializar uma variável `const`/`constexpr` local torna aquele inicializador
"manifestamente avaliado em constante"**, mesmo dentro de uma chamada de runtime:

```cpp
const int n = std::is_constant_evaluated() ? 13 : 17;   // n vira 13, mesmo em runtime
int       m = std::is_constant_evaluated() ? 13 : 17;   // m vira 17
```

`if consteval` **não tem nenhuma das duas**: a sintaxe impede o uso errado.

---

## Demo 3 — `static_assert` e type traits ([`demo_static_assert.hpp`](demo_static_assert.hpp))

**O problema.** Você escreveu um comentário dizendo "`T` precisa ser copiável byte a byte".
Comentários não compilam. `static_assert` é a única forma de fazer o compilador **falhar de
propósito**, com uma mensagem sua.

### A mensagem é o produto

```cpp
static_assert(std::is_trivially_copyable_v<T>,
    "serializar() copia bytes crus e só aceita tipos trivialmente copiáveis. "
    "Tipos com ponteiros, std::string ou destrutor próprio precisam de uma "
    "sobrecarga específica.");
```

Sem a mensagem, o compilador imprime `static assertion failed` e a expressão — o que raramente
ajuda. A mensagem deve dizer **o que fazer**, não o que falhou. Custa uma linha.

### O catálogo de traits que aparece de verdade

Releia a seção 3 antes da tabela: `_v` é o atalho para `::value`, e o resultado é um `bool`.

| Pergunta | Trait |
|---|---|
| dá para `memcpy`? | `is_trivially_copyable_v` |
| tem vtable? | `is_polymorphic_v` |
| o *move* é barato **e** seguro? | `is_nothrow_move_constructible_v` |
| dá para inicializar com `{}`? | `is_aggregate_v` |
| mesmo tipo? · herda de X? · converte para X? | `is_same_v` · `is_base_of_v` · `is_convertible_v` |
| é chamável com estes argumentos? | `is_invocable_v` |

Trecho da saída medida pelo demo, que roda esses traits sobre sete tipos:

```
int            triv.copiável=true  polimórfico=false agregado=false tamanho=4
ComDestrutor   triv.copiável=false polimórfico=false agregado=true  tamanho=1
Polimorfico    triv.copiável=false polimórfico=true  agregado=false tamanho=8
std::string    triv.copiável=false polimórfico=false agregado=false tamanho=32
```

Repare em `Polimorfico`: `tamanho=8` numa struct **sem nenhum campo** — os 8 bytes são o ponteiro
para a vtable ([`../layout/`](../layout/)). Já os traits em `_t` **produzem um tipo**:
`remove_reference_t<int&>` → `int`, `decay_t<const int&>` → `int`, `decay_t<int[5]>` → `int*`.
`decay_t` é exatamente o que o `auto` faz na dedução por valor.

### Escrevendo um trait próprio: o *detection idiom*

Nenhum trait da biblioteca responde "este tipo tem `serializar()`?". A forma clássica (C++17) usa
`std::void_t` mais uma especialização parcial:

```cpp
template <typename, typename = void>
struct tem_serializar : std::false_type {};                      // caso geral: não

template <typename T>
struct tem_serializar<T, std::void_t<decltype(std::declval<const T&>().serializar())>>
    : std::true_type {};                                         // só se a expressão for válida

template <typename T>
inline constexpr bool tem_serializar_v = tem_serializar<T>::value;
```

Por que funciona: `std::void_t<...>` **ignora** seus argumentos e vira `void`. Se `t.serializar()`
for válida, o `decltype` produz um tipo, o `void_t` vira `void`, a especialização casa com o
segundo parâmetro padrão e vence. Se **não** for válida, a substituição falha — e por SFINAE o
compilador descarta a especialização em silêncio, restando a primária, `false_type`.

A forma moderna (C++20) diz a mesma coisa em três linhas, e diz **mais**:

```cpp
template <typename T>
concept Serializavel = requires(const T& t) {
    { t.serializar() } -> std::convertible_to<std::string>;
};
```

Medido no demo, incluindo um tipo cujo `serializar()` devolve `int` em vez de `std::string`:

| Tipo | `tem_serializar_v` | `Serializavel` |
|---|---|---|
| `ComMetodo` (devolve `std::string`) | `true` | `true` |
| `SemMetodo` | `false` | `false` |
| `TipoErrado` (devolve `int`) | **`true`** ← só checa que existe | **`false`** ← checa o retorno |

O concept ganha em três frentes: menos linhas, verifica o tipo de retorno, e falha na **chamada**
em vez de na instanciação. Ver [`../concepts/`](../concepts/).

### `static_assert` ou concept?

Não são substitutos: falham em **momentos** diferentes, e isso muda a resolução de sobrecarga.

| Ferramenta | Quando falha | Afeta a escolha da sobrecarga? |
|---|---|---|
| `static_assert` | **depois** de a sobrecarga ser escolhida | não |
| concept | **durante** a escolha | **sim** |

```cpp
template <typename T> void com_assert(T) { static_assert(std::is_integral_v<T>); }
template <std::integral T> void com_concept(T) {}
void com_concept(double) {}          // sobrecarga alternativa

com_assert(3.14);    // ✗ ERRO — o template foi escolhido, e só então o assert falhou
com_concept(3.14);   // ✓ ok  — a constraint removeu o template; `void(double)` venceu
```

Use **concept** para restringir; use **`static_assert`** para afirmar fatos sem alternativa.

### Onde `static_assert` é insubstituível: layout e ABI

Concepts restringem **templates**; não dizem nada sobre o **tamanho** de uma struct.

```cpp
#pragma pack(push, 1)
struct CabecalhoDeRede {
    std::uint8_t versao; std::uint8_t tipo;
    std::uint16_t comprimento; std::uint32_t timestamp;
};
#pragma pack(pop)

static_assert(sizeof(CabecalhoDeRede) == 8,
    "O cabeçalho tem 8 bytes por especificação. Se este assert falhou, o padding "
    "mudou e os pacotes serão incompatíveis com a outra ponta.");
static_assert(offsetof(CabecalhoDeRede, comprimento) == 2);
static_assert(std::is_trivially_copyable_v<CabecalhoDeRede>);
```

> ⚠ Uma honestidade que o demo mede: **neste** layout, o struct **sem** `#pragma pack` também dá 8
> bytes (`versao` e `tipo` cabem no alinhamento de `comprimento`) — a saída real traz
> `sizeof(CabecalhoDeRede) = 8` e `sizeof(CabecalhoSemPack) = 8`. O `static_assert` não existe
> para provar que o `pack` mudou algo **hoje**; existe para quebrar o build **amanhã**, quando
> alguém acrescentar um campo ou trocar um tipo. É esse o ponto do contrato de ABI da seção 8
> ([`../linkage/`](../linkage/), Demo 5).

---

## Demo 4 — NTTP ([`demo_nttp.hpp`](demo_nttp.hpp))

**O problema.** Um buffer que guarda a capacidade num **campo** paga bytes por objeto e descobre
erros de dimensão só em runtime. Se o valor é conhecido na compilação, ele pode morar no **tipo**.

### O valor entra no tipo

```cpp
template <typename T, std::size_t N>
class BufferFixo {
    std::array<T, N> dados_{};
    std::size_t      tamanho_ = 0;
public:
    constexpr std::size_t capacidade() const { return N; }   // constante, não lê memória
};
```

`BufferFixo<int,4>` e `BufferFixo<int,16>` são **tipos diferentes** — atribuir um ao outro é erro
de compilação. Medido pelo demo: `sizeof` de **24** e **72** bytes.

### Dimensões no tipo: erros de álgebra viram erros de compilação

Este é o argumento mais forte a favor de NTTP.

```
Matriz<2,3> * Matriz<3,4>  →  Matriz<2,4>     ✓   dimensões internas casam (3 = 3)
Matriz<2,3> * Matriz<2,2>  →  NÃO COMPILA     ✗   3 ≠ 2, e o compilador sabe
```

A assinatura que produz isso exige que o operando tenha exatamente `Colunas` linhas:
`template <std::size_t OutrasColunas> Matriz<Linhas, OutrasColunas> operator*(const Matriz<Colunas, OutrasColunas>&) const;`.
Sem NTTP, as dimensões seriam campos e o erro apareceria em runtime — na melhor hipótese como
`assert`, na pior como leitura fora dos limites.

### NTTP de tipo classe (C++20): strings como parâmetro de template

Até o C++17 um NTTP só podia ser inteiro, enum, ponteiro ou referência. O C++20 admitiu tipos de
classe **literais**, o que finalmente permitiu texto:

```cpp
template <std::size_t N>
struct StringFixa {
    char dados[N]{};
    consteval StringFixa(const char (&lit)[N]) { std::copy_n(lit, N, dados); }
    constexpr std::string_view view() const { return {dados, N - 1}; }   // sem o '\0'
    constexpr bool operator==(const StringFixa&) const = default;        // obrigatório
};
template <std::size_t N> StringFixa(const char (&)[N]) -> StringFixa<N>;

Campo<"largura", int> largura{1920};
static_assert(decltype(largura)::nome() == "largura");
```

A regra para um tipo poder ser NTTP (*structural type*): **todos** os membros de dados públicos,
nada `private`/`protected`, nada `virtual`, e os membros também *structural*. Por isso
`std::string` **não** pode — tem membros privados e aloca; daí o `StringFixa`. E
`template <auto Valor>` cobre o caso em que o tipo do valor não importa: `Constante<42>::tipo` é
`int`, `Constante<3.5>::tipo` é `double`.

### ⚠ O custo: uma instanciação por VALOR

Cada combinação distinta de argumentos gera código novo — e com NTTP, "combinação distinta" inclui
cada **valor**: `BufferFixo<int,4>`, `<int,8>` e `<int,16>` são três classes completas no binário.
Consequências: binário maior, compilação mais lenta, pressão no cache de instruções.

| Use NTTP quando | Evite quando |
|---|---|
| poucos valores fixos e conhecidos (2, 3, 4) | o valor vem de configuração, arquivo ou entrada |
| o valor participa da **correção** (dimensões, unidades) | há muitos valores possíveis |
| quer o tamanho no tipo para evitar heap | só quer evitar uma alocação — use `std::span` |

A técnica clássica para conter a explosão é extrair a parte que **não** depende do NTTP para uma
classe-base não-template.

---

## Demo 5 — Gerando dados na compilação ([`demo_tabelas.hpp`](demo_tabelas.hpp))

**O problema.** A tabela de 256 entradas do CRC-32 costuma ser gerada num laço na inicialização
(gastando tempo em toda execução) ou colada à mão num arquivo gerado (que ninguém revisa). Há uma
terceira via.

### A tabela é calculada pelo compilador

```cpp
constexpr std::array<std::uint32_t, 256> gerar_tabela_crc32() {
    constexpr std::uint32_t polinomio = 0xEDB88320u;
    std::array<std::uint32_t, 256> tabela{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? (polinomio ^ (c >> 1)) : (c >> 1);
        tabela[i] = c;
    }
    return tabela;
}

inline constexpr auto TABELA_CRC32 = gerar_tabela_crc32();
static_assert(TABELA_CRC32[255] == 0x2D02EF8Du);
```

`inline constexpr` numa variável de header dá uma **única** instância no programa inteiro, sem
violar a ODR; antes do C++17 isso exigia um `.cpp`. Medido pelo demo: 256 entradas, **1024
bytes**, e `crc32("cpp_academy") == 0x2C7C3747` calculado tanto na compilação quanto na execução,
pela mesma função.

### Onde a tabela foi parar, de verdade

```bash
objdump -tC dist/bin/compile_time | grep ' O ' | grep -E 'TABELA|PRIORIDADES'
```

`-C` demangla os nomes C++ (sem ele você vê `_ZN12demo_tabelas12TABELA_CRC32E`) e o filtro `' O '`
deixa passar só os símbolos de **dados**. Saída real desta máquina — os endereços mudam a cada
build, o resto não:

```
000000000004c780 u     O .data.rel.ro  00000000000000d8  demo_tabelas::TABELA_HTTP
000000000003e0a0 u     O .rodata       0000000000000400  demo_tabelas::TABELA_CRC32
000000000004c860 u     O .data.rel.ro  0000000000000078  demo_tabelas::PRIORIDADES
```

Colunas: endereço · flags (`u` = *unique global symbol*, o que o GCC usa para variáveis `inline`)
· `O` de *object* · seção · **tamanho em hexadecimal** · nome demanglado. `0x400` = 1024 bytes,
exatamente as 256 entradas de 4 bytes. A nuance, com a seção 11 em mãos: `TABELA_CRC32` é só
`uint32_t` — valores puros, vão para `.rodata`; `TABELA_HTTP` e `PRIORIDADES` contêm
`string_view`, que são **ponteiros**, cujo endereço só se conhece quando o loader mapeia o
binário, então vão para `.data.rel.ro`. **Nenhuma** está em `.bss`.

### Ordenação em compilação, e um mapa `constexpr`

`std::sort` é `constexpr` desde o C++20. A tabela HTTP é escrita fora de ordem de propósito, e o
**compilador** ordena; o demo a imprime já em `200 201 204 301 400 403 404 500 503`:

```cpp
static_assert(std::is_sorted(TABELA_HTTP.begin(), TABELA_HTTP.end()));
static_assert(descrever_http(404) == "Not Found");
```

A busca binária em runtime é válida porque a ordenação foi **provada**, não testada — sem isso,
`lower_bound` sobre dados desordenados é UB silencioso ([`../stl/`](../stl/)). O mesmo raciocínio
dá um mapa sem `std::map`:

| Estrutura | `constexpr` | Aloca | Busca |
|---|---|---|---|
| `std::map` | não | sim | O(log n) |
| `std::unordered_map` | não | sim | O(1) médio |
| **array ordenado** | **sim** | **não** | O(log n) |

```cpp
inline constexpr MapaFixo<std::string_view, int, 5> PRIORIDADES{{{
    {"critico", 0}, {"erro", 1}, {"aviso", 2}, {"info", 3}, {"debug", 4},
}}};
static_assert(PRIORIDADES.valor_ou("erro", -1) ==  1);
static_assert(PRIORIDADES.valor_ou("nada", -1) == -1);
```

Para até algumas dezenas de entradas fixas, o array ordenado ganha nas três colunas — inclusive em
velocidade, porque é contíguo.

### ⚠ Uma armadilha do GCC

Detalhe avançado, guardado para o fim: a API natural de um mapa devolveria um **ponteiro**, e você
escreveria `return p ? *p : padrao;`. O `MapaFixo` deste demo devolve um **índice**, e o
comentário no código explica por quê — converter em `bool` um ponteiro para membro de objeto
`constexpr` **não é expressão constante** em todos os modos do GCC: o mesmo código compila em
`-O2` e falha sob `-fsanitize`. Aritmética de **índice** é sempre expressão constante. Regra:
**APIs `constexpr` robustas se baseiam em índices, não em ponteiros.**

### Quando não vale a pena

O custo é pago pelo **compilador**, e o interpretador de `constexpr` do GCC é ordens de grandeza
mais lento que código nativo. Medido nesta máquina (GCC 13.3.0, sem `-O`, três execuções cada,
`g++ -std=c++23 -c … -o /dev/null`):

| O que se compila | Tempo |
|---|---|
| só `#include <fmt/core.h>` + `main` vazio | ≈ **0,42 s** |
| o mesmo + `demo_tabelas.hpp` (CRC-32 + sort + mapa) | ≈ **0,47 s** |
| o `main.cpp` inteiro (os cinco demos) | ≈ **0,95 s** |

Gerar as três tabelas custou cerca de **0,05 s de build**, e o programa nasce com elas prontas.
Bom negócio nesta escala; deixa de ser quando a tabela cresce até o teto de
`-fconstexpr-ops-limit`. **Não vale** quando a tabela é grande e o build fica lento, os dados
mudam com frequência, ou o cálculo já era trivial no startup. **Vale muito** quando a tabela é
pequena ou média e imutável, você quer **provar** uma propriedade, o ambiente não tem heap
(embarcado, kernel), ou você quer eliminar inicialização estática dinâmica.

---

## Regras de ouro

| Situação | Use |
|---|---|
| Função que pode ser útil em compilação | `constexpr` |
| Função que **deve** rodar só em compilação | `consteval` |
| Validar um argumento em compilação | `consteval` + `throw` |
| Global cuja inicialização não pode depender da ordem de link | `constinit` |
| Afirmar layout, tamanho ou contrato de ABI | `static_assert` com mensagem |
| Restringir um template | concept, não `static_assert` |
| Dois algoritmos, um por mundo | `if consteval` (C++23) |
| Saber em qual mundo você está, dentro de um `if` comum | `std::is_constant_evaluated()` |
| Devolver dados de uma função `constexpr` | `std::array` — nunca `vector` |
| Mapa pequeno e fixo | array ordenado + `lower_bound` |
| API `constexpr` robusta | índices, não ponteiros |

---

## Como validar

**1. Rodar o programa.** `make build && ./dist/bin/compile_time`. Espere cinco blocos numerados
(`1 · constexpr…` até `5 · Gerando dados na compilação`) e, na última linha,
`✓ Todos os demos de compile-time concluídos!`. Valores-âncora na saída:
`soma dos primos até 1000 = 76127`, `crc32("cpp_academy") = 0x2C7C3747`,
`sizeof(BufferFixo<int,4>) = 24` e a tabela HTTP em `200 201 204 301 400 403 404 500 503`.

**2. Confirmar em que seção do binário as tabelas foram parar.**

```bash
objdump -tC dist/bin/compile_time | grep ' O ' | grep -E 'TABELA|PRIORIDADES'
```

Espere **três** linhas: `TABELA_CRC32` em `.rodata` com tamanho `0x400`; `TABELA_HTTP` e
`PRIORIDADES` em `.data.rel.ro`; **nenhuma** em `.bss` — uma delas ali seria prova de que é
preenchida em runtime.

**3. Medir o custo de build.**
`time g++ -std=c++23 -c src/compile_time/main.cpp -o /dev/null $(pkg-config --cflags fmt)`.
Nesta máquina: ≈ 0,95 s. Compare com um arquivo que só inclui `<fmt/core.h>` (≈ 0,42 s) para
separar o que é `constexpr` do que é apenas parsing de headers.

**4. Ver o fiasco de inicialização acontecer.** Recrie os três arquivos da seção 9 e troque a
ordem no link: `derivado=20` numa ordem, `derivado=0` na outra, sem nenhum aviso.

**5. Confirmar que os erros prometidos são erros.** Descomente, uma de cada vez, as linhas
marcadas com `✗` nos headers — `constexpr Porta invalida{70000};` em
[`demo_consteval.hpp`](demo_consteval.hpp) e `auto erro = a * c;` em
[`demo_nttp.hpp`](demo_nttp.hpp) — e confirme que o build quebra com a mensagem certa.

---

## Referências

**Neste repositório:** [`../concepts/`](../concepts/) (concepts e `requires`, a mecânica que o
Demo 3 só encosta) · [`../linkage/`](../linkage/) (unidades de tradução, ODR, *name mangling* e o
Demo 5 sobre **ABI**) · [`../layout/`](../layout/) (`sizeof`, padding, alinhamento e vtable — o
"porquê" dos números do Demo 3) · [`../templates/`](../templates/) (instanciação, dedução e o
custo do código genérico) · [`../stl/`](../stl/) (`lower_bound`, `sort` e os containers comparados
no Demo 5) · [`../cpp23/`](../cpp23/) (o que este toolchain tem de C++23, medido) ·
[`../undefined_behavior/`](../undefined_behavior/) (o UB que o contexto `constexpr` proíbe) ·
[`../solid/ocp/demo_registry.hpp`](../solid/ocp/demo_registry.hpp) (o *Meyers singleton*) ·
[`../ROADMAP.md`](../ROADMAP.md).

**Fontes canônicas:** cppreference —
[`constexpr`](https://en.cppreference.com/w/cpp/language/constexpr) ·
[`consteval`](https://en.cppreference.com/w/cpp/language/consteval) ·
[`constinit`](https://en.cppreference.com/w/cpp/language/constinit) ·
[`static_assert`](https://en.cppreference.com/w/cpp/language/static_assert) ·
[expressões constantes](https://en.cppreference.com/w/cpp/language/constant_expression) ·
[`<type_traits>`](https://en.cppreference.com/w/cpp/header/type_traits) ·
[parâmetros de template](https://en.cppreference.com/w/cpp/language/template_parameters).
Propostas: **P1073R3** (*immediate functions*, o `consteval`) · **P0784R7** (*more constexpr
containers*, a alocação transitória que liberou `vector` e `string`) · **P1938R3** (`if
consteval`) · **P0732R2** (*class types in non-type template parameters*, o que permitiu o
`StringFixa`). GCC:
[opções de C++](https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b-Dialect-Options.html), onde vive
`-fconstexpr-ops-limit`.
