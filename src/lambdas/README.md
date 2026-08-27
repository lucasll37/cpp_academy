# lambdas — Capturas, Genéricas e Type Erasure

## O que é este projeto?

Uma **lambda** é uma função escrita no lugar onde ela é usada, sem nome e sem declaração
separada. Este subprojeto cobre o que o compilador realmente gera a partir dela, o que cada
forma de captura custa, e os padrões que só passaram a existir depois do C++11.

```bash
make build
./dist/bin/lambdas
```

> `make build` compila **tudo**; o executável instalado é `dist/bin/lambdas`. Se for a
> primeira vez neste repositório, rode `make configure` antes (ele baixa o `fmt` pelo Conan e
> configura o `build/`). O `dist/` é o **prefixo de instalação**, não o diretório de build.

> ⚠ Este alvo compila com **C++23** (`override_options: ['cpp_std=c++23']` no
> [`meson.build`](meson.build)). O `deducing this` (`this auto&& self`) **não** está
> disponível no GCC 13 — chegou no GCC 14; o Demo 5 documenta a alternativa.

---

## Antes de começar

Esta seção define **todo** o vocabulário usado no resto do arquivo. Se você já conhece um
termo, pule; nada aqui depende de leitura externa.

### O problema que a lambda resolve

Ordenar um vetor de strings por tamanho. Antes do C++11, era preciso declarar a comparação
longe do lugar onde ela é usada:

```cpp
// versão pré-C++11: a lógica mora aqui em cima…
bool por_tamanho(const std::string& a, const std::string& b) {
    return a.size() < b.size();
}

int main() {
    std::vector<std::string> v{"banana", "uva", "abacaxi"};
    std::sort(v.begin(), v.end(), por_tamanho);      // …e é usada aqui embaixo
}
```

Duas frases separadas por trezentas linhas de arquivo, para uma comparação de uma linha. E
se `por_tamanho` precisasse de um parâmetro externo (um limite, uma tabela), não havia como
passá-lo: seria preciso escrever uma `struct` com `operator()` e um membro.

Com lambda, a lógica fica no ponto de uso:

```cpp
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::vector<std::string> v{"banana", "uva", "abacaxi"};

    std::sort(v.begin(), v.end(),
              [](const std::string& a, const std::string& b) { return a.size() < b.size(); });

    for (const auto& s : v) std::cout << s << ' ';    // uva banana abacaxi
    std::cout << '\n';
}
```

### A anatomia, parte por parte

```
[capturas] (parâmetros) mutable -> retorno { corpo }
    │           │          │         │         │
    │           │          │         │         └─ o código
    │           │          │         └─ tipo de retorno; quase sempre dedução automática
    │           │          └─ opcional: permite modificar o que foi capturado por valor
    │           └─ como qualquer lista de parâmetros de função
    └─ o que a lambda leva do escopo de fora consigo
```

Só o `[]` e o `{}` são obrigatórios. Tudo o mais é opcional:

```cpp
auto f = []{ return 42; };     // sem capturas, sem parâmetros, sem retorno declarado
int  n = f();                  // 42
```

A linha acima mostra a coisa mais importante do assunto: **uma lambda é um valor**. Ela se
guarda numa variável, se passa como argumento, se devolve de uma função.

### A ideia que responde quase tudo

Uma lambda é uma **classe**:

```cpp
auto f = [x](int y) { return x + y; };

// o compilador gera, aproximadamente:
class __lambda {
    int x;                                        // ← a captura vira MEMBRO
public:
    auto operator()(int y) const { return x + y; }
};
```

Isso explica sozinho: por que `mutable` existe (ele tira o `const` daquele `operator()`),
por que capturar por referência é perigoso (o membro é um ponteiro), por que cada lambda tem
tipo único (cada uma gera uma classe diferente), e por que `[=]` com `this` é a armadilha
nº 1.

### Glossário

