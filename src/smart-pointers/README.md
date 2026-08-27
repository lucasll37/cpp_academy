# smart-pointers — posse de recursos sem escrever `delete`

## O que é este projeto?

Um curso executável sobre os três ponteiros inteligentes da biblioteca padrão —
`std::unique_ptr`, `std::shared_ptr`, `std::weak_ptr` — e sobre os padrões que eles tornam
possíveis (RAII genérico, factory polimórfica, pimpl).

```bash
make configure                 # Conan 2 baixa as dependências e configura o Meson (uma vez)
make build && make install     # compila tudo e copia os binários para dist/bin/
./dist/bin/smart_pointers
# recompilar só este alvo:
source build/conanbuild.sh && meson compile -C build smart_pointers
./build/src/smart-pointers/smart_pointers
```

> ⚠ `meson setup dist` **não** é o comando deste repositório: `dist/` é o diretório de
> *instalação* (`--prefix`), não o de *build*. O build fica em `build/` e precisa do
> `conanbuild.sh` no ambiente — sem ele o `dependency('fmt')` do `meson.build` falha.

---

## Antes de começar: os conceitos que você precisa

Nada aqui presume mais que C++ básico — sintaxe, funções, classes, destrutores.

### Memória automática (stack) e memória dinâmica (heap)

```
   STACK (automática)                 HEAP (dinâmica)
 ┌──────────────────────────┐   ┌──────────────────────────────┐
 │ locais: nascem ao entrar │   │ objetos de `new`: vivem até  │
 │ e morrem ao SAIR do bloco│   │ alguém chamar `delete`       │
 └──────────────────────────┘   └──────────────────────────────┘
  o compilador move um cursor    o `malloc` da libc procura um
  (o "stack pointer")            bloco livre
```

A stack é confiável: a destruição é garantida, inclusive quando uma exceção atravessa o
escopo. A heap não tem nada disso — liberar depende de você lembrar. **A ideia central
deste subprojeto:** colocar na *stack* um objeto pequeno cujo destrutor libera algo que
está na *heap*; assim a garantia da stack passa a valer para a heap.

### Ponteiro cru (*raw pointer*) e posse (*ownership*)

`T*` é só um número: o endereço de um objeto. Não diz **nada** sobre quem deve destruí-lo,
nem se o objeto ainda existe — duas funções `void f(Widget*)` podem ter contratos opostos
e o compilador não vê diferença. "Possuir" um recurso é ter o **dever** de liberá-lo. A
pergunta que organiza tudo é *quem é o dono?*, e há três respostas:

| Resposta | Ferramenta |
|---|---|
| exatamente um dono, em qualquer instante | `std::unique_ptr<T>` |
| vários donos; o último a sair apaga a luz | `std::shared_ptr<T>` |
| ninguém — só quero observar | `std::weak_ptr<T>` (ou `T&` / `T*`) |

### Destrutor, escopo e RAII

Ao sair de um bloco `{ … }` o compilador chama o destrutor de cada objeto local, na ordem
inversa da construção. Vale para `return`, `break` e **também quando uma exceção está
subindo** (*stack unwinding*): é a única coisa em C++ garantida em todos os caminhos de
saída. **RAII** (*Resource Acquisition Is Initialization*) é o nome ruim de uma ideia
simples: amarre a vida de um recurso à de um objeto na stack — adquira no construtor,
libere no destrutor. Por isso o destrutor é o lugar certo do `delete`, do `fclose`, do
`unlock`. Um smart pointer é RAII aplicado a "um ponteiro para a heap".

### O que um smart pointer É, por dentro

Não há mágica de compilador: é uma classe com **um membro** e um destrutor, em 12 linhas.

```cpp
template <class T>
class MeuUnique {
    T* p_;
public:
    explicit MeuUnique(T* p = nullptr) : p_(p) {}
    ~MeuUnique() { delete p_; }                        // ← o gancho: RAII
    MeuUnique(const MeuUnique&)            = delete;   // copiar duplicaria o dono
    MeuUnique& operator=(const MeuUnique&) = delete;
    MeuUnique(MeuUnique&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }   // rouba
    T& operator*()  const { return *p_; }
    T* operator->() const { return p_; }
    explicit operator bool() const { return p_ != nullptr; }
};
```

