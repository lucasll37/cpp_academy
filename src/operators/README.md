# operators — Sobrecarga de Operadores

## O que é este projeto?

Sobrecarregar um operador é ensinar o compilador o que `a + b`, `a < b`, `a[i]` ou `a(x)`
significam quando `a` é um **tipo seu**. É a ferramenta que faz um tipo definido por você
se comportar como um tipo embutido — e é também a ferramenta que dá ao C++ boa parte da sua
má fama, quando usada onde não devia.

Este subprojeto cobre os cinco grupos de operadores que aparecem na prática, na ordem em
que se aprende: aritméticos e impressão, comparação (`operator<=>`), conversões, chamada e
indexação, e literais definidos pelo usuário.

```bash
make build
./dist/bin/operators
```

> `make build` compila **tudo**; o executável instalado é `dist/bin/operators`. Se for a
> primeira vez neste repositório, rode `make configure` antes (ele baixa o `fmt` pelo Conan
> e configura o `build/`). O `dist/` é o **prefixo de instalação**, não o diretório de build.

> ⚠ Este alvo compila com **C++23** (`override_options: ['cpp_std=c++23']` no
> [`meson.build`](meson.build)): o subscript multidimensional `m[i, j, k]` (Demo 4) e o
> `static operator()` (Demo 4) exigem esse padrão. O `cpp_std` da raiz continua C++20.

---

## Antes de começar

Esta seção define **todo** o vocabulário usado no resto do arquivo. Se você já conhece um
termo, pule; nada aqui depende de leitura externa.

### O mecanismo, em uma linha

**Um operador é açúcar sintático para uma chamada de função.** Quando o compilador vê
`a + b`, ele procura uma função chamada `operator+`:

```cpp
a + b     // significa   operator+(a, b)        se for uma função LIVRE
          // ou          a.operator+(b)         se for um MEMBRO de a
```

Tudo o que vem depois neste README é consequência disso. Sobrecarregar um operador é só
escrever uma função com um nome esquisito.

### O exemplo mínimo completo

Este programa compila e roda sozinho. Ele é o degrau zero do assunto:

```cpp
#include <iostream>

struct Vetor2 {
    double x{}, y{};
};

// operador LIVRE: a soma de dois vetores
Vetor2 operator+(const Vetor2& a, const Vetor2& b) {
    return {a.x + b.x, a.y + b.y};
}

// impressão: TEM que ser livre (o operando da esquerda é o stream)
std::ostream& operator<<(std::ostream& os, const Vetor2& v) {
    return os << '(' << v.x << ", " << v.y << ')';
}

int main() {
    Vetor2 a{1, 2}, b{3, 4};
    std::cout << a + b << '\n';       // imprime (4, 6)
}
```

Três coisas para reparar: (1) `operator+` é uma função normal, com um nome reservado;
(2) `a + b` chama essa função; (3) `operator<<` devolve o próprio `os`, e é isso que
permite encadear `os << a << b`.

### Glossário