| Termo | O que é |
|---|---|
| **functor** (ou *objeto-função*) | uma classe que define `operator()`; um objeto dela é chamável como se fosse função. A lambda é açúcar para isso. Ver [`src/operators/`](../operators/), Demo 4. |
| **chamável** (*callable*) | qualquer coisa que se possa escrever `f(...)`: função, ponteiro para função, lambda, functor, ponteiro para membro. |
| **lvalue** | uma expressão que tem *identidade*: você pode tomar o endereço dela. Uma variável nomeada é um lvalue. |
| **rvalue** | uma expressão temporária, sem nome, que vai morrer no fim da instrução: `std::string("oi")`, ou o resultado de `std::move(s)`. Pode ser **roubada** (movida) em vez de copiada. |
| **forwarding reference** | um `T&&` onde `T` é um parâmetro de template deduzido (ou `auto&&`). Ele capta lvalue **ou** rvalue e lembra qual foi. Não confundir com `std::string&&`, que é só uma referência de rvalue. |
| **perfect forwarding** | repassar um argumento adiante **preservando** se ele era lvalue ou rvalue. `std::forward<T>(x)` é a ferramenta: ele vira `std::move(x)` se o chamador deu um rvalue, e não faz nada se deu um lvalue. |
| **vtable** | tabela de ponteiros para função que cada objeto polimórfico carrega. Uma chamada virtual lê o ponteiro na tabela e salta — o destino só é conhecido em execução. |
| **inlinar** | substituir a chamada pelo corpo da função no ponto de uso. Só é possível quando o compilador sabe **estaticamente** qual função será chamada; por isso uma chamada via vtable não inlina. |
| **type erasure** | apagar o tipo concreto atrás de uma interface fixa: o objeto real é guardado numa classe derivada gerada por template, e só a interface aparece na assinatura. É como `std::function` aceita qualquer chamável. Ver [`src/type_erasure/`](../type_erasure/). |
| **SBO** (*Small Buffer Optimization*) | `std::function` carrega um pequeno buffer **dentro de si**. Um chamável que cabe nesse buffer é guardado ali mesmo, sem nenhuma chamada a `operator new`; o que não cabe vai para o heap. Daí `sizeof(std::function<void()>) == 32` neste compilador. |
| **IIFE** (*Immediately Invoked Function Expression*) | uma lambda definida e chamada na mesma expressão. O `()` no fim é a chamada: `const int x = []{ ... }();`. |
| **y-combinator** | o truque de dar à lambda uma referência a si mesma **como parâmetro**, já que ela não pode se chamar pelo próprio nome. Demo 5. |
| **RAII** (*Resource Acquisition Is Initialization*) | amarrar um recurso ao tempo de vida de um objeto: o construtor adquire, o destrutor libera. Como o destrutor roda mesmo quando uma exceção sobe, a liberação é garantida. Ver [`src/errors/`](../errors/). |
| **name hiding** | quando um nome declarado numa classe derivada esconde **todas** as sobrecargas do mesmo nome nas bases. É o que o `using Ts::operator()...;` do Demo 5 desfaz. Ver [`src/oop/`](../oop/). |
| **default-constructible** | um tipo que pode ser construído sem argumentos: `T t;`. Desde o C++20, lambdas sem captura são default-constructible, e é isso que permite `std::set<T, decltype([]{...})>`. |
| **concept** | uma restrição nomeada sobre um tipo de template (`std::integral`, `std::ranges::range`). Ver [`src/concepts/`](../concepts/). |

---

## Por que estudar isto?

Porque uma lambda parece um detalhe de sintaxe e não é. Três coisas medidas neste
subprojeto:

| Descoberta medida aqui | Efeito |
|---|---|
| `std::function` vs `template <typename F>` no mesmo laço | **~3,4×** mais lento (12 ms vs 41 ms, `-O0`) |
| lambda capturando 64 bytes dentro de `std::function` | **1 alocação** no heap; capturando um `int`, zero |
| `std::forward` num parâmetro `auto&&` | 1 cópia → **1 move** |

E porque as duas armadilhas mais caras de C++ moderno em produção são de lambda:
`[=]` dentro de um método (que copia o `this`, não os membros) e `[&]` numa lambda que
sobrevive ao escopo — as duas são *use-after-free*, e as duas compilam sem aviso.

---

## Estrutura de arquivos

```
src/lambdas/
├── main.cpp               ← ponto de entrada, cor turquoise
├── demo_capturas.hpp      ← 1 · por valor/ref, mutable, init-capture, a armadilha do this
├── demo_genericas.hpp     ← 2 · auto param, template explícito, forwarding, variádicas
├── demo_function.hpp      ← 3 · type erasure por dentro, custo MEDIDO, SBO
├── demo_invoke.hpp        ← 4 · ponteiros p/ membro, invoke, bind_front vs bind
├── demo_padroes.hpp       ← 5 · IIFE, overload idiom, recursão, scope guard, comparadores
├── meson.build
└── README.md
```

