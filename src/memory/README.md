# memory — alocadores de memória, do `new` cru ao `std::pmr`

## O que é este projeto?

Quatro demos que mostram **quem dá a memória ao seu programa e quanto isso custa**: o que
`new`/`delete` realmente fazem, como escrever um *pool* e uma *arena* à mão, e como o C++17
padronizou tudo isso em `std::pmr`.

```bash
make build              # compila o repositório (Conan + Meson + Ninja)
./dist/bin/memory       # roda os 4 demos, com benchmarks, em sequência

# recompilar só este alvo, depois de um `make configure`:
source build/conanbuild.sh && meson compile -C build memory
```

A última linha da saída, se tudo correu bem: `✓ Todos os demos de memória concluídos!`

> ⚠ Não use `meson setup dist`: aqui `dist/` já é o diretório de **instalação** (`dist/bin`,
> `dist/lib`), e apontar o Meson para ele corrompe a árvore instalada. O diretório de build é
> `build/`, criado por `make configure` — que também resolve o `fmt` via Conan, sem o qual o
> `dependency('fmt')` do Meson falha.

---

## Antes de começar: os oito conceitos que o resto do texto usa

Esta é a seção mais importante do README. Nada aqui pressupõe mais do que C++ básico —
ponteiros, referências, construtores e destrutores.

### 1. Stack e heap

Todo programa tem duas fontes de memória, e elas funcionam de maneiras completamente
diferentes.

```
     STACK (pilha)                            HEAP (monte)
  endereços altos ↑                   ┌──────────────────────────────┐
  ┌───────────────────────┐           │ [usado 40B][livre 16B][usado │
  │ main()   locais       │           │  120B][livre 8B][usado 64B]  │
  ├───────────────────────┤           └──────────────────────────────┘
  │ f()      locais       │            blocos de tamanhos variados,
  ├───────────────────────┤            em qualquer ordem; quem sabe
  │ g()      locais       │ ← RSP      o que está livre é uma
  └───────────────────────┘            estrutura de dados do malloc
  cresce para baixo ↓
```

- **Stack**: um único registrador (`RSP`) marca o topo. Alocar é **subtrair** dele; liberar é
  somar. Custa uma instrução, a ordem é rigorosamente LIFO, e o tamanho total é fixo
  (tipicamente 8 MB por thread no Linux). Toda variável local vive aqui.
- **Heap**: uma região que cresce sob demanda. Alocar exige **procurar** um bloco livre do
  tamanho pedido, marcá-lo como usado e devolver o endereço; liberar exige devolvê-lo à
  contabilidade e, de preferência, fundi-lo com os vizinhos livres. É isso que `malloc` e
  `free` fazem, e é por isso que custam muito mais que uma subtração.

Medido pelo próprio demo (seção 1.5), em build **debug** (`-O0`):

```
  stack  (100000 ints locais): 73µs
  heap   (100000 new/delete):  1010µs        ← ~14× mais caro
```

> ⚠ Uma correção importante contra um mito comum: **`new` normalmente não faz syscall**. Uma
> *syscall* é uma chamada ao kernel — cara, na casa das centenas de nanossegundos. O `malloc`
> da glibc mantém uma arena já mapeada em espaço de usuário e serve a maioria dos pedidos de
> lá, sem entrar no kernel; só chama `brk`/`mmap` quando precisa **crescer**. O custo do
> `new` é a busca, a contabilidade e o lock — não o kernel.

### 2. O que é um alocador

Os quatro demos parecem quatro assuntos. São o **mesmo** assunto: uma interface de dois
métodos, com quatro políticas diferentes.

```cpp
void* allocate(std::size_t bytes, std::size_t alinhamento);
void  deallocate(void* p, std::size_t bytes, std::size_t alinhamento);
```

"Dê-me `bytes` bytes começando num endereço múltiplo de `alinhamento`" e "tome de volta".
Nada mais. O que muda de um alocador para o outro é **quem escolhe o bloco, e quando ele
volta**:

| política | custo de alocar | custo de liberar | libera individualmente? |
|---|---|---|---|
| `malloc` / `new` | busca em estruturas internas, variável | devolve e funde vizinhos | ✓ |
| **pool** (blocos fixos) | O(1) — tira o topo da free list | O(1) — devolve ao topo | ✓ |
| **arena** (bump) | O(1) — soma num cursor | **não existe**; só `reset()` total | ✗ |
| `std::pmr` | qualquer uma das acima, escolhida em runtime | idem | depende |

