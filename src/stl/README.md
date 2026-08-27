# stl — Containers, Iteradores e Algoritmos

## O que é este projeto?

Guia sobre a **biblioteca padrão** vista como um sistema: containers de um lado, algoritmos
do outro, e iteradores como a cola que evita `N × M` implementações.

Termina construindo um container próprio (`RingBuffer`) que satisfaz o contrato — e ganha
todo o `<algorithm>`, o range-for e o `<ranges>` de graça.

```bash
make build && ./dist/bin/stl
```

O `make build` configura o Meson, ativa o ambiente do Conan (é ele quem fornece o **fmt**) e
**instala** o binário em `dist/bin/`. Rodar `meson` na mão sem `source build/conanbuild.sh`
falha por não achar o fmt.

---

## Antes de começar

Esta seção define **todo** o vocabulário usado adiante. Se você já sabe, pule para
"Por que estudar isto?".

| Termo | O que significa, em uma frase |
|---|---|
| **container** | um objeto que guarda outros objetos e cuida da memória deles (`vector`, `list`, `map`) |
| **iterador** | um objeto que se comporta como ponteiro: `*it` lê o elemento, `++it` anda para o próximo, `it != fim` diz se acabou |
| **algoritmo** | uma função livre (`std::sort`, `std::find`) que só enxerga **iteradores** — nunca o container |
| **O(n), O(log n), O(1)** | como o custo cresce com o número de elementos `n`: proporcional, logarítmico, constante |
| **amortizado** | o custo **em média por operação** ao longo de muitas: `push_back` é O(1) amortizado porque a realocação cara acontece raramente o bastante para `n` inserções custarem O(n) no total |
| **invalidado** | o iterador (ou ponteiro/referência) deixou de apontar para o elemento — a memória mudou de lugar ou sumiu. Usá-lo depois disso é **UB** |
| **UB** (*undefined behavior*) | comportamento indefinido: o padrão não promete nada. Não é "erro" nem "crash garantido" — é o programa podendo fazer qualquer coisa, inclusive funcionar hoje e quebrar amanhã |
| **predicado** | uma função (ou lambda) que devolve `bool`: `[](int x) { return x > 4; }` |
| **lambda** | função anônima escrita no lugar do uso: `[captura](parâmetros) { corpo }` |
| **proxy** | um objeto que **finge ser uma referência**: guarda por dentro "onde está o dado" e converte para o valor na leitura, escrevendo no lugar certo na atribuição |
| **concept** (C++20) | um predicado avaliado em **compilação** sobre um tipo: `std::random_access_iterator<T>` é `true` se `T` tem os operadores exigidos |
| **ADL** (*argument-dependent lookup*) | busca de nome que também procura no namespace dos **argumentos** — é o que faz `swap(a, b)` sem qualificação achar o `swap` específico do tipo |
| **sentinela** | um "fim" que **não é** do mesmo tipo do iterador; só sabe responder "acabou?" |

### O menor programa completo desta pasta

Cole num arquivo, compile e rode. Tudo o mais neste README é elaboração disto:

```cpp
#include <algorithm>
#include <vector>
#include <fmt/ranges.h>

int main() {
    std::vector<int> v{5, 3, 9, 1, 7};          // o CONTAINER

    std::sort(v.begin(), v.end());              // o ALGORITMO, via ITERADORES

    auto it = std::find_if(v.begin(), v.end(),  // o primeiro maior que 4
                           [](int x) { return x > 4; });

    fmt::print("ordenado     : {}\n", v);
    fmt::print("primeiro > 4 : {} (índice {})\n", *it, it - v.begin());
}
```

Saída:

```
ordenado     : [1, 3, 5, 7, 9]
primeiro > 4 : 5 (índice 2)
```

Repare: `std::sort` **não conhece** `std::vector`. Ele recebe dois iteradores e só. É essa
separação que o resto do README destrincha.

---

## Por que estudar isto?

Porque escolher o container errado não gera erro nem resultado errado: gera lentidão
silenciosa. E porque metade das armadilhas da STL compila sem um único warning.

Medido pelo próprio demo (`./dist/bin/stl`, seção 1.7, nesta máquina):

```
20000 inserções ordenadas:
  vector (memmove contíguo) :     8 ms
  list   (salto de ponteiro):  1192 ms      ← ~150×
```

A `list` tem a **melhor** complexidade e o **pior** desempenho. Os números variam entre
execuções (o `vector` fica em 7–9 ms, a `list` em 1100–1250 ms), mas a ordem de grandeza é
estável.

