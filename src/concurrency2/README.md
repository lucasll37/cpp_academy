# concurrency2 — Concorrência Moderna (C++20)

## O que é este projeto?

A continuação de [`src/concurrency/`](../concurrency/), que cobre o núcleo do C++11:
`std::thread`, `mutex`, `condition_variable`, `future`/`promise`, atômicos básicos e um
thread pool.

Aqui entram três coisas que aquele subprojeto não alcança:

1. os **primitivos que o C++20 acrescentou** — `jthread`, `stop_token`, `latch`, `barrier`,
   `counting_semaphore` e `atomic::wait`;
2. o **modelo de memória** — `happens-before`, `memory_order` e por que data race não é
   "valor errado", é comportamento indefinido;
3. **programação lock-free** — o que ela garante, o que ela não garante, e uma medição
   honesta de quando o `mutex` simplesmente ganha.

## Antes de começar: os cinco conceitos

Se algum destes for novo, leia esta seção antes de rodar qualquer coisa. Tudo no
subprojeto se apoia neles.

### 1. O que é uma thread

Um programa comum tem **um** fluxo de execução: uma instrução por vez, na ordem
escrita. Uma **thread** é um fluxo a mais, dentro do **mesmo processo** — e é
esse "mesmo processo" que importa:

```
PROCESSO
├── thread A ──▶ executa código          ┐
├── thread B ──▶ executa código          ├── TODAS enxergam a MESMA memória
└── thread C ──▶ executa código          ┘
```

Cada thread tem a sua **pilha** (variáveis locais, endereços de retorno), mas o
**heap e as variáveis globais são compartilhados**. Duas threads que leem o
mesmo `std::vector` estão lendo os mesmos bytes, não cópias.

É daí que vem toda a dificuldade — e todo o poder.

### 2. O que é uma condição de corrida (*data race*)

Duas threads acessam a **mesma** posição de memória, **ao menos uma escreve**, e
**nada ordena** um acesso em relação ao outro.

```cpp
int contador = 0;                    // compartilhado

// thread A            // thread B
++contador;            ++contador;   // resultado pode ser 1, não 2
```

`++contador` não é uma instrução: são três (ler, somar, escrever). Se as duas
threads leem 0 antes de qualquer uma escrever, as duas escrevem 1.

> ⚠ **Em C++, data race não é "valor errado": é comportamento indefinido.** O
> programa pode fazer qualquer coisa, inclusive travar num laço que você jurava
> que terminava. A seção 3 mostra esse caso exato.

### 3. O que é um tipo atômico

`std::atomic<T>` garante que a operação acontece **inteira ou não acontece** —
nenhuma outra thread vê um estado pela metade. Trocar `int` por
`std::atomic<int>` no exemplo acima resolve a perda de incrementos.

O que ele **não** faz: tornar uma *sequência* de operações segura.
`if (x > 0) --x;` são duas operações atômicas, e entre elas outra thread pode
agir. Para isso existe `compare_exchange` (seção 4) ou um mutex.

### 4. O que quer dizer "sincronizar"

Duas coisas diferentes, e confundi-las é a origem de metade dos bugs:

| | o que garante |
|---|---|
| **exclusão mútua** | só uma thread por vez naquele trecho (mutex) |
| **ordenação de memória** | o que a thread B enxerga do que a thread A escreveu |

A segunda é menos óbvia e mais importante. O processador e o compilador
**reordenam** acessos à memória livremente; sem uma regra explícita, a thread B
pode ver as escritas de A em ordem diferente da que A executou. É o assunto da
seção 3.

### 5. O que é *lock-free*

Não quer dizer "rápido". Quer dizer uma garantia de **progresso**:

| | garantia |
|---|---|
| bloqueante | uma thread suspensa no lugar errado trava as outras |
| **lock-free** | **alguma** thread sempre progride |
| **wait-free** | **toda** thread progride em passos limitados |

A seção 5 mede um caso em que o `mutex` **ganha** do lock-free.