### 3. Alinhamento

Pré-requisito de tudo o que vem depois, e a parte mais opaca se ninguém explicar.

Um objeto de alinhamento *A* só pode começar num endereço **múltiplo de A**. Não é capricho:
o processador lê a memória em blocos de tamanho fixo, e um objeto que atravessa a fronteira
de um bloco exige duas leituras — no x86-64 isso é lentidão; em ARM e em instruções SIMD,
pode ser uma falha de hardware.

```
alignof(double) == 8, então um double pode começar em 0, 8, 16, 24...

  endereço:  0    4    8   12   16
             ├────┼────┼────┼────┤
     bom:    [    double   ]            ← começa em 0: uma leitura
     ruim:        [    double   ]       ← começa em 4: atravessa a fronteira
```

Nesta máquina (x86-64, GCC 13), `sizeof`/`alignof` medidos:

| tipo | `sizeof` | `alignof` |
|---|---|---|
| `char` | 1 | 1 |
| `short` | 2 | 2 |
| `int` | 4 | 4 |
| `double` | 8 | 8 |
| `void*` | 8 | 8 |
| `std::max_align_t` | 32 | **16** |

`std::max_align_t` é o tipo com o maior alinhamento que o compilador exige "naturalmente" —
`malloc` garante devolver memória alinhada para ele, e por isso qualquer objeto comum cabe no
que o `malloc` deu. `alignas(N)` força um alinhamento maior; `alignof(T)` pergunta qual é.

**A conta de arredondar.** A arena do Demo 3 tem esta linha, e ela parece hieróglifo:

```cpp
return (n + alinhamento - 1) & ~(alinhamento - 1);
```

Ela é "arredonde `n` para cima até o próximo múltiplo de `alinhamento`" — e só funciona
porque o alinhamento é sempre uma **potência de 2**. Com `n = 5`, `a = 8`:

```
  n + a - 1  =  5 + 7   = 12   =  0000 1100
  a - 1      =      7          =  0000 0111
  ~(a - 1)   =                 =  1111 1000    ← zera os 3 bits de baixo
  12 & ~7                      =  0000 1000  = 8   ✓ próximo múltiplo de 8
```

### 4. `placement new`

`new T(args)` faz **duas** coisas: pede bytes e constrói. `placement new` faz só a segunda:

```cpp
#include <new>                             // ← necessário para placement new

alignas(Vec3) std::byte buffer[sizeof(Vec3)];   // bytes crus, na stack
Vec3* v = new (buffer) Vec3{1, 2, 3};           // CONSTRÓI aqui; não aloca nada
v->~Vec3();                                     // destrói à mão; não libera nada
```

Todo alocador customizado precisa dele: `alloc()` devolve **memória crua**, e alguém tem de
rodar o construtor nela.

> ⚠ Com `placement new`, `delete v` é **erro grave** — tentaria devolver ao `malloc` memória
> que nunca veio dele (aqui, a stack). O par correto é `new (p) T` ↔ `p->~T()`.

### 5. Free list — a lista que mora dentro da memória livre

O truque central do pool, e a parte que costuma faltar nas explicações: a lista de blocos
livres **não gasta memória extra**. Ela é gravada *dentro* dos blocos que estão livres —
afinal, se estão livres, o conteúdo deles não interessa a ninguém.

```
  pool com 4 blocos, todos livres:
  ┌──────────┬──────────┬──────────┬──────────┐
  │ →blk1    │ →blk2    │ →blk3    │ nullptr  │      livre_ ─▶ blk0
  └──────────┴──────────┴──────────┴──────────┘

  depois de dois alloc():
  ┌──────────┬──────────┬──────────┬──────────┐
  │ EM USO   │ EM USO   │ →blk3    │ nullptr  │      livre_ ─▶ blk2
  └──────────┴──────────┴──────────┴──────────┘
```

Alocar é `livre_ = *livre_` — uma leitura e uma escrita. Liberar é o inverso. Daí o O(1).

> ⚠ Consequência direta: **cada bloco precisa ter pelo menos `sizeof(void*)` bytes**, senão o
> ponteiro da lista não cabe e transborda no bloco vizinho. É exatamente o que a constante
> `BLOCO` do Demo 2 resolve.

