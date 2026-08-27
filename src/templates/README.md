# templates — genéricos em C++, do zero ao CRTP

## O que é este projeto?

Seis demos que ensinam **templates em C++** — o mecanismo com que se escreve uma função ou classe **uma
vez** e o compilador gera automaticamente uma versão para cada tipo usado. Base de `std::vector`,
`std::sort`, `fmt::print`, Eigen e de toda biblioteca C++ moderna.

```bash
make build                # compila o repositório (Conan + Meson + Ninja)
./dist/bin/templates      # roda os 6 demos em sequência

# recompilar só este alvo, depois de um `make configure`:
source build/conanbuild.sh && meson compile -C build templates
```

A última linha da saída, se tudo correu bem: `✓ Todos os demos de templates concluídos!`

> ⚠ Não use `meson setup dist`: aqui `dist/` já é o diretório de **instalação** (`dist/bin`,
> `dist/lib`), e apontar o Meson para ele corrompe a árvore instalada. O diretório de build é
> `build/`, criado por `make configure`.

---

## Antes de começar: as oito ideias que o resto do texto usa

Esta é a seção mais importante do README. Nada aqui pressupõe mais do que C++ básico — funções, classes,
ponteiros, referências.

### 1. Um template não é código. É uma receita de código.

```cpp
template <typename T>
T maximo(T a, T b) { return (a > b) ? a : b; }
```

Essas três linhas **não geram nenhuma instrução de máquina**. São um molde. O compilador só produz
código quando você *usa* o molde com um tipo concreto:

```
     o molde (não vira código)             o que o compilador gera
  ┌───────────────────────────┐         ┌────────────────────────────┐
  │ template <typename T>     │         │ int maximo<int>(int,int)   │ ← maximo(3, 7)
  │ T maximo(T a, T b) {...}  │ ──────▶ │ double maximo<double>(...) │ ← maximo(3.14, 2.71)
  └───────────────────────────┘         │ string maximo<string>(...) │ ← maximo(s1, s2)
                                        └────────────────────────────┘
```

Se você nunca chamar `maximo` com `float`, a versão `float` **não existe** no binário. Daí as duas
consequências que voltam o tempo todo: **custo zero em runtime** (`maximo<int>` é uma função
`int(int,int)` comum) e **custo em compilação e tamanho** — dez tipos, dez cópias no binário; é o *code
bloat*.

### 2. Instanciação

**Instanciar** é o ato do compilador de pegar o molde, substituir os parâmetros pelos tipos
concretos e gerar a função ou classe resultante:

| termo | o que é |
|---|---|
| `template <typename T> T maximo(T,T)` | o **template** (o molde) |
| `T` | o **parâmetro** de template |
| `int` em `maximo<int>` | o **argumento** de template |
| `maximo<int>` | a **instância** — código real, gerado |
| o ato de gerar | a **instanciação** |

Cada instância é um tipo/função **distinto**. `Pilha<int>` e `Pilha<std::string>` não têm parentesco:
são duas classes independentes que saíram do mesmo molde.

### 3. Dedução de argumentos

Você escreve `maximo(3, 7)` e o compilador descobre que `T = int` olhando os argumentos. Isso é
**dedução**, e vale só para **funções**:

```cpp
maximo(3, 7);               // T deduzido = int
maximo(3, 3.14);            // ✗ ERRO: T seria int (1º arg) ou double (2º)?
maximo<double>(3, 3.14);    // ✓ T forçado; o 3 é convertido
```

A dedução **não faz conversões** para decidir `T`; ela casa o padrão — por isso o caso do meio falha em
vez de "escolher o maior tipo".

### 4. Compile-time × runtime

Compile-time é o instante em que o `g++` roda; runtime é quando o binário roda. Templates, `constexpr`,
`static_assert`, `if constexpr` e `type_traits` são **todos** de compile-time — o efeito deles já
terminou antes de o programa começar:

```
 [ .cpp/.hpp ] → [ COMPILE-TIME: templates, constexpr, traits ] → [ binário ] → [ RUNTIME: if, loops, virtual ]
```

Boa parte do valor dos templates é **mover erros da direita para a esquerda**.

### 5. Nome dependente e a palavra `typename`

O compilador analisa o corpo do template **duas vezes** (*two-phase lookup*): uma ao ler a definição,
sem saber o que é `T`; outra na instanciação, com `T` concreto. Na primeira passagem:

```cpp
template <typename Container>
void imprimir_primeiro(const Container& c) {
    typename Container::value_type primeiro = c.front();   // ← typename obrigatório
}
```