## Por que estudar isto?

Porque a intuição sobre concorrência falha de um jeito específico e caro: o código errado
**funciona** nos seus testes. Um data race não estoura; ele fica latente por meses, e um dia
o compilador atualiza, o `-O2` iça uma leitura para fora do laço e o serviço trava em
produção.

Os três blocos deste subprojeto atacam esse problema em ordem:

| Bloco | O que dá |
|---|---|
| `jthread` + sincronizadores | primitivos que tornam o código correto **por construção** |
| modelo de memória | vocabulário para saber **por que** está correto |
| lock-free | onde a fronteira realmente está, medida em vez de presumida |

Três resultados **medidos** neste subprojeto contrariam o senso comum:

- `seq_cst` custou **1.01×** o `relaxed` num `fetch_add` sem contenção no x86-64 — a otimização
  que "todo mundo faz" não pagou nada aqui (e pagaria num ARM);
- reimplementar `fetch_add` com CAS gerou **43–60% de retrabalho** com 4 threads;
- a pilha lock-free ficou **~2.7× mais lenta** que `vector::push_back` sob `mutex`.

## Estrutura de arquivos

```
src/concurrency2/
├── main.cpp                   ← ponto de entrada, cor medium_purple
├── demo_jthread.hpp           ← 1 · RAII e parada cooperativa
├── demo_sincronizadores.hpp   ← 2 · latch, barrier, semaphore
├── demo_memory_model.hpp      ← 3 · happens-before e memory_order
├── demo_atomicos.hpp          ← 4 · CAS, atomic_flag, wait/notify
├── demo_lockfree.hpp          ← 5 · SPSC, pilha lock-free, ABA, medição
├── meson.build
└── README.md
```

Rodar:

```bash
make build && ./dist/bin/concurrency2
```

---

## Demo 1: `jthread`

### O destrutor que não mata o programa

`~std::thread()` chama `std::terminate()` se a thread ainda for *joinable*. Não é vazamento
nem UB: é morte imediata. Esquecer um `join()` — ou sofrer uma exceção antes dele — derruba
o processo.

A escolha foi deliberada. `detach()` silencioso seria pior: a thread sobreviveria aos objetos
que captura por referência.

`std::jthread` faz `request_stop()` + `join()` no destrutor. É o mesmo RAII de
[`src/errors/demo_exceptions.hpp`](../errors/demo_exceptions.hpp), aplicado a threads.

### `stop_token`

Se a lambda declarar `std::stop_token` como **primeiro parâmetro**, a `jthread` o passa
automaticamente:

```cpp
std::jthread w{[](std::stop_token st) {
    while (!st.stop_requested()) { ... }
}};
// o destrutor pede a parada e junta
```

Antes do C++20 cada projeto tinha o seu `std::atomic<bool> parar` — funcional, mas sem
integração nenhuma com a biblioteca padrão.

### ⚠ A parada é COOPERATIVA

`request_stop()` liga um flag. Só isso. Se a thread nunca consultar `stop_requested()`, ela
**não para** — e o `join()` do destrutor trava para sempre.

Não existe `thread::kill()` em C++, e é proposital: matar uma thread no meio deixaria mutexes
travados, destrutores sem rodar e invariantes quebrados. (O `pthread_cancel` do POSIX existe
e é considerado inseguro em C++ justamente por isso.)

A responsabilidade é sua: **checar entre unidades de trabalho**, nunca no meio de uma, ou o
estado fica pela metade.

> Esta armadilha custou um bug real neste próprio subprojeto: a primeira versão da fila SPSC
> em `demo_lockfree.hpp` usava `stop_token` no consumidor, e o `request_stop()` do destrutor
> o fazia abandonar a fila ainda cheia. A saída acusou `PERDEU DADOS`. A condição de parada
> correta era o trabalho ter **terminado**, não a parada ter sido pedida. O comentário está
> no fonte.

### `stop_callback` e `condition_variable_any`

Uma thread bloqueada não checa flag nenhum. Duas saídas:

- **`std::stop_callback`** dispara no instante do `request_stop()`, na thread que pediu.
  Serve para acordar quem está bloqueado — fechar o socket faz o `read()` retornar com erro.
  Precisa ser rápido e thread-safe.
- **`condition_variable_any::wait(lk, token, pred)`** devolve `false` quando a parada é
  pedida. `std::condition_variable` (sem `_any`) **não** tem essa sobrecarga.

| classe | aceita `stop_token` |
|---|---|
| `condition_variable` | não |
| `condition_variable_any` | sim |

---

## Demo 2: `latch`, `barrier`, `semaphore`

| primitivo | o que é | reutilizável | completion |
|---|---|---|---|
| `latch` | contador que só desce | não | não |
| `barrier` | contador que se rearma por fase | sim | sim |
| `counting_semaphore<N>` | permissões, 0..N | — | — |

### `latch` — dois usos

**Esperar N threads:** `l.count_down()` na worker, `l.wait()` na main.

**A largada:** um `latch{1}` segura todas as threads até o "tiro". Sem isso, a thread 0 já
processou metade quando a thread 3 nasce, e qualquer medição de paralelismo perde o sentido.

`arrive_and_wait()` = `count_down()` + `wait()`, para quem participa **e** espera.

> ⚠ `count_down()` além do zero é UB. Não há como desfazer.

### `barrier` — fases

A **completion function** roda uma vez, em uma das threads participantes, no instante em que
a última chega e **antes** de qualquer uma ser liberada. É o lugar seguro para trocar buffers
entre passos de simulação.

`arrive_and_drop()` reduz o número esperado nas fases seguintes — para uma thread que vai
sair antes. Sem isso, as restantes esperariam para sempre por quem já morreu.

### ⚠ `semaphore` não é `mutex`

| aspecto | `mutex` | `semaphore` |
|---|---|---|
| dono | sim, quem travou | **não, ninguém** |
| quem libera | só o dono | qualquer thread |
| contagem | 0 ou 1 | 0..N |
| RAII | `scoped_lock` | não há padrão |
| uso | exclusão mútua | recursos limitados |

Um `counting_semaphore<1>` *parece* um mutex, mas a thread A pode dar `acquire` e a B dar
`release` — recurso em semáforo, bug em mutex. E não há `unique_lock` para ele: um `return`
no meio ou uma exceção vazam a permissão para sempre.

Semáforo é para **contar**: "no máximo 4 conexões", "o buffer tem 10 vagas".

### Escolha rápida

```
esperar N eventos, uma vez ............... latch
sincronizar rodadas repetidas ............ barrier
limitar acesso concorrente a K ........... counting_semaphore
sinalizar "aconteceu" uma vez ............ binary_semaphore
exclusão mútua ........................... mutex
resultado de UMA operação assíncrona ..... future/promise
```

---

## Demo 3: O modelo de memória

A causa de toda a dificuldade cabe numa linha:

> **Não existe "a ordem em que o programa executou".**

O compilador reordena. A CPU reordena. Os caches propagam escritas em ordens diferentes para
núcleos diferentes. O que o C++11 definiu não foi uma ordem global, e sim uma relação
**parcial** — `happens-before`.

### ⚠ Data race é UB, não "valor errado"

```cpp
bool parar = false;              // NÃO atômico
while (!parar) { trabalho(); }   // outra thread põe true
```

Com `-O2`, o compilador vê que o laço não escreve em `parar`, iça a leitura para fora e gera
`while (true)`. O programa trava para sempre — e o compilador **está certo**, porque a outra
thread escrevendo ali já era UB.

`volatile` **não resolve**: impede a otimização da leitura, mas não dá garantia de ordem
entre variáveis nem atomicidade. Em C++, `volatile` é para MMIO, não para threads.

### `acquire` / `release`

A regra, em uma frase:

> Se a thread B faz um **load-acquire** que **lê o valor escrito** por um **store-release** da
> thread A, então tudo que A escreveu antes do store é visível para B depois do load.

