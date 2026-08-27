# ranges — Views, Pipelines e Projeções (C++20)

## O que é este projeto?

Guia sobre `<ranges>`, a maior adição do C++20 à biblioteca padrão. Cobre as **views**
preguiçosas, a composição por `|`, os algoritmos `std::ranges::*` com **projeções**, e
como escrever os seus próprios ranges, views e adaptadores.

```bash
make build && ./dist/bin/ranges
```

O `make build` configura o Meson, ativa o ambiente do Conan (é ele quem fornece o **fmt**) e
**instala** o binário em `dist/bin/`. Rodar `meson` na mão sem `source build/conanbuild.sh`
falha por não achar o fmt.

---

## Antes de começar

### Os apelidos usados neste README

Os fontes deste diretório abrem cada `demo_*.hpp` com estas duas linhas, e **todo** bloco de
código deste README as pressupõe:

```cpp
namespace vw = std::views;    // vw::filter, vw::transform, vw::take, ...
namespace rg = std::ranges;   // rg::sort, rg::find, rg::distance, ...
```

São apelidos **do autor**, não da linguagem: sem eles, colar qualquer exemplo daqui dá
`'vw' has not been declared`. Escreva-os no topo do seu arquivo (ou use os nomes completos).

### O vocabulário, definido

| Termo | O que significa, em uma frase |
|---|---|
| **range** | qualquer coisa da qual se possa pedir `begin()` e `end()` — todo container da STL já é um; um array cru também |
| **view** | um range que **não possui** os dados e **não trabalha** até ser iterado; copiar uma custa O(1) |
| **adaptador** | o objeto que fabrica a view: `vw::filter(p)` é um adaptador; `v \| vw::filter(p)` é a view |
| **preguiçoso** (*lazy*) | o trabalho acontece na iteração, não na construção |
| **lvalue** | uma expressão que tem nome e endereço, e continua viva depois: uma variável como `v` |
| **prvalue** | um valor temporário sem nome, que morre no fim da expressão: o retorno de `produz()` |
| **dangling** (pendurado) | um iterador ou referência que aponta para memória **já destruída** |
| **UB** (*undefined behavior*) | comportamento indefinido: o padrão não promete nada. O programa pode funcionar hoje e quebrar amanhã |
| **projeção** | uma função aplicada a cada elemento **antes** de comparar ou testar — muda o critério, **não** o resultado (`rg::sort(v, {}, &Pessoa::idade)` ordena as pessoas, não as idades) |
| **materializar** | copiar os elementos de uma view para um container de verdade (um `std::vector`), saindo do mundo preguiçoso |
| **common_range** | um range cujo `begin()` e `end()` têm o **mesmo tipo** — condição que os construtores clássicos de container exigem |
| **CRTP** | *Curiously Recurring Template Pattern*: a classe base é template do **derivado** (`class D : Base<D>`), o que deixa a base chamar os métodos do derivado sem `virtual`. É como `view_interface` funciona — ver [`../templates/demo_crtp.hpp`](../templates/demo_crtp.hpp) |

### O menor programa completo desta pasta

Cole num arquivo, compile e rode. Tudo o mais neste README é elaboração disto:

```cpp
#include <ranges>
#include <vector>
#include <fmt/ranges.h>

namespace vw = std::views;

int main() {
    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto r = v | vw::filter([](int x) { return x % 2 == 0; })   // só os pares
               | vw::transform([](int x) { return x * 10; })    // ×10
               | vw::take(3);                                   // os 3 primeiros

    fmt::print("resultado: {}\n", fmt::join(r, " "));
}
```

Saída:

```
resultado: 20 40 60
```

Repare no que **não** aconteceu: nenhum vetor intermediário foi criado, e os elementos 7, 8,
9 e 10 nunca foram testados — o `take(3)` parou o pipeline assim que juntou três.

---

## Por que estudar isto?

Um encadeamento de algoritmos clássicos custa **N passadas e N−1 containers temporários**:

```cpp
std::vector<int> passo1;  std::copy_if(v.begin(), v.end(), back_inserter(passo1), pred);
std::vector<int> passo2;  std::transform(passo1.begin(), passo1.end(), back_inserter(passo2), f);
std::vector<int> passo3(passo2.begin(), passo2.begin() + 4);
// → 2 temporários, 3 passadas, e processou os 20 elementos antes de descartar 16
```

O mesmo com views:

```cpp
auto r = v | vw::filter(pred) | vw::transform(f) | vw::take(4);
// → 0 temporários, 1 passada, e PARA assim que junta 4
```

E, de quebra, o pipeline **se lê na ordem em que as coisas acontecem** — da esquerda para a
direita — enquanto a versão clássica se lê de dentro para fora.

---

## Estrutura de arquivos

```
src/ranges/
├── main.cpp                 ← ponto de entrada, executa todos os demos
├── demo_views.hpp           ← 1 · preguiça, catálogo, views infinitas, materializar
├── demo_pipelines.hpp       ← 2 · operator|, tempo de vida, owning_view, dangling
├── demo_algorithms.hpp      ← 3 · ranges::*, projeções, proteção contra dangling
├── demo_custom_range.hpp    ← 4 · range próprio, view_interface, adaptador com |
├── meson.build
└── README.md
```

---

## Demo 1: Views

### O que é um range

Um **range** é qualquer coisa da qual se possa pedir `begin()` e `end()`. Todo container da
STL já é um; um array cru também; e o `RingBuffer` de [`../stl/`](../stl/) virou um assim que
ganhou esses dois métodos. É só isso — o conceito `std::ranges::range<T>` não pede mais nada.

Uma **view** é um range especial, com duas propriedades a mais:

- **preguiçosa** — o trabalho acontece na iteração, não na construção;
- **sem posse** — referencia os dados de outro; copiar é O(1).

Um `std::vector` é range e **não** é view (ele possui os elementos, e copiá-lo é O(n)). Um
`v | vw::filter(p)` é os dois.

### A preguiça, provada

Os lambdas do demo imprimem quando são chamados:

```
construindo a view (nada deve ser impresso):
...view construída. Nenhum elemento foi tocado.
agora iterando:
  filter(1)
  filter(2)
  transform(2)
  → 20
  filter(3)
  ...
```

Cada elemento atravessa o pipeline **inteiro** antes do próximo. Não há vetor intermediário
em lugar nenhum. E o tamanho confirma que nada foi copiado (medido no demo 1.2):

```
sizeof(vector<int>)      = 24 bytes
sizeof(a view de filter) = 16 bytes   ← não guarda elementos, só uma referência e o predicado
```

Por não possuir nada, a view **enxerga as mudanças** na origem: depois de
`v.push_back(6)`, a view de pares passa a produzir `2 4 6` sem ser reconstruída.

### Catálogo essencial

Cada linha traz a entrada e a saída **medidas pelo demo** (seção 1.3), com a base
`1 2 3 4 5 6 7 8 9 10`:

| View | Faz | Entrada → saída |
|---|---|---|
| `filter(p)` | mantém os que satisfazem `p` | par → `2 4 6 8 10` |
| `transform(f)` | aplica `f` a cada elemento | `x*x` → `1 4 9 … 100` |
| `take(n)` | os primeiros `n` | `take(3)` → `1 2 3` |
| `drop(n)` | pula os primeiros `n` | `drop(7)` → `8 9 10` |
| `reverse` | ao contrário | → `10 9 8 … 1` |
| `take_while(p)` | **para no primeiro** que falha | `< 5` → `1 2 3 4` |
| `drop_while(p)` | pula enquanto `p`, e daí em diante mantém **tudo** | `< 5` → `5 6 7 8 9 10` |
| `iota(a[, b])` | sequência `a, a+1, …`; sem `b`, **infinita** | `iota(1) \| take(10)` → `1 … 10` |
| `keys` / `values` | o `.first` / `.second` de cada par | `{Ada,10},{Grace,8},{Alan,9}` → `Ada Grace Alan` / `10 8 9` |
| `elements<I>` | o I-ésimo campo de cada tupla | `elements<0>` de tuplas = o mesmo que `keys` |
| `join` | achata um range de ranges | `{{1,2},{3,4},{5}}` → `1 2 3 4 5` |
| `split(delim)` | quebra por delimitador | `"alfa,beta,gama"` com `,` → `'alfa' 'beta' 'gama'` |

Duas confusões comuns:

```
take_while vs filter, em 1 2 9 3 4:
  take_while(< 5)   1 2        ← parou no 9 e desistiu do resto
  filter(< 5)       1 2 3 4    ← testou todos e pulou só o 9

drop_while vs filter, em 1 2 9 3 4:
  drop_while(< 5)   9 3 4      ← parou de descartar no 9; daí em diante passa tudo
  filter(>= 5)      9          ← testa todos, um a um
```

