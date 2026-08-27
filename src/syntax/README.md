# syntax — A Sintaxe Padrão da Linguagem, de Ponta a Ponta

## O que é este projeto?

O **mapa da sintaxe do C++**: toda construção da linguagem — declaração, `if`, `switch`,
`for`, função, tipo, classe, template, exceção — num só lugar, executada e comentada.

É o subprojeto que se lê **primeiro**, e ao qual se volta como referência. Nada aqui exige
conhecimento prévio de C++: a seção [Antes de começar](#antes-de-começar) define, uma a uma,
todas as palavras usadas no resto do texto.

Para rodar (a partir da **raiz do repositório**, `/home/lima/Projects/cpp_academy`):

```bash
make build          # configura (se preciso), compila e instala em ./dist
./dist/bin/syntax   # o executável instalado
```

São 8 blocos, ~1080 linhas de saída colorida. O programa termina com
`✓ Todos os demos de sintaxe concluídos!`.

Onde um tópico tem um subprojeto dedicado, a seção mostra a *sintaxe* e aponta para lá:

| Aqui você vê | Para se aprofundar |
|---|---|
| a forma `static_cast<T>(x)` e os outros três casts | [casts/](../casts/) |
| `template <typename T>`, especialização, NTTP | [templates/](../templates/) |
| `concept`, `requires` | [concepts/](../concepts/) |
| `try` / `throw` / `catch`, a hierarquia padrão | [errors/](../errors/) |
| `virtual`, `override`, `final`, classe abstrata | [oop/](../oop/) |
| lvalue vs rvalue, Regra do 0/3/5 | [value_semantics/](../value_semantics/) |
| capturas de lambda, `std::function` | [lambdas/](../lambdas/) |
| `new` / `delete` e o que os substitui | [smart-pointers/](../smart-pointers/) · [memory/](../memory/) |
| `constexpr`, `static_assert` | [compile_time/](../compile_time/) |
| `union` → `std::variant` | [vocabulary_types/](../vocabulary_types/) |
| padding, `alignof`, o que `virtual` custa em bytes | [layout/](../layout/) |
| linkage, ODR e o linker por dentro | [linkage/](../linkage/) |

---

## Antes de começar

Esta seção existe para que você **não precise de nenhuma outra fonte** para ler o restante.
Cada termo abaixo é usado adiante; nenhum aparece antes de estar definido aqui.

### O caminho do código até o programa

```
seu.cpp  +  headers incluídos
      │
      │  1. PRÉ-PROCESSADOR — cola o texto dos #include, expande #define
      ▼
unidade de tradução (TU)  ← "um .cpp já com tudo colado dentro"
      │
      │  2. COMPILADOR — traduz a TU para código de máquina
      ▼
   arquivo objeto (.o)     ← tem buracos: chamadas a coisas de outras TUs
      │
      │  3. LINKER — junta os .o e preenche os buracos
      ▼
   executável
```

- **Unidade de tradução (TU)**: o `.cpp` **depois** de o pré-processador colar dentro dele,
  textualmente, o conteúdo de cada `#include`. Por isso um header pesado incluído em 50 `.cpp`
  é compilado 50 vezes — e por isso o mesmo header precisa poder aparecer várias vezes sem
  quebrar (é o papel do `#pragma once`).
- **Declaração** × **definição**: declarar é *prometer* que algo existe (`int f(int);`);
  definir é *fornecer* o corpo/armazenamento (`int f(int x) { return x; }`). O compilador
  precisa da declaração; o linker precisa da definição.
- **Linkage**: quem consegue enxergar um nome definido numa TU. *Interna* = só a própria TU;
  *externa* = o programa inteiro (é o que o linker usa para casar as pontas).
- **ODR (One Definition Rule)**: um nome pode ser **declarado** quantas vezes quiser, mas o
  programa inteiro só pode conter **uma definição** dele. Classes, templates e funções `inline`
  podem repetir-se em várias TUs *desde que as definições sejam idênticas token a token*.
  Violar a ODR é *ill-formed, no diagnostic required*: o compilador **não é obrigado** a avisar,
  e o resultado costuma ser um programa que liga e se comporta de forma absurda.

### Palavras que aparecem o tempo todo

| Termo | Significado, sem rodeios |
|---|---|
| **UB** (*undefined behavior*, comportamento indefinido) | Situação em que o padrão não define **nada**. O programa pode funcionar, travar ou piorar depois de otimizado. Não é "valor aleatório": é ausência total de contrato. Ver [undefined_behavior/](../undefined_behavior/). |
| **Escopo** | O trecho de texto em que um nome é visível (o bloco `{ }`, a classe, o namespace). |
| **Tempo de vida** | O intervalo em que o objeto existe na memória. Escopo é sobre *o nome*; tempo de vida é sobre *o objeto*. |
| **Narrowing** (estreitamento) | Conversão que pode perder informação: `double → int`, `int → char`, `long → short`. Com `=` acontece em silêncio; com `{ }` vira erro de compilação. |
| **Categoria de valor** | Toda expressão tem um *tipo* e uma *categoria*. A categoria responde a duas perguntas: a expressão **tem identidade** (dá para tomar o endereço, ela sobrevive à expressão) e **pode ser saqueada** (podemos roubar suas entranhas)? Tabela completa na [seção 7](#demo-7-expressões-operadores-e-conversões). |
| **ADL** (*Argument-Dependent Lookup*) | Ao resolver `f(x)`, o compilador procura `f` também nos namespaces **dos argumentos**, não só no escopo atual. É o que faz `std::cout << x` funcionar sem escrever `std::operator<<`. |
| **Stack unwinding** (desempilhamento) | O que acontece entre o `throw` e o `catch`: o C++ desmonta a pilha quadro a quadro e **roda o destrutor** de cada objeto local já construído. É o mecanismo que faz o RAII funcionar. |
| **RAII** (*Resource Acquisition Is Initialization*) | Idioma em que o construtor adquire o recurso e o destrutor o devolve. Como o destrutor roda em qualquer saída de escopo — inclusive por exceção — o recurso nunca vaza. |
| **Trivial** / **standard-layout** | Duas propriedades independentes de um tipo. *Trivial*: construir, copiar e destruir não fazem trabalho nenhum além de mexer bytes. *Standard-layout*: o arranjo dos bytes é previsível e compatível com C. Só um tipo *trivially copyable* pode ir para `memcpy`, arquivo ou socket. |
| **CTAD** (*Class Template Argument Deduction*, C++17) | Deduzir os argumentos de um template de classe a partir do construtor: `std::pair p{1, 2.0};` em vez de `std::pair<int,double> p{1, 2.0};`. |
| **Forwarding** (repasse) | Passar um argumento adiante **preservando** se ele era temporário ou não. Precisa de `T&&` deduzido + `std::forward<T>`; detalhado em [value_semantics/](../value_semantics/). |
| **`-Wall -Wextra`** | Conjuntos de avisos do compilador. Este subprojeto compila **sem nenhum aviso** com `-Wall -Wextra -Wpedantic` (verificado em [Como validar](#como-validar)). |

### O menor programa C++ completo

Antes de qualquer detalhe, o programa inteiro — copie, compile, rode. Cada elemento sintático
está anotado com o demo que o aprofunda:

```cpp
#include <fmt/core.h>       // 6 · organização: trazer código de fora
#include <string>

struct Saudacao {           // 4/5 · um tipo definido por você
    std::string nome;
    void falar() const { fmt::print("Olá, {}!\n", nome); }
};

int dobro(int x) { return x * 2; }   // 3 · função: entrada → saída

int main() {                // toda execução começa aqui
    int n{3};               // 1 · declaração + inicialização com {}
    if (n > 0) {            // 2 · controle de fluxo
        for (int i = 0; i < n; ++i)
            fmt::print("{} dobrado = {}\n", i, dobro(i));   // 7 · expressão
    }
    Saudacao{"mundo"}.falar();
}
```

Compilando e rodando (o `fmt` vem do Conan; o caminho sai do `build/fmt.pc` gerado por
`make configure`):

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -Wall -Wextra -I$FMTP/include minimo.cpp -o /tmp/minimo -L$FMTP/lib -lfmtd
/tmp/minimo
```

Saída real:

```
0 dobrado = 0
1 dobrado = 2
2 dobrado = 4
Olá, mundo!
```

Três fatos que esse programa já ensina:

1. `main` devolve `int`, e é a **única** função que pode omitir o `return` (equivale a `return 0;`).
2. `#include` é colagem de texto, não importação de módulo.
3. `Saudacao{"mundo"}` cria um objeto **temporário**, chama um método nele e o destrói ao fim
   da linha — sem `new`, sem `delete`, sem vazamento.

---

## Por que estudar isto?

Porque quase toda armadilha clássica do C++ é uma questão de **sintaxe mal lida** — e o
compilador nem sempre avisa. A última coluna diz onde cada uma é demonstrada, executando:

| Escrito assim | O que realmente acontece | Avisa? | Onde ver |
|---|---|---|---|
| `Widget w();` | declara uma **função**, não uma variável | não | 1 · `1.2` |
| `int x;` (local) | lixo indeterminado; ler é UB | não | 1 · `1.2` |
| `if (a) f(); g();` | `g()` roda **sempre** | às vezes | 2 · `2.1` |
| `052` | é **octal** = 42 | não | 1 · `1.8` |
| `#define Q(x) x*x` com `Q(1+2)` | vira `1+2*1+2` = 5 | não | 6 · `6.2` |
| `a < b` com `int a` e `unsigned b` | `-1` vira `4294967295` | `-Wsign-compare` | 7 · `7.4` |
| `int t = 3.99;` | trunca em silêncio | não (com `{}`, sim) | 1 · `1.2` |
| `delete p` para `new T[n]` | UB | não | 7 · `7.7` |
| `[=]` num método | captura `this` por **ponteiro** | não | [lambdas/](../lambdas/) |
| `case` sem `break` | cai para o seguinte | `-Wimplicit-fallthrough` | 2 · `2.3` |
| `codigo:4 = 20` num bitfield | trunca para 4 | não | 4 · `4.5` |

Os avisos citados não vêm ligados por padrão. O mínimo civilizado é:

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion ...
```

- `-Wall -Wextra` pegam `-Wsign-compare` e `-Wimplicit-fallthrough`;
- `-Wshadow` avisa quando uma variável interna **esconde** outra de fora;
- `-Wconversion` avisa nas conversões que perdem dados (o `int t = 3.99;` da tabela).

---

## Estrutura de arquivos

```
src/syntax/
├── main.cpp                ← ponto de entrada, cor turquoise; executa os 8 blocos
├── demo_variaveis.hpp      ← 1 · tipos, as 6 formas de inicializar, auto/decltype,
│                             const/constexpr/constinit, escopo, ponteiros, literais
├── demo_controle.hpp       ← 2 · if (+init, +constexpr), switch, while/do/for,
│                             range-for, break/continue/goto, ternário, vírgula
├── demo_funcoes.hpp        ← 3 · declaração vs definição, passagem, defaults, sobrecarga,
│                             retorno, especificadores, variádicas, ponteiro p/ função
├── demo_tipos.hpp          ← 4 · struct/class, agregados, enum vs enum class, union,
│                             bitfields, tipos aninhados, trivial/standard-layout
├── demo_classes.hpp        ← 5 · construtores, lista de init, = default/delete, const,
│                             ref-qualifiers, friend, herança, Regra do 0/3/5
├── demo_organizacao.hpp    ← 6 · namespaces, ADL, pré-processador, atributos, linkage, ODR
├── demo_expressoes.hpp     ← 7 · operadores, precedência, categorias de valor,
│                             conversões, os 4 casts, sizeof/alignof, new/delete
├── demo_generico.hpp       ← 8 · templates, CTAD, NTTP, especialização, concepts, exceções
├── meson.build             ← alvo `syntax`, instalado em dist/bin
└── README.md               ← este arquivo
```

Tudo mora em header (`.hpp`) com funções `inline` e um `namespace` por demo: um único `.cpp`
compila o subprojeto inteiro, e cada arquivo pode ser lido isoladamente.

---

## Demo 1: Declarações — tipos, variáveis e inicialização

Toda declaração em C++ tem a mesma forma:

```
[especificadores]   [declarador]   [inicializador] ;
   static const        int* p          = &x
   ↑ storage/cv        ↑ tipo+nome     ↑ como nasce
```

### A regra da espiral — como ler qualquer declaração

Comece **no nome**, vá para a **direita** enquanto puder (`[]` e `()`), depois para a
**esquerda** (`*` e o tipo). Parênteses mandam mais que tudo:

```cpp
int* p[5];      // p é array[5] de ponteiro para int
int (*q)[5];    // q é ponteiro para array[5] de int
int (*f)(int);  // f é ponteiro para função(int) que devolve int
```

Passo a passo do segundo caso: `q` → à direita há `)`, então volto → `*`: *q é ponteiro para*
→ saio dos parênteses, à direita `[5]`: *array de 5* → à esquerda `int`. Logo: **ponteiro para
array de 5 ints**. Em código moderno, `using` desfaz o nó:

```cpp
using Linha = int[5];
Linha* q;                       // idêntico a int (*q)[5], legível
using Callback = int(*)(int);   // idêntico a int (*f)(int)
```

### As seis formas de inicializar — e por que `{}` venceu

```cpp
int a = 42;      // copy-init
int b(42);       // direct-init
int c{42};       // list-init      ← a preferida
int d = {42};    // copy-list-init
int e{};         // value-init     → ZERO garantido
int f;           // default-init   → depende de ONDE está (ver abaixo)
```

⚠ **`int f;` só é lixo em escopo automático** (dentro de uma função). A regra completa para
tipos escalares:

| Onde está `int f;` | Valor inicial |
|---|---|
| local (dentro de função, sem `static`) | **indeterminado** — ler é UB |
| global / `namespace` | **0** (zero-inicializado antes do `main`) |
| `static` local ou `static` membro | **0** |
| membro não-`static` de classe | indeterminado, salvo se o construtor o inicializar |

A regra prática que evita ter de lembrar da tabela: **inicialize sempre**.

`{}` é a preferida porque **proíbe narrowing**:

```cpp
int t = 3.14159;   // ✓ compila, perde dados em silêncio  → 3
int s{3.14159};    // ✗ erro de compilação
```

A exceção que dói: com containers, `{}` muda o *significado* —
`vector<int> v(3,0)` é `{0,0,0}`, mas `vector<int> v{3,0}` é `{3,0}`.

E o *most vexing parse*, que o demo mostra rodando:

```cpp
Widget w();     // NÃO é variável: é declaração de FUNÇÃO sem parâmetros
Widget w{};     // isto sim é a variável
```

### `const` e ponteiros — leia da direita para a esquerda

| Declaração | `*p = x` | `p = &y` |
|---|---|---|
| `const int* p` | ✗ | ✓ |
| `int* const p` | ✓ | ✗ |
| `const int* const p` | ✗ | ✗ |

### Storage class

| Forma | Vive quando | Visível onde |
|---|---|---|
| automático | escopo do bloco | bloco |
| `static` (local) | programa inteiro | bloco |
| `static` (global) | programa inteiro | só esta `.cpp` |
| `extern` | programa inteiro | todo o programa |
| `thread_local` | vida da thread | conforme declarado |

---

## Demo 2: Controle de fluxo — if, switch e loops

São seis construções, e todo programa é feito delas:

```
seleção   →  if / else      switch
iteração  →  while  do-while  for  range-for
salto     →  break  continue  return  goto
```

O que o C++ moderno acrescentou não foi construção nova, foi **escopo menor**:

```cpp
if (auto it = m.find(k); it != m.end()) { ... }   // C++17: `it` morre no fim do if
for (auto f = fn(); const auto& x : v) { ... }    // C++20: idem no range-for
```

### `if constexpr` — o galho descartado

```cpp
if constexpr (std::is_integral_v<T>) { ... }
else                                 { valor.size(); }   // nem precisa ser válido para T=int
```

O galho não escolhido é **descartado na compilação**: ele nem precisa compilar para o `T` em
questão. Com um `if` comum, os dois galhos têm de ser válidos sempre.

### `switch`: as cinco regras

1. a condição tem de ser integral ou enum;
2. cada `case` precisa de constante de compile-time única;
3. sem `break`, o controle **cai** para o próximo (`[[fallthrough]]` documenta a queda intencional);
4. declarar variável num `case` exige `{ chaves }`;
5. com `enum class`, **omitir o `default`** faz o compilador avisar quando um enumerador
   novo não for tratado — isso é uma *feature*, não um descuido.

### Ternário, curto-circuito e operador vírgula

```cpp
const char* faixa = idade >= 18 ? "adulto" : "menor";   // ?: é EXPRESSÃO, tem valor
const int limite = idade >= 18 ? 100 : 10;              // por isso inicializa um const
```

⚠ Os dois galhos do `?:` precisam ter um **tipo comum**; `cond ? 1 : "texto"` não compila.

**Curto-circuito**: em `a && b`, se `a` for falso, `b` **não é avaliado** (idem `||` com
verdadeiro). É o que torna seguro o idioma `if (p && p->campo)`.

**Operador vírgula**: `(f(), 42)` avalia `f()`, joga o resultado fora e devolve `42`. Fora da
cláusula de incremento do `for` (`for (i = 0, j = n; ...)`), quase nunca vale a pena.

### `goto` — e por que quase nunca vale a pena

Único uso defensável: sair de laços aninhados de uma vez.

```cpp
for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j)
        if (m[i][j] == alvo) goto achou;
achou:
```

Regras (todas demonstradas no demo `2.6`):

- **não** pode saltar sobre uma inicialização — isso é erro de compilação, não UB;
- não sai da função (para isso existem `return` e as exceções);
- os destrutores dos escopos abandonados **ainda rodam** — o RAII continua valendo;
- o `goto cleanup;` clássico do C, em C++, é substituído por RAII.

A alternativa preferida quase sempre existe: extrair uma função e usar `return` — no demo, a
busca com `goto` e a busca com `return` dão exatamente o mesmo `(2, 3)`.

---

## Demo 3: Funções

```
[[nodiscard]] constexpr auto soma(int a, int b = 0) noexcept -> int;
└─ atributo ─┘└ especif. ┘└nome┘└─ parâmetros ─┘└ spec ┘└─ retorno ─┘
```

### Como passar

| Forma | Copia? | Modifica? | Aceita nulo? | Quando usar |
|---|---|---|---|---|
| `T` | sim | não (a cópia) | não | tipos baratos, *sink* |
| `T&` | não | **sim** | não | saída, in-out |
| `const T&` | não | não | não | **leitura — o padrão** |
| `T*` | não | sim | **sim** | opcional, API C |
| `T&&` | não | consome | não | move; se `T` for deduzido, *forwarding* |

O `T&&` tem dois significados conforme o contexto, e a diferença é a origem de muita confusão:
em `void f(std::string&& s)` é uma referência a temporário; em `template<class T> void f(T&& x)`
é uma **forwarding reference**, que aceita tudo. A distinção é o tema de
[value_semantics/](../value_semantics/).

### Ordem da resolução de sobrecarga

1. exata → 2. promoção → 3. conversão padrão → 4. conversão do usuário → 5. `...`

O **tipo de retorno não entra na assinatura**: `int f(int)` e `bool f(int)` colidem.

### Variádicas: as duas formas

```cpp
int soma(int n, ...);                                    // C: sem tipos, contagem manual, UB fácil
template <typename... A> auto soma(A... a) { return (a + ...); }   // C++: fold, tipado, compile-time
```

---

## Demo 4: Tipos definidos pelo usuário

`struct` e `class` são a **mesma construção** — muda só o acesso padrão (público vs privado).

### `enum` vs `enum class`

| | `enum` | `enum class` |
|---|---|---|
| escopo | vaza para fora | exige `Cor::Verde` |
| converte para `int` | sozinho | só com `static_cast` |
| tipo subjacente | implementação | você escolhe (`: uint8_t`) |
| colisão de nomes | acontece | impossível |

### Designated initializers (C++20)

```cpp
Config c{.host = "api.exemplo.com", .porta = 443, .tls = true};
```

A resposta do C++ à falta de argumentos nomeados. ⚠ A ordem tem de seguir a **declaração**
dos membros (o C é mais permissivo aqui).

### `union` → `std::variant`

| | `union` | `std::variant` |
|---|---|---|
| quem guarda a etiqueta | você | ele |
| ler o membro errado | UB silencioso | `std::bad_variant_access` |
| tipos não-triviais | não | sim |

### Bitfields — bits com nome, em vez de máscaras

```cpp
struct Registrador {
    unsigned pronto      : 1;   // 1 bit  → 0 ou 1
    unsigned erro        : 1;
    unsigned codigo_erro : 4;   // 4 bits → 0..15
    unsigned versao      : 8;
};                              // sizeof = 4 bytes
```

Três avisos, todos verificáveis no demo `4.5`:

- **truncam em silêncio**: `codigo_erro = 20` guarda `4` (20 mod 16), sem aviso nenhum;
- **não têm endereço**: `&reg.pronto` não compila, e um bitfield não se liga a `unsigned&`
  (daí o `+reg.pronto` no código, para promover a `int` antes de imprimir);
- **o layout é definido pela implementação**: ordem dos bits e preenchimento variam entre
  compiladores e plataformas. Nunca use bitfield como formato de arquivo ou de rede — para
  isso, monte os bits à mão (ver [bits/](../bits/)).

### Trivial, standard-layout e POD

São **duas propriedades independentes**, e cada uma autoriza uma coisa diferente:

| Propriedade | Significa | Autoriza |
|---|---|---|
| **trivial** | construtor, cópia e destrutor não fazem trabalho algum | criar sem inicializar; copiar com `memcpy` (se também for *trivially copyable*) |
| **standard-layout** | membros em ordem previsível, sem `virtual`, sem misturar acessos | `offsetof`, interoperar com `struct` de C |
| **POD** | trivial **e** standard-layout | o termo foi **aposentado no C++20** — use os dois nomes acima |

Medição real do demo `4.7`:

```
| tipo          | trivial | standard-layout | memcpy seguro |
|---------------|---------|-----------------|---------------|
| Trivial       |  true   |      true       |     true      |
| ComConstrutor |  false  |      true       |     true      |
| ComVirtual    |  false  |      false      |     false     |
  sizeof(ComVirtual) = 16 — o vptr custou 8 bytes (+ padding)
```

Repare no meio da tabela: um tipo com construtor **não é trivial** e ainda assim pode ser
copiado com `memcpy` — o que importa para `memcpy` é `std::is_trivially_copyable_v<T>`.
Tamanho, padding e o custo do vptr: [layout/](../layout/).

### Tipos aninhados e forward declaration

```cpp
class Motor;                 // declaração adiantada → tipo INCOMPLETO
class Carro {
    struct Rodas { int quantidade; };   // tipo aninhado: usa-se como Carro::Rodas
    Motor* motor_;                      // ✓ ponteiro para tipo incompleto é legal
};
```

Com um tipo incompleto pode-se declarar ponteiro, referência, parâmetro e retorno; **não** se
pode ter membro por valor, herdar dele nem chamar seus métodos. É assim que se quebram
dependências circulares e se acelera a compilação (o header pesado só entra no `.cpp`) — o
idioma pimpl, em [smart-pointers/](../smart-pointers/).

---

## Demo 5: Classes

### A lista de inicialização não é açúcar sintático

```cpp
Classe(const T& r) : membro(r)  { }        // 1 construção
Classe(const T& r) { membro = r; }         // 1 construção + 1 atribuição
```

E membros `const` ou referência **só** podem ser inicializados, nunca atribuídos.

⚠ A ordem de construção é a de **declaração** dos membros, não a da lista escrita.

### As cinco operações especiais

| # | Operação | Assinatura |
|---|---|---|
| 1 | destrutor | `~T()` |
| 2 | construtor de cópia | `T(const T&)` |
| 3 | atribuição de cópia | `T& operator=(const T&)` |
| 4 | construtor de move | `T(T&&)` |
| 5 | atribuição de move | `T& operator=(T&&)` |

**Regra do 0**: não declare nenhuma. Componha com `std::string`, `std::vector`,
`std::unique_ptr` — eles já se gerenciam, e você ganha as cinco de graça. A medição do custo
de errar isso está em [value_semantics/](../value_semantics/).

### `friend` — abrir o privado para alguém específico

```cpp
class ContaBancaria {
    double saldo_;
    friend void auditar(const ContaBancaria& c);   // esta função vê o privado
};

void auditar(const ContaBancaria& c) { /* pode ler c.saldo_ */ }
```

- `friend` é **concedido pela classe**, nunca tomado de fora: só a própria classe decide.
- Não é herdado nem recíproco.
- O uso legítimo mais comum é o operador que precisa ser função livre — `operator<<`,
  `operator==` simétrico, uma função `trocar(a, b)` que mexe nos dois lados.
- Se muitos `friend` aparecem, o problema costuma ser a interface pública, não o `friend`.

### Métodos `const` e ref-qualifiers

`const` no fim de um método é uma promessa: "não modifico o objeto". Sem ela, o método sequer
pode ser chamado através de um `const Buffer&`.

```cpp
int&       operator[](std::size_t i)       { return v_[i]; }   // objeto não-const
const int& operator[](std::size_t i) const { return v_[i]; }   // objeto const
```

Os **ref-qualifiers** `&` e `&&` acrescentam a pergunta seguinte: o objeto sobre o qual o
método foi chamado é descartável?

```cpp
const std::vector<int>& vetor() const&  { return v_; }             // lvalue → empresta
std::vector<int>        vetor() &&      { return std::move(v_); }  // rvalue → rouba
```

`b.vetor()` empresta sem copiar; `Buffer{8}.vetor()` move o interior — o temporário ia morrer
mesmo. Detalhes e o uso para **proibir** chamadas (`T& operator[](int) && = delete;`) em
[value_semantics/](../value_semantics/).

---

## Demo 6: Organização — namespaces e pré-processador

```
.hpp + .cpp ──[pré-processador]──> unidade de tradução ──[compilador]──> .o ──[linker]──> exe
```

(As três etapas estão explicadas em [Antes de começar](#antes-de-começar).)

### Namespaces

```cpp
namespace a::b::c { }        // aninhado numa linha (C++17)
namespace { int x; }         // anônimo → linkage INTERNA (substitui o `static` global)
namespace v = a::b::c;       // alias
namespace lib { inline namespace v2 { } }   // inline → versionamento de ABI
```

⚠ `using namespace` no escopo global de um **header** impõe a decisão a todos os arquivos
que o incluem. Nunca faça isso.

**ADL na prática** — o demo define `geometria::Vetor` e `geometria::norma_quadrada`:

```cpp
geometria::Vetor v{3, 4};
auto n = norma_quadrada(v);   // ← SEM qualificar: o argumento é geometria::Vetor,
                              //   então o compilador procura também em `geometria`
```

É o mesmo mecanismo por trás de `std::cout << x` e do idioma
`using std::swap; swap(a, b);` (que deixa a ADL achar um `swap` melhor, se existir).

### Macros: quase toda uma tem substituto melhor

| Macro | Substituto |
|---|---|
| `#define PI 3.14` | `constexpr double pi = 3.14;` |
| `#define MAX(a,b)` | `template <class T> T max(T, T)` |
| `#define DEBUG_ON` | `constexpr bool` + `if constexpr` |

Sobram os casos que **exigem** texto: guardas de inclusão, `__LINE__`, portabilidade.
As duas armadilhas clássicas estão medidas no demo `6.2`:

```
QUADRADO_ERRADO(1+2) = 5     ← sem parênteses, vira 1+2*1+2. Precedência!
QUADRADO(proximo())  = 2     ← dupla avaliação: o argumento foi COLADO duas vezes
```

### Linkage e ODR

| Construção | Linkage |
|---|---|
| `int x;` global | externa |
| `static int x;` / `namespace { int x; }` | interna |
| `inline int x = 1;` (C++17) | externa, uma instância — pode ficar no header |
| `const int x = 1;` global | **interna** (surpresa comum) |

**A ODR em um exemplo.** Duas TUs, a mesma função `inline`, corpos diferentes:

```cpp
// a.cpp
inline int dobro(int x) { return x * 2; }
// b.cpp
inline int dobro(int x) { return x + x + 1; }   // ✗ mesma entidade, definição DIFERENTE
```

Isso viola a ODR. O programa **liga sem erro**, e o linker fica livre para escolher qualquer
uma das duas — inclusive para chamadas que você achava que usariam a outra. É o caso mais puro
de *ill-formed, no diagnostic required*: nenhum diagnóstico é exigido, e o bug aparece meses
depois, só em release. Por isso a definição inline mora **no header**, uma vez só. Mais em
[linkage/](../linkage/).

`extern "C"` desliga o *name mangling* (a codificação de tipos no nome do símbolo, que
existe porque C++ tem sobrecarga e C não): `soma_para_c` sai com esse nome literal em vez de
`_Z11soma_para_cii` — é o que permite expor C++ a C, a `ctypes` do Python ou a um carregador
de plugins.

---

## Demo 7: Expressões, operadores e conversões

Toda expressão tem **tipo** e **categoria de valor**. As duas perguntas que definem a
categoria são independentes:

| Categoria | Tem identidade? | Pode ser saqueada? | Exemplos |
|---|---|---|---|
| **lvalue** | sim | não | `x`, `*p`, `arr[0]`, uma função que devolve `T&` |
| **xvalue** | sim | **sim** | `std::move(x)`, uma função que devolve `T&&` |
| **prvalue** | não | **sim** | `42`, `x + 1`, `T{}`, uma função que devolve `T` |

Os dois nomes-guarda-chuva: **glvalue** = lvalue ∪ xvalue (tem identidade);
**rvalue** = xvalue ∪ prvalue (pode ser saqueado). A regra prática que resolve 90% dos casos:
*se tem nome, é lvalue* — mesmo que tenha sido declarado `T&&`. Esta tabela é a mesma de
[value_semantics/](../value_semantics/); as duas não podem divergir.

### A armadilha que mais aparece em produção

```cpp
int a = -1;  unsigned b = 1;
a < b   // FALSE: `a` vira 4294967295
```

Daí o `-Wsign-compare` ao escrever `int i; i < v.size()`.

### Os quatro casts

| Cast | Faz | Verifica |
|---|---|---|
| `static_cast` | conversões razoáveis | compile-time |
| `dynamic_cast` | desce na hierarquia | **runtime** (exige `virtual`) |
| `const_cast` | remove `const` | nada — escrever é UB |
| `reinterpret_cast` | reinterpreta bits | nada |

O cast à moda C, `(int)x`, tenta **nesta ordem** ([expr.cast]/4, a mesma lista de
[`../casts/demo_c_style.hpp`](../casts/demo_c_style.hpp)):

1. `const_cast`
2. `static_cast`
3. `static_cast` + `const_cast`
4. `reinterpret_cast`
5. `reinterpret_cast` + `const_cast`

O primeiro que "funciona" vence, **sem avisar qual foi**. Note que `dynamic_cast` **nunca**
entra nessa lista: o cast C não faz verificação em runtime. E como `(int)x` não é uma palavra
única, não dá para auditar com `grep` — dois motivos para nunca usá-lo em código novo.

⚠ **Precedência ≠ ordem de avaliação.** `1 << 2 + 3` é `1 << 5`, não `(1<<2)+3`.

### `new` / `delete` — a sintaxe, e por que você não vai usá-la

```cpp
int* p = new int(42);      delete p;      // objeto único
int* a = new int[5]{};     delete[] a;    // array — o par TEM de bater
```

⚠ Cada `new` cru é um vazamento esperando por uma exceção: em
`T* p = new T; f(); delete p;`, se `f()` lançar, o `delete` nunca roda. A sintaxe que se usa em
C++ moderno é outra:

```cpp
auto p = std::make_unique<T>();   // libera sozinho, em qualquer saída de escopo
std::vector<T> v(n);              // em vez de new T[n]
```

Ver [smart-pointers/](../smart-pointers/) e [memory/](../memory/).

---

## Demo 8: Sintaxe genérica e de erros

### Templates

```cpp
template <typename T> T maior(T, T);              // função
template <typename T> class Pilha { };            // classe
template <typename T, std::size_t N> class Buf;   // NTTP: um VALOR como parâmetro
template <typename T = int> struct V { };         // argumento padrão
template <> struct X<int> { };                    // especialização TOTAL
template <typename T> struct X<T*> { };           // especialização PARCIAL
template <typename T> constexpr bool eh_v = ...;  // template de VARIÁVEL
template <typename T> using Par = std::pair<T,T>; // template de ALIAS
```

⚠ **Especialização parcial vale para templates de classe e de variável — funções NÃO a
admitem.** Para funções, use sobrecarga, `if constexpr` ou concepts.

⚠ Nomes dependentes exigem `typename` (`typename T::value_type`) — sem ela, o compilador
lê aquilo como um valor.

### CTAD — deixar o compilador deduzir os `<>`

Desde o C++17, os argumentos de um template de **classe** podem ser deduzidos do construtor:

```cpp
std::pair<int, double> p1{1, 2.0};   // antes do C++17: escrever tudo
std::pair              p2{1, 2.0};   // C++17: deduzido → std::pair<int, double>
std::vector            v {1, 2, 3};  // → std::vector<int>
std::lock_guard        lk{mtx};      // → std::lock_guard<std::mutex>
```

É o que aposentou as fábricas `std::make_pair` / `std::make_tuple`. Quando a dedução do
construtor não basta, escreve-se um **deduction guide**:

```cpp
template <typename T> struct Caixa { T v; };
template <typename T> Caixa(T) -> Caixa<T>;   // guia explícito
```

⚠ CTAD é só para **classes**: funções sempre deduziram seus parâmetros. E `Vetor<>` com os
`<>` vazios continua obrigatório quando você quer os argumentos **padrão** do template — sem
eles, `Vetor` é o nome do template, não de um tipo.

### As quatro formas de restringir com concepts

```cpp
template <Somavel T>            T f(T, T);              // (a)
template <typename T> requires Somavel<T> T f(T, T);    // (b)
template <typename T> T f(T, T) requires Somavel<T>;    // (c)
auto f(Somavel auto a, Somavel auto b);                 // (d) abreviada
```

### Exceções

```cpp
try { ... }
catch (const Especifica& e) { ... }      // do mais específico...
catch (const std::exception& e) { ... }  // ...para o mais geral
catch (...) { throw; }                   // relança a MESMA exceção
```

O destrutor dos objetos locais roda **antes** do `catch` — é o *stack unwinding* definido em
[Antes de começar](#antes-de-começar), e é a razão de o RAII funcionar. Lance por valor,
capture por `const&` (capturar por valor **fatia**: a parte derivada do objeto é perdida).

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Declarando variável | Inicialize sempre, de preferência com `{}` |
| `if`, `for`, `while` de uma linha | Use chaves mesmo assim |
| Valor usado só dentro do `if` | `if (auto x = f(); cond)` |
| `switch` sobre `enum class` | Não escreva `default` — deixe o compilador avisar |
| Queda intencional no `switch` | `[[fallthrough]]` |
| Parâmetro que você só lê | `const T&` (ou `T`, se for barato) |
| Construtor de um argumento | `explicit`, salvo se a conversão for desejada |
| Retorno que não pode ser ignorado | `[[nodiscard]]` |
| Classe com método virtual | Destrutor virtual |
| Sobrescrevendo um virtual | `override`, sempre |
| Membro que precisa mudar em método `const` | `mutable` (cache, mutex — nunca estado lógico) |
| Símbolo só desta `.cpp` | Namespace anônimo, não `static` |
| Constante global | `constexpr` / `inline constexpr`, nunca `#define` |
| Alocando memória | `make_unique` / container — não `new` cru |
| Escrevendo uma macro | Pergunte-se qual construção da linguagem resolve |
| Declaração ilegível (`int (*f[3])(int)`) | Dê nome com `using` |
| Compilando qualquer coisa | `-Wall -Wextra -Wpedantic` no mínimo |

---

## Como validar

### 1. Rodar o programa instalado

```bash
make build
./dist/bin/syntax
```

São 8 blocos com o cabeçalho em turquesa. Trechos **reais** do que deve aparecer:

```
╔══════════════════════════════════════════════════════╗
║  1 · Declarações — tipos, variáveis e inicialização  ║
╚══════════════════════════════════════════════════════╝
      int x{};       → 0    value-init → SEMPRE zero
      int x;         → ???  default-init → indeterminado (UB ao ler)
      ⚠ Most vexing parse:
        Widget w();     ← NÃO é variável: é declaração de FUNÇÃO

── 2.7 Ternário, curto-circuito e vírgula ──
      Operador vírgula: `(f(), 42)` → devolve o último = 42

── 4.5 Bitfields ──
        sizeof = 4 bytes — 32 bits com NOME, em vez de máscaras
        ⚠ codigo_erro:4 só guarda 0..15 — atribuir 20 TRUNCA em silêncio

── 6.2 O pré-processador ──
        QUADRADO_ERRADO(1+2) = 5   ← vira 1+2*1+2 = 5. Precedência!

── 7.5 Os quatro casts nomeados ──
      E o cast à moda do C, `(int)x`?
        tenta, nesta ordem: const_cast → static_cast → reinterpret_cast

── 8.2 Template de classe e CTAD ──
        std::vector v{1,2,3};      → std::vector<int>
```

A última linha do programa, em verde:

```
✓ Todos os demos de sintaxe concluídos!
```

### 2. Compilar avulso, com todos os avisos

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/syntax/main.cpp -o /tmp/sy -L$FMTP/lib -lfmtd && /tmp/sy
```

### 3. Rodar sob sanitizers

```bash
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/syntax/main.cpp -o /tmp/sysan -L$FMTP/lib -lfmtd && /tmp/sysan
```

**Estado atual (verificado):** compila **sem nenhum aviso** com `-Wall -Wextra -Wpedantic` e
roda com **0 achados** sob ASan+UBSan. Um único `#pragma GCC diagnostic` local, em
`demo_tipos.hpp`, silencia `-Wmissing-field-initializers` exatamente no ponto em que a
inicialização agregada parcial **é a lição** — os membros omitidos são zero-inicializados.

O subprojeto usa o **C++20 padrão da raiz**, sem `override_options`: designated initializers,
`[[likely]]`, `__VA_OPT__`, concepts e init-statement no range-for já existem no GCC 13.3.

---

## Referências

Neste repositório:

- [`src/casts/`](../casts/) — os quatro casts em profundidade, `bit_cast`, e a ordem do cast C
  em [`demo_c_style.hpp`](../casts/demo_c_style.hpp)
- [`src/value_semantics/`](../value_semantics/) — categorias de valor, move, forwarding
- [`src/layout/`](../layout/) — `sizeof`, padding, trivial/standard-layout medidos
- [`src/linkage/`](../linkage/) — TU, linkage, ODR e o linker por dentro
- [`src/undefined_behavior/`](../undefined_behavior/) — o que "UB" custa na prática
- [`src/templates/`](../templates/) · [`src/concepts/`](../concepts/) — genéricos além da sintaxe
- [`src/errors/`](../errors/) — exceções, garantias, `expected` e `error_code`
- [`src/stl/`](../stl/) — os containers e algoritmos que a sintaxe serve para usar

Fontes canônicas:

- **cppreference** — [Declarations](https://en.cppreference.com/w/cpp/language/declarations),
  [Statements](https://en.cppreference.com/w/cpp/language/statements),
  [Expressions](https://en.cppreference.com/w/cpp/language/expressions),
  [Value categories](https://en.cppreference.com/w/cpp/language/value_category),
  [CTAD](https://en.cppreference.com/w/cpp/language/class_template_argument_deduction)
- **C++ Core Guidelines** — [ES.20](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es20-always-initialize-an-object)
  (sempre inicialize), ES.23 (prefira `{}`), ES.45 (nada de constantes mágicas),
  [NR.x](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-not) (as "não-regras")
- **Padrão ISO** — `[expr.cast]/4` (a ordem do cast estilo C), `[basic.def.odr]` (a ODR),
  `[basic.lval]` (categorias de valor), `[class.bit]` (bitfields)
- *A Tour of C++*, Bjarne Stroustrup — a visão geral da linguagem em ~200 páginas