É assim que se publica uma estrutura inteira com um único atômico:

```cpp
carga.a = 42;                                    // ┐
carga.b = 3.5;                                   // │ escritas comuns, não atômicas
carga.c = "publicado";                           // ┘
pronto.store(true, std::memory_order_release);   // publica tudo de uma vez
```

A barreira é **direcional**, e é isso que a torna barata:

- `release` → nada de **antes** desce para depois do store
- `acquire` → nada de **depois** sobe para antes do load

> ⚠ `release` sozinho não faz nada. Só há sincronização se o `acquire` **ler o valor que
> aquele release escreveu**. Ler um valor mais antigo não sincroniza com nada.
>
> ⚠ O par tem que ser **no mesmo átomo**.

### `relaxed`

Garante **apenas** atomicidade: nenhum incremento se perde, nenhum valor meio escrito aparece.
Nada sobre ordem em relação a outras variáveis.

- **Legítimo:** contadores de estatística, flags de "já vi isso", incremento de refcount.
- **Errado:** publicar dados. `pronto.store(true, relaxed)` deixa a outra thread ver
  `pronto == true` e `dados == 0`.

Detalhe do refcount: o **incremento** pode ser `relaxed`, mas o **decremento** precisa ser
`acq_rel` — quem zera o contador vai destruir o objeto e precisa enxergar tudo que as outras
threads fizeram com ele. Ver [`src/smart-pointers/`](../smart-pointers/).

### `seq_cst` — o padrão, e o preço

`x.store(1)` é `x.store(1, memory_order_seq_cst)`: existe **uma** ordem total de todas as
operações `seq_cst`, e todas as threads concordam com ela.

Medido neste subprojeto, 2.000.000 de `fetch_add` sem contenção:

```
relaxed :    2.96 ms
seq_cst :    2.99 ms
razão   :    1.01×
```

No x86-64 a diferença é ruído: `fetch_add` vira `lock xadd` de qualquer forma, a arquitetura
é fortemente ordenada e `acquire`/`release` saem de graça. Num ARM ou POWER, onde `seq_cst`
exige barreiras explícitas (`dmb ish`), a diferença é grande.

**É por isso que "otimizei trocando para relaxed" precisa ser medido na arquitetura alvo.**

Regra prática: comece com `seq_cst`. Só desça quando (a) o profiler apontar e (b) você
conseguir escrever qual `happens-before` está preservando. Se não consegue escrever, não desça.

### ⚠ Armadilhas

- **Atômico não é thread-safe automaticamente.** `if (x > 0) x = x - 1;` são duas operações.
- **`atomic<T>` com T grande usa lock escondido.** Medido: `atomic<int>` (4 B) e
  `atomic<double>` (8 B) são lock-free; uma struct de 32 B não é. O corte no x86-64 é 16 bytes
  (`cmpxchg16b`). Use o `static constexpr` **`is_always_lock_free`**, não o `is_lock_free()`
  de membro — este último vira chamada a `__atomic_is_lock_free` e obriga a linkar `-latomic`.
- **`memory_order_consume` é para não usar.** Nenhum compilador o implementa; todos o promovem
  para `acquire`. Em revisão desde C++17.
- **Nada substitui o TSan.** `g++ -fsanitize=thread` constrói o grafo de `happens-before` em
  runtime e acusa o race mesmo quando ele não se manifesta.

---

## Demo 4: CAS, `atomic_flag` e espera atômica

### O laço de `compare_exchange`

A única primitiva que importa de verdade. Tudo o mais (`fetch_add`, `fetch_or`, `exchange`)
se constrói a partir dela.

```cpp
long atual = contador.load(std::memory_order_relaxed);
while (!contador.compare_exchange_weak(atual, atual + 1)) {
    // corpo VAZIO: o CAS já reescreveu `atual` com o valor real
}
```

`esperado` é passado **por referência** e atualizado na falha — é isso que faz o laço
convergir sem uma releitura extra.