> ⚠ `split` produz **subranges**, não `std::string`. Para imprimir ou guardar, converta:
> `std::string_view(parte.begin(), parte.end())`.

### Views infinitas

```cpp
vw::iota(1)                                   // 1, 2, 3, ... sem fim
    | vw::transform([](int n) { return n*n; })
    | vw::filter([](int n) { return n % 2; })
    | vw::take(8);                            // → 1 9 25 49 81 121 169 225
```

Só é possível porque nada é materializado. Um `iota` "eager" tentaria alocar infinitos
elementos; o preguiçoso só produz o próximo quando alguém pede — e o `take(8)` para de pedir.

> ⚠ Um lambda `mutable` numa view (ex.: Fibonacci com estado) é frágil: a view pode ser
> percorrida mais de uma vez e o estado **não** reinicia. Para estado, use uma corrotina —
> ver [`../coroutines/demo_generator.hpp`](../coroutines/demo_generator.hpp).

### Materializar: saindo do mundo preguiçoso

Enquanto está **processando**, mantenha como view. Para **guardar** o resultado, ou para
atravessar uma fronteira de API que espera um container, materialize:

```cpp
auto view = v | vw::filter(par) | vw::transform(vezes100);
std::vector<int> resultado(view.begin(), view.end());   // → 200 400 600 800
```

> ⚠ **Esse construtor só funciona se a view for um `common_range`** — isto é, se `begin()` e
> `end()` tiverem o **mesmo tipo**. `vector | filter | transform` é comum; a view infinita
> logo acima **não é** (o fim dela é um sentinela de outro tipo), e o código não compila:
>
> ```
> error: no matching function for call to 'std::vector<int>::vector(
>          std::counted_iterator<...>, std::ranges::take_view<...>::_Sentinel<false>)'
> note: deduced conflicting types for parameter '_InputIterator'
> ```
>
> As duas saídas:
>
> ```cpp
> auto comum = r | vw::common;                          // C++20: iguala os dois tipos
> std::vector<int> v1(comum.begin(), comum.end());      // ✓ compila
>
> auto v2 = r | std::ranges::to<std::vector>();         // C++23 — só a partir do GCC 14
> ```

---

## Demo 2: Pipelines e Tempo de Vida

### O `operator|` não é mágica

As três formas são equivalentes:

```cpp
v | vw::filter(p)              // pipe
vw::filter(p)(v)               // chamada
rg::filter_view(v, p)          // construtor
```

`a | b | c` é literalmente `c(b(a))`, escrito na ordem em que se lê.

### Adaptadores são objetos

Podem ser nomeados, guardados e **compostos antes de ver um range**:

```cpp
auto so_pares       = vw::filter([](int x) { return x % 2 == 0; });
auto dobrar         = vw::transform([](int x) { return x * 2; });
auto pares_dobrados = so_pares | dobrar;      // ainda sem nenhum range

a | pares_dobrados;   b | pares_dobrados;     // reutilizável
```

Os algoritmos clássicos não permitem isso — não dá para guardar "um `copy_if` seguido de
`transform`".

### ⚠ A view pendurada

O que a view faz com a origem depende da **categoria de valor** dela:

| Origem | O que é | O que acontece |
|---|---|---|
| **lvalue** (`v` — uma variável, tem nome, sobrevive) | `ref_view` — 8 bytes | **referencia** o container |
| **prvalue** (`produz()` — temporário sem nome) | `owning_view` — 24 bytes | **assume a posse**: move o container para dentro da view |

Medido no demo (seção 2.4): `sizeof(ref_view) = 8` (só um ponteiro),
`sizeof(owning_view) = 24` (o `vector` inteiro por dentro).

Então isto é **seguro** — a view levou o vetor consigo:

```cpp
auto r = produz_vetor() | vw::filter(par);    // owning_view assumiu a posse
```

E isto **não é** — a view guardou uma referência para algo que vai morrer:

```cpp
auto f() {
    std::vector<int> local{1,2,3};
    return local | vw::filter(par);           // ← `local` é lvalue: ref_view para ele
}                                              // `local` morre aqui → view pendurada
```

`borrowed_range` é o conceito que responde "os iteradores sobrevivem ao objeto?":

```
std::vector<int>    : false
std::vector<int>&   : true
std::string_view    : true
std::span<int>      : true      ← por isso span é seguro devolver
```

### Um pipeline de verdade