Sem saber o que é `Container`, o compilador não tem como decidir se `Container::value_type` é um *tipo*
(como em `std::vector<int>::value_type`) ou um *membro estático* (como em `struct X { static int
value_type; }`). A regra padrão é "na dúvida, **não** é tipo" — então sem `typename` a linha vira uma
multiplicação sem sentido. `typename` é você dizendo:
**isto é um tipo**. Por isso ele aparece em `typename std::enable_if<...>::type` no Demo 4.

> ⚠ Nos *parâmetros* de template, `typename` e `class` são intercambiáveis e não têm
> relação alguma com este uso: `template <typename T>` ≡ `template <class T>`.

### 6. `std::true_type` e `std::false_type`

Aparecem como classes-base nos Demos 4 e 5. Não são mágica; são duas structs de uma linha — `template
<class T, T v> struct integral_constant { static constexpr T value = v; };`, com `true_type =
integral_constant<bool,true>` e `false_type = integral_constant<bool,false>`. Escrever `struct tem_size
: std::true_type {}` significa apenas herdar um `::value` valendo `true`. É a convenção da STL para
"trait que responde sim/não".

### 7. Por que templates moram em arquivos `.hpp`

A dúvida nº 1 de quem começa. Separe declaração e definição como se faz com funções comuns —
`maximo.hpp` com `template <typename T> T maximo(T,T);`, `maximo.cpp` com o corpo, `main.cpp` chamando
`maximo(3, 7)` — e `g++ main.cpp maximo.cpp` dá exatamente isto (reproduzido aqui):

```
/usr/bin/ld: undefined reference to `int maximo<int>(int, int)'
collect2: error: ld returned 1 exit status
```

**Por quê:** compilando `main.cpp`, o compilador sabe que precisa de `maximo<int>` mas só tem
a *declaração* — sem corpo, não gera nada. Compilando `maximo.cpp`, tem o corpo mas ninguém pediu `int`
ali — também não gera nada. Cada unidade faz metade e o linker fica sem nada. Duas saídas: **(a)
definição no header**, o que todos os demos daqui e a STL fazem, ao custo de recompilar o template em
cada `.cpp` que o inclui; ou **(b) instanciação explícita**, em que você lista no `.cpp` as instâncias
desejadas — `template int maximo<int>(int,int);` — e o linker acha essas, e **apenas** essas. Serve
quando o conjunto de tipos é fechado.

### 8. `vtable`, `vptr` e os dois polimorfismos

O Demo 6 compara CRTP com `virtual`; sem estes termos a comparação não fecha.

**Polimorfismo dinâmico** (`virtual`): a decisão de qual função chamar é tomada **em
runtime**. Cada classe com métodos virtuais tem uma **vtable** — array estático de ponteiros de função —
e cada objeto carrega um campo escondido, o **vptr**, apontando para ela.

```
  objeto Cachorro                 vtable de Cachorro
 ┌──────────────┐               ┌──────────────────────────┐
 │ vptr ────────┼──────────────▶│ [0] &Cachorro::falar     │
 ├──────────────┤               │ [1] &Cachorro::~Cachorro │
 │ campos...    │               └──────────────────────────┘
 └──────────────┘

 a->falar()  ⇒  1. lê o vptr do objeto   2. lê a entrada [0]   3. chama o endereço lido
```

Custo: 8 bytes por objeto, duas leituras por chamada e — o que mais dói — **a chamada não é inlinável**.
**Polimorfismo estático** (templates, CRTP): a decisão é de **compile-time**; sem vptr, sem vtable,
chamada direta e inlinável. O preço é o tipo concreto precisar ser conhecido na compilação, o que impede
`vector<Base*>` com tipos misturados.

---

## Por que estudar isto?

Comece pelo problema. Você precisa de "o maior entre dois valores". Sem templates:

```cpp
int         maximo(int a, int b)                               { return (a > b) ? a : b; }
double      maximo(double a, double b)                         { return (a > b) ? a : b; }
float       maximo(float a, float b)                           { return (a > b) ? a : b; }
std::string maximo(const std::string& a, const std::string& b) { return (a > b) ? a : b; }
// ... e long, e unsigned, e o seu Vetor3, e o seu Dinheiro, e ...
```

Quatro funções **idênticas linha a linha**; um bug corrigido numa continua nas outras três. Com
template, uma só — e o compilador gera as quatro, **só as que você usar**.

| sem templates | com templates |
|---|---|
| uma sobrecarga por tipo, copiadas à mão | uma definição |
| `void*` + casts (estilo C), sem verificação | tipos verificados em compile-time |
| macro `#define MAX(a,b)`: avalia os argumentos duas vezes, ignora escopo | função de verdade |
| herança + `virtual` para tudo: vtable, alocação, indireção | zero custo em runtime |