### Por que a `list` perde

A tabela de complexidade conta a história de um modelo de máquina que não existe mais — um
em que ler qualquer endereço custa o mesmo. Na máquina real:

- a memória chega ao processador em **linhas de cache de 64 bytes**. Ler um `int` do
  `vector` traz **15 vizinhos de graça**; os próximos 15 acessos não tocam a RAM;
- o **prefetcher** observa o padrão de acesso. Percurso linear ele acerta e busca o próximo
  bloco antes de você pedir; salto de ponteiro imprevisível ele erra sempre;
- cada nó da `list` carrega **dois ponteiros de overhead** (16 bytes) por elemento e mora
  onde o alocador quiser — então percorrê-la é aproximadamente **um cache miss por nó**.

O `vector` faz "mais trabalho" (um `memmove` a cada inserção no meio) e mesmo assim ganha,
porque o trabalho dele é sequencial e a `list` passa o tempo esperando a RAM. Um cache miss
custa uns 100 ciclos; nesse tempo o `memmove` move centenas de bytes.

### Por que iteradores existem

Sem iteradores, cada algoritmo precisaria de uma versão por container. Com **20 algoritmos**
e **10 containers** isso é **200 funções** para escrever, testar e manter. Com iteradores no
meio, cada algoritmo é escrito **uma vez** contra o contrato e cada container o cumpre
**uma vez**: **20 + 10 = 30**.

É o único motivo de `std::sort` funcionar num `vector`, num array cru (`int*` já é um
iterador random access!) e no `RingBuffer` do Demo 4 — que a biblioteca padrão nunca viu.

---

## Estrutura de arquivos

```
src/stl/
├── main.cpp                    ← ponto de entrada, executa todos os demos
├── demo_containers.hpp         ← 1 · complexidade, capacity, invalidação, emplace
├── demo_iterators.hpp          ← 2 · categorias, traits, adaptadores, sentinelas
├── demo_algorithms.hpp         ← 3 · busca, ordenação, partição, transformação, numéricos
├── demo_custom_container.hpp   ← 4 · RingBuffer compatível com STL e <ranges>
├── demo_pitfalls.hpp           ← 5 · o que compila, roda, e está errado
├── meson.build
└── README.md
```

---

## Demo 1: Containers

| Container | Acesso | Inserção | Busca | Memória |
|---|---|---|---|---|
| `vector` | O(1) | O(1) no fim* | O(n) | contígua |
| `deque` | O(1) | O(1) nas 2 pontas | O(n) | blocos |
| `list` | O(n) | O(1) com iterador | O(n) | nós |
| `map` / `set` | — | O(log n) | O(log n) | nós |
| `unordered_map` / `set` | — | O(1) médio** | O(1) médio** | baldes |

\* **amortizado** — o buffer **dobra** a cada estouro, então o custo caro é raro:

```
inserindo 20 elementos: 0→1 1→2 2→4 4→8 8→16 16→32
com reserve(20):        0 realocações
```

\*\* **médio, não garantido.** No pior caso — todas as chaves caindo no mesmo balde — a busca
degrada para **O(n)**, porque o balde vira uma lista. Com chaves escolhidas por um atacante
que conhece a função de hash, isso é um vetor de **negação de serviço** (*hash flooding*).
Quando a entrada é hostil, ou você usa hash com semente aleatória, ou usa `map` (O(log n)
garantido).

### `size` vs `capacity`

- `clear()` zera o `size` mas **não devolve** a memória — a `capacity` continua a mesma;
- `shrink_to_fit()` é uma **requisição**, não uma ordem: a implementação pode ignorar;
- por isso o idioma antigo ainda é ensinado — quando você precisa da **garantia** de devolver
  a memória, é o que resta:

```cpp
std::vector<int>{}.swap(v);   // troca v por um vector vazio; o antigo morre no fim da linha

v.clear(); v.shrink_to_fit(); // a forma preferida desde C++11 — mas o shrink é só um pedido
```

O demo mostra o libstdc++ **atendendo** ao `shrink_to_fit` (capacity 32 → 20). Escreva a
segunda forma; conheça a primeira porque ela é a única com garantia no papel.

### Invalidação de iteradores

**Invalidado** = o iterador deixou de apontar para o elemento (a memória foi realocada ou
liberada). Ele continua parecendo válido e o compilador não reclama; usá-lo é **UB**.

