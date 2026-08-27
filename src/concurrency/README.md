# concurrency — Threads, Locks e Atômicos (o núcleo do C++11)

## O que é este projeto?

Um programa executável que percorre, em ordem, **todos os primitivos de concorrência que o
C++11 trouxe para a biblioteca padrão**: `std::thread`, `std::mutex`,
`std::condition_variable`, `std::future`/`std::promise`, `std::atomic` — e termina montando
com eles duas peças que todo sistema real tem: um **thread pool** e um **pipeline**.

Cada demo imprime o que fez, para que você compare o texto com o comportamento observado.

```bash
make build && ./dist/bin/concurrency
```

> `make build` compila em `build/` e instala o binário em `dist/bin/`. Se for a primeira vez
> neste clone, rode `make configure` antes (ele baixa o `fmt` pelo Conan e chama o `meson`).

A continuação em C++20 — `jthread`, `latch`, `barrier`, modelo de memória formal e lock-free —
está em [`src/concurrency2/`](../concurrency2/).

---

## Antes de começar

Esta seção define **todo** o vocabulário usado no resto do arquivo. Se você já sabe o que é
um data race, pule para [Por que estudar isto?](#por-que-estudar-isto).

### As quatro palavras que sustentam tudo

**Processo** é o seu programa rodando: um espaço de endereçamento (a memória dele) mais
recursos do sistema operacional. **Thread** ("linha de execução") é um fluxo de instruções
dentro desse processo. Um processo começa com uma thread — a que executa `main()` — e pode
criar outras.

O ponto que muda tudo: **threads do mesmo processo compartilham a mesma memória**. Uma
variável global, ou uma variável local capturada por referência numa lambda, é *a mesma
variável* para todas elas. Isso é ao mesmo tempo a vantagem (comunicar custa uma escrita) e o
perigo (duas threads escrevendo no mesmo lugar ao mesmo tempo).

**Núcleo** (*core*) é a unidade de execução física do processador. Se a sua máquina tem 8
núcleos, 8 threads podem estar *realmente* executando ao mesmo tempo; a nona espera a vez. O
programa descobre esse número com `std::thread::hardware_concurrency()`.

**Concorrência** é estruturar o programa em tarefas que progridem de forma independente.
**Paralelismo** é executá-las literalmente ao mesmo tempo, em núcleos diferentes. Concorrência
é uma propriedade do *código*; paralelismo, uma propriedade da *execução*. Um programa
concorrente numa máquina de um núcleo só continua correto — apenas não fica mais rápido.

### Estado compartilhado e seção crítica

**Estado compartilhado** é qualquer dado que mais de uma thread alcança. **Seção crítica** é o
trecho de código que mexe nesse dado e que precisa executar sem interferência — porque no meio
dele um **invariante** (uma verdade que o resto do programa assume, do tipo "o tamanho da fila
é igual ao número de elementos") está temporariamente quebrado.

**Exclusão mútua** é a garantia de que no máximo uma thread está dentro da seção crítica por
vez. O objeto que a implementa é o **mutex** (de *mutual exclusion*).

### Data race — a definição exata

Este é o conceito mais importante do arquivo, então vale a definição literal do padrão. Existe
um **data race** quando, no mesmo endereço de memória:

1. duas threads diferentes fazem acessos **conflitantes** (pelo menos um deles é uma
   **escrita**);
2. nenhum dos dois é uma operação atômica;
3. **nenhum dos dois "acontece antes"** (*happens-before*) do outro — isto é, não há mutex,
   `join()`, atômico nem qualquer outra sincronização ordenando um em relação ao outro.

Se as três condições valem, o programa tem **comportamento indefinido** (UB). Não "um valor
errado": *indefinido*. O compilador tem permissão de assumir que data races não existem e
otimizar em cima disso — e um programa com UB pode fazer qualquer coisa, inclusive parecer
funcionar por dois anos. O tratamento completo, com o exemplo do laço que vira `while(true)`
sob `-O2`, está em [`src/concurrency2/`](../concurrency2/#demo-3-o-modelo-de-memória).

⚠ **Data race** (dois acessos sem ordenação) não é a mesma coisa que **race condition** (o
resultado do programa depende de *quem chegou primeiro*). Um programa pode ter race condition
sem data race — por exemplo, dois pedidos corretamente sincronizados por mutex, mas cuja ordem
de chegada muda o saldo final. Todo data race é um bug; nem toda race condition é.

**Deadlock** ("abraço mortal") é o travamento em que a thread A espera um recurso que B segura
enquanto B espera um recurso que A segura. Nenhuma avança, nunca. Está demonstrado no
[Demo 2](#deadlock--duas-threads-dois-mutexes).

### Termos que aparecem mais adiante

| Termo | Uma frase |
|---|---|
| `join()` | "espere esta thread terminar antes de seguir" |
| `detach()` | "solte esta thread; não vou mais esperá-la" (*daemon*: roda em segundo plano até o processo morrer) |
| *joinable* | uma `std::thread` que ainda representa uma thread viva e para a qual você ainda não fez `join` nem `detach` |
| **RAII** | *Resource Acquisition Is Initialization*: o construtor adquire, o destrutor libera. É como C++ garante limpeza mesmo com exceções — ver [`src/errors/`](../errors/) |
| *spurious wakeup* | o SO pode acordar uma thread adormecida **sem** que ninguém tenha notificado. Por isso toda espera precisa reconferir a condição |
| **CAS** | *Compare-And-Swap*: "se o valor ainda for X, troque por Y; senão me diga qual é". Uma única instrução da CPU |
| **lock-free** | garantia de que **alguma** thread sempre progride, mesmo que o SO suspenda as outras no pior lugar. **Não** quer dizer "rápido" |
| *memory ordering* | quanto o compilador e a CPU podem reordenar operações de memória em volta de um atômico |
| **thread pool** | um conjunto fixo de threads que consome tarefas de uma fila, em vez de criar uma thread por tarefa |
| **TSan** | *ThreadSanitizer*: instrumentação do GCC/Clang que detecta data races em tempo de execução. Ver [Como validar](#como-validar) |
| `-pthread` | flag que o GCC exige para threads. Não é só `-lpthread`: ela também define macros que mudam o cabeçalho e o modelo de código. **Sempre use `-pthread`** |

---

## Por que estudar isto?

Comece pelo problema, não pela API.

**O problema 1 — o relógio parou de acelerar.** Até meados dos anos 2000, código lento ficava
rápido sozinho: bastava esperar o próximo processador. Isso acabou; o ganho passou a vir de
*mais núcleos*, não de núcleos mais rápidos. Um programa de uma thread nesta máquina usa
**1 de 24** unidades de execução (`hardware_concurrency()` mediu 24 aqui) — os outros 23 ficam
parados por definição, e nenhum compilador conserta isso por você.

**O problema 2 — esperar é o que os programas mais fazem.** Ler um arquivo, uma resposta de
rede ou uma consulta ao banco custa milissegundos em que a CPU não faz *nada*. Uma thread
bloqueada em I/O não impede as outras de trabalhar. Aqui a concorrência não busca velocidade
de cálculo: busca não desperdiçar a espera.

**E o problema 3, que é o motivo real de existir este subprojeto — em concorrência, o código
errado passa nos testes.**

Não é retórica; é mensurável, e o próprio programa deste diretório é a evidência. O demo 2.1
(`demo_race_condition`, em [`demo_mutex.hpp`](demo_mutex.hpp)) incrementa um `int` de duas
threads **sem nenhuma sincronização**. É um data race puro, comportamento indefinido de
manual. Nesta máquina ele foi executado 12 vezes seguidas e imprimiu:

```
  esperado: 20000, obtido: 20000 ✓
```

**Doze vezes o resultado "certo".** Um teste automatizado que conferisse o valor final teria
passado nas doze. Compilado com o ThreadSanitizer, o mesmo binário continua imprimindo
`20000 ✓` — e **acusa o data race na linha exata**, `demo_mutex.hpp:50`. O bug estava lá o
tempo todo; só nenhuma execução tinha decidido revelá-lo ainda.

É por isso que aqui não basta "testar e ver se dá certo". Você precisa de vocabulário para
argumentar que o código está correto, e de ferramentas que enxerguem o que a execução esconde.
Este subprojeto dá as duas coisas:

| Bloco | O que dá |
|---|---|
| threads, `mutex`, `condition_variable` | os primitivos para **construir** exclusão mútua e espera sem desperdício |
| `future`/`promise`, `packaged_task`, `async` | transferir **resultados** entre threads sem inventar variáveis compartilhadas |
| `atomic` e memory ordering | o vocabulário para dizer **por que** está correto — e o custo de cada garantia |
| thread pool e pipeline | os dois formatos em que quase todo sistema real acaba organizado |
| `-fsanitize=thread` | a ferramenta que encontra o que o teste não encontra |

---

## O menor programa completo

Antes de qualquer detalhe, um programa inteiro que compila, roda e demonstra o ciclo completo:
criar threads, proteger o dado compartilhado, esperar o fim.

```cpp
#include <iostream>
#include <mutex>
#include <thread>

int  contador = 0;   // dado compartilhado pelas duas threads
std::mutex mtx;      // o cadeado que protege esse dado

void incrementar(int vezes) {
    for (int i = 0; i < vezes; ++i) {
        std::lock_guard<std::mutex> trava(mtx);  // tranca aqui
        ++contador;
    }                                            // destrava aqui (destrutor)
}

int main() {
    std::thread t1(incrementar, 100000);
    std::thread t2(incrementar, 100000);
    t1.join();                       // espera t1 terminar
    t2.join();                       // espera t2 terminar
    std::cout << "contador = " << contador << '\n';
}
```

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic min.cpp -o min -pthread && ./min
```

Saída, invariável em qualquer execução:

```
contador = 200000
```

Quatro coisas para reter deste programa antes de seguir:

1. `std::thread t1(f, arg)` **já começa a executar** `f(arg)`. Não existe `start()`.
2. `lock_guard` tranca no construtor e destranca no destrutor. Você nunca escreve `unlock()`,
   e por isso nunca esquece — nem quando uma exceção salta do meio do bloco.
3. `join()` é **obrigatório**. Sem ele, o destrutor de `std::thread` chama `std::terminate()`
   e o programa morre (detalhado no Demo 1).
4. Tire o `lock_guard` e o resultado deixa de ser 200000 — às vezes. Essa é a lição inteira.

---

## Estrutura de arquivos

```
src/concurrency/
├── main.cpp              ← ponto de entrada, cor coral; chama os 6 blocos em ordem
├── demo_threads.hpp      ← 1 · criação, join/detach, argumentos, this_thread,
│                             thread_local, RAII guard
├── demo_mutex.hpp        ← 2 · race condition, mutex, lock_guard, unique_lock,
│                             shared_mutex, call_once, deadlock evitado
├── demo_condition.hpp    ← 3 · wait/notify, notify_all, wait_for, fila thread-safe
├── demo_futures.hpp      ← 4 · async, promise, exceções, packaged_task,
│                             shared_future, soma paralela
├── demo_atomics.hpp      ← 5 · atomic<T>, CAS, memory orders, atomic vs mutex
├── demo_patterns.hpp     ← 6 · thread pool (+ tarefas heterogêneas) e
│                             pipeline de 3 estágios
├── meson.build
└── README.md
```

O executável se chama `concurrency` (é o primeiro argumento de `executable()` em
[`meson.build`](meson.build)) e é instalado em `dist/bin/concurrency`.

---

## Demo 1: `std::thread` — criação e ciclo de vida

Fonte: [`demo_threads.hpp`](demo_threads.hpp)

### Criar: três formas, a mesma ideia

O construtor de `std::thread` recebe **qualquer coisa chamável** mais os argumentos dela, e a
execução começa imediatamente.

```cpp
#include <string>
#include <thread>

void tarefa(int id, const std::string& msg);   // declarada FORA, no escopo de arquivo

struct Trabalhador {
    int id;
    void executar() const;
};

void exemplo() {
    Trabalhador w{3};
    int dado = 42;

    std::thread t1(tarefa, 1, "via função livre");        // função livre + argumentos
    std::thread t2([dado] { /* usa a cópia de dado */ }); // lambda
    std::thread t3(&Trabalhador::executar, &w);           // método: ponteiro-para-membro
                                                          // + o objeto (o "this")
    t1.join();
    t2.join();
    t3.join();
}
```

⚠ Note que `tarefa` é declarada **fora** de `exemplo()`. C++ não permite definir uma função
dentro de outra função — para lógica local, use uma lambda.

### `join()` vs `detach()` — e o que acontece se você esquecer

Toda `std::thread` viva está em um de dois destinos possíveis, e você **tem** de escolher um:

- **`t.join()`** — bloqueia quem chamou até a thread terminar. Depois disso `t.joinable()` é
  `false`. É a escolha correta em 95% dos casos.
- **`t.detach()`** — desliga o objeto `std::thread` da thread do sistema. Ela continua
  rodando, mas você perde qualquer forma de esperar por ela ou saber se terminou. É o que
  outras linguagens chamam de *daemon thread*: sobrevive até o processo acabar, e aí é
  destruída abruptamente, no meio do que estiver fazendo.

> ### ⚠ Destruir uma `std::thread` *joinable* chama `std::terminate()`
>
> ```cpp
> {
>     std::thread t(trabalho);
> }   // ← ~thread() com joinable() == true → std::terminate() → o programa ABORTA
> ```
>
> Não é vazamento e não é UB: é morte imediata do processo. E a decisão foi deliberada — as
> duas alternativas eram piores. `join()` implícito no destrutor travaria em silêncio; `detach()`
> implícito deixaria a thread viva depois que os objetos que ela captura por referência já
> tivessem sido destruídos.
>
> Como isso acontece na vida real: uma exceção salta **entre** a criação da thread e o `join()`.
> O `join()` nunca executa, a pilha desenrola, o destrutor de `t` roda com `joinable() == true`,
> e o processo morre. A solução é RAII, logo abaixo.

> ### ⚠ `detach()` + captura por referência = ponteiro pendurado
>
> ```cpp
> void perigoso() {
>     int resultado = 0;
>     std::thread t([&resultado] { resultado = 999; });  // captura por REFERÊNCIA
>     t.detach();
> }   // resultado morre aqui — a thread pode escrever no endereço depois. UB.
> ```
>
> Só use `detach()` quando a thread não tocar em **nada** do escopo que a criou. Na dúvida,
> capture por valor e faça `join()`.

### Argumentos: `std::thread` **copia**

```cpp
void incrementar(int& valor, int quantidade);

int resultado = 0;
std::thread t(incrementar, std::ref(resultado), 42);  // std::ref é OBRIGATÓRIO
t.join();
// resultado == 42
```

Os argumentos são **copiados** para o armazenamento interno da thread, sempre. Se o parâmetro
é `int&`, passar `resultado` cru não compila (a cópia não se liga a uma referência não-const);
`std::ref(x)` embrulha a referência num `reference_wrapper` copiável e a intenção fica
explícita no código. Para referência constante, `std::cref(x)`.

Esse "explícito" é proposital: passar por referência para uma thread significa que o objeto
precisa **sobreviver** à thread. O `std::ref` é você assinando embaixo.

### `std::this_thread` — a thread de dentro

```cpp
std::this_thread::get_id();            // identificador único desta thread
std::this_thread::sleep_for(10ms);     // dorme por uma duração
std::this_thread::sleep_until(t);      // dorme até um instante
std::this_thread::yield();             // "posso ceder o núcleo agora"
```

`yield()` não dorme: apenas sugere ao escalonador que outra thread pronta rode agora. É o que
se usa dentro de uma espera ativa curta, para não queimar o quantum inteiro girando à toa.

### `thread_local` — estado sem compartilhamento

```cpp
thread_local int contador_local = 0;   // UMA variável POR THREAD
```

Cada thread recebe sua própria instância, criada quando ela a usa pela primeira vez e destruída
quando ela termina. Não há compartilhamento, portanto **não há data race e não é preciso
mutex algum**.

Na execução do programa, três threads incrementando o mesmo *nome* de variável 5, 10 e 3 vezes
imprimem exatamente 5, 10 e 3 — nenhuma enxerga o contador da outra:

```
  [t2] contador_local = 10 (exclusivo desta thread)
  [t3] contador_local = 3 (exclusivo desta thread)
  [t1] contador_local = 5 (exclusivo desta thread)
```

Usos típicos: *buffers* de rascunho, semente de gerador aleatório, contexto de log, o
`errno` da vida. É a estratégia mais barata que existe para eliminar contenção — em vez de
proteger o dado compartilhado, **não compartilhe**.

### `hardware_concurrency()` — quantas threads criar?

```cpp
unsigned n = std::thread::hardware_concurrency();   // mediu 24 nesta máquina
if (n == 0) n = 4;   // 0 significa "não sei" — o padrão permite. Tenha um plano B.
```

A regra prática, e o motivo dela:

- **trabalho de CPU** (cálculo puro): ~`hardware_concurrency()` threads. Mais que isso só
  acrescenta troca de contexto — não existem núcleos para elas;
- **trabalho de I/O** (rede, disco): pode ser bem mais, porque as threads passam a maior parte
  do tempo bloqueadas, sem ocupar núcleo nenhum;
- na dúvida, **meça**. E prefira um [thread pool](#demo-6-padrões--thread-pool-e-pipeline) a
  escolher o número na mão em cada lugar.

### RAII: nunca esquecer o `join`

Se o destrutor de uma thread *joinable* mata o programa, a solução é a mesma de qualquer
outro recurso em C++: um objeto cujo destrutor faz a limpeza.

```cpp
class ThreadGuard {
public:
    explicit ThreadGuard(std::thread t) : t_(std::move(t)) {}
    ~ThreadGuard() { if (t_.joinable()) t_.join(); }

    ThreadGuard(const ThreadGuard&)            = delete;   // dono exclusivo
    ThreadGuard& operator=(const ThreadGuard&) = delete;

private:
    std::thread t_;
};

// uso:
{
    ThreadGuard g(std::thread([] { /* trabalho */ }));
}   // join automático aqui — inclusive se uma exceção estiver desenrolando a pilha
```

O guard **toma posse** da thread (recebe por valor e move), em vez de guardar uma referência
para uma thread que vive fora dele. É a diferença entre `unique_ptr` e um ponteiro cru.

> Desde o **C++20 você não precisa escrever esta classe**: `std::jthread` faz exatamente isso
> (e ainda pede parada cooperativa) de fábrica. Ver
> [`src/concurrency2/`](../concurrency2/#demo-1-jthread). O `ThreadGuard` continua aqui porque
> entender *por que* ele existe é o que ensina o problema.

---

## Demo 2: `mutex` — exclusão mútua

Fonte: [`demo_mutex.hpp`](demo_mutex.hpp)

### O problema: `contador++` não é uma operação

```cpp
int contador = 0;

auto incrementar = [&] {
    for (int i = 0; i < 10000; ++i)
        ++contador;      // ⚠ data race: leia → some 1 → escreva. TRÊS passos.
};

std::thread t1(incrementar);
std::thread t2(incrementar);
t1.join();
t2.join();
// resultado: comportamento INDEFINIDO.
// Na prática costuma sair entre 10000 e 20000 — mas o compilador não deve nada aqui.
```

`++contador` compila para algo como `carregue da memória` / `some 1` / `guarde na memória`.
Se as duas threads carregam o valor 7 antes de qualquer uma guardar, ambas guardam 8: um
incremento **evaporou**.

⚠ E a frase decisiva: dizer "o resultado fica entre 10000 e 20000" **já é otimismo indevido**.
Isso é um data race, e data race é comportamento indefinido — o compilador tem permissão de
assumir que ele não acontece e reescrever o laço inteiro. O intervalo é o que costuma
acontecer, não o que é garantido.

**E este demo, nesta máquina, imprime o resultado certo em 100% das execuções** (ver
[Por que estudar isto?](#por-que-estudar-isto)): as duas threads são curtas o bastante para a
primeira terminar antes de a segunda começar. O race continua existindo — o
[TSan prova](#como-validar). É a demonstração mais honesta possível de que "rodou e deu certo"
não é evidência de nada em concorrência.

### A solução: `std::lock_guard` (o padrão do dia a dia)

```cpp
std::mutex mtx;
int contador = 0;

auto incrementar = [&] {
    for (int i = 0; i < 10000; ++i) {
        std::lock_guard<std::mutex> trava(mtx);  // lock() no construtor
        ++contador;                              // seção crítica
    }                                            // unlock() no destrutor
};
// resultado: sempre 20000
```

O mutex não protege *o dado* — ele não sabe que `contador` existe. Ele protege **o trecho de
código**. É uma convenção que só funciona se **todos** os acessos ao dado passarem pelo mesmo
mutex. Por isso a prática recomendada é encapsular: mutex privado, dado privado, e métodos
públicos que travam antes de tocar no dado.

Por que não `mtx.lock()` / `mtx.unlock()` na mão? Porque qualquer `return`, `break`,
`continue` ou exceção entre os dois pula o `unlock()`, e aí o mutex fica travado **para
sempre**: a próxima thread que tentar entrar dorme e nunca acorda. `lock_guard` é a mesma ideia
de `unique_ptr` aplicada a cadeados.

⚠ `std::lock_guard<std::mutex> (mtx);` com parênteses em volta do nome declara uma *variável
chamada `mtx`*, não uma trava — e o compilador não reclama. Dê sempre um nome à trava
(`trava`, `lk`), nunca a deixe anônima: `std::lock_guard<std::mutex>(mtx);` cria um temporário
que é destruído **na mesma linha**, deixando a seção crítica desprotegida.

### `std::unique_lock` — quando você precisa de folga

```cpp
std::unique_lock<std::mutex> lk(mtx, std::defer_lock);  // NÃO trava ainda
// ... trabalho que não precisa do lock ...
lk.lock();                                              // trava quando quiser
// ... seção crítica ...
lk.unlock();                                            // e pode destravar antes do fim

std::unique_lock<std::mutex> lk2(mtx2, std::try_to_lock);  // tenta sem bloquear
if (lk2.owns_lock()) { /* consegui */ }
```

Comparado ao `lock_guard`, o `unique_lock` guarda um bit a mais ("estou ou não travado") e por
isso permite destravar e retravar, ser **movido** para outra função, e — o uso obrigatório —
ser passado para `condition_variable::wait()` (Demo 3), que precisa destravar e retravar por
baixo.

| | `lock_guard` | `unique_lock` | `scoped_lock` (C++17) |
|---|---|---|---|
| trava no construtor | sim | opcional (`defer_lock`) | sim |
| destravar/retravar | não | sim | não |
| movível | não | sim | não |
| serve para `condition_variable` | não | **sim** | não |
| vários mutexes de uma vez | não | não | **sim, sem deadlock** |
| custo | zero | um `bool` a mais | zero |

Regra: **`lock_guard` por padrão** (ou `scoped_lock`, que o substitui bem em C++17),
`unique_lock` só quando precisar de uma das capacidades extras.

### `std::shared_mutex` — muitos leitores, um escritor

Se o dado é **lido muito e escrito pouco** (cache, tabela de configuração, tabela de rotas),
um mutex comum serializa até os leitores, que não atrapalhariam uns aos outros.

```cpp
#include <shared_mutex>

std::shared_mutex rw;
std::string dado = "valor inicial";

auto ler = [&] {
    std::shared_lock lk(rw);      // COMPARTILHADO: vários leitores ao mesmo tempo
    return dado;
};

auto escrever = [&](std::string novo) {
    std::unique_lock lk(rw);      // EXCLUSIVO: bloqueia leitores e escritores
    dado = std::move(novo);
};
```

⚠ Não é grátis: um `shared_mutex` é bem mais caro que um `std::mutex` (precisa contar
leitores atomicamente). Se a seção crítica é curta, o `std::mutex` simples costuma ganhar
mesmo com muitas leituras. Vale quando a leitura é **longa**, e sempre depois de medir.

### `std::call_once` — inicializar exatamente uma vez

```cpp
std::once_flag flag;
int recurso = 0;

auto usar = [&] {
    std::call_once(flag, [&] {
        recurso = 42;             // roda UMA vez, mesmo com 3 threads chegando juntas
    });
    // aqui `recurso` está garantidamente pronto
};
```

A garantia forte: as threads que perderam a corrida **esperam** a inicialização terminar antes
de seguir. Uma `std::atomic_flag` diria quem chegou primeiro, mas deixaria as outras passarem
direto e usarem um recurso ainda não construído.

Alternativa mais simples, e preferível quando serve: **uma variável estática local**.

```cpp
Config& config() {
    static Config c = carregar_do_disco();   // thread-safe desde o C++11 ("magic statics")
    return c;
}
```

O compilador gera exatamente a mesma espera. Use `call_once` quando a inicialização não couber
numa estática — por exemplo, quando depende de argumentos ou precisa reagir a falhas (se o
callable lançar, o flag **não** é consumido e a próxima thread tenta de novo).

### Deadlock — duas threads, dois mutexes

Até aqui, um mutex. O problema aparece com dois:

```cpp
std::mutex m1, m2;

// Thread A                     // Thread B
std::lock_guard a(m1);          std::lock_guard b(m2);   // ← ordem INVERTIDA
std::lock_guard a2(m2);         std::lock_guard b2(m1);
```

Se A tranca `m1` e B tranca `m2` antes de qualquer uma pedir o segundo, chega-se a isto:

```
        segura                espera
  A  ──── m1 ────────►  ┌──── m2
                        │      ▲
                        │      │ segura
                        └───►  B
                     espera m1

  espera circular: nenhuma solta o que tem, nenhuma consegue o que falta.
  O programa TRAVA. Para sempre. Sem erro, sem log, sem estouro de pilha.
```

As quatro condições clássicas do deadlock (Coffman) são exclusão mútua, posse com espera,
não-preempção e **espera circular**. As três primeiras são inerentes a um mutex; a quarta é a
única que você controla — e é por onde se ataca.

**Solução 1 (a que se usa): peça os dois de uma vez.**

```cpp
std::scoped_lock trava(m1, m2);   // C++17 — trava os DOIS ou nenhum, na ordem que der certo
```

`std::scoped_lock` com vários mutexes usa um algoritmo à prova de deadlock: tenta travar, e se
o segundo não vem, **solta o primeiro** e recomeça. Como nunca fica segurando um enquanto
espera o outro, a espera circular deixa de existir. É irrelevante em que ordem você lista os
mutexes. Em C++11 o equivalente é mais verboso:

```cpp
std::lock(m1, m2);                                   // trava os dois
std::lock_guard<std::mutex> l1(m1, std::adopt_lock); // "já está travado, só cuide de soltar"
std::lock_guard<std::mutex> l2(m2, std::adopt_lock);
```

`std::adopt_lock` diz ao guard: não trave, o mutex já está travado — apenas destrave no
destrutor. É o que faz [`demo_mutex.hpp:257`](demo_mutex.hpp) e por isso o demo 2.7 imprime
`nenhum deadlock!` mesmo com uma das threads pedindo os mutexes na ordem inversa.

**Solução 2 (quando os locks não podem ser pedidos juntos): ordem global.** Defina uma ordem
total para todos os mutexes do sistema (por endereço, por id, por nível de hierarquia) e
**sempre** adquira nessa ordem. Sem inversão, sem ciclo. É a disciplina que grandes bases de
código adotam, porque `scoped_lock` só resolve quando os dois locks são pedidos no mesmo ponto.

⚠ Três regras que evitam a maioria dos deadlocks reais:

- **nunca chame código de terceiros com um lock na mão** — ele pode travar outro mutex, e você
  não controla a ordem;
- **segure um lock pelo menor tempo possível**, e nunca através de I/O;
- se você precisa de dois locks, pergunte antes se não dá para precisar de um só.

---

## Demo 3: `condition_variable` — esperar sem queimar CPU

Fonte: [`demo_condition.hpp`](demo_condition.hpp)

O mutex protege *dados*. A `condition_variable` coordena *quando* agir. Sem ela, a única forma
de esperar por uma condição seria a espera ativa:

```cpp
while (!pronto) { }   // ⚠ queima 100% de um núcleo sem fazer nada. E, sem atômico, é UB.
```

Com ela a thread **dorme de verdade** (o SO a tira da fila de execução) e só volta quando
alguém sinaliza.

### O par `wait` / `notify`

```cpp
std::mutex mtx;
std::condition_variable cv;
bool pronto = false;
int  dado   = 0;

// Consumidor
std::thread consumidor([&] {
    std::unique_lock<std::mutex> lk(mtx);
    cv.wait(lk, [&] { return pronto; });   // ← o predicado é obrigatório na prática
    std::cout << dado << '\n';             // aqui pronto é true E o mutex está travado
});

// Produtor
{
    std::lock_guard<std::mutex> lk(mtx);
    dado   = 42;
    pronto = true;
}                        // solta o mutex ANTES de notificar
cv.notify_one();
```

`cv.wait(lk, pred)` faz, em laço:

1. avalia `pred()`. Se já for verdadeiro, **retorna na hora** — nem dorme;
2. senão, **destrava o mutex** (senão o produtor nunca entraria) e dorme, atomicamente;
3. ao acordar, **retrava o mutex** e volta ao passo 1.

Quando `wait` retorna, você tem as duas garantias que interessam: a condição é verdadeira **e**
o mutex está na sua mão.

> ### ⚠ Nunca use `wait(lk)` sem predicado
>
> `cv.wait(lk)` sozinho tem dois modos de falhar:
>
> - **spurious wakeup**: o SO pode acordar a thread **sem** que ninguém tenha notificado. Não é
>   bug, é permitido pelo POSIX e pelo padrão C++;
> - **notificação perdida**: se o produtor notificar **antes** de o consumidor chegar ao `wait`,
>   o sinal se perde — `condition_variable` não guarda estado. O consumidor dorme para sempre.
>
> O predicado resolve os dois: ele é reconferido a cada acordada (mata o spurious) e é
> conferido **antes** de dormir (mata a notificação perdida). `cv.wait(lk, pred)` é
> literalmente `while (!pred()) cv.wait(lk);`.
>
> A condição precisa ser um **estado durável** protegido pelo mutex (`pronto`, `!fila.empty()`)
> — nunca "o evento aconteceu agora".

Detalhe do exemplo: o produtor solta o mutex **antes** de `notify_one()`. Se notificasse com o
mutex na mão, a thread acordada tentaria retravá-lo, encontraria travado e voltaria a dormir —
uma ida e volta desperdiçada (*hurry up and wait*). Correto nos dois casos, mais rápido assim.

### `notify_one` vs `notify_all`

| | acorda | quando usar |
|---|---|---|
| `notify_one()` | **uma** thread esperando (qual, é decisão do SO) | todas as esperas são equivalentes e um item serve a exatamente uma delas — a fila de um thread pool |
| `notify_all()` | **todas** | a mudança interessa a todas (uma "largada"), ou as threads esperam por **condições diferentes** no mesmo `cv` |

⚠ Com `notify_all` e N threads esperando, todas acordam, todas tentam retravar o mesmo mutex,
uma segue e N-1 voltam a dormir. É o *thundering herd* ("rebanho acordado") — desperdício
proporcional a N. Use `notify_one` sempre que a semântica permitir.

⚠ E o inverso é pior: usar `notify_one` quando as threads esperam por condições **diferentes**
pode acordar justamente a que não pode prosseguir, enquanto a que podia continua dormindo. Se
há mais de uma condição no mesmo `cv`, use `notify_all` — ou, melhor, use um `cv` por condição.

O demo 3.2 usa `notify_all` para uma largada: três "corredores" bloqueiam em
`cv.wait(lk, [&]{ return iniciar; })` e só saem depois do `GO!`.

### `wait_for` / `wait_until` — espera com prazo

```cpp
// Com predicado: retorna bool — true = condição satisfeita, false = deu o prazo
bool ok = cv.wait_for(lk, 50ms, [&] { return pronto; });
if (!ok) { /* timeout */ }

// Sem predicado: retorna std::cv_status::timeout ou ::no_timeout
// (aí você mesmo precisa reconferir a condição — spurious wakeup continua valendo)
```

Use quando esperar para sempre é inaceitável: um serviço que precisa responder a um
desligamento, um *heartbeat*, um limite de tempo de requisição. O demo 3.3 deliberadamente não
notifica ninguém e imprime `[t] timeout! pronto ainda é false`.

⚠ Prefira `wait_until` (instante absoluto) quando estiver dentro de um laço com prazo total.
Com `wait_for` dentro de um laço, cada spurious wakeup **reinicia** a contagem, e o prazo
global se estica sem que você perceba.

### A peça que sai daqui: fila thread-safe

Juntando mutex + `condition_variable`, sai a estrutura mais reutilizada da concorrência —
[`FilaSegura`](demo_condition.hpp), a base do produtor/consumidor:

```cpp
template <typename T>
class FilaSegura {
public:
    void enfileirar(T item) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            fila_.push(std::move(item));
        }                       // solta antes de notificar
        cv_.notify_one();
    }

    T desenfileirar() {         // bloqueia até haver item
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this] { return !fila_.empty(); });
        T item = std::move(fila_.front());
        fila_.pop();
        return item;
    }

private:
    mutable std::mutex      mtx_;
    std::condition_variable cv_;
    std::queue<T>           fila_;
};
```

⚠ Note que **não existe** um método `vazia()` útil para decidir se vale a pena chamar
`desenfileirar()`: entre o `if (!fila.vazia())` e o `desenfileirar()`, outra thread pode
esvaziar a fila. Qualquer consulta de estado a uma estrutura concorrente já nasce velha. É por
isso que a operação correta é uma só (`desenfileirar` espera por conta própria), e não duas.
O `mutable` no mutex é o que permite travá-lo dentro de métodos `const`.

---

## Demo 4: `future` e `promise` — resultados entre threads

Fonte: [`demo_futures.hpp`](demo_futures.hpp)

Mutex e `condition_variable` sincronizam **acesso**. `future`/`promise` transferem um
**resultado**: "vou te entregar um `int`, ainda não sei quando". Você não declara variável
compartilhada, não declara mutex, e ganha de brinde o transporte de exceções.

- `std::future<T>` — o lado do **leitor**. `.get()` bloqueia até o valor existir e o devolve.
  Só pode ser chamado **uma vez** (ele *move* o resultado para fora).
- `std::promise<T>` — o lado do **escritor**. `.set_value(v)` ou `.set_exception(e)`.

### `std::async` — o caminho curto

```cpp
auto futuro = std::async(std::launch::async, [] {
    std::this_thread::sleep_for(20ms);
    return 42;
});

// ... outro trabalho aqui, em paralelo ...

int val = futuro.get();   // bloqueia se ainda não terminou
```

O primeiro argumento é uma **política de lançamento**, e ela importa:

| política | o que faz |
|---|---|
| `std::launch::async` | **garante** uma thread nova; começa imediatamente |
| `std::launch::deferred` | não cria thread nenhuma: a função só roda **dentro do `.get()`**, na thread que chamou (avaliação preguiçosa). Se você nunca chamar `.get()`, ela **nunca roda** |
| omitido (o padrão) | `async \| deferred`: a implementação escolhe. Pode virar `deferred` silenciosamente e destruir todo o paralelismo que você esperava |

⚠ **Sempre passe `std::launch::async` explicitamente** quando quiser paralelismo de verdade.

> ### ⚠ A armadilha clássica: o destrutor do `future` de `async` **bloqueia**
>
> O `std::future` devolvido por `std::async` é especial: o destrutor dele espera a tarefa
> terminar. Consequência direta:
>
> ```cpp
> std::async(std::launch::async, tarefa);   // ⚠ o temporário é destruído nesta MESMA linha
>                                           //    → espera a tarefa acabar → é SÍNCRONO
> ```
>
> Medido nesta máquina, três tarefas de 100 ms cada:
>
> ```
> sem capturar o future : 301 ms     ← uma depois da outra
> capturando os futures : 101 ms     ← de fato em paralelo
> ```
>
> A mesma armadilha aparece dentro de um laço: `for (...) { auto f = std::async(...); }` faz
> `f` morrer a cada volta e serializa tudo. O certo é guardar os futuros e só depois coletar:
>
> ```cpp
> std::vector<std::future<int>> futuros;
> for (int i = 0; i < 4; ++i)
>     futuros.push_back(std::async(std::launch::async, tarefa, i));
> for (auto& f : futuros) usar(f.get());     // agora sim, em paralelo
> ```
>
> O GCC ajuda: `std::async` é `[[nodiscard]]`, então descartar o retorno gera
> `-Wunused-result`. Não ignore esse aviso.
>
> (Um `std::future` obtido de `promise` ou `packaged_task` **não** tem esse destrutor
> bloqueante. A regra vale só para o de `async`.)

### `std::promise` — quando o valor não vem de "chamar uma função"

```cpp
std::promise<int> promessa;
std::future<int>  futuro = promessa.get_future();   // conecta os dois lados

std::thread produtor([](std::promise<int> p) {
    p.set_value(99);                                // cumpre a promessa
}, std::move(promessa));                            // promise é movível, não copiável

int val = futuro.get();
produtor.join();
```

Use quando o resultado chega de um **callback**, de um evento, do meio de um algoritmo, ou de
um lugar que não é "o retorno de uma função" — nesses casos `async` não se aplica.

⚠ Se a `promise` for destruída sem `set_value` nem `set_exception`, o `future` recebe uma
exceção `std::future_error` com código `broken_promise`. Melhor que travar para sempre, mas
ainda é um bug: alguém esqueceu de cumprir.

### Exceções atravessam a fronteira de graça

```cpp
auto futuro = std::async(std::launch::async, []() -> int {
    throw std::runtime_error("algo deu errado na thread!");
});

try {
    futuro.get();                        // a exceção é RELANÇADA aqui
} catch (const std::exception& e) {
    std::cout << e.what() << '\n';       // "algo deu errado na thread!"
}
```

Isto é o que `future` tem de melhor. Uma exceção que escapa do topo de uma `std::thread` chama
`std::terminate()` e mata o processo — não há como capturá-la de fora. Com `future`, ela é
armazenada e relançada em `.get()`, na thread que espera. Manualmente, o padrão é
`catch (...) { p.set_exception(std::current_exception()); }`. Ver [`src/errors/`](../errors/).

### `std::packaged_task` — separar "o que fazer" de "quando fazer"

```cpp
std::packaged_task<int(int, int)> tarefa(calcular);
std::future<int> futuro = tarefa.get_future();

std::thread t(std::move(tarefa), 6, 7);   // executa em outro lugar (aqui: outra thread)
int r = futuro.get();
t.join();
```

`packaged_task` embrulha um chamável e liga o retorno dele a um `future`, **sem** decidir onde
vai rodar. É exatamente a peça que falta para construir um thread pool: você empacota, empurra
para uma fila, e alguma worker executa mais tarde. É o que o [Demo 6](#demo-6-padrões--thread-pool-e-pipeline)
faz em `ThreadPool::submit`.

### `std::shared_future` — um resultado, vários leitores

```cpp
std::promise<std::string> p;
std::shared_future<std::string> sf = p.get_future().share();

auto leitor = [sf](int id) {     // copiado para cada thread
    auto r = sf.get();           // TODOS podem chamar .get(), quantas vezes quiserem
};
```

`future::get()` só pode ser chamado uma vez porque ele **move** o valor. `.share()` converte
num `shared_future`, copiável, cujo `.get()` devolve uma referência const ao valor guardado.
Use para difundir um resultado único a várias threads — um sinal de "configuração carregada",
uma "largada" com valor.

### Qual dos quatro usar?

```
tenho uma função e quero rodá-la agora, em paralelo ........ std::async(std::launch::async, f)
tenho uma função e quero decidir DEPOIS onde ela roda ...... std::packaged_task
o valor vem de um callback / evento / meio de um algoritmo . std::promise
vários esperam pelo MESMO resultado ........................ std::shared_future
preciso controlar a thread (afinidade, prioridade, nome) ... std::thread / std::jthread
```

---

## Demo 5: `std::atomic` e o modelo de memória

Fonte: [`demo_atomics.hpp`](demo_atomics.hpp)

### O que `std::atomic<T>` garante

```cpp
std::atomic<int> contador{0};

++contador;                       // indivisível: nenhum incremento se perde
contador.fetch_add(1);            // idem, e devolve o valor ANTERIOR
contador.fetch_sub(3);
contador.store(42);               // escrita atômica
int v = contador.load();          // leitura atômica
int velho = contador.exchange(100);   // escreve e devolve o anterior
```

A garantia é **atomicidade**: a operação acontece por inteiro ou não acontece; nenhuma outra
thread observa um estado intermediário. Não há mutex nem troca de contexto — o hardware faz o
trabalho, em geral com uma instrução dedicada (no x86-64, `lock xadd` para o `fetch_add`).

⚠ "Lock-free" tem letras miúdas: **`std::atomic<T>` só é lock-free para `T` pequeno**. Para
tipos grandes, a implementação usa um mutex escondido — continua correto, mas não é lock-free e
não pode ser usado em *signal handler*. Confira com `std::atomic<T>::is_always_lock_free` (um
`static constexpr`, avaliado em compilação). O corte no x86-64 é 16 bytes; medições em
[`src/concurrency2/`](../concurrency2/#demo-3-o-modelo-de-memória).

⚠ E o erro mais comum de todos: **atômico não torna a sua lógica atômica.**

```cpp
std::atomic<int> x;
if (x > 0) x = x - 1;   // ⚠ isto é um load, uma comparação e um store: TRÊS operações
```

Entre o `if` e a atribuição, outra thread pode mexer em `x`. Para uma sequência
leia-modifique-escreva composta, você precisa de um laço de CAS ou de um mutex.

### Compare-And-Swap (CAS) — a primitiva de que tudo o mais é feito

```cpp
// compare_exchange_strong(esperado, desejado):
//   se *this == esperado  → *this = desejado, retorna true
//   se *this != esperado  → esperado = *this (por REFERÊNCIA!), retorna false
std::atomic<int> valor{10};
int esperado = 10;
bool ok = valor.compare_exchange_strong(esperado, 20);   // true; valor == 20
```

"Se ninguém mexeu desde que eu li, aplique minha mudança; senão, me diga qual é o valor atual
para eu tentar de novo." O fato de `esperado` ser atualizado na falha é o que faz o laço de
retry convergir sem uma releitura extra:

```cpp
int atual = contador.load(std::memory_order_relaxed);
while (!contador.compare_exchange_weak(
           atual, atual + 1,
           std::memory_order_release,     // ordem no SUCESSO
           std::memory_order_relaxed)) {  // ordem na FALHA (não pode ser mais forte)
    // corpo VAZIO de propósito: o CAS já reescreveu `atual` com o valor real
}
```

`_weak` pode falhar **espuriamente** (mesmo com o valor correto), porque em ARM e POWER ele
mapeia direto no par de instruções LL/SC, que falha se qualquer coisa tocar a linha de cache.
Em troca é mais barato. Use `_weak` **dentro de um laço** (a falha só custa outra volta) e
`_strong` quando não houver laço externo. Custo do retry medido em
[`src/concurrency2/`](../concurrency2/#demo-4-cas-atomic_flag-e-espera-atômica).

### Memory ordering: o que o compilador e a CPU podem reordenar

Ninguém executa o seu programa na ordem em que você o escreveu. O compilador reordena para
otimizar; a CPU executa fora de ordem; os caches propagam escritas para núcleos diferentes em
momentos diferentes. Em uma thread isso é invisível — a linguagem garante que o *resultado*
seja o mesmo. **Entre** threads, é exatamente o que causa os bugs.

Cada operação atômica leva um `std::memory_order` que diz quanta reordenação continua
permitida em volta dela:

| `std::memory_order_` | O que garante | Custo |
|---|---|---|
| `relaxed` | **só** atomicidade. Nenhuma garantia de ordem em relação a outras variáveis | mínimo |
| `acquire` (em loads) | nenhuma operação de memória **posterior** no código pode ser movida para **antes** deste load | ver nota |
| `release` (em stores) | nenhuma operação de memória **anterior** no código pode ser movida para **depois** deste store | ver nota |
| `acq_rel` (em RMW) | as duas coisas, para operações leia-modifique-escreva (`fetch_add`, CAS) | ver nota |
| `seq_cst` | tudo de `acq_rel` **mais** uma ordem total única sobre a qual todas as threads concordam | máximo (**é o padrão**) |

⚠ Note a formulação: `acquire` e `release` restringem **todas as operações de memória**
vizinhas — atômicas ou não, leituras ou escritas — e não apenas "as leituras" ou "as
escritas". É justamente por restringirem os acessos **comuns** em volta que eles servem para
publicar dados não atômicos.

> ⚠ **Sobre o custo:** ele depende da arquitetura, e a intuição "seq_cst é caro" engana. Em
> **x86-64**, que é fortemente ordenado, `acquire`/`release` saem praticamente de graça e a
> diferença para `seq_cst` num `fetch_add` fica no ruído: medido em
> [`src/concurrency2/`](../concurrency2/#seq_cst--o-padrão-e-o-preço), a razão
> `seq_cst / relaxed` variou entre **0,95× e 1,14×** em 8 execuções — ou seja, às vezes o
> `relaxed` saiu *mais lento*. Em **ARM ou POWER**, onde `seq_cst` exige instruções de barreira
> explícitas (`dmb ish`), a diferença é real. Conclusão: "otimizei trocando para relaxed"
> precisa ser **medido na arquitetura alvo**.

### O padrão acquire/release: publicar dados sem mutex

```cpp
#include <atomic>
#include <thread>

std::atomic<bool> publicado{false};
int dados = 0;                       // NÃO atômico — protegido pelo handshake abaixo

// Produtor
std::thread produtor([&] {
    dados = 42;                                              // escrita comum
    publicado.store(true, std::memory_order_release);        // publica tudo de uma vez
});

// Consumidor
std::thread consumidor([&] {
    while (!publicado.load(std::memory_order_acquire))
        std::this_thread::yield();                           // ← cede o núcleo enquanto espera
    std::cout << dados << '\n';                              // garantidamente 42
});
```

A regra, em uma frase: **se o load-acquire lê o valor escrito pelo store-release, então tudo
que a thread produtora escreveu antes do store é visível para a consumidora depois do load.**
Uma única operação atômica publica uma estrutura inteira.

⚠ Duas letras miúdas que invalidam tudo se ignoradas:

- **`release` sozinho não sincroniza nada.** A sincronização só existe se o `acquire` **ler o
  valor que aquele `release` escreveu**. Ler um valor mais antigo não estabelece relação
  nenhuma;
- **o par tem de ser no mesmo átomo.** `release` em `a` e `acquire` em `b` não sincronizam.

⚠ E note o `yield()` no laço de espera: um `while (!flag.load(...));` puro é uma espera ativa
que queima um núcleo inteiro. Em código de verdade, prefira `condition_variable`, ou o
`atomic::wait` do C++20 (que dorme de verdade) — ver
[`src/concurrency2/`](../concurrency2/#demo-4-cas-atomic_flag-e-espera-atômica).

Note também que **todos os `memory_order` acima estão qualificados com `std::`**. Eles vivem no
namespace `std`; sem o `using`, o código não compila sem a qualificação.

### `relaxed` — onde ele é legítimo

```cpp
std::atomic<long> hits{0};
hits.fetch_add(1, std::memory_order_relaxed);   // contador de estatística
```

Legítimo: contadores de telemetria, flags de "já vi isto", incremento de contagem de
referências. Errado: **publicar dados**. `pronto.store(true, relaxed)` permite que a outra
thread veja `pronto == true` e `dados == 0`.

### `atomic` ou `mutex`?

```
atomic<T>                                    mutex
  ✓ T é simples: int, bool, ponteiro           ✓ proteger VÁRIAS variáveis como unidade
  ✓ a operação é uma instrução                 ✓ seção crítica não trivial
  ✓ signal handler (se is_always_lock_free)    ✓ integra com condition_variable
  ✗ operações compostas                        ✗ contenção custa troca de contexto
  ✗ estruturas de dados complexas
```

Regra prática: **comece com `mutex`**. Ele é mais fácil de acertar, de revisar e de estender.
Só migre para atômico quando o profiler apontar contenção real — e quando você conseguir
escrever qual `happens-before` está preservando. Se não consegue escrever, não migre. Os
números que sustentam essa recomendação estão medidos em
[`src/concurrency2/`](../concurrency2/#demo-5-lock-free-na-prática).

---

## Demo 6: Padrões — thread pool e pipeline

Fonte: [`demo_patterns.hpp`](demo_patterns.hpp)

### Por que um pool: a conta que justifica tudo

Criar uma thread não é criar um objeto. O sistema operacional aloca uma pilha (tipicamente 8 MB
de espaço virtual), registra a thread no escalonador e faz uma chamada de sistema.

**Medido nesta máquina** (GCC 13, `-O2`, média de 2000 repetições): criar e juntar uma
`std::thread` que não faz nada custou **~48 µs**.

Ponha esse número na conta de um servidor que processa 10 000 tarefas de 1 µs cada:

| estratégia | custo |
|---|---|
| uma thread por tarefa | 10 000 × (48 µs de gestão + 1 µs de trabalho) ≈ **490 ms** |
| pool de 8 threads | 8 × 48 µs + 10 000 × 1 µs ≈ **10 ms** |

98% do tempo estava sendo gasto em *criar threads para jogá-las fora*. O thread pool inverte
isso: cria as threads **uma vez**, e elas ficam consumindo tarefas de uma fila.

### O `ThreadPool`

```cpp
class ThreadPool {
public:
    explicit ThreadPool(std::size_t n) {
        workers_.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            workers_.emplace_back([this] { loop_worker(); });
    }

    ~ThreadPool() {
        { std::unique_lock<std::mutex> lk(mtx_); parar_ = true; }
        cv_.notify_all();                       // acorda TODAS para que saiam do laço
        for (auto& t : workers_)
            if (t.joinable()) t.join();
    }

    // Aceita qualquer chamável, devolve o future do retorno dele
    template <typename Func, typename... Args>
    auto submit(Func&& f, Args&&... args)
        -> std::future<std::invoke_result_t<Func, Args...>>
    {
        using R = std::invoke_result_t<Func, Args...>;

        auto tarefa = std::make_shared<std::packaged_task<R()>>(
            [f = std::forward<Func>(f),
             args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
                return std::apply(std::move(f), std::move(args));
            });

        std::future<R> futuro = tarefa->get_future();
        {
            std::unique_lock<std::mutex> lk(mtx_);
            if (parar_) throw std::runtime_error("ThreadPool encerrado");
            tarefas_.push([tarefa] { (*tarefa)(); });   // apaga o tipo: std::function<void()>
        }
        cv_.notify_one();
        return futuro;
    }

private:
    void loop_worker() {
        while (true) {
            std::function<void()> tarefa;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] { return parar_ || !tarefas_.empty(); });
                if (parar_ && tarefas_.empty()) return;   // só sai com a fila VAZIA
                tarefa = std::move(tarefas_.front());
                tarefas_.pop();
            }
            tarefa();       // ⚠ executa FORA do lock
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tarefas_;
    std::mutex                        mtx_;
    std::condition_variable           cv_;
    bool                              parar_ = false;
};
```

Cinco decisões de projeto que valem mais que o código:

1. **`tarefa()` roda fora do lock.** Se rodasse dentro, as outras N-1 workers ficariam paradas
   esperando o mutex — o pool viraria uma fila de uma thread só. Isso é o *ponto inteiro* do
   `{ }` em volta da região que mexe na fila;
2. **`shared_ptr<packaged_task>`.** `std::function` exige que o alvo seja copiável, e
   `packaged_task` é apenas movível. O `shared_ptr` contorna isso sem alocar duas vezes. (Em
   C++23, `std::move_only_function` resolve direto.);
3. **`if (parar_ && tarefas_.empty()) return;`** — as duas condições. Assim o destrutor não
   descarta trabalho já aceito: as workers drenam a fila antes de sair;
4. **`notify_all()` no destrutor.** Um `notify_one` acordaria uma worker só; as outras
   continuariam dormindo em `wait` e o `join()` travaria para sempre;
5. **`submit` devolve um `future`.** É assim que resultados e **exceções** voltam para quem
   pediu — uma exceção lançada dentro de uma tarefa é capturada pelo `packaged_task` e
   relançada no `.get()`, em vez de derrubar a worker (e, com ela, o processo).

⚠ Este pool é didático. Falta a ele: *work stealing* (fila por worker, para reduzir contenção
no mutex único), limite de tamanho da fila (do jeito que está, um produtor rápido demais
consome toda a memória) e prioridades. Em produção, use `std::execution` (C++17, com TBB),
`tbb::task_arena`, `folly::CPUThreadPoolExecutor` ou `asio::thread_pool`.

O demo 6.2 mostra o pool aceitando tarefas de **tipos de retorno diferentes** (`int`,
`std::string`, `double`, `void`) — é o `std::invoke_result_t` no retorno do `submit` que
permite isso.

### Pipeline — estágios em série

O outro formato clássico: em vez de N threads fazendo **a mesma** coisa, cada thread faz **uma
etapa** e passa adiante, como os `|` do shell.

```
  [gerador] ──c1──► [transformador] ──c2──► [consumidor]
   thread 1          thread 2                thread 3
     int              int → string            string