| Termo | O que é |
|---|---|
| **açúcar sintático** | uma sintaxe mais curta que se traduz em outra já existente. `a + b` é açúcar para `operator+(a, b)`. |
| **ADL** (*Argument-Dependent Lookup*) | ao ver `f(x)`, o compilador procura `f` não só no escopo atual, mas **também nos namespaces dos tipos dos argumentos**. É por isso que um `friend` definido dentro da classe — que não está em nenhum escopo nomeado — ainda é encontrado quando um dos operandos é daquele tipo. |
| **resolução de overload** | quando existem várias funções com o mesmo nome, o compilador monta um conjunto de **candidatos**, descarta os que não aceitam os argumentos, e escolhe o melhor. Se dois empatam, é erro de ambiguidade. |
| **candidato reescrito / revertido** | candidatos que o C++20 acrescenta sozinho a partir de `<=>` e `==`. `a < b` vira o candidato reescrito `(a <=> b) < 0`; e o revertido `0 < (b <=> a)`, que troca a ordem dos operandos. |
| **conversão implícita** | conversão que o compilador aplica sem você pedir, para fazer os argumentos baterem com os parâmetros. |
| **contextualmente convertível para `bool`** | um punhado de posições da gramática (a condição do `if`, do `while`, o operando de `!`…) onde o compilador aplica uma conversão para `bool` **mesmo se ela for `explicit`**. A lista completa está no Demo 3. |
| **functor** (ou *objeto-função*) | uma classe qualquer que define `operator()`. Um objeto dela é chamável como se fosse função: `MaiorQue f{10}; f(20);`. Diferente de uma função, ele **carrega estado**. |
| **lambda** | açúcar para um functor: o compilador gera uma classe anônima com `operator() const` e transforma cada captura num membro. Ver [`src/lambdas/`](../lambdas/). |
| **inlinar** | substituir a chamada pelo corpo da função no ponto de uso. Só é possível quando o compilador sabe **estaticamente** qual função será chamada. |
| **type erasure** | guardar um objeto de tipo concreto atrás de uma interface fixa, de modo que o tipo original suma da assinatura mas continue vivo por dentro. É como `std::function` aceita qualquer chamável. Ver [`src/type_erasure/`](../type_erasure/). |
| **proxy** | objeto intermediário devolvido só para interceptar a operação seguinte. `m[i]` devolvendo um proxy só para que `[j]` funcione em cima dele é o exemplo clássico. |
| **curto-circuito** | a regra de que, em `a && b`, `b` **não é avaliado** se `a` for falso — e, no *ou* lógico, `b` não é avaliado se `a` for verdadeiro. Vale para os operadores embutidos; some se você sobrecarregá-los. |
| **ordem total** | uma relação em que quaisquer dois valores são comparáveis **e** valores equivalentes são indistinguíveis. `int` com `<` é uma ordem total. |
| **preordem total** | quaisquer dois valores são comparáveis, mas dois valores **diferentes** podem ser equivalentes na ordem (`"ABC"` e `"abc"` numa comparação sem distinguir maiúsculas). |
| **`NaN`** | *Not a Number* — o valor de ponto flutuante que representa "resultado inválido" (`0.0/0.0`). Ele não é igual, nem menor, nem maior que nada, **nem que ele mesmo**. É a razão de `double` ser `partial_ordering`. |
| **UDL** (*User-Defined Literal*) | um sufixo que você define para literais: `100_km`, `42_pct`. Demo 5. |

### O que já é bom saber

Nada é obrigatório, mas estes subprojetos aparecem citados aqui:

- [`src/value_semantics/`](../value_semantics/) — cópia, move e por que o `+` canônico recebe o primeiro operando por valor;
- [`src/templates/`](../templates/) — os `template <...>` do Demo 5;
- [`src/lambdas/`](../lambdas/) — o que o compilador gera para uma lambda.

---

## Por que estudar isto?

Porque a decisão é quase sempre **não sobrecarregar**, e reconhecer isso exige entender o
que se ganha e o que se perde.

O que se ganha, quando o operador é o certo:

| Situação | Sem sobrecarga | Com sobrecarga |
|---|---|---|
| somar dois vetores | `soma(escalar(a, 2), b)` | `2 * a + b` |
| ordenar um tipo seu | 6 funções escritas à mão | `auto operator<=>(const T&) const = default;` |
| unidades físicas | comentário `// em metros` | `5_km + 300_m` compila, `5_km + 2_s` **não** |
| matriz 3D | `vol.at(i, j, k)` | `vol[i, j, k]` |

O que se perde, quando não é:

> **Não surpreenda.** Se `+` não for comutativo, associativo e barato, escreva um método com
> nome. `a.concatenar(b)` nunca confundiu ninguém — e a **precedência** dos operadores é
> fixa: sobrecarregar `^` para potência não faz `2 ^ 3 + 1` virar `2³ + 1`.

---

## Estrutura de arquivos

```
src/operators/
├── main.cpp                ← ponto de entrada, cor orchid
├── demo_aritmeticos.hpp    ← 1 · membro vs livre, hidden friends, forma canônica,
│                                operator<<, ++/--
├── demo_comparacao.hpp     ← 2 · operator<=>, as 3 categorias, candidatos revertidos
├── demo_conversao.hpp      ← 3 · explicit, operator T(), explicit operator bool
├── demo_chamada.hpp        ← 4 · functors, operator[] multidim, operator->, static operator()
├── demo_literais.hpp       ← 5 · UDLs, unidades dimensionais, literais de template
├── meson.build
└── README.md
```