| Operação | Invalida |
|---|---|
| `vector`: realocação | **tudo** |
| `vector`: `erase(it)` | de `it` até o fim |
| `deque`: insert/erase no meio | tudo |
| `deque`: `push_front/back` | iteradores sim, referências não |
| `list`: qualquer | só o elemento removido |
| `map`/`set`: `insert` | nada |
| `map`/`set`: `erase` | só o elemento removido |
| `unordered_*`: rehash | iteradores sim, referências não |

Os dois casos "iteradores sim, referências não" surpreendem, e o mecanismo é o mesmo: o
elemento **não se move** (o `deque` guarda blocos; o `unordered_map` guarda nós), mas a
estrutura que o iterador usa para andar — o mapa de blocos, o vetor de baldes — foi refeita.

### Apagar durante a iteração

```cpp
// ✗ `it` é invalidado por erase, e o ++it opera em lixo
for (auto it = v.begin(); it != v.end(); ++it)
    if (pred(*it)) v.erase(it);

// ✓ o idioma canônico: erase DEVOLVE o iterador do próximo
for (auto it = v.begin(); it != v.end(); ) {
    if (pred(*it)) it = v.erase(it);   // não avance: erase já avançou
    else           ++it;               // avance só quando não apagou
}

// ✓ melhor ainda (C++20)
std::erase_if(v, pred);
```

O `++it` fica **fora** do `for` justamente porque avançar é condicional: quem apagou já está
na posição seguinte.

### `emplace` vs `push` — o que realmente muda

```
push_back(Caro(...))       ctor=1 cópias=0 moves=1
emplace_back("a","b")      ctor=1 cópias=0 moves=0    ← constrói no lugar
push_back(lvalue)          ctor=0 cópias=1 moves=0
push_back(std::move(c))    ctor=0 cópias=0 moves=1    ← idêntico a emplace
```

`emplace` ganha quando você tem os **argumentos**. Com um objeto pronto,
`push_back(std::move(x))` é a mesma coisa.

> ⚠ `emplace_back` não é sempre melhor: ele aceita conversões **explícitas**
> silenciosamente, porque chama o construtor direto.

### As quatro formas de inserir num map

| Forma | Comportamento |
|---|---|
| `m[k] = v` | insere ou sobrescreve (exige default ctor; não existe em map const) |
| `insert({k,v})` | **não** sobrescreve |
| `insert_or_assign(k,v)` | sobrescreve |
| `try_emplace(k, args...)` | só constrói o valor se a chave faltar |

---

## Demo 2: Iteradores

### O que é um iterador

Um iterador é um objeto que **generaliza o ponteiro**. Para servir ao algoritmo mais simples
ele precisa saber fazer exatamente três coisas:

```cpp
*it        // ler (e, às vezes, escrever) o elemento atual
++it       // andar para o próximo
it != fim  // saber se ainda não acabou
```

Nada mais. Um `int*` apontando para um array cru **já é** um iterador válido — por isso isto
compila e ordena:

```cpp
int arr[5] = {5, 3, 9, 1, 7};
std::sort(arr, arr + 5);        // dois ponteiros: começo e "um passado do fim"
```

As **categorias** abaixo respondem "o que este iterador sabe fazer **além** desse mínimo?".
Cada degrau inclui o anterior:

```
input        → ler, avançar 1×, passagem única      (istream_iterator)
forward      → ler/escrever, múltiplas passagens    (forward_list)
bidirectional→ + retroceder (--it)                  (list, map, set)
random access→ + saltar em O(1), it + n             (vector, deque, array)
contiguous   → + memória contígua (C++20)           (vector, array, span)
```

A categoria decide **o que compila**:

```cpp
*(v.begin() + 3)        // ✓ vector é random access
*(l.begin() + 3)        // ✗ list não tem operator+
std::advance(it, 3);    // ✓ genérico: O(1) ou O(n) conforme a categoria
```

### Como um algoritmo se especializa

```cpp
if constexpr (std::is_base_of_v<std::random_access_iterator_tag, cat>)
    return ultimo - primeiro;              // O(1)
else
    while (primeiro != ultimo) { ++primeiro; ++n; }   // O(n)
```

Antes do C++17 isso era *tag dispatch*: escreviam-se **duas funções** distintas, cada uma
recebendo um parâmetro extra da tag, e o compilador escolhia por sobrecarga —