| | falha espúria | uso |
|---|---|---|
| `_weak` | sim | dentro de um laço (mapeia direto em LL/SC: ARM, POWER) |
| `_strong` | não | quando não há laço externo |

Os dois `memory_order`: `compare_exchange_weak(esp, novo, ORDEM_SUCESSO, ORDEM_FALHA)`. A
ordem de falha não pode ser mais forte que a de sucesso, nem ser `release` — uma falha não
escreve nada.

**Medido**, reimplementando `fetch_add` com CAS, 4 threads × 20 000:

```
incrementos efetivados : 80000
tentativas de CAS      : 141160 – 201402   (varia por execução)
falhas por contenção   : 43% – 60% de retrabalho
```

Nenhum incremento se perde, mas cada falha custa outra volta. É esse retrabalho que faz um
algoritmo lock-free degradar sob contenção: ele nunca **bloqueia**, e ainda assim não é de
graça. Na prática use `fetch_add` — o hardware faz num `lock xadd` só, sem laço nenhum.

> A primeira versão deste experimento media a atualização de um máximo com faixas disjuntas
> e crescentes por thread. Resultado: **0% de retrabalho**, porque cada valor já era um novo
> máximo e o CAS acertava de primeira. A medição contradizia o texto — e o texto estava
> errado, não a medição. O experimento foi trocado por um que força a disputa.

### `atomic_flag`

O único tipo que o padrão **exige** ser lock-free em toda implementação. Três operações:
`test_and_set()` (devolve o valor anterior), `clear()` e `test()` (só a partir do C++20).

⚠ Para inicialização única prefira `std::call_once` ou uma estática local (thread-safe desde
o C++11): elas fazem as outras threads **esperarem**. O flag só diz quem chegou primeiro.

### `atomic::wait` / `notify` (C++20)

Todo `std::atomic<T>` ganhou `wait`/`notify_one`/`notify_all`, implementados sobre futex.

| mecanismo | precisa de mutex | espera |
|---|---|---|
| `while (x.load()) {}` | não | **spin** |
| `atomic::wait` | não | dorme |
| `condition_variable` | **sim** | dorme |

A semântica é "espere **enquanto** for igual a este valor" — o inverso da intuição de
`cv.wait(lk, pred)`. Também sofre wakeup espúrio: em código real, laço.

### ⚠ Spin lock

A implementação em `demo_atomicos.hpp` usa *test and test-and-set*: o laço interno chama
`test()` puro, que não escreve na linha de cache. Ficar em `test_and_set` faria a linha
pingar entre os núcleos a cada volta.

Funciona — e ainda assim quase sempre é a escolha errada:

- quem espera queima CPU integral, num core que o dono do lock talvez precisasse;
- se o SO preempta o dono, os outros giram um quantum inteiro à toa;
- sem RAII, não é reentrante, não detecta deadlock.

O `std::mutex` do glibc **já faz um spin curto** antes de chamar o futex. Você ganha o melhor
dos dois de graça.

---

## Demo 5: Lock-free na prática

"Lock-free" **não** quer dizer rápido. Quer dizer uma garantia de progresso:

| | garantia |
|---|---|
| bloqueante | uma thread suspensa no lugar errado trava as outras |
| **lock-free** | **alguma** thread sempre progride |
| **wait-free** | **toda** thread progride em passos limitados |

A garantia importa em duas situações concretas: signal handler (não pode travar mutex) e
tempo real (o pior caso é o que conta).

### Fila SPSC — a única que vale escrever à mão

Com exatamente **um** produtor e **um** consumidor o problema fica tratável: o produtor é o
único que escreve `cabeca_`, o consumidor é o único que escreve `cauda_`. **Zero CAS** — só
um par `release`/`acquire` de cada lado.

Medido: 200 000 itens em ~2,3 ms (~90 M itens/s), soma íntegra.

Cada índice fica `alignas(hardware_destructive_interference_size)`, senão o produtor
escrevendo `cabeca_` invalidaria a linha de cache do consumidor a cada `push`. Ver
[`src/performance/`](../performance/) para o efeito medido.