---

## Estrutura de arquivos

```
templates/
├── README.md / meson.build   ← este arquivo; executable('templates') → dist/bin/templates
├── main.cpp                  ← chama os 6 run() em ordem, com cabeçalho colorido
├── demo_basics.hpp           ← 1. function/class template, NTTP, args padrão, typename
├── demo_specialization.hpp   ← 2. especialização total e parcial, e quem ganha
├── demo_variadic.hpp         ← 3. parameter packs, fold expressions, perfect forwarding
├── demo_sfinae.hpp           ← 4. enable_if, void_t, detection idiom, if constexpr
├── demo_type_traits.hpp      ← 5. <type_traits>, constexpr, trait próprio
└── demo_crtp.hpp             ← 6. CRTP: polimorfismo estático, mixins, contador, clone
```

Todos são *header-only* (`#pragma once` + `inline`), pelo motivo da ideia 7, e expõem um `inline void
run()` no próprio namespace (`demo_basics::run()`, ...).

---

## Os demos, um a um

### Demo 1 — Fundamentos (`demo_basics.hpp`)

**Function template.** O código usa `maximo`, não `max`: `max` colidiria com `std::max` assim
que alguém incluísse `<algorithm>`.

```cpp
template <typename T> T maximo(T a, T b) { return (a > b) ? a : b; }
template <typename T, typename U>
auto soma(T a, U b) -> decltype(a + b) { return a + b; }   // tipo do retorno deduzido
```

O `-> decltype(a + b)` é um *trailing return type*: "o retorno é o que `a + b` produzir". Sem ele,
`soma(10, 3.14)` teria que escolher arbitrariamente entre `T` e `U`. Saída real: `soma(10, 3.14) =
13.14` (double) e `soma(1, 2) = 3` (int). **Class template:** `Pilha<T>` guarda um `std::vector<T>`;
repare nas duas sobrecargas de `empurrar`, uma `const T&` (copia) e uma `T&&` (move).

**Non-Type Template Parameters (NTTP).** Um parâmetro pode ser um **valor** de compile-time,
não só um tipo — é assim que `std::array<T,N>` funciona:

```cpp
template <typename T, std::size_t N>   // N é um VALOR, não um tipo
class BufferFixo {
    std::array<T, N> dados_;           // tamanho fixo, na stack, sem alocação
    std::size_t tamanho_;
public:
    std::size_t capacidade() const { return N; }   // N existe na compilação
};
```

`BufferFixo<int,8>` e `BufferFixo<int,16>` são **tipos diferentes**, e o demo mede:
`sizeof(BufferFixo<int,8>) = 40 bytes` (8 ints + um `size_t`) contra `sizeof(BufferFixo<int,16>) = 72
bytes`.

> ⚠ Sendo tipos diferentes, uma função que recebe `BufferFixo<int,8>` **não** aceita um
> `BufferFixo<int,16>`: para aceitar qualquer N, ela também precisa ser template em N — o
> mesmo motivo pelo qual `std::array<int,3>` não vai onde se espera `std::array<int,4>`.

**Argumentos de template padrão** — o formato de `std::vector<T, Allocator = ...>`:
`template <typename T, typename Container = std::vector<T>> class FilaGenerica;`, usado como
`FilaGenerica<int> fila;` (com `Container = std::vector<int>`). E `imprimir_primeiro` fecha o demo com o
caso de `typename` da ideia 5 — vale reler aquela seção com o código à frente.

### Demo 2 — Especialização (`demo_specialization.hpp`)

**O problema.** O template primário serve para quase todos os tipos, mas para *um* está
errado: comparar `const char*` com `==` compara **endereços**, não conteúdo.

```cpp
template <typename T>                              // primário
bool sao_iguais(const T& a, const T& b) { return a == b; }

template <>                                        // especialização TOTAL: T fixado
bool sao_iguais<const char*>(const char* const& a, const char* const& b) {
    return std::strcmp(a, b) == 0;
}
```

A lista vazia em `template <>` significa "todos os parâmetros já estão fixados".

**Especialização parcial** fixa só *parte* dos parâmetros, ou impõe uma *forma*:

```cpp
template <typename T, typename U> struct Par { /* genérico */ };
template <typename T> struct Par<T, T>  { /* os dois tipos são iguais */ };
template <typename T> struct Par<T, T*> { /* o segundo é ponteiro do primeiro */ };
```