---

## Demo 1: Aritméticos e impressão

### O caso mais simples, escrito do zero

Antes de qualquer sutileza: um tipo pequeno com `+` e `+=`.

```cpp
class Polinomio {
public:
    // 1) a operação que MODIFICA — membro, e é aqui que mora a lógica
    Polinomio& operator+=(const Polinomio& o);      // devolve *this, para encadear

    // 2) a que CRIA — derivada da de cima
    friend Polinomio operator+(Polinomio a, const Polinomio& b) { return a += b; }
};
```

Repare que a lógica da soma existe **uma vez só**. É a forma canônica, e vale para
`+ - * / % & | ^ << >>`: escreva o `op=` membro, derive o `op` livre dele.

O `a` de `operator+` é recebido **por valor** de propósito: ele já é a cópia de trabalho que
vamos modificar e devolver — e, se o chamador passou um temporário, essa cópia é na verdade
um *move* ([`src/value_semantics/`](../value_semantics/)).

### Imprimir o próprio tipo

O primeiro operador que qualquer pessoa quer sobrecarregar. Ele **tem** que ser função
livre, porque o operando da esquerda é o stream — e você não pode acrescentar um membro a
`std::ostream`:

```cpp
friend std::ostream& operator<<(std::ostream& os, const Contador& c) {
    return os << "Contador(" << c.v_ << ')';
}
```

Três regras que cabem nessa linha:

1. recebe e devolve **`std::ostream&`** — é o retorno que permite `os << a << b`;
2. o segundo parâmetro é `const T&`;
3. **nunca** ponha `'\n'` ou `std::endl` lá dentro: quem imprime decide isso.

A alternativa moderna, que este repositório usa, é especializar o *formatter*. Ele precisa
morar no namespace da biblioteca, não no seu:

```cpp
template <>
struct fmt::formatter<Contador> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
    auto format(const Contador& c, fmt::format_context& ctx) const {
        return fmt::format_to(ctx.out(), "Contador({})", c.valor());
    }
};
```

Com isso, `fmt::print("{}", c)` funciona. Trocando `fmt::` por `std::`, o mesmo código serve
para `std::format` — ver [`src/strings/`](../strings/), Demo 3.

Saída medida (`── 1.5`):

```
via operator<<     : Contador(7)
via fmt::formatter : Contador(7)
```

### `++` e `--`: o parâmetro `int` que não existe

Prefixo e posfixo têm a mesma cara. A linguagem os distingue por um parâmetro `int`
**sem nome e sem uso** — quem tem o `int` é o posfixo:

```cpp
Tipo& operator++();          // ++c — prefixo: incrementa e devolve REFERÊNCIA
Tipo  operator++(int);       // c++ — posfixo: devolve uma CÓPIA do estado anterior
```

```
++p  → devolveu 1, p agora é 1        ← o valor NOVO
q++  → devolveu 0, q agora é 1        ← o valor ANTIGO
```

Daí a regra prática: **em laços, prefira `++i`**. O posfixo é obrigado a fabricar uma cópia
do estado anterior para devolver; em `int` o otimizador apaga isso, num iterador de árvore
ou numa classe grande, não apaga.

### Membro ou função livre?

Agora a sutileza. A diferença está na **conversão implícita do operando da esquerda**: um
operador membro não converte o `this`, uma função livre converte os dois lados.

```cpp
m + 5.0   // ✓ com operador membro (o double vira Dinheiro)
5.0 + m   // ✗ NÃO COMPILA — o `this` não converte
```

| Operador | Forma |
|---|---|
| simétrico (`+` `-` `*` `/` `==`) | função **livre** |
| modifica o objeto (`+=`) | **membro** |
| `=` `[]` `()` `->` `->*` | **membro** (exigido pela linguagem) |
| `<<` `>>` (streams) | **livre** (exigido: o operando da esquerda é o stream) |

> `operator=` é o único da lista que você raramente escreve à mão hoje: `= default` /
> `= delete` e a *Rule of 5* resolvem o caso normal. Ver
> [`src/value_semantics/`](../value_semantics/).

### Hidden friends

Um `friend` **definido no corpo da classe** não entra em nenhum escopo nomeado: só é
encontrável por **ADL**, a partir do tipo dos seus argumentos (ver o glossário).