> ⚠ A garantia **depende** de ser 1 produtor e 1 consumidor. Com dois produtores, dois podem
> ler a mesma `cabeca_`, escrever no mesmo slot e um sobrescrever o outro.

### Pilha lock-free e o problema ABA

```
thread 1: lê topo = A, vai chamar CAS(A → A->prox = B)
thread 2: pop A, pop B, push A de volta
thread 1: CAS(A → B) SUCEDE — o ponteiro é A de novo!
          Mas B já saiu da pilha. O topo aponta para lixo.
```

O CAS compara **endereços**, não identidades. E o alocador adora reciclar o endereço que
acabou de ser liberado, o que torna o ABA muito mais provável do que parece.

Defesas, todas caras: contador de versão junto do ponteiro (CAS de 128 bits, `cmpxchg16b`),
hazard pointers, RCU/epoch-based reclamation, ou nunca liberar.

### ⚠ Quem libera o nó?

```cpp
No* t = topo_.load();
while (t && !topo_.compare_exchange_weak(t, t->prox)) { }
                                            ▲
                          lê t->prox — e se outra thread já
                          deu `delete t` aqui? use-after-free.
```

Este é o calcanhar de Aquiles do lock-free: **não há momento seguro para liberar um nó**,
porque não há como saber se alguma thread ainda tem um ponteiro para ele.

A demo escapa porque separa as fases — primeiro só pushes concorrentes, depois só pops de uma
thread. Com pops concorrentes aquele `delete` é use-after-free, e o ASan acusa. Está marcado
no fonte com um ⚠.

`std::atomic<std::shared_ptr<T>>` (C++20) resolve o ciclo de vida e o GCC 13 o tem — mas
`is_always_lock_free` é `false`: internamente há um mutex. Fica correto, deixa de ser lock-free.

**Na prática, use as prontas:** `moodycamel::ConcurrentQueue`, `boost::lockfree::*`,
`folly::MPMCQueue`.

### A medição que fecha o assunto

4 threads × 100 000 operações:

```
contador:
  atomic fetch_add           :  3.87 ms
  mutex + ++                 :  9.10 ms   (2.3× mais lento)

inserção:
  mutex + vector::push_back  :  8.68 ms
  pilha lock-free (com new)  : 23.61 ms   (2.7× mais LENTO)
```

O atômico ganha com folga quando a seção crítica é **uma instrução**. Quando o trabalho é
maior, o mutex ganha: a pilha lock-free paga um `new` por nó — que tem o próprio lock interno
no alocador — e sofre o retrabalho do CAS.

**Para ganhar desempenho, primeiro reduza a CONTENÇÃO:** dados por thread, sharding, ou
simplesmente uma seção crítica menor. Lock-free é para quando você precisa da **garantia**.

---

## Como validar

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/concurrency2/main.cpp -o /tmp/c2 -L$FMTP/lib -lfmtd -pthread

# race detector — o teste que importa neste subprojeto
g++ -std=c++20 -O1 -g -fsanitize=thread -I$FMTP/include \
    src/concurrency2/main.cpp -o /tmp/c2t -L$FMTP/lib -lfmtd -pthread && /tmp/c2t
```

Estado atual: compila sem warnings com `-Wall -Wextra -Wpedantic` e roda **limpo sob TSan**
(0 achados).

## Referências

- [`src/concurrency/`](../concurrency/) — o C++11: thread, mutex, future, thread pool
- [`src/coroutines/`](../coroutines/) — concorrência sem threads
- [`src/performance/`](../performance/) — false sharing e localidade de cache, medidos
- [`src/undefined_behavior/`](../undefined_behavior/) — TSan e os outros sanitizers
- *C++ Concurrency in Action*, 2ª ed., Anthony Williams — a referência do assunto
- Herb Sutter, *atomic<> Weapons* (C++ and Beyond 2012) — o modelo de memória em duas horas