Ela pode oferecer membros que a genérica não tem — `Par<T,T>` ganha um `media()`, que só faz sentido com
tipos iguais. É assim que a STL implementa traits: o demo reimplementa `is_pointer` com um primário
`false` e duas parciais (`T*` e `T* const`) `true`.

**Quem ganha.** A mais especializada vence: total > parcial > primário; empate entre duas
parciais é erro de ambiguidade. Saída real da seção 2.5, com o mesmo `Prioridade<T,U>`: `<double,float>`
→ primário; `<double,double>` → parcial `<T,T>`; `<double,int>` → parcial `<T,int>`; `<int,int>` → a
total `<int,int>`, a mais específica de todas.

> ⚠ **Especialização parcial de *função* não existe na linguagem.** Só class templates (e
> variable templates) a admitem. Para funções, o que você quer é **sobrecarga**.

> ⚠ **Especialização total de função não participa da resolução de sobrecarga como você
> espera** — ela especializa *um* template escolhido antes, e a escolha é feita sem olhar
> para ela. Verificado nesta máquina:
> ```cpp
> template <class T> void f(T)   { puts("(1) primario f(T)"); }
> template <class T> void f(T*)  { puts("(2) sobrecarga f(T*)"); }
> template <> void f<int*>(int*) { puts("(3) especializacao de (1)"); }
> int x = 0; int* p = &x;  f(p);      // imprime "(2) sobrecarga f(T*)"
> ```
> A (3) **nunca roda**: (2) vence primeiro, e (3) especializa (1), não (2). Para funções,
> **prefira sobrecarga a especialização**.

> ⚠ **A ordem importa.** A especialização precisa estar visível *antes* do primeiro uso que a
> selecionaria; se o primário já foi instanciado para aquele tipo e ela vem depois, o programa
> é *ill-formed, no diagnostic required* — pode compilar em silêncio e fazer a coisa errada.

### Demo 3 — Variadic templates e parameter packs (`demo_variadic.hpp`)

**O problema.** Uma função que aceite 1, 2, 5 ou 40 argumentos de tipos quaisquer — como
`printf`, mas com segurança de tipos. Sem variadic, seriam 40 sobrecargas à mão.

**Parameter pack** é um parâmetro que representa *zero ou mais* de algo. Vem com `...`:

```cpp
template <typename... Args>       // Args: pack de TIPOS
void contar(Args... args) {       // args: pack de VALORES
    fmt::print("{} argumentos\n", sizeof...(args));   // conta o pack
}
```

`sizeof...(args)` devolve **quantos** elementos há — operador diferente do `sizeof` comum, que devolve
bytes. **Expandir** o pack é escrever `pack...`: o compilador repete o padrão à esquerda dos três
pontos, uma vez por elemento, separado por vírgula.

**Recursão clássica (C++11).** Antes do C++17, percorrer um pack exigia tirar o primeiro e
recursar no resto, com um caso base sem template:

```cpp
void imprimir_recursivo() { fmt::print("(fim)\n"); }            // caso base
template <typename T, typename... Rest>
void imprimir_recursivo(T primeiro, Rest... resto) {
    fmt::print("{}", primeiro);
    if constexpr (sizeof...(resto) > 0) fmt::print(", ");
    imprimir_recursivo(resto...);                               // pack com 1 a menos
}
```

Cada nível instancia uma função **diferente** — 4 argumentos geram 5 instâncias, o que torna a técnica
cara em compilação. **Fold expression (C++17)** substitui tudo isso por uma expressão: `(args + ...)`,
`(... && args)`, `((fmt::print("{} ", args)), ...)`.

| forma | expande para |
|---|---|
| `(pack op ...)` — unário à direita | `a op (b op (c op d))` |
| `(... op pack)` — unário à esquerda | `((a op b) op c) op d` |
| `(pack op ... op init)` / `(init op ... op pack)` | idem, com um valor inicial na ponta |

O operador vírgula merece destaque: `(f(args), ...)` é "chame `f` para cada elemento, em ordem" — o
idioma para *fazer algo* com cada item.

**Perfect forwarding** — como `std::make_unique` repassa argumentos sem cópia extra:
`template <typename T, typename... Args> T* meu_make(Args&&... args) { return new
T(std::forward<Args>(args)...); }`.

- `Args&&...` **não é** referência a rvalue. Num template, `&&` colado a um parâmetro
  *deduzido* é uma **forwarding reference**: passe um lvalue e `Args` deduz `T&`, com o `&&`
  colapsando para `T&`; passe um rvalue e `Args` deduz `T`, virando `T&&`. O parâmetro preserva a
  categoria do que chegou.