### 6. `reinterpret_cast` e tempo de vida

O pool e a arena fazem `reinterpret_cast<void**>(bloco)` para gravar a free list, e
`static_cast<T*>` para devolver o bloco. O que autoriza isso é o fato de a memória estar
**crua**: nenhum objeto vive ali no momento do cast. Enquanto o bloco está na free list ele é
um `void*`; depois do `placement new` ele é um `T` e o ponteiro anterior deixa de valer.
Misturar as duas visões — ler o `next` de um bloco em uso, ou usar um `T*` de um bloco já
devolvido (*use-after-free*) — é comportamento indefinido.

### 7. Fragmentação: externa e interna

São duas coisas diferentes, e trocar uma pela outra é o que o pool faz.

```
  EXTERNA — há bytes livres suficientes, mas não CONTÍGUOS:
    [ A:20 ][ livre:20 ][ B:20 ][ livre:60 ]   →  alocar 70 bytes FALHA
                                                  (80 livres, o maior buraco tem 60)

  INTERNA — o bloco entregue é maior que o pedido; a sobra se perde:
    pool de blocos de 8 bytes guardando um int (4 bytes):
    [ int:4 | 4 desperdiçados ]                →  50% de perda
```

`malloc` sofre da externa; o pool a elimina (todos os blocos são iguais, qualquer um serve)
ao custo de introduzir a interna.

### 8. Linha de cache e localidade

A RAM não é lida byte a byte: o processador traz sempre uma **linha de cache** inteira — 64
bytes nesta máquina (`getconf LEVEL1_DCACHE_LINESIZE`). Percorrer 16 `int` contíguos custa
uma ida à memória; percorrer 16 `int` espalhados pela heap custa até 16. É por isso que
"pool e arena são cache-friendly" não é slogan: os objetos saem contíguos porque foram
tirados, em ordem, de um único bloco grande.

---

## Por que estudar isto?

Porque `new`/`delete` é uma política de propósito geral, e propósito geral significa "boa em
média, ótima em nada". Estes são os números que o próprio subprojeto imprime nesta máquina,
em build **debug** (`-O0`, que é o padrão do `make build` aqui):

```
1.5  stack  (100000 ints locais):   73µs      heap (100000 new/delete):  1010µs
2.2  heap   (50000 new/delete):   2340µs      pool (50000 alloc/free):    757µs   ← 3,1×
3.4  heap   (100000 new/delete):  4900µs      arena (100000 criar+reset): 3000µs  ← 1,6×
4.5  std::vector (heap):           690µs      pmr::vector (monotonic):   4000µs   ← 5,8× PIOR
```

Três lições, e a terceira é a mais valiosa:

1. Alocar na heap custa uma ordem de grandeza mais que não alocar.
2. Trocar a política por uma especializada dá um ganho real, mas **modesto**: 3× no pool, não
   100×. Ganhos de duas ordens de grandeza aparecem quando o alocador também conserta a
   localidade de cache num percurso, não no microbenchmark de alocar e liberar.
3. **A quarta linha é uma medição contraintuitiva, e está certa.** Ver a explicação no
   Demo 4 — em resumo: é um artefato de compilar sem otimização, e com `-O2` a diferença
   desaparece (medido: 117µs contra 114µs). Nunca conclua nada de um benchmark `-O0`.

| sem alocador customizado | com |
|---|---|
| custo por alocação variável e imprevisível | O(1) previsível — importa em tempo real |
| fragmentação externa cresce com o tempo | pool: zero; arena: zero |
| objetos espalhados pela heap, cache frio | objetos contíguos, cache quente |
| liberar N objetos custa N chamadas | arena: `reset()` custa uma soma |
| `malloc` é thread-safe ⇒ paga lock sempre | pool/arena por thread ⇒ sem lock |

---

## Os dois tipos de alocador em C++

Antes dos demos, uma distinção que confunde muita gente porque a linguagem tem **duas**
interfaces de alocador, com propósitos diferentes.

**1. O `Allocator` clássico (C++98)** — um parâmetro de *template*:

```cpp
template <class T> struct MeuAlloc {
    using value_type = T;
    T*   allocate(std::size_t n);
    void deallocate(T* p, std::size_t n);
};
std::vector<int, MeuAlloc<int>> v;
```