```cpp
struct Vetor2 {
    double x{}, y{};
    friend Vetor2 operator+(const Vetor2& a, const Vetor2& b) {
        return {a.x + b.x, a.y + b.y};
    }
};

Vetor2 a{1, 2}, b{3, 4};
auto s = a + b;      // encontrado por ADL — nenhum `using` foi necessário
```

Ganhos: o namespace não fica poluído com N operadores · a resolução de overload tem menos
candidatos a considerar (é tempo de compilação em projeto grande) · os dois operandos ainda
convertem implicitamente.

Recomendação: declare operadores livres **sempre** como hidden friends, a menos que precise
deles fora do ADL.

### O que NÃO sobrecarregar

- `&&` e `||` — perdem o **curto-circuito**. Viram chamada de função normal, e **ambos** os
  lados passam a ser avaliados sempre. Isso muda o comportamento de código que dependia de
  `p && p->campo`.
- `,` (vírgula) — perde a ordem de avaliação garantida da esquerda para a direita.
- `&` unário — quebra código genérico que espera obter o endereço do objeto.
- `->*` — ninguém entende, ninguém usa.
- `new` / `delete` — só se você está escrevendo um alocador
  ([`src/memory/`](../memory/)).

E os que são possíveis mas quase sempre erro de design: `+` não comutativo (a concatenação
de string é a exceção aceita), `<<` que não é saída de stream, `%` para formatação, `^` para
potência.

---

## Demo 2: `operator<=>`

### Seis operadores viram um

```cpp
struct Versao {
    int maior{}, menor{}, patch{};
    auto operator<=>(const Versao&) const = default;
};
```

Essa linha gera `<`, `<=`, `>`, `>=`, `==` e `!=`. A comparação é **lexicográfica na ordem
de declaração** dos membros — trocar a ordem dos campos muda a semântica.

Saída medida (`── 2.1`), com `a = {1,2,3}` e `b = {1,3,0}`:

```
a <  b → true      a >  b → false     a == b → false
a != b → true      a <= b → true
ordenadas: 1.2.3 1.2.10 1.10.0 2.0.0
```

### As três categorias

O **tipo devolvido** por `<=>` documenta que garantias a comparação oferece:

| Categoria | Todos comparáveis | Equivalente = igual | Exemplo |
|---|---|---|---|
| `strong_ordering` | sim | **sim** | `int`, um `Id` |
| `weak_ordering` | sim | não | string comparada sem distinguir maiúsculas |
| `partial_ordering` | **não** | não | `double` (por causa do `NaN`) |

Só `strong_ordering` é uma **ordem total** sobre os valores. `weak_ordering` é uma
**preordem total**: ordena as *classes de equivalência*, não os valores — dois valores
distintos podem empatar. Em `partial_ordering` existem pares simplesmente
**incomparáveis**.

Saída medida (`── 2.2`):

```
"ABC" <=> "abc" é equivalente? true       ← weak: empatam na ordem…
mas as strings são idênticas?   false     ← …mas não são o mesmo valor

1.0 <  NaN → false                        ← partial
1.0 >  NaN → false
1.0 == NaN → false                        ← TRÊS falsos: o resultado é `unordered`
```

Prefira a categoria **mais forte que for verdadeira**: quanto mais forte, mais os
containers e algoritmos podem otimizar.

### Por que `==` continua separado

A ordenação precisa comparar campo a campo até achar diferença. A **igualdade** quase sempre
tem um atalho barato — comparar o tamanho primeiro descarta a maioria dos pares sem olhar o
conteúdo.

Por isso o padrão **não** deriva `==` de um `<=>` escrito à mão; só de um `= default`.

> **Regra:** escreveu `<=>` à mão? Escreva o `==` também.

### Candidatos reescritos e revertidos

O mecanismo pelo qual um operador vira seis. Ao ver `a < b`, o compilador considera, nesta
ordem: (1) um `operator<` próprio; (2) o candidato **reescrito** `(a <=> b) < 0`; (3) o
candidato **revertido** `0 < (b <=> a)`.

Declarando apenas `Metros <=> double` e `Metros == double`, saem **8** operações
(4 relacionais × 2 ordens de operando):