- `std::forward<Args>(args)` **restaura** essa categoria ao repassar. Sem ele, `args` dentro
  da função é um lvalue nomeado e o `std::string` seria **copiado** em vez de movido.

Saída real, provando o move: `[Motor] construído: 4cil, 300cv, gasolina` seguido de `comb após move: ''
(string movida)`.

> ⚠ O demo usa `new`/`delete` cru **de propósito**, para mostrar a mecânica sem camadas. Em
> código real este repositório usa RAII — `std::unique_ptr<T> meu_make(Args&&... args)
> { return std::make_unique<T>(std::forward<Args>(args)...); }` — sem `delete` e sem vazamento
> se o construtor lançar. Veja [`src/smart-pointers/`](../smart-pointers/).

**`std::tuple` por dentro.** A seção 3.5 monta uma tuple por **herança recursiva** — cada
nível guarda um elemento (`T cabeca`) e herda do resto: `struct MinhaTuple<T, Rest...> :
MinhaTuple<Rest...>`, com o caso base `template <typename... Ts> struct MinhaTuple {}` vazio. A
`std::tuple` real usa herança
**múltipla** indexada em vez de recursão, para não pagar profundidade de instanciação, mas
a ideia é a mesma.

### Demo 4 — SFINAE (`demo_sfinae.hpp`)

**SFINAE** = *Substitution Failure Is Not An Error*. Quando o compilador substitui os tipos
numa sobrecarga candidata e o resultado é inválido, ele **não gera erro** — descarta a candidata e segue
para a próxima. Só se *nenhuma* sobreviver é que há erro. **E `enable_if` não é diretiva do compilador;
são duas linhas:**

```cpp
template <bool B, class T = void> struct enable_if          {};                  // sem ::type
template <class T>                struct enable_if<true, T> { using type = T; }; // com ::type
template <bool B, class T = void>
using enable_if_t = typename enable_if<B, T>::type;                              // atalho C++14
```

Quando `B` é `false`, a struct é **vazia** — `::type` **não existe**. Referir-se a um membro inexistente
⇒ substituição falha ⇒ a sobrecarga some da lista, em silêncio. Toda a técnica é isso. (É aqui que o
`typename` da ideia 5 aparece: `enable_if<...>::type` é nome dependente.)
**Forma 1 — no tipo de retorno**, a que o demo usa:

```cpp
template <typename T>                                   // some quando T não é inteiro
typename std::enable_if<std::is_integral<T>::value, void>::type
descrever(T val) { fmt::print("  integral: {} ({})\n", val, sizeof(T)); }

template <typename T>                                   // some quando T não é float
typename std::enable_if<std::is_floating_point<T>::value, void>::type
descrever(T val) { fmt::print("  floating: {:.4f} ({})\n", (double)val, sizeof(T)); }
```

Saída real — note `bool` e `long` entrando como integrais: `integral: 42 (4)`, `integral: true (1)`,
`floating: 3.1400 (4)`, `integral: 100 (8)`.

**Forma 2 — como parâmetro de template.** Mais legível, com uma armadilha grande:

> ⚠ **`template <typename T, typename = std::enable_if_t<cond>>` não serve para criar duas
> sobrecargas.** Um argumento de template **padrão não faz parte da assinatura**: duas funções
> que diferem *apenas* nele são a mesma função, redeclarada. Testado aqui:
> ```cpp
> template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>       void f(T) {}
> template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>> void f(T) {}
> // error: redefinition of 'template<class T, class> void f(T)'
> ```
> A forma que **funciona** põe o `enable_if` num parâmetro sem nome mas **com tipo**, que aí
> entra na assinatura — `template <typename T, std::enable_if_t<cond, int> = 0> void f(T) {}`.
> No demo, as duas `dobrar` escapam do erro só porque os tipos de **retorno** também diferem
> (`T` e `std::string`), o que já basta para serem templates distintos — não conte com isso.

**Detection idiom.** Duas peças novas — `template <class...> using void_t = void;` (descarta
qualquer lista de tipos e devolve `void`) e `template <class T> std::add_rvalue_reference_t<T> declval()
noexcept;` (declarada, nunca definida):

- `std::void_t<A,B,C>` é sempre `void`. O ponto não é o resultado: é que, para chegar em
  `void`, `A`, `B` e `C` precisam ser tipos **válidos** — um inválido ⇒ substituição falha ⇒ a
  especialização parcial não casa.
- `std::declval<T>()` produz um `T&&` **fictício**, em contexto não avaliado. Permite
  escrever `decltype(std::declval<T>().size())` sem precisar de um `T` construível. Nunca é definida de
  propósito: chamá-la de verdade é erro de link.