Processar CSV sem construir uma única `std::string`:

```cpp
auto registros = csv
    | vw::split('\n')
    | vw::transform(para_sv)
    | vw::filter([](std::string_view l) { return !l.empty() && l.front() != '#'; });
```

> ⚠ Ao converter um subrange em `string_view`, use `std::to_address(r.begin())` — **não**
> `&*r.begin()`. `std::to_address` obtém o ponteiro **sem desreferenciar** o iterador; e isso
> importa porque a última linha de um CSV terminado em `\n` produz um subrange vazio cujo
> `begin() == end()`, e desreferenciar o `end()` é UB.

### Custo, medido

Somar `x*2` para os múltiplos de 3 em **10 milhões de `int`**, loop à mão contra pipeline de
duas views — o programa completo está em "Como validar", e estes são os números **desta
máquina** (GCC 13), com a faixa observada em quatro execuções:

```
-O2:
  loop à mão            6.6 ms      (faixa 5.9 – 6.7)
  pipeline de views     6.5 ms      (faixa 6.1 – 8.2)   ← indistinguíveis: a variação
                                                          entre execuções é maior que a
                                                          diferença entre as versões
-O0:
  loop à mão          104.0 ms      (faixa  59 – 104)
  pipeline de views   686.8 ms      (faixa 484 – 693)   ← ~7× mais lento
```

Em `-O2` a abstração é de fato gratuita: cada adaptador é uma classe cujo `operator++` é
inlined, e o compilador funde tudo num laço só. Em `-O0` (o default de debug deste repo) não
há inlining, e sobram camadas de chamada de função por elemento — **não meça views em
debug**.

Onde views ganham mesmo sem otimização: eliminam containers temporários e curto-circuitam
com `take`. Onde podem perder: `filter_view::begin()` é O(n) (precisa achar o primeiro
elemento válido antes de devolver o iterador), e pipelines muito longos custam tempo de
compilação e mensagens de erro imensas.

---

## Demo 3: `std::ranges::*` e Projeções

Os exemplos desta seção usam o tipo dos fontes (`demo_algorithms.hpp`):

```cpp
struct Pessoa {
    std::string nome;
    int         idade;
    double      salario;
};

std::vector<Pessoa> v = {
    {"Ada", 36, 9500.0}, {"Linus", 28, 8200.0}, {"Grace", 45, 11000.0},
    {"Dennis", 28, 7800.0}, {"Barbara", 52, 12500.0}, {"Alan", 41, 10200.0},
};
```

A assinatura é sempre `algoritmo(range, comparador = {}, projeção = {})` — o range **inteiro**
no lugar do par de iteradores, e dois parâmetros opcionais no fim.

```cpp
std::sort(v.begin(), v.end(),
          [](const Pessoa& a, const Pessoa& b) { return a.idade < b.idade; });

rg::sort(v, {}, &Pessoa::idade);      // ← a mesma coisa
```

A segunda diz **o que** ordenar; a primeira, **como** comparar. O `{}` no meio é o
comparador default (`rg::less{}`), e `&Pessoa::idade` é um ponteiro-para-membro — a
**projeção**, aplicada a cada elemento antes de comparar. Ela muda o critério, não o
resultado: quem é ordenado continua sendo a `Pessoa` inteira.

```cpp
rg::sort(v, rg::greater{}, &Pessoa::salario);   // do maior salário para o menor
rg::find(v, 45, &Pessoa::idade);                // a primeira pessoa com 45 anos
rg::count(v, 28, &Pessoa::idade);               // quantas têm 28 → 2
rg::max_element(v, {}, &Pessoa::idade);         // a mais velha
rg::sort(v, {}, [](const Pessoa& p) { return std::tie(p.idade, p.nome); });  // multi-critério
```

### Retornos mais informativos

Os clássicos jogam informação fora; os de ranges devolvem structs nomeadas:

| Algoritmo | Devolve |
|---|---|
| `copy_n` | `{in, out}` — os **dois** iteradores |
| `minmax` | `{min, max}` |
| `mismatch` | `{in1, in2}` |
| `for_each` | `{in, fun}` — recupera o functor com estado |

### Proteção contra dangling, em compilação

```cpp
auto it = rg::find(produz(), 3);    // tipo é std::ranges::dangling
*it;                                 // ✗ NÃO COMPILA
```