```cpp
long dist(It a, It b, std::random_access_iterator_tag) { return b - a; }
long dist(It a, It b, std::input_iterator_tag)         { /* contar */ }
long dist(It a, It b) { return dist(a, b, typename std::iterator_traits<It>::iterator_category{}); }
```

— hoje `if constexpr` num corpo só basta.

### Um iterador do zero

Basta os 5 typedefs (`iterator_category`, `value_type`, `difference_type`, `pointer`,
`reference`) e os operadores da categoria. O `Intervalo` do demo gera os números sob
demanda — nada é armazenado — e ainda assim `accumulate`, `count_if` e `copy` funcionam
nele.

### Iteradores adaptadores

Um adaptador é um iterador que **não aponta para um elemento**: ele traduz `*it = valor` em
outra ação. Isso responde à dúvida mais clássica da STL:

```cpp
std::vector<int> destino;                                  // vazio! size == 0
std::copy(origem.begin(), origem.end(), destino.begin());  // ✗ UB: escreve onde não há espaço
```

`std::copy` só sabe fazer `*saida = valor; ++saida;` — ele **não pode** chamar `push_back`,
porque não conhece o container. O adaptador resolve exatamente isso:

```cpp
std::copy(origem.begin(), origem.end(), std::back_inserter(destino));  // ✓
```

`back_inserter(destino)` devolve um objeto cujo `operator=` faz `destino.push_back(valor)` e
cujo `++` não faz nada. O algoritmo não percebe a diferença.

| Adaptador | `*it = v` (ou `*it`) vira |
|---|---|
| `back_inserter(c)` | `c.push_back(v)` |
| `front_inserter(c)` | `c.push_front(v)` (exige `deque`/`list`) |
| `inserter(c, pos)` | `c.insert(pos, v)` |
| `ostream_iterator<T>(os, sep)` | `os << v << sep` |
| `istream_iterator<T>(is)` | `is >> v` na leitura; o construtor **sem argumento** é o fim do stream |
| `reverse_iterator` | anda ao contrário (`rbegin()`/`rend()`) |

Saída medida do demo (seção 2.4):

```
rbegin/rend:        5 4 3 2 1
back_inserter:      destino tem 5 elementos
ostream_iterator:   1,2,3,4,5,
istream_iterator:   4 valores, soma = 100
```

Ler um stream inteiro para um vetor vira, então, uma linha:

```cpp
std::istringstream iss("10 20 30 40");
std::vector<int> lidos{std::istream_iterator<int>(iss), std::istream_iterator<int>()};
```

> ⚠ `reverse_iterator` tem um **deslocamento de um**: ele aponta para o elemento
> *anterior* ao que o iterador interno guarda (é o que faz `rbegin()` ser construído a partir
> de `end()`). Por isso `rit.base()` devolve o iterador normal **um à frente** de `*rit` —
> na hora de apagar, `v.erase(std::next(rit).base())`.

### Sentinelas (C++20)

Até o C++17, `begin()` e `end()` tinham que ser do **mesmo tipo**. Em C++20 o fim pode ser
um sentinela que só sabe responder "acabou?":

```cpp
struct SentinelaDeNulo {};                  // sizeof == 1, sem estado

bool operator==(SentinelaDeNulo) const { return *p_ == '\0'; }

for (IteradorDeCString it(texto); it != SentinelaDeNulo{}; ++it) ...
```

Percorre uma C-string **sem chamar `strlen` antes**. É o que permite `views::take_while`
ser preguiçoso de verdade.

---

## Demo 3: `<algorithm>`

### Busca

| Algoritmo | Custo | Exige ordenação? |
|---|---|---|
| `find` / `find_if` | O(n) | não |
| `binary_search` | O(log n) | **sim** |
| `lower_bound` | O(log n) | **sim** — primeiro `>=` |
| `upper_bound` | O(log n) | **sim** — primeiro `>` |
| `equal_range` | O(log n) | **sim** — o intervalo dos iguais |

> ⚠ Busca binária em sequência não ordenada é **UB**: sem erro, sem warning, só resposta
> errada.

O uso mais útil do `lower_bound` é descobrir **onde inserir** mantendo a ordem.

### Ordenação

| Algoritmo | Custo | Garante |
|---|---|---|
| `sort` | O(n log n) | tudo ordenado |
| `stable_sort` | O(n log n) + memória | + ordem relativa dos iguais |
| `partial_sort` | O(n log k) | os k primeiros |
| `nth_element` | O(n) médio | só a posição n |