```

A cola entre os estágios é um **canal**: uma fila thread-safe que sabe ser *fechada*.

```cpp
template <typename T>
class Canal {
public:
    void enviar(T item) {
        std::lock_guard<std::mutex> lk(mtx_);
        fila_.push(std::move(item));
        cv_.notify_one();
    }

    // false = o canal está fechado E vazio: acabou de verdade
    bool receber(T& out) {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this] { return !fila_.empty() || fechado_; });
        if (fila_.empty()) return false;
        out = std::move(fila_.front());
        fila_.pop();
        return true;
    }

    void fechar() {
        std::lock_guard<std::mutex> lk(mtx_);
        fechado_ = true;
        cv_.notify_all();          // acorda todo mundo para ver que acabou
    }
    // ... membros: fila_, mtx_, cv_, fechado_
};
```

O `fechar()` é a peça que muita gente esquece e que causa travamento no encerramento: sem um
sinal explícito de fim, o consumidor fica em `wait` para sempre esperando um item que nunca
vem. Note que `receber` devolve `false` **só quando o canal está fechado *e* vazio** — os itens
já enfileirados ainda são entregues. O laço do consumidor fica limpo:

```cpp
int val;
while (c1.receber(val)) { /* processa */ }   // sai sozinho quando o produtor fecha
c2.fechar();                                  // e propaga o fim para o próximo estágio
```

⚠ Um pipeline é tão rápido quanto o seu estágio mais lento, e todos os outros ficam
bloqueados esperando por ele. Ele vale quando os estágios têm custos parecidos, ou quando um
deles é I/O (e portanto não disputa núcleo). Se um estágio domina, replique **aquele** estágio
em várias threads, ou volte para o thread pool.

---

## Armadilhas comuns

| Problema | Causa | Solução |
|---|---|---|
| **Data race** | dois acessos ao mesmo endereço, um deles escrita, sem ordenação | `mutex` ou `atomic` — e rode o TSan |
| **`std::terminate()` no destrutor** | `std::thread` destruída ainda *joinable* | RAII (`ThreadGuard`) ou `std::jthread` (C++20) |
| **Ponteiro pendurado** | `detach()` + captura por referência | capture por valor, ou faça `join()` |
| **Deadlock** | duas threads pedem dois mutexes em ordens opostas | `std::scoped_lock(m1, m2)` ou ordem global |
| **Notificação perdida / spurious wakeup** | `cv.wait(lk)` sem predicado | `cv.wait(lk, pred)`, sempre |
| **`async` que não paraleliza** | `future` temporário destruído na hora → destrutor bloqueia | guarde o `future`; use `std::launch::async` |
| **Atômico com lógica não atômica** | `if (x > 0) x = x - 1` são três operações | laço de `compare_exchange`, ou mutex |
| **Thundering herd** | `notify_all` com N threads e uma só pode prosseguir | `notify_one`, ou um `cv` por condição |
| **Criar thread por tarefa** | ~48 µs de gestão por tarefa (medido) | thread pool |
| **Problema ABA** | o CAS compara **endereços**: o valor voltou a ser A, mas não é o mesmo A | contador de versão, *hazard pointers* — ver [`src/concurrency2/`](../concurrency2/#pilha-lock-free-e-o-problema-aba) |

---

## Como validar

### Rodar o programa

```bash
make build && ./dist/bin/concurrency
```

Você deve ver **seis faixas cor de coral** — uma por bloco — e, no fim,
`✓ Todos os demos de concorrência concluídos!` em verde. Alguns marcos para conferir:

```
── 1.5 thread_local ──
  [t2] contador_local = 10 (exclusivo desta thread)   ← 5, 10 e 3: nenhuma vê a da outra