Rápido (tudo resolvido em compile-time), mas o alocador **entra no tipo**:
`std::vector<int, MeuAlloc<int>>` e `std::vector<int>` são tipos **incompatíveis**. Uma
função que recebe `const std::vector<int>&` não aceita o primeiro.

**2. O `memory_resource` do `std::pmr` (C++17)** — um ponteiro para uma **base virtual**:

```cpp
std::pmr::vector<int> v(&algum_resource);
// std::pmr::vector<int>  ≡  std::vector<int, std::pmr::polymorphic_allocator<int>>
```

O tipo do container é sempre o mesmo, seja qual for o resource por trás; a escolha é feita em
**runtime**, por despacho virtual. É isso que a palavra "**polimórfico**" significa em
"alocadores polimórficos": não tem nada a ver com herança dos *seus* tipos — é o alocador que
é acessado por uma base virtual. O preço é uma chamada indireta por alocação.

---

## Estrutura de arquivos

```
memory/
├── README.md / meson.build   ← este arquivo; executable('memory') → dist/bin/memory
├── main.cpp                  ← chama os 4 run() em ordem, com cabeçalho colorido
├── demo_new_delete.hpp       ← 1. as duas fases de new/delete, placement new, new[], nothrow
├── demo_pool_allocator.hpp   ← 2. PoolAllocator<T,N> + PoolRaii, com benchmark
├── demo_arena_allocator.hpp  ← 3. Arena (bump allocator) + padrão de frame, com benchmark
└── demo_pmr.hpp              ← 4. std::pmr: monotonic, fallback, pool, resource próprio
```

Todos são *header-only* (`#pragma once` + `inline`) e expõem um `inline void run()` no
próprio namespace (`demo_pool_allocator::run()`, ...).

## Demo 1 — `new`/`delete` por dentro (`demo_new_delete.hpp`)

**As duas fases.** `new` e `delete` não são funções: são operadores que fazem duas coisas
cada um.

```cpp
// T* p = new T(args);   equivale a:
void* mem = operator new(sizeof(T));   // 1. aloca bytes (pode lançar std::bad_alloc)
T*    p   = new (mem) T(args);         // 2. constrói (placement new)

// delete p;             equivale a:
p->~T();                               // 1. destrói
operator delete(mem);                  // 2. libera os bytes
```

`operator new` e `operator delete` são funções comuns, e **substituíveis**: defini-las no seu
programa faz toda alocação passar por elas — é o truque de qualquer detector de vazamento
caseiro. O demo faz a mesma coisa de forma mais didática, com `meu_malloc`/`meu_free`
explícitos, e imprime as três fases uma a uma:

```
  Fase 1: alocação de bytes (operator new)
    36 bytes alocados em 0x561e3d16f2c0
  Fase 2: construção (placement new)
    [ctor] Recurso id=42
  Fase 3: destruição manual + liberação
    [dtor] Recurso id=42
```

**`placement new` na prática** (seção 1.2). Um `Vec3` construído dentro de um buffer da
stack, sem tocar na heap — saída real: `buffer na stack: 12 bytes`, `alignof(Vec3) = 4`.

**`new[]` e `delete[]`** (seção 1.3). São operadores *diferentes* de `new`/`delete`.

```cpp
int* arr = new int[5]{1,2,3,4,5};
delete[] arr;      // ✓ correto
```

> ⚠ Usar `delete` em memória vinda de `new[]` (ou vice-versa) é **comportamento indefinido**.
> Para tipos com destrutor, `new[]` guarda a contagem de elementos num cabeçalho logo antes
> do array, e só `delete[]` sabe lê-lo; `delete` chamaria um destrutor só e liberaria a partir
> do endereço errado. Em código moderno, prefira `std::vector` ou `std::unique_ptr<T[]>` — que
> chama `delete[]` sozinho — e o problema deixa de existir. O demo mostra as três formas.

**`nothrow new`** (seção 1.4). Por padrão, `new` lança `std::bad_alloc` quando não consegue
memória. `new (std::nothrow)` devolve `nullptr` em vez de lançar:

```cpp
int* p = new (std::nothrow) int[std::size_t(1) << 40];   // 1 TB
if (!p) { /* falhou graciosamente */ }
```

Útil em código que não pode usar exceções (embarcado, kernel, jogos com `-fno-exceptions`).