Peça a **menor** garantia que resolve — é a mais barata.

> A instabilidade do `sort` **não aparece** com 6 elementos. O `std::sort` do libstdc++ é um
> **introsort**: quicksort no caso geral, que troca para **heapsort** se a recursão fica
> profunda demais (é o "intro", de *introspective* — o que garante o O(n log n) de pior
> caso) e para **insertion sort** em intervalos pequenos (< 16 elementos), porque para poucos
> elementos ele é mais rápido. E o insertion sort é estável **por acidente** — daí um vetor
> pequeno sair "estável" sem nenhuma garantia disso. O demo mede com 200 itens e 4 chaves
> distintas, e aí a diferença salta:
>
> ```
> sort == stable_sort ? NÃO — sort embaralhou os empates
> primeiros 6 do sort       : #116 #032 #152 #148 #036 #144
> primeiros 6 do stable_sort: #000 #004 #008 #012 #016 #020
> ```

### Strict weak ordering

O comparador responde "a vem **antes** de b?" e precisa ser irreflexivo, assimétrico e
transitivo.

```cpp
return a.idade <  b.idade;   // ✓
return a.idade <= b.idade;   // ✗ comp(a,a) == true → std::sort pode ler fora do intervalo
```

Não é exagero — é UB de verdade. Detectável com `-D_GLIBCXX_DEBUG`.

O **"weak"** do nome é a parte que se costuma ignorar, e é a mais útil: a ordem **não** exige
que dois elementos distintos sejam comparáveis. Se `comp(a,b)` e `comp(b,a)` são ambos
falsos, `a` e `b` são **equivalentes** para essa ordem — não necessariamente iguais. É
exatamente essa equivalência que `map` e `set` usam como identidade de chave: eles nunca
chamam `operator==`, só o comparador, duas vezes. Ordenar pessoas por idade torna duas
pessoas de 28 anos equivalentes — e num `set` com esse comparador a segunda **não entra**.

Para múltiplos critérios, `std::tie` dá o desempate lexicográfico de graça:

```cpp
return std::tie(a.idade, a.nome) < std::tie(b.idade, b.nome);
```

### Partição

Separar em dois grupos é **O(n)**; ordenar é O(n log n). Se você só precisa separar, não
ordene.

```cpp
auto fronteira = std::partition(v.begin(), v.end(),
                                [](const Pessoa& p) { return p.idade < 40; });
// [v.begin(), fronteira)  → os que satisfazem
// [fronteira,  v.end())   → os demais
```

| Algoritmo | Faz | Custo |
|---|---|---|
| `partition` | separa; **não** preserva a ordem relativa | O(n) |
| `stable_partition` | separa **preservando** a ordem | O(n) com memória extra, O(n log n) sem |
| `partition_point` | acha a fronteira de um range **já particionado** | O(log n) |
| `is_partitioned` | verifica | O(n) |

`partition_point` é busca binária sobre um **predicado**: se o range é monotônico (todos os
`true` antes de todos os `false`), ele acha a fronteira em O(log n) — a generalização do
`lower_bound` para além de "menor que um valor".

Saída medida (seção 3.4):

```
< 40 anos:            Ada Linus Dennis
>= 40 anos:           Grace Barbara Alan
```

### Transformação

`std::transform` é o "map" da programação funcional: aplica uma função a cada elemento e
**escreve o resultado em outro lugar** (que pode ser o mesmo container).

```cpp
std::vector<std::string> nomes;
nomes.reserve(v.size());                                  // evita as realocações
std::transform(v.begin(), v.end(), std::back_inserter(nomes),
               [](const Pessoa& p) { return p.nome; });   // 1 entrada → 1 saída

std::transform(base.begin(), base.end(), bonus.begin(),   // versão BINÁRIA:
               std::back_inserter(total), std::plus<>{}); // 2 entradas → 1 saída
```

| Algoritmo | Faz |
|---|---|
| `transform` (unário) | `saida[i] = f(entrada[i])` |
| `transform` (binário) | `saida[i] = f(a[i], b[i])` — o 2º range só precisa do `begin` |
| `generate` | preenche chamando uma fábrica sem argumentos |
| `iota` (`<numeric>`) | preenche com `v, v+1, v+2, …` — a sequência mais comum, já pronta |

Saída medida (seção 3.5):