---

## Demo 1: Capturas

### A lambda é uma classe — medido

Se a captura vira membro, então `sizeof` da lambda é o `sizeof` das capturas. Saída medida
(`── 1.1`):

```
sizeof([]{})      = 1      ← classe VAZIA (nenhum membro; 1 byte porque objetos
                             distintos precisam de endereços distintos)
sizeof([a]{})     = 4      = sizeof(int)
sizeof([a,b]{})   = 16     = int + double + padding
sizeof([&a]{})    = 8      = um ponteiro
sizeof([s]{})     = 32     = sizeof(std::string)
```

Duas lambdas com corpo **idêntico** têm tipos **diferentes** — o demo confirma
(`duas lambdas idênticas têm o mesmo tipo? false`). É exatamente por isso que
`std::function` existe.

### As formas

| Forma | O que faz | Risco |
|---|---|---|
| `[x]` | copia `x` | nenhum |
| `[&x]` | referencia `x` | dangling |
| `[=]` | copia tudo que usa (**+ `this` por ponteiro**) | ⚠ `this` |
| `[&]` | referencia tudo que usa | dangling |
| `[x=e]` | init-capture: cria membro do zero | nenhum |

Recomendação: prefira capturas **explícitas**. `[=]` e `[&]` economizam digitação e escondem
de que a lambda depende — inclusive do `this`.

### `mutable`

Remove o `const` do `operator()` gerado, permitindo modificar a **cópia** capturada — o que
transforma a lambda num functor com estado:

```cpp
auto contar = [c = 0]() mutable { return ++c; };
contar();  // 1
contar();  // 2      ← o estado persiste entre chamadas
```

> ⚠ Uma medição que revelou um bug no próprio demo: `print("{} {} {}", f(), f(), f())` tem
> ordem de avaliação **não especificada**. O GCC avalia da direita para a esquerda e
> imprimiria `3 2 1`. Quando o efeito colateral importa, use variáveis nomeadas — é o que o
> fonte faz hoje, de propósito.

### Init-capture (C++14)

Resolve quatro problemas de uma vez:

```cpp
[r = std::move(ptr)]      // capturar tipos move-only (unique_ptr, thread…)
[total = v.size() * 10]   // pré-calcular uma vez, não a cada chamada
[h = cfg.host]            // capturar um MEMBRO sem copiar o objeto inteiro
[&n = contador]           // dar nome curto a algo com nome longo
```

### ⚠ A armadilha nº 1: `[=]` captura `this` por ponteiro

Dentro de um método, `[=]` **não copia os membros** — copia o `this`:

```cpp
[=] { return nome_; }
  → NÃO copia `nome_`
  → copia `this`, e `nome_` vira `this->nome_`
```

| Captura | Copia | Seguro após o objeto morrer |
|---|---|---|
| `[this]` | o ponteiro | **não** |
| `[=]` | o ponteiro (!) | **não** |
| `[*this]` | o **objeto** | sim (C++17) |
| `[n=nome_]` | só o membro | sim, e é o mais barato |

O C++20 **depreciou** a captura implícita de `this` via `[=]` (o GCC avisa com
`-Wdeprecated`).

> **Regra:** se a lambda pode sobreviver ao objeto (callback, thread, `std::function`
> guardada), nunca capture `this`.

### ⚠ A armadilha nº 2: captura por referência e threads

É o mesmo erro, na sua forma mais cara — e a mais difícil de reproduzir num teste:

```cpp
void enviar() {
    std::vector<int> dados = carregar();
    std::thread t([&]{ processar(dados); });     // ⚠ captura `dados` por REFERÊNCIA
    t.detach();
}                                                // `dados` morre AQUI
                                                 // a thread continua lendo memória liberada
```

O `[&]` guarda um ponteiro para `dados`. A função retorna, o vetor é destruído, e a thread
segue usando o endereço — *use-after-free*. Como a thread pode chegar lá antes ou depois, o
bug aparece uma vez em mil execuções, sempre em produção.