```cpp
template <typename T, typename = void>
struct tem_size : std::false_type {};                          // caso geral: não tem
template <typename T>
struct tem_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};                                       // se .size() existir, esta casa
```

Saída real: `tem_size<std::vector<int>>: true`, `tem_size<SemMembros>: false`, `tem_adicao<std::string>:
true` (existe `string + string`), `tem_adicao<SemMembros>: false`.

**`if constexpr` (C++17).** O ramo não escolhido é **descartado sem ser compilado**, o que
permite escrever numa única função código inválido para alguns `T`:

```cpp
template <typename T>
std::string para_string(const T& val) {
    if constexpr (std::is_same_v<T, std::string>) return "\"" + val + "\"";
    else if constexpr (std::is_arithmetic_v<T>)   return std::to_string(val);
    else                                          return "[tipo desconhecido]";
}
```

Com um `if` comum, o primeiro ramo teria que compilar para `T = int` — e não compila. A regra que o
próprio demo imprime: **condição sobre o comportamento interno → `if constexpr`; condição sobre qual
função chamar → sobrecarga/SFINAE**.

> ⚠ Em **C++20** (o padrão deste repositório) quase tudo desta seção foi substituído por
> **concepts**: `template <std::integral T> void descrever(T)` faz o que quatro linhas de
> `enable_if` faziam, com erro legível. Estude SFINAE para *ler* código existente; escreva
> concepts — [`src/concepts/`](../concepts/).

### Demo 5 — `<type_traits>` e `constexpr` (`demo_type_traits.hpp`)

Traits respondem **perguntas sobre tipos em compile-time**: perguntas (`::value`, atalho `_v`) e
transformações (`::type`, atalho `_t`).

```cpp
std::is_integral_v<int>                // true
std::is_same_v<int, long>              // false
std::remove_const_t<const int>         // int
std::decay_t<int[5]>                   // int*   ← array decai para ponteiro
std::conditional_t<true, int, double>  // int    ← ternário de tipos
```

Resultados da saída real que costumam surpreender:

```
  is_integral<bool>              true    ← bool é um tipo integral
  is_array<std::array<int,5>>    false   ← std::array é uma classe, não um array
  is_enum<std::byte>             true    ← std::byte é um enum class
  is_abstract<Base>              false   ← só ter destrutor virtual não torna abstrata
  is_polymorphic<Base>           true    ← mas torna polimórfica
```

`std::decay_t<T>` é exatamente o que acontece a um tipo passado **por valor**: remove referência, remove
`const`/`volatile` de escalares, array → ponteiro, função → ponteiro.
**`constexpr`** é a outra metade da computação em compile-time:

```cpp
constexpr int fatorial(int n) { return (n <= 1) ? 1 : n * fatorial(n - 1); }
constexpr int fat5 = fatorial(5);          // 120, calculado pelo g++, não pelo binário
static_assert(fatorial(5) == 120, "...");  // verificado na compilação; falha = erro de build

template <int N> constexpr std::array<int, N> gerar_quadrados() {
    std::array<int, N> r{};
    for (int i = 0; i < N; ++i) r[i] = i * i;
    return r;
}
constexpr auto quad = gerar_quadrados<6>();   // 0 1 4 9 16 25 — já no binário
```

> ⚠ `constexpr` diz *pode ser*, não *sempre é*: `fatorial(x)` com `x` lido do teclado roda em
> runtime. Para **exigir** compile-time existe `consteval` (C++20) — [`src/compile_time/`](../compile_time/).

**Trait próprio.** A seção 5.5 junta tudo — `void_t` para "é um container?" e especialização
total para "é uma string?":

```cpp
template <typename T, typename = void> struct eh_container : std::false_type {};
template <typename T>
struct eh_container<T, std::void_t<typename T::value_type,
                                   decltype(std::declval<T>().begin()),
                                   decltype(std::declval<T>().size())>> : std::true_type {};
```

Saída real: `std::string` → 32 bytes, container=true, string=true; `std::array<int,3>` → 12 bytes,
container=true, string=false; `const char*` → 8 bytes, container=false.

### Demo 6 — CRTP (`demo_crtp.hpp`)

**CRTP** = *Curiously Recurring Template Pattern*: a derivada passa **a si mesma** como
argumento de template para a própria base.

```cpp
template <typename Derived>
struct FormaBase {
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();   // desce para a derivada
    }
};

struct Circulo : FormaBase<Circulo> {        // ← passa a si mesma
    double raio;
    double area_impl() const { return 3.14159 * raio * raio; }
    const char* nome()  const { return "Círculo"; }
};
```