```
transform (extrair):  Ada, Linus, Grace, Dennis, Barbara, Alan
transform (in-place): ADA, LINUS, GRACE, DENNIS, BARBARA, ALAN
transform (binário):  110 220 330
generate:             3 6 9 12 15 18 21 24
iota(100):            100 101 102 103 104 105 106 107
```

> ⚠ Na versão binária, o segundo range precisa ter **pelo menos** tantos elementos quanto o
> primeiro. O algoritmo não checa; sobrar curto é UB.

### Predicados e conjuntos

```cpp
std::all_of (v.begin(), v.end(), par);   // todos satisfazem?      (true num range vazio)
std::any_of (v.begin(), v.end(), par);   // pelo menos um?         (false num range vazio)
std::none_of(v.begin(), v.end(), impar); // nenhum?                (true num range vazio)
std::is_sorted(v.begin(), v.end());
std::equal(a.begin(), a.end(), b.begin(), b.end());   // prefira a versão de 4 iteradores
```

As operações de **conjunto** trabalham sobre sequências (não sobre `std::set`) e produzem a
saída por um iterador de escrita:

```cpp
std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(r));
```

| Operação | Resultado com `a = 1 2 3 4 5`, `b = 4 5 6 7` |
|---|---|
| `set_intersection` | `4 5` |
| `set_difference` | `1 2 3` |
| `set_union` | `1 2 3 4 5 6 7` |
| `includes` | `a` contém todo `b`? → `false` |

> ⚠ **Todas** exigem entrada **ordenada** — exatamente como `binary_search`. Em sequência
> não ordenada não há erro nem warning: a saída sai errada, silenciosamente.

### Numéricos

```cpp
std::accumulate(v.begin(), v.end(), 0)     // ⚠ o TIPO do inicial decide a acumulação
std::accumulate(reais.begin(), reais.end(), 0)     // → truncou para int!
std::accumulate(reais.begin(), reais.end(), 0.0)   // → correto
```

| Algoritmo | Faz |
|---|---|
| `accumulate` | **dobra à esquerda**: aplica a operação da esquerda para a direita, um elemento por vez, carregando o acumulador — `f(f(f(init,a),b),c)`. Sequencial e determinístico |
| `reduce` (C++17) | idem, **sem** garantia de ordem nem de agrupamento — permite paralelizar (e por isso exige operação associativa e comutativa; com `double` o resultado pode variar) |
| `inner_product` | soma dos produtos par a par |
| `partial_sum` | soma acumulada (prefix sum) |
| `adjacent_difference` | o inverso do `partial_sum` |
| `minmax_element` | mínimo e máximo numa passada só |

---

## Demo 4: Um Container Compatível

`RingBuffer<T, N>` — fila circular de capacidade fixa, **sem alocação** (os dados vivem num
`std::array`).

O contrato pago:

```cpp
using value_type = T;  using size_type = std::size_t;  /* ... */
iterator begin();  iterator end();
const_iterator begin() const;  const_iterator end() const;
```

### O truque da constância, mostrado

Escrever `iterator` e `const_iterator` separados duplica ~60 linhas. O padrão é um **único**
template parametrizado pela constância, com os tipos escolhidos por `std::conditional_t`:

```cpp
template <bool EhConst>
class iterador_base {
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = std::conditional_t<EhConst, const T*, T*>;
    using reference         = std::conditional_t<EhConst, const T&, T&>;
    using buffer_ptr        = std::conditional_t<EhConst, const RingBuffer*, RingBuffer*>;

    iterador_base() = default;                       // random access exige default-construtível
    iterador_base(buffer_ptr b, size_type i) : buf_(b), idx_(i) {}

    // conversão implícita iterator → const_iterator (e NÃO o contrário)
    operator iterador_base<true>() const { return {buf_, idx_}; }

    reference operator*() const { return buf_->dados_[buf_->fisico(idx_)]; }

    iterador_base& operator++() { ++idx_; return *this; }
    iterador_base& operator--() { --idx_; return *this; }
    iterador_base& operator+=(difference_type n) { idx_ += static_cast<size_type>(n); return *this; }

    friend iterador_base  operator+(iterador_base it, difference_type n) { return it += n; }
    friend difference_type operator-(const iterador_base& a, const iterador_base& b) {
        return static_cast<difference_type>(a.idx_) - static_cast<difference_type>(b.idx_);
    }

    friend bool operator==(const iterador_base& a, const iterador_base& b) { return a.idx_ == b.idx_; }
    friend auto operator<=>(const iterador_base& a, const iterador_base& b) { return a.idx_ <=> b.idx_; }

private:
    buffer_ptr buf_ = nullptr;
    size_type  idx_ = 0;      // índice LÓGICO; `fisico()` faz o módulo da volta
};

using iterator       = iterador_base<false>;
using const_iterator = iterador_base<true>;
```