```
m <  7.0  ✓ reescrito:  (m <=> 7.0) < 0
7.0 >  m  ✓ REVERTIDO:  0 < (m <=> 7.0)
```

Antes do C++20, isso exigiria 12 funções escritas à mão.

### Onde a ordenação é exigida: containers ordenados

`std::set` e `std::map` precisam de uma ordem sobre a chave. Com `<=> = default`, o tipo
entra neles sem nenhuma linha extra:

```cpp
struct Prioridade {
    int         nivel;
    std::string etiqueta;
    auto operator<=>(const Prioridade&) const = default;
};

std::set<Prioridade> fila{{2, "compilar"}, {1, "urgente"}, {2, "avaliar"}, {3, "opcional"}};
```

Saída medida (`── 2.5`): `[1] urgente · [2] avaliar · [2] compilar · [3] opcional`.

> ⚠ A ordem dos **membros** na declaração define a ordem lexicográfica. Se `etiqueta`
> viesse antes de `nivel`, este `set` ordenaria alfabeticamente.

---

## Demo 3: Conversões

### Um construtor de um argumento já é uma conversão

```cpp
Metros(double v);            // ⚠ conversão IMPLÍCITA de double para Metros
explicit Metros(double v);   // ✓ exige Metros{3.5} no ponto de chamada
```

Sem `explicit`, um `double` vira `Metros` sem ninguém pedir — e se a função esperasse
**segundos**, compilaria igual.

> É por isso que `std::vector<int> v(10);` cria 10 zeros e `std::vector<int> v = 10;` não
> compila: o construtor de tamanho é `explicit`.

### `operator T()` — o caminho de volta

O construtor converte **para** o seu tipo; `operator T()` converte o seu tipo **para outro**.
Mesmos riscos, mesmo remédio:

```cpp
class Temperatura {
public:
    explicit Temperatura(double c) : c_(c) {}
    operator double() const { return c_; }        // ⚠ implícito
private:
    double c_;
};

class Duracao {
public:
    explicit Duracao(long s) : s_(s) {}
    explicit operator long() const { return s_; } // ✓ exige static_cast
private:
    long s_;
};
```

Saída medida (`── 3.2`):

```
Temperatura{25} + 10.0 = 35.0            (conversão implícita)
⚠ `t * 2` também compila, e devolve 50.0 — um DOUBLE, não uma Temperatura
⚠ pior: `t == 25` compara com double, e Temperatura deixou de ser tipo forte

COM explicit:
  `d + 1`                    → NÃO COMPILA
  `static_cast<long>(d) + 1` → 3601
```

O tipo forte se **dissolve** na primeira conversão implícita. Por isso a recomendação, nesta
ordem:

1. prefira um **método com nome**: `t.celsius()`, `d.segundos()` — é o mais legível e não
   participa de nenhuma resolução de overload surpresa;
2. se precisar do operador de conversão, marque-o `explicit`;
3. deixe implícito só quando o seu tipo é honestamente *o outro tipo* com um embrulho fino.

### A regra da UMA conversão definida pelo usuário

Numa sequência de conversão implícita, o compilador aplica **no máximo uma** conversão
definida pelo usuário:

```cpp
struct A { A(int); };  struct B { B(const A&); };

recebe_b(B{A{1}})  ✓ explícito
recebe_b(A{1})     ✓ UMA conversão (A → B)
recebe_b(1)        ✗ DUAS (int → A → B)
```

Conversões **padrão** não contam para o limite: `int → double`, derivado → base, array →
ponteiro, `T*` → `const T*`, enum → int. Por isso `recebe_b(A{1.0})` funciona: `double→int`
é padrão, e só `A→B` é definida pelo usuário.

### `explicit operator bool` — a exceção que confirma a regra

É o único operador de conversão que se marca `explicit` **e** ainda funciona sem
`static_cast` — nos contextos *contextualmente convertíveis para `bool`* (glossário):

```
if   while   for(;cond;)   do-while   !   &&   ||   ?:   static_assert   noexcept(expr)
```

```cpp
if (c)      ✓ funciona
int n = c;  ✗ NÃO COMPILA
c + 1;      ✗ NÃO COMPILA
```

Sem `explicit`, `conexao + 41` daria 42. E pior: `conexaoA == conexaoB` compararia os
**bools** — duas conexões fechadas seriam "iguais".