A regra é a mesma da armadilha nº 1, dita de outro jeito: **capture por valor sempre que a
lambda puder rodar depois do escopo terminar.** Se copiar for caro, mova:
`std::thread t([d = std::move(dados)]{ processar(d); });`.

`std::jthread` (C++20) resolve metade do problema — ele faz `join()` no destrutor, então a
thread não sobrevive ao escopo. Ver [`src/concurrency2/`](../concurrency2/).

### Quando `[&]` é o certo

Quando o algoritmo **consome** a lambda antes de retornar:

```cpp
std::sort(v.begin(), v.end(), [&](auto& a, auto& b) { return chave[a] < chave[b]; });
```

Aqui a lambda morre antes de `sort` retornar, e o `[&]` evita cópias.

---

## Demo 2: Lambdas Genéricas

### `auto` como parâmetro (C++14)

Um `auto` num parâmetro transforma o `operator()` gerado num **template**: um único objeto,
uma instanciação por tipo usado.

```cpp
auto dobrar = [](auto x) { return x * 2; };
dobrar(21);     // 42     (int)
dobrar(1.5);    // 3.0    (double)
```

### `auto` vs template explícito (C++20)

Com `auto` você não tem **nome** para o tipo, o que impede `std::forward<T>` limpo, usar `T`
para declarar variáveis, e acessar `T::algo`:

```cpp
[](auto&& x)          { return std::forward<decltype(x)>(x); }   // verboso
[]<typename T>(T&& x) { return std::forward<T>(x); }             // legível
```

### Perfect forwarding — medido

O parâmetro `x` de `[](auto&& x)` **tem nome**, e tudo que tem nome é lvalue — mesmo que o
chamador tenha passado um temporário. Sem `std::forward`, a informação "isto era um rvalue,
pode ser roubado" se perde no caminho. Saída medida (`── 2.3`):

```
sem forward, arg rvalue: 1 cópia(s), 0 move(s)     ← perdeu a chance de mover
com forward, arg rvalue: 0 cópia(s), 1 move(s)     ← recuperou
com forward, arg lvalue: 1 cópia(s), 0 move(s)     ← correto: lvalue não pode ser roubado
```

O `forward` é **condicional**: move se o chamador deu rvalue, copia se deu lvalue. É por
isso que ele existe, e é por isso que não dá para simplesmente usar `std::move`.

### Variádicas e folds

| Fold | Expande para |
|---|---|
| `(args + ...)` | `a1 + (a2 + (a3 + a4))` |
| `(... + args)` | `((a1 + a2) + a3) + a4` |
| `(init + ... + args)` | com valor inicial |
| `((f(args)), ...)` | `f(a1), f(a2), f(a3), ...` |

### Restringindo com concepts

Um *concept* é uma restrição nomeada sobre o tipo. As três sintaxes que a lambda aceita:

```cpp
// 1) concept no lugar do `auto` — a forma mais curta
auto dobrar_numero = [](std::integral auto x) { return x * 2; };

// 2) cláusula `requires` sobre um parâmetro de template nomeado
auto media = []<typename C>(const C& c) requires std::ranges::range<C> { /* ... */ };

// 3) `requires requires` ad-hoc: exige que a expressão seja válida
auto tem_size = []<typename T>(const T& x) requires requires { x.size(); } { return x.size(); };
```

Saída medida (`── 2.5`): `dobrar_numero(21) = 42`, e `dobrar_numero(1.5)` **não compila** —
está comentado no fonte de propósito. Sem o concept, `dobrar_numero(1.5)` compilaria e
devolveria `3.0` silenciosamente; com ele, o erro é na **chamada**, e diz qual restrição
falhou. Ver [`src/concepts/`](../concepts/).

---

## Demo 3: `std::function`

### O problema

Cada lambda tem tipo próprio, então isto não compila:

```cpp
auto somar    = [](int a, int b) { return a + b; };
auto subtrair = [](int a, int b) { return a - b; };
std::vector<decltype(somar)> ops{somar, subtrair};    // ✗ tipos diferentes
```

`std::function<int(int,int)>` resolve: ela guarda **qualquer** chamável com aquela
assinatura.

### O type erasure por dentro