Todo o comportamento de `std::unique_ptr` decorre disso — e o custo é literalmente zero.
Medido nesta máquina (x86-64, GCC 13, `-O2`):

```
sizeof(T*)                                     = 8
sizeof(std::unique_ptr<T>)                     = 8   ← idêntico ao ponteiro cru
sizeof(std::unique_ptr<T, lambda sem captura>) = 8   ← deleter vazio some (EBO)
sizeof(std::unique_ptr<T, void(*)(T*)>)        = 16  ← ponteiro de função TEM estado
sizeof(std::shared_ptr<T>)                     = 16  ← ptr do objeto + ptr do bloco
sizeof(std::weak_ptr<T>)                       = 16
static_assert(sizeof(std::unique_ptr<int>) == sizeof(int*));   // passa
```

### `std::move` e semântica de movimento

`std::move(x)` **não move nada**: é um `static_cast` que reclassifica `x` como
"descartável", autorizando o construtor de movimento a *roubar* o conteúdo em vez de
copiá-lo. Para o `unique_ptr`, roubar é copiar o ponteiro e **zerar o original** — contrato
do padrão, não acaso.

```cpp
auto p = std::make_unique<Widget>(42);
auto q = std::move(p);
assert(!p);            // garantido
// p->metodo();        // ⚠ UB — desreferenciar ponteiro nulo
```

### Deleter

O *deleter* é o invocável que o smart pointer chama **no lugar do** `delete` — para
recursos que não se liberam com `delete` (`fclose`, `::close`, `sqlite3_close`).

| | o deleter faz parte do tipo? | consequência |
|---|---|---|
| `unique_ptr<T, D>` | **sim**, `D` é parâmetro de template | deleters diferentes = **tipos diferentes**; deleter com estado aumenta o `sizeof` |
| `shared_ptr<T>` | **não**, fica apagado no bloco de controle | políticas diferentes cabem no mesmo `vector<shared_ptr<T>>` |

### Contagem de referências e bloco de controle

`shared_ptr` guarda **dois** ponteiros: o do objeto e o de um **bloco de controle** — uma
struct na heap com o contador *forte* (quantos `shared_ptr`), o *fraco* (quantos
`weak_ptr`) e o deleter.

```
  shared_ptr p     shared_ptr q     weak_ptr w
  ┌─────┬─────┐    ┌─────┬─────┐    ┌─────┬─────┐
  │ obj │ctrl │    │ obj │ctrl │    │ obj │ctrl │
  └──┬──┴──┬──┘    └──┬──┴──┬──┘    └─────┴──┬──┘
     └──┐  └────────┐ │     └───────┐  ┌─────┘
        ▼           ▼ ▼             ▼  ▼
  ┌───────────┐   ┌───────────────────┐  strong → 0 : destrói o OBJETO
  │  Widget   │   │ strong=2  weak=1  │  weak   → 0 : libera o BLOCO
  └───────────┘   │ deleter           │
                  └───────────────────┘
```

São **dois eventos separados**. É por isso que um `weak_ptr` consegue responder "o objeto
ainda existe?" depois de ele morrer: o bloco continua lá, com `strong == 0`.

### `exception-safe`

Uma expressão é *exception-safe* quando nenhuma exceção no meio dela vaza um recurso. Em
`f(std::unique_ptr<A>(new A), g())` o compilador podia ordenar (1) `new A`, (2) `g()`,
(3) construir o `unique_ptr`: se `g()` lançasse no passo 2, o `A` já estava alocado e
ninguém tinha o endereço. `std::make_unique<A>()` faz alocação e construção em **uma
chamada só**, indivisível. (Desde o C++17 a ordem de avaliação é restrita e esse vazamento
não ocorre mais; os `make_` continuam preferíveis por intenção e, no `make_shared`, por
fazer uma alocação em vez de duas.)

### UB, polimorfismo e destrutor virtual