Três detalhes que fazem o conjunto funcionar:

1. o iterador guarda **índice lógico**, não ponteiro — quem faz a volta do círculo é o
   `fisico(i)` do container, então `it + 3` continua sendo O(1) e a aritmética não quebra na
   dobra do buffer;
2. `operator==` mais `operator<=>` geram os outros **quatro** operadores de comparação
   sozinhos (C++20);
3. a conversão só existe no sentido `iterator → const_iterator`, que é o que torna
   `it == cit` bem-formado sem abrir buraco na constância.

O que se ganha sem escrever mais nada:

```
range-for, accumulate, max_element, count_if, find,
sort, make_reverse_iterator, ranges::sort, ranges::max
```

E o contrato antigo (typedefs + operadores) já satisfaz os **conceitos** novos — os
predicados de compilação do C++20:

```cpp
static_assert(std::random_access_iterator<RB::iterator>);
static_assert(std::ranges::random_access_range<RB>);
```

`std::random_access_iterator<It>` é `true` quando `It` tem todos os operadores da categoria
(`*`, `++`, `--`, `+= n`, `- it`, `<=>`, `[]`), é default-construtível e regular;
`std::ranges::random_access_range<R>` é `true` quando `begin(R)` devolve um desses. Nada foi
escrito para satisfazê-los: cumprir o contrato antigo já bastou. É por isso que o
`RingBuffer` entra num pipeline de [`src/ranges/`](../ranges/).

O que um container de biblioteca ainda teria: `allocator_type`, `insert`/`erase` com
iterador, um `friend swap` achável por **ADL** (a busca que encontra o `swap` do namespace do
próprio tipo, e não o `std::swap` genérico), `operator<=>`, *deduction guides* (as regras que
deixam escrever `RingBuffer rb{1,2,3}` sem repetir `<int,3>`), garantias de exceção
documentadas.

> Só escreva um container quando nenhuma combinação da STL resolver. Na dúvida, componha.

---

## Demo 5: Armadilhas

### `std::remove` não remove

Um algoritmo só enxerga **iteradores** — ele não tem acesso ao container e não pode mudar o
tamanho.

```
original:            1 2 3 2 4 2 5  (size=7)
após std::remove(2): 1 3 4 5 4 2 5  (size=7)   ← size NÃO mudou
elementos válidos:   1 3 4 5
```

```cpp
v.erase(std::remove(v.begin(), v.end(), 2), v.end());   // idioma erase-remove
std::erase(v, 2);                                        // C++20
```

Vale para `unique`, `remove_if` e todo algoritmo que "remove".

### `vector<bool>` não é um container de bool

É uma especialização que empacota 8 valores por byte. `operator[]` devolve um **proxy** — um
objeto que finge ser `bool&`: por dentro ele guarda o ponteiro para a palavra e a máscara do
bit, converte para `bool` quando lido e escreve o bit quando recebe uma atribuição.

```cpp
auto x = vb[0];      // tipo é std::_Bit_reference, não bool
bool& r = vb[0];     // ✗ não compila — não existe referência para um bit
x = false;           // ← muda o vector! o "valor" era uma referência disfarçada
```

Alternativas: `vector<char>`, `deque<bool>`, `bitset<N>`. Se precisar mesmo dele, escreva
`bool x = vb[i];` — nunca `auto`.

### `map::operator[]` insere

```cpp
if (estoque["banana"] > 0) ...   // ← inseriu {"banana", 0}
```

Por isso `operator[]` **não existe** em map const. Para consultar: `.find()`, `.at()`,
`.count()`, `.contains()` (C++20).

### Outras