```cpp
struct Conceito { virtual R chamar(Args...) const = 0; };      // a interface fixa
template <typename F> struct Modelo : Conceito { F f_; /* ... */ };  // guarda o tipo real
std::unique_ptr<Conceito> impl_;                               // o dono
```

O tipo concreto some da assinatura mas continua vivo dentro do `Modelo<F>`. É o mesmo padrão
do [`std::any`](../vocabulary_types/) e do [ECS](../ecs/), e o assunto de
[`src/type_erasure/`](../type_erasure/).

**E é daí que vem o custo:** a chamada passa por uma **vtable** e o corpo não pode ser
inlinado.

### O custo, medido

Saída medida (`── 3.3`), 2 000 000 de elementos, dobrando cada um:

```
template<F>        :  12 ms   ← inlinável
std::function      :  41 ms   ← chamada virtual (~3,4×)
ponteiro p/ função :  12 ms
```

#### Onde os números foram medidos

| | |
|---|---|
| CPU | AMD Ryzen 9 5900X (12 núcleos) |
| Compilador | GCC 13.3.0 (Ubuntu 24.04, WSL2) |
| Padrão / flags | `-std=c++23 -O0 -g -Wall -Wextra -Wpedantic` (`buildtype=debug` do `make build`) |
| Carga | `std::vector<int>` de 2 000 000 elementos |
| Reprodução | `make build && ./dist/bin/lambdas` |

Em três execuções seguidas nesta máquina: `template` 12–13 ms, `std::function` 41–44 ms,
ponteiro para função 12 ms. Os valores absolutos variam com o processador e a carga; a razão
`std::function` ≈ 3,4× não varia.

> Medido em `-O0`, o build de debug deste repositório. Em `-O2` a diferença **aumenta**: o
> template costuma sumir por completo (o laço vira vetorizado), enquanto `std::function`
> mantém a indireção.

### SBO — quando `function` aloca

`std::function` carrega um pequeno buffer interno (*Small Buffer Optimization*). O chamável
que cabe nele é construído ali mesmo, sem nenhuma chamada a `operator new`; o que não cabe
vai para o heap. Saída medida (`── 3.4`), com `operator new` sobrescrito para contar:

```
sizeof(std::function<void()>) = 32       ← o objeto inteiro, buffer incluído
lambda sem captura      → 0 alocações
lambda capturando 1 int → 0 alocações
lambda capturando 64 B  → 1 alocação     ← estourou o SBO
```

Consequência prática: um `std::function` **construído dentro de um laço quente** pode fazer
`malloc`/`free` a cada iteração. Se a captura for grande, construa fora do laço.

### Quando usar cada um

| Ferramenta | Inlina | Aloca | Guardável | Assinatura |
|---|---|---|---|---|
| `template <typename F>` | **sim** | não | não | genérica |
| `auto f = lambda` | **sim** | não | local | genérica |
| ponteiro para função | não | não | sim | fixa |
| `std::function` | não | talvez | **sim** | fixa |

Use `template <typename F>` quando a função **consome** o chamável e retorna (algoritmos),
quando performance importa, ou em código de biblioteca genérico.

Use `std::function` quando precisa **guardar** o chamável (callback, handler, fila), precisa
de heterogeneidade num container, ou a assinatura precisa atravessar uma fronteira de ABI
(um template não atravessa um `.so` — ver [`src/linkage/`](../linkage/)).

> ⚠ O erro mais comum: usar `std::function` como **parâmetro** de uma função que só o chama
> e descarta:
> ```cpp
> void ordenar(std::vector<int>&, std::function<bool(int,int)>);   // ✗
> ```
> Isso força alocação e impede inlining sem nenhum ganho. Ali o certo é
> `template <typename Comp>`.

---

## Demo 4: `std::invoke` e Ponteiros para Membro

`std::invoke` unifica as **cinco** sintaxes de chamada que a linguagem trata diferente:

```
f(args...)              função, lambda, functor
(obj.*pm)(args...)      ponteiro p/ método
(ptr->*pm)(args...)     idem, via ponteiro
(obj.*pd)               ponteiro p/ campo
(ptr->*pd)              idem, via ponteiro
```

Sem `invoke`, código genérico precisaria de `if constexpr` para escolher entre elas.

O uso mais prático de um ponteiro para membro é como **projeção** em ranges:

```cpp
std::ranges::sort(equipe, {}, &Pessoa::idade);
```