> ⚠ Este demo tem uma consequência prática: rodar o binário sob AddressSanitizer **aborta**
> nesse pedido de 1 TB, porque o ASan recusa alocações acima do limite dele em vez de devolver
> `nullptr`. A seção **Como validar** mostra a variável de ambiente que resolve.

**Stack × heap medido** (seção 1.5): os `73µs` contra `1010µs` da seção "Por que estudar
isto?" saem daqui.

## Demo 2 — Pool allocator (`demo_pool_allocator.hpp`)

**O problema.** Um jogo cria e destrói milhares de partículas por frame; uma árvore aloca e
libera milhares de nós. Todos do **mesmo tamanho**. Com `malloc`, cada um paga busca,
metadados (~16 bytes por bloco na glibc) e lock — e ainda fragmenta a heap.

**A solução.** Um bloco grande, dividido em N pedaços iguais, com uma free list por dentro
(conceito 5). O código real:

```cpp
template <typename T, std::size_t N>
class PoolAllocator {
    // ⚠ cada bloco precisa caber um ponteiro da free list
    static constexpr std::size_t BLOCO = sizeof(T) >= sizeof(void*) ? sizeof(T) : sizeof(void*);
public:
    PoolAllocator() {
        pool_ = static_cast<std::byte*>(std::aligned_alloc(alignof(T), N * BLOCO));
        livre_ = pool_;
        for (std::size_t i = 0; i < N - 1; ++i)             // encadeia os blocos
            *reinterpret_cast<void**>(pool_ + i * BLOCO) = pool_ + (i + 1) * BLOCO;
        *reinterpret_cast<void**>(pool_ + (N - 1) * BLOCO) = nullptr;
    }
    ~PoolAllocator() { std::free(pool_); }
    PoolAllocator(const PoolAllocator&) = delete;            // dono exclusivo do pool

    T* alloc() {                                             // O(1) — memória CRUA
        if (!livre_) return nullptr;
        void* bloco = livre_;
        livre_ = *reinterpret_cast<void**>(livre_);
        ++em_uso_;
        return static_cast<T*>(bloco);
    }
    void free(T* ptr) {                                      // O(1) — devolve à lista
        *reinterpret_cast<void**>(ptr) = livre_;
        livre_ = ptr;
        --em_uso_;
    }
};
```

Repare no que `alloc()` **não** faz: não chama construtor. E `free()` não chama destrutor. A
API é de **memória**, e a construção fica com quem chama — `new (p) T(...)` e `p->~T()`. Essa
simetria é o que torna a classe segura de compor; um `free()` que destruísse escondido
tornaria um par `alloc`/`free` puro num destrutor rodando sobre um objeto que nunca existiu.

> ⚠ **Por que `BLOCO` existe.** Sem ele, um `PoolAllocator<int, N>` num sistema de 64 bits
> teria blocos de 4 bytes, e gravar o ponteiro de 8 bytes da free list no primeiro bloco
> corromperia o segundo. A saída real do demo mostra a regra em ação: um pool de `Conexao`
> (que tem só um `int`) imprime **`4 blocos de 8 bytes = 32 bytes totais`** — 8, não 4.

> ⚠ `std::aligned_alloc` exige, por padrão, que o tamanho seja **múltiplo do alinhamento**.
> Vale para todos os tipos dos demos; para um `T` exótico em que não valha, arredonde `BLOCO`
> para cima com a conta do conceito 3 antes de passar adiante.

Saída real do pool básico, com `Particula` (4 `float` + `float` + `int` = 24 bytes):

```
    pool criado: 8 blocos de 24 bytes = 192 bytes totais
  em uso: 3, disponíveis: 5
  após free(p2) — disponíveis: 6
  p4 == antigo p2? true (reutilização de bloco)
```

`p4 == p2` é a free list funcionando: o bloco devolvido volta para o topo e é o próximo a
sair. **Benchmark** (seção 2.2), 50 000 nós de árvore:

```
  heap  (50000 new/delete): 2340µs
  pool  (50000 alloc/free):  757µs      ← 3,1× mais rápido
```

**RAII em cima do pool** (seção 2.3). Memória crua é perigosa; o demo embrulha o par em um
`unique_ptr` com *deleter* customizado, e aí o pool fica seguro por construção:

```cpp
template <typename... Args>
auto acquire(Args&&... args) {
    T* ptr = pool_.alloc();
    if (!ptr) throw std::bad_alloc{};
    new (ptr) T(std::forward<Args>(args)...);
    auto deleter = [this](T* p) { p->~T(); pool_.free(p); };   // destrói E devolve
    return std::unique_ptr<T, decltype(deleter)>(ptr, deleter);
}
```

## Demo 3 — Arena / bump allocator (`demo_arena_allocator.hpp`)

**A ideia mais simples possível.** Um bloco grande e um cursor. Alocar é somar; liberar
individualmente **não existe**.

```
  ┌───────────────────────────────────────────────────────────┐
  │ obj A │ obj B │ obj C │                                   │
  └───────────────────────────────────────────────────────────┘
  base_                    ↑ usado_                     capacidade_

  alloc(n): alinha usado_, devolve base_+usado_, e faz usado_ += n
  reset():  usado_ = 0     ← "libera" tudo em uma instrução
```

```cpp
void* alloc(std::size_t tamanho, std::size_t alinhamento = alignof(std::max_align_t)) {
    std::size_t offset = alinhado(usado_, alinhamento);   // a conta do conceito 3
    if (offset + tamanho > capacidade_) return nullptr;   // ⚠ devolve nullptr, não lança
    void* ptr = base_ + offset;
    usado_ = offset + tamanho;
    return ptr;
}
void reset() { usado_ = 0; }
```

O demo usa `std::malloc` no construtor e devolve `nullptr` quando esgota (imprimindo
`[Arena] ESGOTADA`), em vez de lançar `std::bad_alloc` — quem chama precisa **testar o
retorno**. E há um `criar<T>(args...)` que faz `alloc` + `placement new` de uma vez.

> ⚠ **`reset()` NÃO chama destrutores.** Ele só recua o cursor. Uma `std::string` ou um
> `std::vector` criados na arena têm buffers próprios na heap, e esses buffers **vazam** no
> `reset()`. Ou você garante que só tipos trivialmente destrutíveis entram —
> `static_assert(std::is_trivially_destructible_v<T>)` em `criar<T>()` — ou a arena registra
> os destrutores numa lista e os roda no `reset()`. O demo fica no primeiro caminho: os tipos
> que ele aloca (`No`, `DrawCall`, `Item`) são todos triviais de destruir, e o código diz isso
> em comentário.

> ⚠ Alinhar apenas o **offset** só garante o resultado se `base_` já estiver alinhado ao valor
> pedido. `malloc` garante `alignof(std::max_align_t)` — 16 bytes aqui — o que cobre qualquer
> tipo comum, mas **não** um `alignas(64)`. Para tipos super-alinhados, alinhe também o
> ponteiro-base (`std::align` ou `aligned_alloc`).

Saída real. Na seção 3.1, três alocações (`int`, `float`, 32 bytes) consomem **48** bytes de
1024 — mais que os 40 pedidos, e a diferença é exatamente o alinhamento padrão de 16. Depois
do `reset()`, `n2` volta ao mesmo endereço de `n1`. Na 3.2, uma árvore de 5 nós ocupa
`120 / 384 bytes` e some inteira num `reset()`, sem percorrer nó nenhum.

**O padrão de frame** (seção 3.3) é o caso de uso canônico: uma arena por frame, `reset()` no
fim de cada um. Serve igual para uma requisição HTTP ou uma AST descartável de parser.

**Benchmark** (seção 3.4), 100 000 objetos: `heap 4900µs` contra `arena 3000µs`. Ganho real,
mas note que boa parte do tempo da arena é o `push_back` no vetor de ponteiros, não a
alocação — mais um lembrete de medir o programa inteiro, não a linha isolada.

## Demo 4 — `std::pmr` (`demo_pmr.hpp`)

O C++17 padronizou tudo isto na biblioteca. Um **`memory_resource`** é a interface (a base
virtual da seção "Os dois tipos de alocador"), e a STL já traz as políticas prontas:

| resource | política | quando usar |
|---|---|---|
| `new_delete_resource()` | delega a `new`/`delete` | padrão, sem requisito especial |
| `monotonic_buffer_resource` | **arena** (bump) | lifetime único: frame, requisição, parsing |
| `unsynchronized_pool_resource` | **pools** por faixa de tamanho | muitos objetos, uma thread |
| `synchronized_pool_resource` | idem, com lock | o mesmo, multi-thread |
| `null_memory_resource()` | lança `bad_alloc` sempre | **testar** que nada foi à heap |