── 2.1 Race condition (sem proteção) ──
  esperado: 20000, obtido: 20000 ✓                    ← ⚠ leia a nota abaixo
── 2.7 Deadlock — como evitar ──
  nenhum deadlock!
── 3.3 wait_for() com timeout ──
  [t] timeout! pronto ainda é false
── 4.6 Soma paralela com async ──
  soma paralela = 500500, esperado = 500500, correto = true
── 6.3 Pipeline de 3 estágios ──
  [consumidor] processados 5 itens
```

⚠ A ordem das linhas **muda entre execuções** — é concorrência. Dentro do bloco 1.5, por
exemplo, `t1`, `t2` e `t3` imprimem em qualquer ordem. Isso é o esperado, não um defeito.

### Compilar avulso, com todos os avisos

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/concurrency/main.cpp -o /tmp/c1 -L$FMTP/lib -lfmtd -pthread
```

Estado atual: **compila sem nenhum aviso** com `-Wall -Wextra -Wpedantic`.

### O teste que realmente importa: ThreadSanitizer

Em concorrência, "rodou e deu certo" não é evidência. O **TSan** instrumenta cada acesso à
memória, constrói em tempo de execução o grafo de `happens-before` entre as threads e acusa
todo par de acessos conflitantes sem ordenação — **mesmo quando o race não se manifestou
naquela execução**. É a única ferramenta que enxerga o bug antes de ele acontecer.

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -O1 -g -fsanitize=thread -I$FMTP/include \
    src/concurrency/main.cpp -o /tmp/c1t -L$FMTP/lib -lfmtd -pthread && /tmp/c1t