Saída medida (`── 4.1`): `Linus(28) Ada(36) Grace(45)`.

### `bind_front` vs `bind` vs lambda

| Forma | Legível | Inlinável | Reordena | Aloca |
|---|---|---|---|---|
| `std::bind` | não | mal | **sim** | talvez |
| `bind_front` (C++20) | ok | sim | não | não |
| lambda | **sim** | **sim** | sim\* | não |

\* a lambda "reordena" simplesmente escrevendo na ordem certa.

Por que `std::bind` saiu de moda: placeholders opacos · copia os argumentos ligados por
padrão (e `std::ref` para evitar é fácil de esquecer) · aninhamento gera erros ilegíveis ·
trata `bind` aninhado de forma surpreendente. (Core Guidelines F.50: use lambda.)

---

## Demo 5: Padrões

### IIFE — *Immediately Invoked Function Expression*

Uma lambda **definida e chamada na mesma expressão**. O nome descreve exatamente isso: o
`()` no fim é a chamada.

```cpp
const std::string prefixo = [nivel] {
    if (nivel >= 3) return "[DEBUG] ";
    if (nivel >= 2) return "[INFO] ";
    return "[ERRO] ";
}();                              // ← ESTES parênteses invocam na hora
```

Serve para inicializar um `const` que precisa de várias instruções, sem função auxiliar.
Quase obrigatório para inicializar um **membro `const`** com lógica na lista de
inicialização — a alternativa é abrir mão do `const`.

### O overload idiom — por que funciona

```cpp
template <typename... Ts> struct Overload : Ts... { using Ts::operator()...; };
```

1. cada lambda é uma classe com `operator()`
2. `Overload : Ts...` herda de todas
3. `using Ts::operator()...;` traz os `operator()` para o mesmo escopo
4. a resolução de overload normal escolhe

Sem o passo 3, eles ficariam escondidos uns pelos outros — é o *name hiding* de
[`src/oop/`](../oop/).

Saída medida (`── 5.2`): `sizeof(Overload de 3 lambdas sem captura) = 1` — classes vazias,
e a EBO faz o objeto inteiro caber em 1 byte.

### Recursão: o y-combinator

Uma lambda **não pode se chamar pelo próprio nome**. A razão é a dedução de tipo: em
`auto fat = [](int n){ ... fat(n-1) ... };`, o tipo de `fat` só fica conhecido **depois** que
a inicialização termina — e o corpo já precisa dele. `auto` não pode ser usado dentro da sua
própria inicialização.

A saída é passar a lambda **para si mesma**, como parâmetro:

```cpp
auto fat = [](auto&& self, int n) -> int {
    return n <= 1 ? 1 : n * self(self, n - 1);
};

fat(fat, 5);      // 120
```

O `auto&& self` é deduzido no momento da chamada, quando o tipo já existe. O `-> int`
explícito é necessário: sem ele, o compilador tentaria deduzir o retorno a partir de uma
chamada recursiva ainda não tipada.

| Forma | Aloca | Inlina | Chamada | GCC 13 |
|---|---|---|---|---|
| `std::function` | talvez | não | `f(n)` | ✓ |
| y-combinator | não | sim | `f(f, n)` | ✓ |
| `deducing this` (C++23) | não | sim | `f(n)` | **✗** |

Enquanto o `deducing this` não chega (GCC 14, P0847), o y-combinator é a opção certa em
código quente: não aloca e inlina. A feiura do `f(f, n)` fica contida numa linha.

### Scope guard

**RAII** para qualquer ação, no ponto de uso, sem escrever uma classe por recurso. A classe
inteira cabe aqui:

```cpp
template <typename F>
class GuardaDeEscopo {
public:
    explicit GuardaDeEscopo(F f) : limpeza_(std::move(f)) {}

    ~GuardaDeEscopo() { if (ativo_) limpeza_(); }      // roda TAMBÉM no unwinding

    // move-only: dois guards não podem executar a mesma limpeza
    GuardaDeEscopo(const GuardaDeEscopo&)            = delete;
    GuardaDeEscopo& operator=(const GuardaDeEscopo&) = delete;
    GuardaDeEscopo(GuardaDeEscopo&& o) noexcept
        : limpeza_(std::move(o.limpeza_)), ativo_(o.ativo_) { o.ativo_ = false; }

    void dispensar() { ativo_ = false; }               // cancelar a limpeza (commit)

private:
    F    limpeza_;
    bool ativo_ = true;
};

// deduction guide, para escrever GuardaDeEscopo{[]{...}} sem dizer o tipo
template <typename F> GuardaDeEscopo(F) -> GuardaDeEscopo<F>;
```