> ⚠ Repare nos parênteses: `new_delete_resource` e `null_memory_resource` são **funções** que
> devolvem `std::pmr::memory_resource*`, não tipos. Escrever `std::pmr::new_delete_resource
> fallback;` não compila.

**Arena na stack** (seção 4.1) — um `vector` e um `map` que não tocam na heap:

```cpp
std::array<std::byte, 4096> buf;                                  // memória na stack
std::pmr::monotonic_buffer_resource arena(buf.data(), buf.size());
std::pmr::vector<int> v(&arena);
std::pmr::map<std::string, int> m(&arena);                        // o MESMO resource
```

**Cadeia de recursos** (seção 4.2) — o terceiro parâmetro é o *upstream*, usado quando o
buffer esgota:

```cpp
std::array<std::byte, 128> buf_pequeno;
std::pmr::monotonic_buffer_resource arena(
    buf_pequeno.data(), buf_pequeno.size(),
    std::pmr::new_delete_resource()      // ← fallback para a heap quando encher
);
std::pmr::vector<int> v(&arena);
for (int i = 0; i < 50; ++i) v.push_back(i);   // começa na stack, termina na heap
```

Troque `new_delete_resource()` por `null_memory_resource()` e você transforma qualquer
escape para a heap em `std::bad_alloc`: é assim que se **prova**, num teste, que o container
não alocou fora do buffer.

**Um `memory_resource` seu** (seção 4.4). Basta herdar e implementar três virtuais:

```cpp
class LoggingResource : public std::pmr::memory_resource {
    void* do_allocate(std::size_t bytes, std::size_t align) override {
        void* p = upstream_->allocate(bytes, align);
        fmt::print("    [alloc] {} bytes em {}\n", bytes, p);
        return p;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t align) override {
        upstream_->deallocate(p, bytes, align);
    }
    bool do_is_equal(const memory_resource& o) const noexcept override { return this == &o; }

    std::pmr::memory_resource* upstream_ = std::pmr::new_delete_resource();
};
```

> ⚠ O público chama `allocate`; o que você sobrescreve é `do_allocate`. São **não-virtuais
> públicos chamando virtuais privados** — o idioma NVI. `do_is_equal` responde "dois
> containers com estes resources podem trocar memória entre si?"; devolver `this == &other` é
> a resposta conservadora e correta na dúvida.

Saída real: um `std::pmr::vector<int>` com `reserve(4)` faz exatamente **uma** alocação de
**16 bytes**, liberada no fim do escopo.

**O benchmark contraintuitivo** (seção 4.5):

```
  std::vector (heap):       690µs
  pmr::vector (monotonic):  4000µs      ← quase 6× MAIS LENTO
```

Isto não é erro do demo, e a explicação vale mais que o número. O binário do repositório é
compilado em **debug** (`buildtype=debug`, `optimization=0`): nada é inlinado. E
`polymorphic_allocator::construct` não é o `allocator::construct` trivial — ele passa pela
maquinaria de *uses-allocator construction*, uma cadeia de funções que, sem inline, é paga em
**cada** `push_back`. Recompilando o mesmo benchmark isolado nesta máquina:

```
  -O0:  heap  685µs   pmr  4605µs        ← o que o binário do repositório mede
  -O2:  heap  117µs   pmr   114µs        ← a diferença desaparece
```

**Conclusão a levar:** benchmark em build de debug mede o overhead do build de debug. Meça em
release, e meça o programa inteiro.

---

## Quando usar cada um

| cenário | escolha |
|---|---|
| uso geral, sem problema medido | `new`/`delete` — e pare de ler aqui |
| muitos objetos do **mesmo** tamanho, criados e destruídos o tempo todo | pool |
| tudo nasce e morre junto (frame, requisição, parsing) | arena / `monotonic_buffer_resource` |
| quer trocar a política sem mudar o tipo do container | `std::pmr` |
| vários containers compartilhando o mesmo bloco de memória | `std::pmr`, um resource só |
| provar num teste que um trecho não aloca na heap | `null_memory_resource()` como upstream |
| tamanhos variados, uma thread | `unsynchronized_pool_resource` |
| precisa liberar objetos individualmente | pool ou `new`/`delete` — arena **não** serve |
| tipos com destrutor não trivial numa arena | registre os destrutores, ou não use arena |