Um programa com **UB** (*undefined behavior*) não tem *nenhum* comportamento previsto:
pode funcionar hoje e travar depois do próximo `-O2`, porque o compilador tem permissão de
assumir que UB nunca acontece. Ponteiro nulo desreferenciado, memória já liberada e
`delete` duplo são UB — ver [`../undefined_behavior/README.md`](../undefined_behavior/README.md).
"Polimórfico" aqui é guardar um `Derivado` atrás de um ponteiro para `Base` e chamar
métodos `virtual`, que despacham em tempo de execução.

> ⚠ Uma factory que devolve `unique_ptr<Base>` **exige** `virtual ~Base()`. Sem isso,
> `delete` sobre `Base*` apontando para `Derivado` é UB — o destrutor do derivado não roda.
> O `Forma` de `demo_patterns.hpp` tem `virtual ~Forma() = default;` por esse motivo.

---

## Por que estudar isto?

O problema não é que escrever `delete` seja chato. É que **existe mais de um caminho de
saída** de uma função, e o `delete` só está em um deles.

```cpp
void ruim() {                              void bom() {
    Widget* w = new Widget();                  auto w = std::make_unique<Widget>();
    if (condicao) return;  // vazou            if (condicao) return;  // dtor roda
    pode_lancar();         // vazou            pode_lancar();         // dtor roda
    delete w;  // só o caminho feliz       }                          // dtor roda
}
```

| Sem smart pointers | Com smart pointers |
|---|---|
| a posse mora num comentário | a posse mora no **tipo**, e o compilador verifica |
| todo `return`/`throw` é um vazamento em potencial | o destrutor cobre todos os caminhos |
| `delete` duplo e *use-after-free* silenciosos | `unique_ptr` não é copiável; `shared_ptr` conta |

---

## Estrutura de arquivos

```
src/smart-pointers/
├── main.cpp            ← ponto de entrada; chama os 4 demos em ordem (cor gold)
├── demo_unique.hpp     ← 1 · escopo, move, passagem, T[], deleter, release/reset
├── demo_shared.hpp     ← 2 · ref-count, make_shared, containers, deleter,
│                              enable_shared_from_this, construtor de aliasing
├── demo_weak.hpp       ← 3 · lock/expired, ciclo que vaza, ciclo resolvido, observer
├── demo_patterns.hpp   ← 4 · regras de passagem, factory polimórfica, pimpl, RAII genérico
├── meson.build         ← alvo `smart_pointers`, depende de fmt
└── README.md           ← este arquivo
```

Todos os demos são *header-only* (`inline void run()` por namespace); só `main.cpp` vira
objeto.

---

## Os demos, um a um

### Demo 1 · `unique_ptr` — um dono, e só um

#### 1.1 O exemplo mínimo, completo e compilável

```cpp
#include <fmt/core.h>
#include <memory>

struct Recurso {
    int id;
    explicit Recurso(int i) : id(i) { fmt::print("  [Recurso #{}] construído\n", id); }
    ~Recurso()                      { fmt::print("  [Recurso #{}] destruído\n", id); }
    void usar() const               { fmt::print("  [Recurso #{}] em uso\n", id); }
};

int main() {
    {
        auto p = std::make_unique<Recurso>(10);   // aloca + constrói
        p->usar();
    }   // ← aqui, e em nenhum outro lugar, o destrutor roda
    fmt::print("  (bloco encerrado — recurso já foi liberado)\n");
}
```

Saída real (seção 1.1 do binário): `construído` → `em uso` → `destruído` →
`(bloco encerrado — recurso já foi liberado)`.

#### 1.2 Cópia é proibida; movimento é a transferência

Precisam ser dois blocos separados, porque o primeiro **não compila**:

```cpp
auto p = std::make_unique<Recurso>(20);
auto q = p;              // ❌ erro: unique_ptr não tem construtor de cópia
```

```cpp
auto p = std::make_unique<Recurso>(20);
auto q = std::move(p);   // ✔ transferência de posse
assert(!p);              // p ficou nulo — contrato do unique_ptr
```