Quando o range é um temporário que não é `borrowed_range`, o algoritmo **se recusa** a
devolver um iterador: devolve o tipo vazio `std::ranges::dangling`, que não tem `operator*`.
Um bug de runtime virou erro de compilação. O clássico
`std::find(produz().begin(), produz().end(), 3)` compila, roda, e é UB **duplo** (são dois
vetores diferentes, e ambos já morreram).

### O que ainda falta no GCC 13

- os algoritmos **numéricos** não têm versão em ranges (`accumulate`, `reduce`,
  `inner_product`) — em C++23 chegou `ranges::fold_left`
- as versões paralelas (`std::execution`) não têm
- **chegaram só no GCC 14**: `ranges::to<C>()`, `views::zip`, `views::enumerate`,
  `views::chunk`, `views::slide`, `ranges::fold_left`

Para acumular sobre uma view hoje (a view de `transform` sobre um `vector` é `common_range`,
então os iteradores servem direto ao algoritmo clássico):

```cpp
auto sal = v | vw::transform(&Pessoa::salario);
std::accumulate(sal.begin(), sal.end(), 0.0);      // → 59200.00
```

---

## Demo 4: Escrevendo os Seus

Três níveis, em ordem crescente de esforço:

### Nível 1 — um `range`

Basta `begin()` e `end()`. **Dois métodos** e você ganha range-for, `rg::sort` com
projeção, e o direito de iniciar um pipeline.

### Nível 2 — `view_interface`

Herdar de `std::ranges::view_interface<Derivada>` (CRTP — a base é template do derivado e
por isso pode chamar os métodos dele sem `virtual`; igual ao de
[`../templates/demo_crtp.hpp`](../templates/demo_crtp.hpp)) deriva de graça, a partir apenas
do seu `begin()`/`end()`:

```
empty(), front(), back(), operator[], operator bool, size()
```

### Nível 3 — um adaptador com `operator|`

Precisa de um objeto que, chamado com `(range, args)`, devolva a view; chamado só com
`(args)`, devolva um adaptador parcial; e que defina `operator|` para esse parcial.
São ~15 linhas em C++20 — em C++23, `std::ranges::range_adaptor_closure` dispensa o
`operator|` à mão. Feito isso, o seu adaptador é indistinguível dos da biblioteca:

```cpp
v | passo_reverso(2) | vw::filter(mult3) | vw::transform(vezes100);   // → 1200 600
```

### A escada das categorias

A categoria de um range é a do iterador dele, e diz **o que ele sabe fazer**. Cada degrau
inclui todos os anteriores:

| Degrau | O que acrescenta | Exemplo |
|---|---|---|
| `input` | ler e `++`, **uma passada só** | `istream_iterator`, `filter` sobre um input |
| `forward` | **múltiplas passadas** com o mesmo resultado | `forward_list` |
| `bidirectional` | `--it` (andar para trás) | `list`, `map`, `set` |
| `random_access` | `it + n` e `b - a` em **O(1)** | `deque` |
| `contiguous` | os elementos são **vizinhos na memória** (dá `&*it` e `memcpy`) | `vector`, `array`, `string`, `span` |

### ⚠ A ordem no pipeline é o sistema de tipos

Cada adaptador **preserva** ou **degrada** a categoria do range:

| Adaptador | Efeito na categoria |
|---|---|
| `vector` (origem) | contiguous / random access |
| `transform` | preserva |
| `take` | preserva |
| `filter` | **degrada** para bidirectional |

`filter` degrada porque não dá para saltar N elementos sem testar o predicado em cada um —
o `it + n` em O(1) deixa de existir. Consequência prática: se o seu adaptador exige
`random_access_range`, ele **não compila** depois de um `filter`. Ponha-o **antes**. O GCC
diz exatamente isso:

```
'std::random_access_iterator_tag' is not a base of 'std::bidirectional_iterator_tag'
```

Traduzindo a mensagem: "você pediu random access, e o que chegou foi bidirectional" — ou
seja, alguma etapa anterior do pipeline degradou a categoria.

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Encadeando transformações | views — 1 passada, 0 temporários |
| Precisa guardar o resultado | materialize: `std::vector v(r.begin(), r.end())` — **só se `r` for `common_range`**; senão `r \| vw::common` antes (ou `ranges::to<std::vector>()` do GCC 14 em diante) |
| Devolvendo uma view de função | só se a origem for `borrowed_range` (`span`, `string_view`) |
| Origem é um temporário | ok — vira `owning_view` |
| Origem é uma variável local | **nunca** devolva a view |
| Ordenando por um campo | `rg::sort(v, {}, &T::campo)` |
| Múltiplos critérios | projeção para `std::tie` |
| Medindo performance | nunca em `-O0` |
| Seu adaptador exige random access | ponha-o **antes** de qualquer `filter` |
| Precisa de estado na geração | corrotina, não lambda `mutable` |
| Converteu subrange em `string_view` | `std::to_address(r.begin())`, nunca `&*r.begin()` |