---

## Como validar

Alocador errado quase nunca falha na hora: ele corrompe memória que só estoura mais tarde,
noutro lugar. Por isso este é, de todo o repositório, o subprojeto que mais precisa de
sanitizer.

```bash
P=$(sed -n 's|^prefix=||p' build/fmt.pc)        # prefixo do fmt instalado pelo Conan

# 1. rodar — deve terminar com a linha verde e sair com 0
./dist/bin/memory | tail -3 ; echo $?
# 2. compilar isolado, com todos os avisos
g++ -std=c++20 -Wall -Wextra -Wpedantic -I"$P/include" -c src/memory/main.cpp -o /dev/null
# 3. ASan + UBSan
g++ -std=c++20 -g -fsanitize=address,undefined -I"$P/include" \
    src/memory/main.cpp -o /tmp/mem_asan -L"$P/lib" -lfmtd
ASAN_OPTIONS=allocator_may_return_null=1 /tmp/mem_asan
```

O que se espera, com o estado atual nesta máquina:

- Passo 2: **zero avisos**.
- Passo 3 **sem** a variável de ambiente: o programa **aborta** com
  `AddressSanitizer: requested allocation size ... exceeds maximum supported size`. Não é bug
  do alocador: é o `nothrow new` de 1 TB da seção 1.4, e o ASan recusa em vez de devolver
  `nullptr`. `allocator_may_return_null=1` restaura o comportamento esperado.
- Passo 3 **com** a variável: sai com 0, sem vazamento — e o UBSan aponta duas linhas reais:
  ```
  src/memory/demo_new_delete.hpp:196:18: runtime error: signed integer overflow: 92682 + 2147441940 ...
  src/memory/demo_new_delete.hpp:210:18: runtime error: signed integer overflow: ...
  ```
  É o acumulador do benchmark stack×heap: `volatile int soma` somando `2*i` para 100 000
  iterações passa de `INT_MAX`. Não afeta a medição de tempo, mas é UB de verdade — a correção
  é declarar `volatile long long soma`. Fica registrado aqui como exercício, e como lembrete
  de que o sanitizer acha o que a leitura não acha.

> ⚠ **O limite decisivo do ASan.** Ele instrumenta `malloc`/`new`, e só. Memória servida pelo
> **seu** pool ou pela **sua** arena já pertence ao programa: escrever 40 bytes num bloco de
> 24 do pool não dispara nada, porque, para o ASan, aquilo é um único bloco grande e válido.
> Alocador customizado precisa de defesa própria: envenenar manualmente com
> `ASAN_POISON_MEMORY_REGION(p, n)` ao devolver um bloco à free list e
> `ASAN_UNPOISON_MEMORY_REGION` ao entregá-lo, ou testes que verifiquem invariantes
> (`em_uso()`, `pertence()`, endereços reciclados) — como as asserções que o Demo 2 imprime.

---

## Referências

Neste repositório: [`src/smart-pointers/`](../smart-pointers/) (`unique_ptr` com deleter customizado, o
embrulho do Demo 2); [`src/layout/`](../layout/) (alinhamento, padding e por que `sizeof` não é a soma
dos campos); [`src/undefined_behavior/`](../undefined_behavior/) (o catálogo de UB que os ⚠ daqui
citam); [`src/performance/`](../performance/) (medir antes de otimizar, e como não mentir num
benchmark); [`src/ecs/`](../ecs/) (um `ComponentStore` que é um pool com outro nome);
[`src/bits/`](../bits/) (as máscaras e potências de 2 do conceito 3).

Fontes canônicas:

- cppreference — [`<memory_resource>`](https://en.cppreference.com/w/cpp/header/memory_resource), [`<new>`](https://en.cppreference.com/w/cpp/header/new), [operator new](https://en.cppreference.com/w/cpp/memory/new/operator_new), [`std::align`](https://en.cppreference.com/w/cpp/memory/align)
- Pablo Halpern, *Allocators: The Good Parts* (CppCon 2017) — a apresentação de referência sobre `pmr`.
- Andrei Alexandrescu, *std::allocator Is to Allocation what std::vector Is to Vexation* (CppCon 2015).
- [AddressSanitizer manual poisoning](https://github.com/google/sanitizers/wiki/AddressSanitizerManualPoisoning) — as macros citadas no ⚠ acima.