Uso — o padrão *commit/rollback*:

```cpp
void transferir(Conta& a, Conta& b, int valor) {
    a.debitar(valor);
    GuardaDeEscopo desfazer{[&]{ a.creditar(valor); }};   // rollback armado

    b.creditar(valor);                                    // pode lançar
    desfazer.dispensar();                                 // deu certo: commit
}
```

Se `b.creditar` lançar, o destrutor de `desfazer` roda durante o *unwinding* e o débito é
revertido. Se não lançar, `dispensar()` desarma o guard. É o `scope_exit` do Boost e a
proposta `std::scope_exit`. Ver [`src/errors/`](../errors/) para a relação entre RAII e
exceções.

### Lambdas como comparadores

Desde o C++20, lambdas sem captura são *default-constructible*, então elas podem aparecer
direto no argumento de template de um container ordenado:

```cpp
using PorTamanho = decltype([](const std::string& a, const std::string& b) {
    return a.size() != b.size() ? a.size() < b.size() : a < b;
});

std::set<std::string, PorTamanho> ordenado{"banana", "uva", "maçã", "abacaxi", "kiwi"};
// uva kiwi banana maçã abacaxi
```

> ⚠ **O comparador precisa ser um *strict weak ordering*.** Se ele fosse só
> `return a.size() < b.size();`, duas strings **distintas** de mesmo tamanho seriam
> equivalentes — e o `std::set` descartaria uma delas como duplicata: `"uva"` e `"chá"`
> viram um elemento só. Por isso o comparador acima desempata pelo conteúdo (`a < b`)
> quando os tamanhos são iguais. `std::tie(a.size(), a) < std::tie(b.size(), b)` diz a mesma
> coisa em uma linha.

> ⚠ O comparador faz parte do **tipo**: dois sets com lambdas diferentes são incompatíveis,
> mesmo com a mesma lógica. Se precisar de heterogeneidade, use
> `std::function<bool(const T&, const T&)>` — e pague o type erasure do Demo 3.

Antes do C++20, era preciso construir o set passando a lambda:

```cpp
auto comp = [](int a, int b) { return a > b; };
std::set<int, decltype(comp)> decrescente(comp);   // ← precisava passar
```

---

## Regras de ouro

| Situação | Faça |
|---|---|
| Lambda usada e descartada no mesmo escopo | `[&]` é seguro e evita cópias |
| Lambda guardada, devolvida ou passada a thread | capture por **valor** (ou mova) |
| Dentro de um método, lambda que sobrevive | `[*this]` ou `[m = membro_]` — nunca `[=]` |
| Precisa de estado entre chamadas | `mutable` (mas cuidado ao copiar a lambda) |
| Capturar `unique_ptr` | init-capture: `[p = std::move(ptr)]` |
| Repassar argumento genérico | `[]<typename T>(T&& x)` + `std::forward<T>` |
| Função que só chama e descarta | `template <typename F>` — **não** `std::function` |
| Precisa guardar chamáveis heterogêneos | `std::function` |
| Aplicação parcial | lambda; `bind_front` se for literal — nunca `bind` |
| Chamar algo genérico | `std::invoke` |
| Inicializar `const` complexo | IIFE |
| Limpeza garantida no escopo | scope guard com lambda |
| Recursão numa lambda | y-combinator: `[](auto&& self, int n) -> int` |
| Comparador de `set`/`map` | lambda em `decltype`, e confira o *strict weak ordering* |

---

## Como validar

Rodar pelo build do repositório:

```bash
make build
./dist/bin/lambdas
```

Compilar direto, sem o Meson, para conferir que não há avisos:

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/lambdas/main.cpp -o /tmp/lb -L$FMTP/lib -lfmtd
/tmp/lb
```

Estado atual: **zero avisos** com `-Wall -Wextra -Wpedantic`.

Sob os sanitizers — importante aqui porque o Demo 1 exercita casos de *dangling* e o Demo 3
sobrescreve `operator new` para contar alocações:

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++23 -fsanitize=address,undefined -g -I$FMTP/include \
    src/lambdas/main.cpp -o /tmp/lb_asan -L$FMTP/lib -lfmtd
/tmp/lb_asan
```