| Armadilha | Correção |
|---|---|
| Referência sobrevivendo à realocação | guarde o **índice** |
| `string_view` de um temporário | um view é **parâmetro**, não membro nem retorno |
| `string_view` não é null-terminated | não passe `.data()` para API C |
| Comparar iteradores de containers diferentes | UB — pego por `-D_GLIBCXX_DEBUG` |

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Na dúvida sobre o container | `vector` |
| Trocar de container | só quando puder **nomear** a operação que incomoda |
| Sabe o tamanho final | `reserve()` |
| Precisa de ordem | `map`/`set`; senão `unordered_*` |
| Poucos itens (< ~30) | `vector` de pares vence o map |
| Removendo de um container | erase-remove ou `std::erase_if` |
| Escrevendo um loop | procure primeiro o algoritmo com esse nome |
| Comparador | `<`, nunca `<=` |
| Guardando posição num vector | índice, não iterador/referência |
| Antes de confiar no código | `-fsanitize=address,undefined` **e** `-D_GLIBCXX_DEBUG` |

---

## Como validar

O caminho normal — configura, compila e instala em `dist/bin/`:

```bash
make build
./dist/bin/stl
```

Compilar só este alvo, sem passar pelo `make`:

```bash
source build/conanbuild.sh      # sem isto o meson não acha o fmt
meson compile -C build stl
./dist/bin/stl
```

Compilar o arquivo isolado, com todos os avisos ligados (o `-I`/`-L` do fmt vêm do pacote do
Conan, e é preciso rodar da **raiz do repositório**):

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/stl/main.cpp -o /tmp/stl -L$FMTP/lib -lfmtd && /tmp/stl
```

As duas ferramentas de diagnóstico que este subprojeto merece — e **o que cada uma pega**:

```bash
# ASan + UBSan: a referência que sobreviveu à realocação (Demo 1.3 e Demo 5) e o
# string_view pendurado. Ambos são heap-use-after-free, invisíveis sem instrumentação.
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/stl/main.cpp -o /tmp/stlsan -L$FMTP/lib -lfmtd && /tmp/stlsan

# _GLIBCXX_DEBUG: troca os iteradores do libstdc++ por versões checadas. Pega o
# comparador com `<=` (Demo 3.3), a comparação de iteradores de containers
# diferentes (Demo 5) e o uso de iterador invalidado.
g++ -std=c++20 -D_GLIBCXX_DEBUG -I$FMTP/include \
    src/stl/main.cpp -o /tmp/stldbg -L$FMTP/lib -lfmtd && /tmp/stldbg
```

Estado atual, verificado: compila **sem warnings** com `-Wall -Wextra -Wpedantic`, roda com
**0 achados** sob ASan+UBSan e **0 violações** sob `-D_GLIBCXX_DEBUG`. Isso é esperado: os
demos *descrevem* a UB sem executá-la. Para ver o ASan apitar, acrescente uma leitura do
ponteiro invalidado em `demo_containers.hpp`, logo depois do `w.push_back(4)` que realoca
(seção 1.3):

```cpp
w.push_back(4);                 // estourou -> REALOCOU
fmt::print("{}\n", *q);         // ← acrescente esta linha e recompile com ASan
```

O relatório sai assim:

```
==ERROR: AddressSanitizer: heap-use-after-free on address 0x502000000010
READ of size 4 at 0x502000000010 thread T0
```

Saída esperada ao final: `✓ Todos os demos da STL concluídos!`

## Referências

- [`src/ranges/`](../ranges/) — o sucessor de C++20: os mesmos algoritmos com range inteiro,
  projeções e pipelines preguiçosos
- [`src/design-of-pattern/iterator/`](../design-of-pattern/iterator/) — o padrão de projeto Iterator, a mesma ideia vista pelo lado do desenho
- [`src/performance/`](../performance/) — cache, localidade e AoS vs SoA: o mecanismo por trás
  do `vector` × `list` desta página
- [`src/templates/`](../templates/) — `if constexpr`, traits e o despacho por categoria
- [`src/concepts/`](../concepts/) — o que `std::random_access_iterator` é por dentro
- [`src/strings/`](../strings/) — `string_view` e as armadilhas de tempo de vida do Demo 5
- [`src/undefined_behavior/`](../undefined_behavior/) — os sanitizers em detalhe
- [cppreference: `<algorithm>`](https://en.cppreference.com/w/cpp/algorithm),
  [`<iterator>`](https://en.cppreference.com/w/cpp/iterator),
  [`<numeric>`](https://en.cppreference.com/w/cpp/numeric) — a referência que vale ter aberta;
  cada página traz a complexidade e as pré-condições
- *The C++ Standard Library*, 2ª ed., Nicolai Josuttis — o livro de referência da STL
- Sean Parent, *C++ Seasoning* (GoingNative 2013) — a palestra que popularizou "sem loops
  crus": a origem direta da regra "procure primeiro o algoritmo com esse nome"