O mesmo vale para funções: `apenas_usa(*p)` (recebe `const Recurso&`) deixa `p` intacto;
`toma_posse(std::move(p))` (recebe `unique_ptr<Recurso>` por valor) esvazia `p` e o objeto
morre dentro da função — saída `p válido após transferência? false`. A tabela completa
está em [Como passar smart pointers para funções](#como-passar-smart-pointers-para-funções).

#### 1.3 `unique_ptr<T[]>` — a especialização para arrays

```cpp
auto arr = std::make_unique<int[]>(4);   // o destrutor chama delete[]
for (int i = 0; i < 4; ++i) arr[i] = i * i;
```

> ⚠ `unique_ptr<T[]>` tem `operator[]` e **não** tem `operator->`. O contrário também
> mata: um `unique_ptr<T>` (sem `[]`) segurando memória de `new T[n]` chama `delete`, não
> `delete[]` — UB. Em código novo prefira `std::vector<T>`.

#### 1.4 Deleter customizado

```cpp
auto fechar = [](FILE* f) { if (f) std::fclose(f); };
std::unique_ptr<FILE, decltype(fechar)> arquivo(std::fopen("/dev/null", "w"), fechar);
```

> ⚠ `std::fopen` devolve `nullptr` quando falha e `std::fclose(nullptr)` é UB. Aqui isso
> nunca acontece: **o `unique_ptr` não chama o deleter se o ponteiro for nulo**. É
> exatamente por isso que o wrapper é mais seguro que o `fclose` escrito à mão.

Para um descritor de arquivo (`int`) o `unique_ptr` **não serve** — ele guarda ponteiros, e
`0`/`-1` são `fd` válidos. Escreva um guard com valor-sentinela:

```cpp
struct FdGuard {                       // ⚠ NÃO tente unique_ptr sobre um socket_fd:
    int fd = -1;                       //   T seria `int` e a dedução falha
    explicit FdGuard(int f) : fd(f) {}
    ~FdGuard() { if (fd >= 0) ::close(fd); }
    FdGuard(const FdGuard&) = delete;  FdGuard& operator=(const FdGuard&) = delete;
    FdGuard(FdGuard&& o) noexcept : fd(o.fd) { o.fd = -1; }
};
```

#### 1.5 `release()` e `reset()`

`p.release()` **não** destrói: devolve o ponteiro cru, zera `p` e transfere o dever do
`delete` para você — é a única forma de vazar com `unique_ptr`. `p.reset()` destrói e zera;
`p.reset(novo)` destrói o antigo e passa a possuir `novo`.

---

### Demo 2 · `shared_ptr` — posse compartilhada

#### 2.1 O contador e a inspeção

Cada cópia incrementa o contador forte, cada destruição decrementa; ao chegar a zero, o
destrutor do objeto roda.

```
  use_count após make_shared: 1   |  p.get()       → ponteiro cru, não transfere posse
  use_count com p2+p3: 3          |  p.use_count() → quantos shared_ptr fortes existem
  use_count após sair do bloco: 1 → [Nodo 'A'] destruído
```

> ⚠ `p.unique()` foi **depreciado no C++17 e removido no C++20**. Use `p.use_count() == 1`,
> ciente de que em código multithread esse teste é uma corrida: o valor pode mudar entre a
> leitura e o `if`.

#### 2.2 `make_shared` vs `shared_ptr<T>(new T)` — o trade-off inteiro

Medido com `operator new` global instrumentado e `struct W { int a; };`:

```
make_shared<W>()      : 1 alocação,  24 bytes   ← objeto + bloco de controle juntos
shared_ptr<W>(new W)  : 2 alocações, 28 bytes   ← 4 (objeto) + 24 (bloco)
make_unique<W>()      : 1 alocação,   4 bytes
```

`make_shared` ganha em quase tudo: metade das chamadas ao alocador e o objeto na mesma
linha de cache do contador.

> ⚠ Mas ele **não** é puramente superior. Como objeto e bloco moram na mesma alocação, os
> bytes só voltam ao sistema quando o **último `weak_ptr`** morre — não quando o último
> `shared_ptr` morre. O destrutor do objeto roda na hora certa; a memória fica retida. Com
> objeto grande e `weak_ptr` de vida longa (um cache), `shared_ptr<T>(new T)` — duas
> alocações — devolve a memória do objeto imediatamente. `make_shared` também não aceita
> deleter customizado.

#### 2.3 Deleter customizado, que aqui **não** entra no tipo

```cpp
auto deleter = [](Nodo* p) { /* … */ delete p; };
std::shared_ptr<Nodo> p(new Nodo("D"), deleter);   // o tipo continua shared_ptr<Nodo>
```

O oposto do `unique_ptr` (1.4) — e o motivo de caberem, no mesmo `vector<shared_ptr<Nodo>>`,
objetos com políticas de liberação diferentes. O demo 2.3 do binário mostra também que duas
entradas do mesmo ponteiro num `vector` contam duas vezes (`use_count() = 3`).

#### 2.4 `enable_shared_from_this`

Um objeto que precisa entregar um `shared_ptr` de si mesmo herda de
`std::enable_shared_from_this<T>` e chama `shared_from_this()`:

```cpp
struct Observavel : std::enable_shared_from_this<Observavel> {
    std::shared_ptr<Observavel> get_self() { return shared_from_this(); }
};
```

```
  use_count original: 1 → use_count com self: 2 → após destruir self: 1
```

> ⚠ **Nunca** faça `return std::shared_ptr<T>(this);` — isso cria um **segundo** bloco de
> controle sobre o mesmo objeto; cada um chega a zero na sua hora e o objeto é destruído
> **duas vezes**.
>
> ⚠ `shared_from_this()` só funciona se o objeto já for gerenciado por um `shared_ptr`. Num
> objeto da stack, ou dentro do próprio construtor, lança `std::bad_weak_ptr`.

#### 2.5 Construtor de aliasing

```cpp
auto carro = std::make_shared<Carro>();
std::shared_ptr<Motor> motor_ptr(carro, &carro->motor);  // aponta pro membro,
carro.reset();                                           // mantém o TODO vivo
motor_ptr->rpm;   // 3000 — o Carro continua vivo por causa de motor_ptr
```

Saída: `use_count() = 2` → após `reset`, `1`; `rpm` ainda acessível; `[Carro 'Fusca']
destruído` só quando `motor_ptr` sai de escopo. É aqui que os dois ponteiros do
`shared_ptr` (por isso `sizeof == 16`) ficam visíveis: o construtor de aliasing desacopla o
que se desreferencia do que se conta.

---

### Demo 3 · `weak_ptr` — observar sem prender

#### 3.1 `lock()` e `expired()`

`weak_ptr` não incrementa o contador forte, logo não mantém nada vivo. Para usar o objeto é
obrigatório promover a `shared_ptr` com `lock()`:

```cpp
if (auto forte = observador.lock()) {   // shared_ptr vazio se o objeto já morreu
    forte->usar();                      // enquanto `forte` viver, o objeto vive
}
```

```
  forte.use_count() = 1 (weak não conta)
  observador.expired() = true
  lock() retornou nullptr — objeto foi destruído
```

> ⚠ `weak_ptr` **tem** um `use_count()` — o que ele devolve é a contagem de `shared_ptr`
> fortes, não de `weak_ptr`; `expired()` é exatamente `use_count() == 0`.
>
> ⚠ Nunca escreva `if (!w.expired()) w.lock()->usar();`. Entre o teste e o uso, outra
> thread pode zerar o contador. `lock()` testa e promove atomicamente — é para isso que ele
> existe.

#### 3.2 O problema: ciclo de `shared_ptr` vaza

```cpp
struct NodoComCiclo { std::shared_ptr<NodoComCiclo> proximo; };
auto a = std::make_shared<NodoComCiclo>("A");
auto b = std::make_shared<NodoComCiclo>("B");
a->proximo = b;   // use_count(b) = 2
b->proximo = a;   // use_count(a) = 2
```

Quando `a` e `b` saem de escopo os contadores caem de 2 para 1, nunca para 0. Na saída, os
`construído` aparecem e os `destruído` **não**:

`[NodoComCiclo 'A-ciclo'] construído`, `use_count(a) = 2`, idem para `B` — e nenhum
`destruído`. `shared_ptr` é *reference counting*, não *garbage collection*: contadores não
detectam ciclos. É a única classe de vazamento que os smart pointers não resolvem sozinhos.

#### 3.3 A solução: uma das arestas vira `weak_ptr`

```cpp
struct NodoSemCiclo { std::weak_ptr<NodoSemCiclo> proximo; };   // use_count(a) = 1
```

Agora a saída traz os dois `destruído`. Regra prática: numa relação pai↔filho, o **pai possui** o filho (`unique_ptr`/`shared_ptr`
descendo) e o **filho observa** o pai (`weak_ptr` subindo). O demo 3.4 aplica a mesma ideia
ao padrão Observer: o `Subject` guarda `vector<weak_ptr<Observer>>` e remove os mortos a
cada notificação (`(observer morto removido)`), sem que o observador precise se desregistrar.

---

### Demo 4 · Padrões

#### 4.1 Factory polimórfica

```cpp
std::unique_ptr<Forma> criar_forma(const std::string& tipo) {
    if (tipo == "circulo")   return std::make_unique<Circulo>(5.0);
    if (tipo == "retangulo") return std::make_unique<Retangulo>(4.0, 3.0);
    return nullptr;                       // "não sei criar isso"
}
```

`unique_ptr` no retorno diz ao chamador, **no tipo**, que ele recebeu a posse. Funciona com
herança porque `unique_ptr<Derivado>` converte implicitamente para `unique_ptr<Base>` —
desde que `Base` tenha destrutor virtual.

#### 4.2 Pimpl

Esconder os membros privados atrás de um `unique_ptr<Impl>` incompleto no cabeçalho: quem
inclui o `.hpp` não recompila quando a implementação muda.

> ⚠ O destrutor **precisa** ser declarado no `.hpp` e definido onde `Impl` é completo
> (`Conexao::~Conexao() = default;`). Se o compilador gerá-lo no cabeçalho, ele instanciará
> `delete pimpl_` com `Impl` incompleto — UB, e o GCC só avisa com `-Wdelete-incomplete`.
> Pelo mesmo motivo `Conexao` declara os movimentos explicitamente.

#### 4.3 RAII genérico para APIs C

`std::unique_ptr<HandleC, decltype(fechar)> h(abrir_handle(99), fechar);` embrulha qualquer
handle de biblioteca C. Um lambda **sem captura** como deleter é grátis: o `sizeof` do
`unique_ptr` continua 8 (*empty base optimization*); já `void(*)(HandleC*)` custa 8 bytes a
mais — ver a tabela de tamanhos nos pré-requisitos.

#### 4.4 Como passar smart pointers para funções

Portado de `demo_patterns.hpp` (seção 1):

| Assinatura | O que ela declara |
|---|---|
| `f(Widget&)` | só usa o objeto; não fala de posse. **Prefira esta.** |
| `f(const Widget&)` | só lê o objeto |
| `f(Widget*)` | só usa, e aceita ausência (`nullptr`) |
| `f(unique_ptr<Widget>)` | **toma** a posse exclusiva; o caller precisa de `std::move` |
| `f(unique_ptr<Widget>&)` | vai **reatribuir** o ponteiro do caller |
| `f(shared_ptr<Widget>)` | vai **guardar** uma cópia; participa da posse |
| `f(const shared_ptr<Widget>&)` | pode ou não copiar; observa sem custo obrigatório |

**A regra**: se a função só *usa* o objeto, receba `T&` ou `const T&`. Passe smart pointer
apenas quando a função mexe na posse. Receber `shared_ptr` por valor sem guardar cópia paga
um par de operações atômicas por chamada, à toa.

---

## Custo e thread-safety (medido)

`g++ -std=c++20 -O2`, 50 milhões de iterações, x86-64, GCC 13. Números idênticos com e sem
`-pthread`:

| operação | custo | comparação |
|---|---|---|
| copiar um `T*` | **0,23 ns** | linha de base |
| copiar um `shared_ptr<T>` | **4,5 ns** | ~19× mais caro |
| passar `const shared_ptr<T>&` | **0,5–1,0 ns** | nenhum incremento |
| `unique_ptr<T>` com deleter vazio | idêntico a `T*` | overhead **zero** |

1. **`unique_ptr` é de graça** — mesmo tamanho, mesmo código gerado; nenhum motivo de
   performance para preferir `T*`.
2. **Copiar `shared_ptr` custa uma operação atômica** — daí a regra de passar
   `const shared_ptr<T>&` (ou `T&`) quando você só observa.
3. **Só o contador é thread-safe; o objeto não.** Escrever nos **membros** do objeto de
   duas threads continua sendo uma data race — ver
   [`../concurrency2/README.md`](../concurrency2/README.md).

---

## Como validar

```bash
make configure && make build && make install && ./dist/bin/smart_pointers
```

Todo `construído` deve ter um `destruído` correspondente — **exceto** na seção 3.2, onde o
vazamento é intencional. Para *provar* isso:

```bash
FMT=$(PKG_CONFIG_PATH=$PWD/build pkg-config --cflags --libs fmt)
g++ -std=c++20 -g -fsanitize=address,undefined src/smart-pointers/main.cpp -o /tmp/sp $FMT
LD_LIBRARY_PATH=$(PKG_CONFIG_PATH=$PWD/build pkg-config --variable=libdir fmt) /tmp/sp
```

Saída real (resumida), com código de saída **1**:

```
==119266==ERROR: LeakSanitizer: detected memory leaks
Indirect leak of 64 byte(s) in 1 object(s) allocated from:
    #9 demo_weak::demo_ciclo_com_leak() src/smart-pointers/demo_weak.hpp:85
SUMMARY: AddressSanitizer: 128 byte(s) leaked in 2 allocation(s).
```

128 bytes = dois blocos de 64 (objeto + bloco de controle de cada `NodoComCiclo`, juntos
porque `make_shared` faz uma alocação só). É exatamente o ciclo da seção 3.2 — e nada mais:
nenhum outro demo vaza.

> ⚠ O programa termina imprimindo **"✓ Todos os demos concluídos sem leaks!"**. Essa frase
> vem do `main.cpp` e **não é verdade**: o demo 3.2 vaza de propósito, como o
> LeakSanitizer acaba de provar. O sanitizer é a fonte da verdade, não a mensagem final.
> Para checar o resto isoladamente, comente `demo_ciclo_com_leak();` em
> `demo_weak.hpp::run()`: a saída fica limpa e o código de retorno vira 0.

---

## Referências

### Neste repositório

- [`../memory/README.md`](../memory/README.md) — o que acontece **abaixo** do smart
  pointer: `new`/`delete` por dentro, pool, arena, `std::pmr`.
- [`../value_semantics/README.md`](../value_semantics/README.md) — regra dos 0/3/5; por que
  um membro `unique_ptr` torna sua classe *move-only*.
- [`../undefined_behavior/README.md`](../undefined_behavior/README.md) — o catálogo de UB.
- [`../layout/README.md`](../layout/README.md) — `sizeof`, alinhamento, *empty base optimization*.
- [`../type_erasure/README.md`](../type_erasure/README.md) — como o `shared_ptr` apaga o
  tipo do deleter dentro do bloco de controle.
- [`../concurrency2/README.md`](../concurrency2/README.md) — data race e por que "o contador
  é atômico" não torna o objeto seguro.

### Fora

- cppreference — [`<memory>`](https://en.cppreference.com/w/cpp/header/memory),
  [`unique_ptr`](https://en.cppreference.com/w/cpp/memory/unique_ptr),
  [`shared_ptr`](https://en.cppreference.com/w/cpp/memory/shared_ptr),
  [`weak_ptr`](https://en.cppreference.com/w/cpp/memory/weak_ptr).
- C++ Core Guidelines, *Resource management*: R.20 (smart pointer para possuir), R.21
  (prefira `unique_ptr`), R.22/R.23 (`make_shared`/`make_unique`), R.24 (`weak_ptr` para
  quebrar ciclos), R.30 (só passe smart pointer para falar de posse) —
  <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-resource>
- Scott Meyers, *Effective Modern C++*, Capítulo 4 ("Smart Pointers"), itens 18–22.
- Herb Sutter, *GotW #91 — Smart Pointer Parameters*:
  <https://herbsutter.com/2013/06/05/gotw-91-solution-smart-pointer-parameters/>