```

(Use `-O1 -g`: `-g` dá nomes de arquivo e linha ao relatório, e otimização alta demais atrapalha
o rastreamento. O binário fica ~5–10× mais lento e usa bem mais memória — é esperado.)

**Resultado esperado: exatamente 1 achado**, e ele é proposital — o `demo_race_condition()` do
bloco 2.1 existe justamente para ser flagrado:

```
WARNING: ThreadSanitizer: data race (pid=...)
  Read of size 4 at 0x... by thread T14:
    #0 operator() src/concurrency/demo_mutex.hpp:50
  Previous write of size 4 at 0x... by thread T13:
    #0 operator() src/concurrency/demo_mutex.hpp:50
SUMMARY: ThreadSanitizer: data race src/concurrency/demo_mutex.hpp:50 in operator()
...
ThreadSanitizer: reported 1 warnings
```

A linha 50 de [`demo_mutex.hpp`](demo_mutex.hpp) é o `++contador` sem proteção. Todo o resto do
programa — os seis blocos, o thread pool, o pipeline, os atômicos — passa **limpo**.

E o detalhe que fecha o argumento: **naquela mesma execução instrumentada, o demo continuou
imprimindo `obtido: 20000 ✓`**. O valor final estava certo e o programa estava errado. É
exatamente isso que nenhum teste de valor final consegue pegar, e que o TSan pega sempre.

⚠ O TSan não pode ser combinado com o AddressSanitizer no mesmo binário (`-fsanitize=address`).
Rode em compilações separadas.

---

## Referências

Neste repositório:

- [`src/concurrency2/`](../concurrency2/) — **a continuação direta**: `jthread`, `stop_token`,
  `latch`, `barrier`, `semaphore`, o modelo de memória formal e lock-free medido
- [`src/coroutines/`](../coroutines/) — concorrência **sem** threads: suspender e retomar
- [`src/performance/`](../performance/) — *false sharing*, localidade de cache, medidos
- [`src/undefined_behavior/`](../undefined_behavior/) — o TSan e os demais sanitizers
- [`src/errors/`](../errors/) — RAII e exceções, a base do `ThreadGuard` e do `lock_guard`
- [`src/smart-pointers/`](../smart-pointers/) — por que o refcount de `shared_ptr` é atômico

Fora dele:

- Anthony Williams, *C++ Concurrency in Action*, 2ª ed. — a referência do assunto; os capítulos
  2 a 4 cobrem exatamente este subprojeto
- Herb Sutter, *atomic<> Weapons* (C++ and Beyond, 2012) — o modelo de memória em duas horas
- [cppreference: Thread support library](https://en.cppreference.com/w/cpp/thread) — a
  consulta rápida de assinaturas e garantias
- [ThreadSanitizer](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual) — o
  manual do TSan em C++