A base chama métodos da derivada **sem `virtual`**, via `static_cast`; como `Derived` é conhecido em
compile-time, a chamada é direta e inlinável. Saída real: `Círculo → área=78.54, perímetro=31.42` e
`Retângulo → área=12.00, perímetro=14.00`.

> ⚠ **A armadilha nº 1 do CRTP.** Copiar e colar a linha de herança sem trocar o argumento —
> `struct Gato : FormaBase<Circulo> { ... };` — **compila**. O `static_cast<const Circulo*>(this)`
> da base converte um `Gato*` num `Circulo*` e o programa lê campos que não existem: UB
> silencioso. Defesa barata, no destrutor da base:
> `static_assert(std::is_base_of_v<FormaBase, Derived>);`
>
> ⚠ E os métodos `_impl` da derivada precisam ser **acessíveis** à base: torná-los `private`
> (vontade legítima) quebra a compilação, e a saída é `friend struct FormaBase<Circulo>;`.

**Mixin: gerar operadores** — o que o Boost.Operators faz. Você implementa `==` e `<`; o
mixin fornece `!=`, `>`, `<=`, `>=`:

```cpp
template <typename Derived>
struct Comparavel {
    bool operator!=(const Derived& o) const { return !(static_cast<const Derived&>(*this) == o); }
    bool operator> (const Derived& o) const { return o < static_cast<const Derived&>(*this); }
    bool operator<=(const Derived& o) const { return !(static_cast<const Derived&>(*this) > o); }
    bool operator>=(const Derived& o) const { return !(static_cast<const Derived&>(*this) < o); }
};
struct Temperatura : Comparavel<Temperatura> {   // só implementa == e <
    double celsius;
    bool operator==(const Temperatura& o) const { return celsius == o.celsius; }
    bool operator< (const Temperatura& o) const { return celsius <  o.celsius; }
};
```

Seis operadores pelo preço de dois. (Para este caso, C++20 tem um atalho: `auto operator<=>(const
Temperatura&) const = default;` — mas o CRTP segue valendo quando a lógica não é derivável
automaticamente.)

**Estado estático por tipo.** Como cada `ContadorInstancias<X>` é uma classe distinta, cada
uma tem **seu próprio** `contagem`:

```cpp
template <typename Derived>
struct ContadorInstancias {
    static inline int contagem = 0;     // um contador por instanciação da base
    ContadorInstancias()  { ++contagem; }
    ~ContadorInstancias() { --contagem; }
};
struct Conexao : ContadorInstancias<Conexao> {};   // contador próprio
struct Sessao  : ContadorInstancias<Sessao>  {};   // outro contador, independente
```

Saída real: `Conexoes: 2, Sessoes: 1` → `Conexoes: 3, Sessoes: 3 (dentro do bloco)` → `Conexoes: 2,
Sessoes: 1 (após bloco)` — os destrutores decrementam sozinhos.

**`clone()` com tipo exato.** Com `virtual`, `Base* clone()` obriga o chamador a fazer cast;
com CRTP o retorno já é o tipo certo — `Derived* clonar() const { return new Derived( static_cast<const
Derived&>(*this)); }`, de modo que `doc.clonar()` devolve `Documento*`.

> ⚠ Também aqui o demo usa `new`/`delete` cru para mostrar o mecanismo (e faz o `delete`
> correspondente); em produção o retorno seria `std::unique_ptr<Derived>` via `make_unique`.

**CRTP × `virtual`**, com os termos da ideia 8:

| aspecto | `virtual` | CRTP |
|---|---|---|
| quando se decide qual função chamar | runtime | compile-time |
| custo por chamada / por objeto | vptr + vtable, chamada indireta / +8 bytes | zero, inlinável / sem overhead |
| coleção heterogênea (`vector<Base*>`) | ✓ | ✗ — cada derivada é um tipo sem base comum |
| extensível por biblioteca sem recompilar | ✓ | ✗ — o chamador recompila |
| quando escolher | runtime, coleções mistas, plugins | mixins, operadores, hot path medido |

Se você precisa de coleção heterogênea **e** quer evitar herança, há um terceiro caminho:
[`src/type_erasure/`](../type_erasure/).

## Como ler um erro de template

Vale ver um de verdade antes de encontrar o primeiro sozinho. Este programa — `struct P { int x; };` e
`std::vector<P> v(3); std::sort(v.begin(), v.end());` — produz
**78 linhas** de erro no GCC 13, quase todas *pilha de instanciação*, não o erro:


```
.../stl_algo.h:1819:   required from '... std::__insertion_sort(...)'
.../stl_algo.h:4861:   required from '... std::sort(...)'
terr.cpp:4:43:         required from here            ← ★ a SUA linha
.../predefined_ops.h:45:23: error: no match for 'operator<' (operand types are 'P' and 'P')
                                   ↑ ★ a CAUSA
```

Duas linhas importam: a do `required from here` com o seu arquivo (**onde** você pediu) e a última,
`error:` (**o quê** faltou — aqui, `P` não tem `operator<`). O resto é o caminho entre as duas. Filtre
com `2>&1 | grep -E "error:|required from here"`; e prefira concepts, que trocam esta pilha por uma
frase.

---

## Regras de ouro

| situação | faça |
|---|---|
| o mesmo algoritmo / a mesma estrutura para vários tipos | function template / class template |
| o parâmetro é um número, não um tipo | NTTP: `template <typename T, std::size_t N>` |
| mudar o comportamento para **um** tipo | especialização total (`template <>`) |
| mudar para uma **família** de tipos (`T*`, `Par<T,T>`) | especialização parcial de **classe** |
| o mesmo, mas para uma **função** | sobrecarga — parcial de função não existe |
| número variável de argumentos | variadic + fold expression |
| repassar argumentos sem cópia extra | `Args&&...` + `std::forward<Args>(args)...` |
| escolher o **comportamento interno** por propriedade do tipo | `if constexpr` (C++17) |
| escolher **qual sobrecarga existe** — C++11/14/17 / C++20 | `std::enable_if_t<c, int> = 0` / **concept** |
| detectar se um tipo tem um membro | detection idiom (`void_t` + `declval`) |
| reuso de comportamento sem custo de `virtual` | CRTP |
| guardar tipos diferentes no mesmo container | `virtual` ou type erasure — CRTP **não** serve |

---

## Como validar

```bash
P=$(sed -n 's|^prefix=||p' build/fmt.pc)        # prefixo do fmt instalado pelo Conan

# 1. rodar — deve terminar com a linha verde e sair com 0
./dist/bin/templates | tail -3 ; echo $?
# 2. compilar isolado, com todos os avisos
g++ -std=c++20 -Wall -Wextra -Wpedantic -I"$P/include" -c src/templates/main.cpp -o /dev/null
# 3. sanitizers — os demos 3 e 6 fazem new/delete manual, é onde um leak apareceria
g++ -std=c++20 -g -fsanitize=address,undefined -I"$P/include" \
    src/templates/main.cpp -o /tmp/tpl_asan -L"$P/lib" -lfmtd && /tmp/tpl_asan >/dev/null && echo OK
# 4. ver o que o compilador gerou — prova concreta da ideia 1
nm -C dist/bin/templates | grep 'demo_basics::maximo<'
```

Estado atual nesta máquina: passo 2 com **zero avisos**; passo 3 **sem nenhum leak e nenhum erro de
UBSan**; passo 4 devolvendo exatamente **três** símbolos (`int`, `double`, `std::string`) e nenhuma
versão `float`, porque ninguém chamou `maximo` com `float`.

---

## Referências

Neste repositório: [`src/concepts/`](../concepts/) (o substituto de SFINAE em C++20 — leia depois do
Demo 4); [`src/compile_time/`](../compile_time/) (`constexpr`, `consteval`, tabelas geradas na
compilação); [`src/type_erasure/`](../type_erasure/) (coleções heterogêneas sem herança, onde o CRTP não
alcança); [`src/ranges/`](../ranges/) (`view_interface` é CRTP dentro da biblioteca padrão);
[`src/smart-pointers/`](../smart-pointers/) (`make_unique`, o variadic mais usado do C++);
[`src/oop/`](../oop/) (`virtual`, vtable e RTTI: o lado dinâmico do Demo 6); [`src/stl/`](../stl/)
(containers e algoritmos, construídos com o que está aqui).

Fontes canônicas:

- cppreference — [Templates](https://en.cppreference.com/w/cpp/language/templates),
  [SFINAE](https://en.cppreference.com/w/cpp/language/sfinae), [Parameter
  pack](https://en.cppreference.com/w/cpp/language/parameter_pack),
  [`<type_traits>`](https://en.cppreference.com/w/cpp/header/type_traits)
- Herb Sutter, *Why Not Specialize Function Templates?* (GotW #49) — fonte da armadilha do Demo 2.
- Vandevoorde, Josuttis & Gregor, *C++ Templates: The Complete Guide*, 2ª ed.