Estado atual: roda limpo, **sem nenhum relatório** de ASan/UBSan — e a contagem do SBO
(`── 3.4`) continua a mesma (0/0/1), porque o `operator new` sobrescrito continua sendo
chamado. As lambdas realmente perigosas do demo estão **comentadas** no fonte de propósito:
o `fabrica_perigosa()` é descrito, não executado.

O que se espera ver na saída:

| Seção | Confirma |
|---|---|
| `── 1.1` | `sizeof` da lambda = soma das capturas; duas lambdas idênticas têm tipos diferentes |
| `── 1.2` | `[x]` congela no valor da criação, `[&x]` enxerga a mudança |
| `── 1.5` | `[*this]` e `[n=nome_]` sobrevivem ao objeto; `[=]` e `[this]` não |
| `── 2.3` | `com forward, arg rvalue: 0 cópia(s), 1 move(s)` |
| `── 3.3` | `std::function` cerca de 3× mais lento que o template |
| `── 3.4` | 0, 0 e **1** alocação — a terceira estourou o SBO |
| `── 5.3` | `deducing this` marcado como NÃO DISPONÍVEL no GCC 13 |
| `── 5.4` | o guard imprime a limpeza **também** no caminho com exceção |

A última linha tem de ser:

```
✓ Todos os demos de lambdas concluídos!
```

Para ver com os próprios olhos o aviso da captura implícita de `this` (armadilha nº 1). O
fonte do demo usa `[this]` **explícito** de propósito, então ele não dispara o aviso — este
trecho dispara:

```bash
cat > /tmp/dep.cpp <<'EOF'
#include <string>
struct S {
    std::string nome_;
    auto f() { return [=] { return nome_; }; }     // ⚠ captura implícita de `this`
};
int main() { S s; return static_cast<int>(s.f()().size()); }
EOF
g++ -std=c++20 -Wall -c /tmp/dep.cpp -o /dev/null
```

Saída esperada:

```
warning: implicit capture of 'this' via '[=]' is deprecated in C++20 [-Wdeprecated]
note: add explicit 'this' or '*this' capture
```

---

## Referências

Dentro deste repositório:

- [`src/operators/`](../operators/) — `operator()`, o functor que a lambda realmente é
- [`src/concepts/`](../concepts/) — as restrições usadas no Demo 2
- [`src/type_erasure/`](../type_erasure/) — o padrão de `std::function`, generalizado
- [`src/templates/`](../templates/) — o `operator()` template por trás da lambda genérica
- [`src/value_semantics/`](../value_semantics/) — as cópias e moves que a medição de forwarding conta
- [`src/concurrency2/`](../concurrency2/) — captura por referência, threads e `jthread`
- [`src/errors/`](../errors/) — RAII e exceções, o alicerce do scope guard
- [`src/vocabulary_types/`](../vocabulary_types/) — `std::any`, o mesmo type erasure
- [`src/ranges/`](../ranges/) — projeções e a armadilha da lambda `mutable` em views
- [`src/linkage/`](../linkage/) — por que um template não atravessa uma fronteira de ABI
- [`src/ROADMAP.md`](../ROADMAP.md) — o que o GCC 13 ainda não tem (`deducing this`)

Fora dele:

- cppreference — [*Lambda expressions*](https://en.cppreference.com/w/cpp/language/lambda)
- **N3649** — *Generic (Polymorphic) Lambda Expressions* (o `auto` em parâmetro, C++14)
- **P0428R2** — *Familiar template syntax for generic lambdas* (o `[]<typename T>`, C++20)
- **P0847R7** — *Deducing this* (C++23; GCC 14)
- **P0356R5** — *Simplified partial function application* (`std::bind_front`, C++20)
- **N4169** — *A proposal to add invoke function template* (`std::invoke`, C++17)
- *C++ Core Guidelines* **F.50** (use lambda quando uma função não serve), **F.52**
  (capture por referência em lambdas locais) e **F.53** (evite capturar por referência em
  lambdas não locais)