---

## Como validar

O caminho normal — configura, compila e instala em `dist/bin/`:

```bash
make build
./dist/bin/ranges
```

Compilar só este alvo, sem passar pelo `make`:

```bash
source build/conanbuild.sh      # sem isto o meson não acha o fmt
meson compile -C build ranges
./dist/bin/ranges
```

Compilar o arquivo isolado, com todos os avisos ligados (rodando da **raiz do repositório**;
o `-I`/`-L` do fmt vêm do pacote do Conan):

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/ranges/main.cpp -o /tmp/rg -L$FMTP/lib -lfmtd && /tmp/rg

# ASan + UBSan: pega a view pendurada e o string_view do pipeline de CSV
g++ -std=c++20 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/ranges/main.cpp -o /tmp/rgsan -L$FMTP/lib -lfmtd && /tmp/rgsan
```

Estado atual, verificado: compila **sem warnings** com `-Wall -Wextra -Wpedantic` e roda com
**0 achados** sob ASan+UBSan.

### Reproduzindo a medição de custo

O programa que gerou os números da seção "Custo, medido":

```bash
cat > /tmp/bench_ranges.cpp <<'EOF'
#include <chrono>
#include <cstdio>
#include <numeric>
#include <ranges>
#include <vector>
namespace vw = std::views;

int main() {
    std::vector<int> v(10'000'000);
    std::iota(v.begin(), v.end(), 1);
    auto crono = [](auto f) {
        auto t0 = std::chrono::steady_clock::now();
        long long r = f();
        auto t1 = std::chrono::steady_clock::now();
        printf("  %8.1f ms   (soma = %lld)\n",
               std::chrono::duration<double, std::milli>(t1 - t0).count(), r);
    };
    printf("loop a mao:\n");
    crono([&] {
        long long s = 0;
        for (int x : v) if (x % 3 == 0) s += (long long)x * 2;
        return s;
    });
    printf("pipeline de 2 views:\n");
    crono([&] {
        long long s = 0;
        for (long long x : v | vw::filter([](int n){ return n % 3 == 0; })
                             | vw::transform([](int n){ return (long long)n * 2; }))
            s += x;
        return s;
    });
}
EOF
g++ -std=c++20 -O2 /tmp/bench_ranges.cpp -o /tmp/b2 && /tmp/b2
g++ -std=c++20 -O0 /tmp/bench_ranges.cpp -o /tmp/b0 && /tmp/b0
```

As duas versões precisam imprimir a **mesma soma** (`33333336666666`) — é o que garante que
está sendo medida a mesma conta. Espere `-O2` empatado dentro do ruído e `-O0` com o
pipeline várias vezes mais lento.

Saída esperada ao final do demo: `✓ Todos os demos de Ranges concluídos!`

## Referências

- [`../stl/`](../stl/) — os iteradores e os algoritmos clássicos que `<ranges>` substitui;
  leia antes se "iterador" e "categoria" ainda não forem familiares
- [`../concepts/`](../concepts/) — a mecânica dos concepts que definem `range`, `view` e
  `borrowed_range`
- [`../templates/`](../templates/) — CRTP, a técnica por trás de `view_interface`
- [`../coroutines/`](../coroutines/) — geração com estado, a alternativa correta ao lambda
  `mutable`
- [`../cpp23/`](../cpp23/) — `ranges::to`, `views::zip`, `views::enumerate` e o que chegou depois
- [`../lambdas/`](../lambdas/) — captura, `mutable` e o custo dos functores que alimentam as views
- [cppreference: Ranges library](https://en.cppreference.com/w/cpp/ranges) — a lista completa
  de views e as pré-condições de cada uma
- Eric Niebler, *range-v3* — a biblioteca original da qual `<ranges>` foi extraído; tem views
  que ainda não entraram no padrão
- Tristan Brindle, *C++20 Ranges in Practice* (CppCon 2020) — a palestra que cobre justamente
  tempo de vida e a armadilha da view pendurada