> Nos streams isso corrigiu um bug histórico: até o C++03 eles convertiam para `void*`, e
> `cout << cin` compilava. É também como `unique_ptr`, `shared_ptr` e `optional` respondem
> a `if (p)`.

### `explicit` condicional (C++20)

```cpp
template <typename U>
explicit(!std::is_convertible_v<U, T>) Caixa(U&& v);
```

O `explicit` passa a aceitar uma expressão booleana de compilação: o construtor é explícito
**só quando** a conversão subjacente não é implícita. É assim que `std::pair`, `std::tuple`
e `std::optional` propagam a explicitude dos tipos que embrulham.

---

## Demo 4: `operator()`, `operator[]`, `operator->`

### O que é um functor

Uma classe qualquer que define `operator()`. Um objeto dela é chamável como se fosse uma
função — e, ao contrário de uma função, **carrega estado**:

```cpp
struct MaiorQue {
    int limite;                                        // ← o estado
    bool operator()(int x) const { return x > limite; }
};

MaiorQue m10{10};
m10(20);                                               // true — chamada como função
std::count_if(v.begin(), v.end(), MaiorQue{5});        // o mesmo TIPO, outro estado
```

Uma lambda **é** um functor: o compilador gera exatamente essa classe, anônima, e transforma
cada captura num membro ([`src/lambdas/`](../lambdas/)).

### Functors batem ponteiros de função

| Forma | Estado | Inlinável | Tipo próprio |
|---|---|---|---|
| ponteiro para função | não | **não** | não |
| functor | sim | **sim** | sim |
| lambda | sim | **sim** | sim (anônimo) |
| `std::function` | sim | não | apagado ([type erasure](../type_erasure/)) |

O functor tem **tipo próprio**, então o compilador sabe estaticamente qual código será
chamado e pode inlinar. Um ponteiro para função só é conhecido em tempo de execução.

### `static operator()` (C++23)

Um functor **sem estado** não precisa do `this`. Marcar o `operator()` como `static` elimina
o parâmetro implícito:

```cpp
struct EhPar {
    static bool operator()(int x) { return x % 2 == 0; }   // C++23
};

std::count_if(v.begin(), v.end(), EhPar{});                // usa como sempre
EhPar{}(4);                                                // ou chama direto
```

Saída medida (`── 4.5`): `count_if(EhPar{}) = 3`, `sizeof(EhPar) = 1`,
`sizeof(EhParClassico) = 1`.

O ganho **não** é o `sizeof` (uma classe vazia já ocupa 1 byte nos dois casos): é a ausência
do parâmetro `this` implícito em cada chamada. É por isso que os adaptadores de `<ranges>`
do C++23 usam `static operator()`.

### Subscript multidimensional (C++23)

Antes, uma matriz tinha três saídas, todas com defeito:

| Saída | Defeito |
|---|---|
| `m(i,j)` | parece chamada de função, não indexação |
| `m[i][j]` | exige uma classe **proxy** por dimensão (glossário), e o proxy pode ficar pendurado |
| `m[{i,j}]` | constrói um `pair` a cada acesso |

Em C++23, `operator[]` aceita N argumentos:

```cpp
double& operator[](std::size_t i, std::size_t j, std::size_t k);
vol[1, 1, 1];
```

Saída medida (`── 4.3`): `vol[0,0,0] = 1`, `vol[0,1,0] = 3`, `vol[1,1,1] = 8`.

> ⚠ **Mudança de significado.** Em C++20, `a[i, j]` era o operador **vírgula**: avaliava
> `i`, descartava, e indexava por `j`. O C++20 deprecou esse uso e o C++23 reciclou a
> sintaxe — código antigo com `a[i, j]` **muda de comportamento**. O GCC avisa com
> `-Wcomma-subscript`, ligado por padrão nos dois modos (o comando está em *Como validar*).
> Para manter o significado antigo, ponha parênteses: `a[(i, j)]`.

É esta feature que sustenta o `std::mdspan` do C++23 (que o GCC 13 ainda não tem — ver
[`src/ROADMAP.md`](../ROADMAP.md)).

### `operator->` e o drill-down

Regra única na linguagem: se `operator->` devolve algo que **não** é ponteiro cru, o
compilador chama `operator->` de novo no resultado, recursivamente, até chegar a um ponteiro:

```
reg->titulo
  → reg.operator->()      devolve Acesso (não é ponteiro)
  → Acesso.operator->()   devolve Documento* (é ponteiro!)
  → Documento*->titulo
```

Nenhum outro operador se comporta assim. É o mecanismo por trás de `unique_ptr`, `optional`,
dos iteradores e de qualquer proxy que precise repassar o acesso sem replicar a interface.

---

## Demo 5: Literais definidos pelo usuário

### As quatro formas

Um UDL é uma função `operator""_sufixo`. O **tipo do literal** decide a assinatura:

| Sufixo em… | Assinatura |
|---|---|
| `42_x` | `(unsigned long long)` |
| `4.2_x` | `(long double)` |
| `"abc"_x` | `(const char*, std::size_t)` |
| `'a'_x` | `(char)` |

> ⚠ **A regra do underscore:** sufixos **sem** `_` inicial são reservados para a biblioteca
> padrão. `operator""km` compila hoje e pode colidir amanhã. Sempre `_km`.

### Unidades com dimensões verificadas

As dimensões físicas viram **expoentes no tipo**:

```cpp
template <int Comprimento, int Massa, int Tempo> class Grandeza;
using Velocidade = Grandeza<1, 0, -1>;    // comprimento / tempo
```

```
100_km / 2_h      = 13.89 m/s   ← tipo Velocidade, DERIVADO pelo compilador
70_kg * 9.81 m/s² = 686.70 N    ← tipo Forca, derivado
5_km + 300_m      = 5300 m      ✓
5_km + 2_s        → NÃO COMPILA ✗ dimensões incompatíveis
```

E o custo em tempo de execução é **zero**: `sizeof(Comprimento) == sizeof(double)`. Os
expoentes existem só no tipo, e somem na compilação. É assim que funcionam mp-units,
Boost.Units — e o próprio `std::chrono`, que é um sistema de unidades de tempo
([`src/chrono/`](../chrono/)).

### Os literais da biblioteca padrão

| Header | Namespace | Sufixos |
|---|---|---|
| `<chrono>` | `std::chrono_literals` | `h` `min` `s` `ms` `us` |
| `<string>` | `std::string_literals` | `s` |
| `<string_view>` | `std::string_view_literals` | `sv` |
| `<complex>` | `std::complex_literals` | `i` `if` `il` |

O caso em que `"..."s` importa: `"a\0b"s` tem **3** caracteres (preserva o `\0`), enquanto
`strlen("a\0b")` daria 1.

### Literais de template — validação em compilação

Existe uma quinta forma: receber os **dígitos** como parâmetros de template. Aí eles podem
ser validados na compilação:

```cpp
template <char... Digitos>
constexpr double operator""_pct() {
    constexpr int v = para_inteiro<Digitos...>();
    static_assert(v >= 0 && v <= 100, "porcentagem precisa estar entre 0 e 100");
    return v / 100.0;
}

50_pct   ✓
150_pct  ✗ NÃO COMPILA, com a mensagem acima
```

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Operador simétrico | função livre, como **hidden friend** |
| Operador que modifica | membro (`+=`), e derive o `+` dele |
| Imprimir o próprio tipo | `friend std::ostream& operator<<(std::ostream&, const T&)` — ou um `formatter` |
| Percorrer um laço | `++i`, não `i++` |
| Comparação | `auto operator<=>(const T&) const = default;` |
| Escreveu `<=>` à mão | escreva o `==` também |
| Construtor de um argumento | `explicit` por padrão |
| `operator T()` | um método com nome; se precisar do operador, `explicit` |
| Quer `if (obj)` | `explicit operator bool` |
| Objeto chamável | functor ou lambda — nunca ponteiro de função |
| Functor sem estado (C++23) | `static operator()` |
| Matriz | `operator[](i, j)` em C++23; `operator()` antes |
| Wrapper que repassa acesso | `operator->` (o drill-down faz o resto) |
| Grandeza física | tipo forte + UDL — custo zero, correção garantida |
| Sufixo de literal | sempre com `_` inicial |
| Em dúvida se sobrecarregar | **não** — escreva um método com nome |

---

## Como validar

Rodar pelo build do repositório:

```bash
make build
./dist/bin/operators
```

Compilar direto, sem o Meson, para conferir que não há avisos (é o mesmo compilador e as
mesmas flags que o `-O0 -g` do build de debug usa, mais o `-Wpedantic`):

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/operators/main.cpp -o /tmp/op -L$FMTP/lib -lfmtd
/tmp/op
```

Estado atual: **zero avisos** com `-Wall -Wextra -Wpedantic`. O `-std=c++23` é obrigatório;
com `-std=c++20` o arquivo não compila (`vol[i, j, k]` e `static operator()`).

Sob os sanitizers, para confirmar que nenhum demo tem UB nem acesso inválido:

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -fsanitize=address,undefined -g -I$FMTP/include \
    src/operators/main.cpp -o /tmp/op_asan -L$FMTP/lib -lfmtd
/tmp/op_asan
```

Estado atual: roda limpo, **sem nenhum relatório** de ASan/UBSan.

O que se espera ver na saída:

| Seção | Confirma |
|---|---|
| `── 1.1` | `5.0 + l` funciona com função livre e não com membro |
| `── 1.5` | `operator<<` e `fmt::formatter` imprimem o mesmo texto; `++p` devolve 1 e `q++` devolve 0 |
| `── 2.2` | os **três** `false` do `NaN` — é o `unordered` do `partial_ordering` |
| `── 2.4` | `7.0 > m` funciona sem ninguém ter declarado `double > Metros` |
| `── 3.4` | `if (c)` compila e `int n = c;` não |
| `── 4.3` | `vol[1,1,1] = 8` — três índices, um `operator[]` |
| `── 5.x` | `5_km + 2_s` listado como "NÃO COMPILA" (está comentado no fonte de propósito) |

A última linha tem de ser:

```
✓ Todos os demos de operadores concluídos!
```

Para ver com os próprios olhos a mudança de significado do `a[i, j]` (Demo 4), compile o
mesmo trecho nos dois padrões:

```bash
echo 'int main(){ int v[3]{}; int i=0,j=1; return v[i,j]; }' > /tmp/cs.cpp
g++ -std=c++20 -Wcomma-subscript -c /tmp/cs.cpp -o /dev/null
g++ -std=c++23 -Wcomma-subscript -c /tmp/cs.cpp -o /dev/null
```

O primeiro diz `top-level comma expression in array subscript is deprecated`; o segundo diz
`... changed meaning in C++23`. É literalmente o mesmo código com dois significados. Com
parênteses — `v[(i,j)]` — nenhum dos dois avisa, e o significado antigo (operador vírgula)
fica preservado.

---

## Referências

Dentro deste repositório:

- [`src/lambdas/`](../lambdas/) — o functor que o compilador gera para cada lambda
- [`src/value_semantics/`](../value_semantics/) — cópia, move, e a *Rule of 5* por trás de `operator=`
- [`src/type_erasure/`](../type_erasure/) — o "tipo apagado" de `std::function` na tabela do Demo 4
- [`src/memory/`](../memory/) — o único caso legítimo de sobrecarregar `new`/`delete`
- [`src/strings/`](../strings/) — `std::format` e a especialização de `formatter` do Demo 1
- [`src/chrono/`](../chrono/) — o sistema de unidades da biblioteca padrão, feito com UDLs
- [`src/templates/`](../templates/) — os templates variádicos por trás das unidades do Demo 5
- [`src/ROADMAP.md`](../ROADMAP.md) — o que o GCC 13 ainda não tem (`std::mdspan`)

Fora dele:

- cppreference — [*Operator overloading*](https://en.cppreference.com/w/cpp/language/operators) e [*User-defined literals*](https://en.cppreference.com/w/cpp/language/user_literal)
- **P0515R3** — *Consistent comparison* (o `operator<=>`, C++20)
- **P0892R2** — *`explicit(bool)`* (o `explicit` condicional do Demo 3)
- **P2128R6** — *Multidimensional subscript operator* (o `m[i, j, k]` do Demo 4)
- **P1169R4** — *`static operator()`* (C++23)
- *C++ Core Guidelines*, seção **C.160–C.168** — as regras canônicas de sobrecarga, em
  especial C.161 (operadores simétricos como função livre) e C.168 (defina o operador no
  namespace dos operandos)
