# coroutines — Funções que param no meio e continuam depois

## O que é este projeto?

Um curso executável sobre **coroutines do C++20**: o que são, o que o compilador
faz com elas, como escrever `Generator<T>` e `Task<T>` do zero, e como usar as
coroutines nativas do Asio para I/O de rede sem bloquear threads.

```bash
make configure          # conan + meson; só na primeira vez
make build
./dist/bin/coroutines   # o alvo chama-se `coroutines` (ver meson.build)
```

---

## Antes de começar: os conceitos que você precisa

Nada aqui pressupõe mais do que C++ básico — sintaxe, funções, classes,
`unique_ptr`. Todo termo usado no resto do README é definido nesta seção.

### 1. O que é uma coroutine

Uma **função normal** tem um único ponto de saída. Quando retorna, tudo o que era
local morre: variáveis, posição no laço, tudo. Uma **coroutine** é uma função que
pode **parar no meio**, devolver o controle a quem a chamou, e **continuar mais
tarde exatamente da linha seguinte**, com as variáveis locais intactas.

```
função normal                      coroutine
chamador ──chama──> f()            chamador ──chama──> g()
                    executa tudo                       executa um pouco
chamador <──retorna─┘              chamador <─SUSPENDE─┤   (locais VIVAS)
         (tudo local morreu)       chamador ──RETOMA──> │   continua daqui
                                   chamador <─SUSPENDE─┤
                                   chamador ──RETOMA──> │
                                   chamador <──retorna─┘   (agora sim morreu)
```

**Suspender** = parar no ponto atual, guardar onde estava, devolver o controle.
**Retomar** = voltar àquele ponto exato e seguir. Exemplo mínimo completo, com o
`Generator<int>` de [`demo_generator.hpp`](demo_generator.hpp):

```cpp
Generator<int> tres() { co_yield 1; co_yield 2; co_yield 3; }
for (int n : tres()) fmt::print("{} ", n);   // imprime: 1 2 3
```

Instante a instante: (1) o `for` chama `tres()` e **nada** do corpo roda — é
"lazy", item 6; (2) o `for` pede o 1º valor, `tres()` roda até `co_yield 1` e
suspende; (3) o `for` imprime `1` — quem executa agora é o `for`, `tres()` está
parada; (4) o `for` pede o próximo, `tres()` **retoma na linha após** o
`co_yield 1`, roda até `co_yield 2` e suspende; ... até `tres()` chegar ao fim da
função, e o `for` descobrir que acabou.

Os dois lados **nunca rodam ao mesmo tempo**. Coroutine não é thread: não há
paralelismo aqui, só troca de controle.

### 2. Os três operadores

Uma função é uma coroutine se — e só se — o corpo usa um destes três. Não existe
palavra-chave `coroutine`; é o operador que muda tudo.

| Operador | O que faz | Análogo |
|---|---|---|
| `co_yield v` | suspende e **entrega** `v` a quem chamou | `yield` do Python |
| `co_await e` | suspende **até** `e` ficar pronto e devolve o resultado | `await` do JS |
| `co_return v` | **termina** a coroutine com o valor `v` | `return` normal |

⚠ `co_return` não é `return`: dentro de uma coroutine, `return valor;` não
compila. E uma coroutine não pode ser `constexpr`, `consteval`, construtor,
destrutor, nem `main`.

### 3. O frame — onde a coroutine mora

Se as locais sobrevivem à suspensão, elas não estão na pilha (a pilha é
desmontada quando o controle volta). Estão no **frame da coroutine**
(*coroutine state*), um bloco que o compilador aloca **na heap**, quase sempre:

```
   frame (heap)                                       ▲
  ┌──────────────────────────────────────────────┐    │ coroutine_handle<> h
  │ promise_type promise;  ← controle (item 5)   │────┘ é só um ponteiro
  │ parâmetros COPIADOS    ← cópia dos argumentos│       para este bloco
  │ locais vivas           ← as que cruzam uma suspensão │
  │ int __state;           ← em qual ponto de suspensão  │
  └──────────────────────────────────────────────┘
```

- **Chamar uma coroutine custa uma alocação de heap.** Um milhão de coroutines
  num laço quente é um milhão de `new`. Ver [`../performance/`](../performance/).
- **Heap elision**: o padrão *permite* pôr o frame na pilha quando o compilador
  prova que ele não sobrevive ao chamador. Exige enxergar o corpo inteiro, então
  quase nunca acontece através de fronteiras de unidade de tradução.
- **O frame não se libera sozinho** se `final_suspend()` suspender (item 6). Por
  isso `Generator<T>` e `Task<T>` são RAII: o destrutor chama `handle.destroy()`.
  Sem isso, cada coroutine criada vaza.

### 4. `coroutine_handle` — a alça do frame

`std::coroutine_handle<P>` é um ponteiro cru tipado para o frame. ⚠ Ele **não é
dono de nada**: copia como um `int`, e dois `destroy()` no mesmo frame são
*double free*. Daí o RAII.

```cpp
h.resume();    // volta ao ponto de suspensão e continua
h.done();      // já chegou ao final_suspend?
h.destroy();   // libera o frame — nunca numa coroutine em execução
h.address();   // void* bruto, para atravessar APIs C
h.promise();   // referência ao promise_type que vive dentro do frame
std::coroutine_handle<P>::from_promise(p);   // promise  → handle
std::coroutine_handle<>::from_address(ptr);  // void*    → handle
```

### 5. `promise_type` — o objeto de controle, e a ordem das chamadas

Para `Generator<int> f()` ser aceita como coroutine, `Generator<int>` precisa ter
um membro `promise_type`. **Quem cria o promise é o compilador**, dentro do
frame, e é ele quem chama os métodos — você nunca chama nenhum à mão:

```
1. aloca o frame (heap)
2. COPIA os parâmetros para dentro do frame
3. constrói o promise_type
4. promise.get_return_object()        → o objeto que o CHAMADOR recebe agora
5. co_await promise.initial_suspend() → suspende aqui? (lazy vs eager)
6. ---- corpo ----   co_yield v  →  co_await promise.yield_value(v)
                     co_return v →  promise.return_value(v)
                     co_return;  →  promise.return_void()
7. exceção escapou do corpo → promise.unhandled_exception()
8. co_await promise.final_suspend()
9. frame destruído: sozinho se (8) não suspendeu; no seu destroy() se suspendeu
```

Sobre o passo 7: a exceção **não** propaga sozinha — o chamador pode nem estar na
pilha naquele instante. O idioma é guardá-la (`exception_ =
std::current_exception();`) e relançá-la depois no chamador
(`std::rethrow_exception(...)`), que é o que este repositório faz.

### 6. `suspend_always` vs `suspend_never`; lazy vs eager

Os passos 5 e 8 devolvem um objeto que diz "suspenda" ou "não suspenda":
`std::suspend_always` tem `await_ready() == false` e **suspende**;
`std::suspend_never` tem `true` e segue direto. Daí saem os quatro casos:

- `initial_suspend → suspend_always` = coroutine **lazy**: o corpo só roda no
  primeiro `resume()`. É o que `Generator<T>` e `Task<T>` fazem aqui.
- `initial_suspend → suspend_never` = coroutine **eager**: o corpo roda
  imediatamente na chamada (o `EagerTask` do demo 3).
- `final_suspend → suspend_always` = ao terminar, o frame fica parado esperando
  `destroy()`. **Necessário** para ler algo do promise depois do fim (o valor de
  `co_return`, a exceção): com `suspend_never` o frame já teria evaporado.
- ⚠ `final_suspend()` precisa ser `noexcept`, ou não compila.

### 7. Awaitable vs Awaiter — o protocolo do `co_await`

Os dois termos são tratados como sinônimos por toda a internet, e não são:
**Awaitable** é a *expressão* depois do `co_await`; **Awaiter** é o *objeto com
os três métodos* que o compilador usa. Para ir de um ao outro ele tenta, nesta
ordem: `promise.await_transform(expr)`, depois `expr.operator co_await()`; se
nada existe, o Awaitable **é** o Awaiter — que é o caso deste projeto, e é por
isso que a confusão passa despercebida.

```cpp
struct MeuAwaiter {
    bool await_ready();                            // true = já pronto, não suspende
    ???  await_suspend(std::coroutine_handle<> h); // roda JÁ suspenso
    T    await_resume();                           // o valor que co_await devolve
};
```

O `h` é o handle **da coroutine que acabou de suspender**: guarde-o, ou ninguém
a retomará jamais. E `await_suspend` aceita três tipos de retorno: `void`
(suspende e devolve o controle a quem chamou `resume()`), `bool` (`true`
suspende; `false` **cancela** a suspensão e continua) e `coroutine_handle<>`
(suspende e **retoma já** o handle devolvido — item 8).

### 8. Symmetric transfer e `noop_coroutine()`

Quando a coroutine A termina e precisa acordar B, o jeito ingênuo é `b.resume()`
dentro de `await_suspend`. Mas `resume()` é uma chamada de função comum: ela
**empilha um frame de pilha**. Numa cadeia de mil coroutines, mil frames — a
pilha estoura.

```
RUIM (void):                          BOM (symmetric transfer):
void await_suspend(h) {               coroutine_handle<>
    outro.resume();   ← empilha           await_suspend(h) { return outro; }
}
pilha: [A][B][C][D]... cresce         pilha: [ ] constante (salto de cauda)
```

Devolver um `coroutine_handle<>` significa "não volte ao chamador de `resume()`;
salte direto para esta outra coroutine", e o padrão garante que isso não consome
pilha. Quando não há ninguém para retomar você **não pode** devolver um handle
nulo: devolve-se `std::noop_coroutine()`, um handle sentinela cujo `resume()` só
devolve o controle. É o que o `FinalAwaiter` do `Task<T>` faz:
`return cont ? cont : std::noop_coroutine();`.

### 9. ⚠ Referências pendentes — a armadilha nº 1

O passo 2 do item 5 diz "**copia** os parâmetros". Copiar uma referência copia a
**referência**, não o objeto. E como a coroutine é lazy, o original costuma
morrer **antes** da primeira retomada:

```cpp
Generator<char> letras(const std::string& s) {   // ERRADO: referência
    for (char c : s) co_yield c;
}
for (char c : letras("abc"))  // o std::string temporário morre no fim desta
    fmt::print("{}", c);      // linha; o frame guardou só a referência → UB
```

O mesmo vale para `string_view`, ponteiros e lambdas coroutine com `[&]`
(o objeto-lambda morre antes da retomada). **Regra: parâmetro de coroutine é
sempre por valor; lambda coroutine captura sempre por valor.** É o mesmo assunto
de [`../value_semantics/`](../value_semantics/), visto de outro ângulo.

### 10. O vocabulário do Asio (demo 4)

- **`asio::io_context`** — o *loop de eventos*: uma fila de trabalho mais um
  `run()` que fica desenfileirando. Uma única thread basta.
- **executor** — o objeto que diz *onde* um trabalho roda;
  `co_await asio::this_coro::executor` pergunta "qual é o meu?".
- **`asio::co_spawn(ctx, cor, token)`** — *lança* uma coroutine num `io_context`.
- **`asio::detached`** — *completion token*: "não me avise ao terminar".
- **`asio::use_awaitable`** — *completion token* que transforma uma operação de
  callback (`async_wait`, `async_read`) em algo `co_await`-ável.
- **`asio::awaitable<T>`** — o tipo de retorno das coroutines do Asio: um
  `Task<T>` pronto, já ligado ao `io_context`.

---

## Por que estudar isto?

O problema é antigo: **ler um dado, processar, escrever** — sendo ler e escrever
lentos (rede, disco). As três soluções:

```cpp
// (a) callbacks: funciona, escala, ilegível
ler(arq, [](Dado d){ processar(d, [](Result r){ escrever(r, [](bool){ /*...*/ }); }); });
// (b) std::future + get(): legível, e uma thread parada por espera
Dado d = ler_async(arq).get();     // esta thread PARA aqui, sem fazer nada
// (c) co_await: legível E sem thread parada
Dado d = co_await ler_async(arq);  // a COROUTINE para; a thread vai fazer outra coisa
```

| | callbacks | `future::get()` | `co_await` |
|---|---|---|---|
| forma do código | aninhada, invertida | linear | linear |
| tratamento de erro | um `if (ec)` por nível | `try/catch` | `try/catch` |
| custo por operação em voo | ~0 | **1 thread parada** (megabytes de pilha reservada) | 1 alocação de frame (dezenas de bytes) |
| 10 000 operações simultâneas | possível, ilegível | inviável | rotina |

O custo real de uma thread está **medido** em
[`../concurrency2/`](../concurrency2/). ⚠ Coroutine **não** é concorrência: é
estruturação de controle. Quem executa as coroutines é um agendador — o
`io_context` do demo 4.

---

## Estrutura de arquivos

```
src/coroutines/
├── main.cpp              ← ponto de entrada; roda os 4 demos em ordem
├── demo_generator.hpp    ← 1 · co_yield, Generator<T> do zero, pipelines, exceções
├── demo_task.hpp         ← 2 · co_await/co_return, Task<T>, Awaiter, SuspendFor
├── demo_internals.hpp    ← 3 · coroutine_handle, máquina de estados, symmetric transfer
├── demo_asio.hpp         ← 4 · io_context, timers, echo server TCP com coroutines
├── meson.build           ← alvo `coroutines` + as duas flags explicadas adiante
└── README.md             ← este arquivo
```

---

## Os demos, um a um

### Demo 1 — `Generator<T>`: valores sob demanda

**O problema.** Uma sequência de Fibonacci "até 1000". Sem coroutines: devolver
um `vector` pronto (e se a sequência for infinita?) ou escrever um iterador à
mão, com `a`, `b` e a lógica do laço virando membros de uma classe — o laço
deixa de parecer um laço. **Com coroutine**, é literalmente o laço, com
`co_yield` no lugar do `push_back`:

```cpp
Generator<long long> fibonacci(long long limite) {
    long long a = 0, b = 1;
    while (a <= limite) {
        co_yield a;              // suspende aqui; `a` e `b` sobrevivem
        auto prox = a + b; a = b; b = prox;
    }
}                                 // fim → return_void() → final_suspend()
for (auto n : fibonacci(1000)) fmt::print("{} ", n);
```

Medido: `0 1 1 2 3 5 8 13 21 34 55 89 144 233 377 610 987`.

**O tipo completo** ([`demo_generator.hpp`](demo_generator.hpp)) — é ele que
torna o `for` acima legal:

```cpp
struct promise_type {
    std::optional<T> value_;  std::exception_ptr exception_;
    Generator get_return_object() {                                  // passo 4
        return Generator{std::coroutine_handle<promise_type>::from_promise(*this)}; }
    std::suspend_always initial_suspend() noexcept { return {}; }    // lazy
    std::suspend_always final_suspend()   noexcept { return {}; }    // espera destroy()
    std::suspend_always yield_value(T v) { value_ = std::move(v); return {}; }
    void return_void() noexcept { value_.reset(); }
    void unhandled_exception() { exception_ = std::current_exception(); }
};
Generator(const Generator&) = delete;              // RAII: móvel, não copiável
Generator(Generator&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }
~Generator() { if (handle_) handle_.destroy(); }   // impede o vazamento do item 3
```

**Pipeline.** Como cada `Generator` é móvel, encadeiam-se como canos do Unix, e
nada é computado antes de ser pedido:

```cpp
auto pares    = filtrar(contar(0),        [](int n){ return n % 2 == 0; });
auto dobrados = mapear (std::move(pares), [](int n){ return n * 2; });
// medido: 0 4 8 12 16 20   (os 6 primeiros; contar() é infinita)
```

⚠ `contar(0)` nunca termina; o demo sai com `break`. Sair do `for` por `break` é
seguro: o destrutor do `Generator` roda e chama `destroy()` na coroutine que
ficou suspensa no meio.

**Exceções.** `com_erro(6)` lança no valor 3. A exceção não pula direto para o
seu `catch`: vai para `unhandled_exception()`, fica num `exception_ptr`, e
`next()` a relança no chamador — medido: `Exceção capturada no chamador: valor
proibido: 3`.

⚠ Este `Generator` **não é** o `std::generator` do C++23: o padrão tem um pronto,
mas o GCC 13.3 deste toolchain não traz o cabeçalho (`fatal error: generator: No
such file or directory`, verificado) — por isso ele é escrito à mão aqui. Ver
[`../cpp23/`](../cpp23/).

### Demo 2 — `Task<T>`: esperar um valor com `co_await`

**O problema.** `Generator` produz *N* valores. E quando você quer *um* valor que
só fica pronto depois de uma espera?

```cpp
Task<int> buscar_dado(int id) {
    co_await SuspendFor{50ms};        // suspende — leia o aviso abaixo
    co_return id * 10;
}
Task<int> processar() {
    int a = co_await buscar_dado(1);  // linear, sem callback
    int b = co_await buscar_dado(2);
    co_return a + b;                  // medido: 30
}
```

**Como `co_await task` funciona.** `Task<T>` implementa os três métodos do
Awaiter (item 7) no próprio tipo; ao terminar, o `FinalAwaiter` devolve
`continuacao_` (symmetric transfer, item 8) e o chamador acorda na linha do
`co_await`:

```cpp
bool await_ready() const noexcept { return handle_.done(); }
void await_suspend(std::coroutine_handle<> chamador) noexcept {
    handle_.promise().continuacao_ = chamador;   // "me avise quando terminar"
    handle_.resume();                            // começa a executar a Task
}
T await_resume() { return handle_.promise().get(); }   // devolve, ou relança
```

⚠ **`SuspendFor` NÃO economiza thread.** O bordão dos tutoriais ("`co_await`
libera a thread") é **falso para este Awaiter**. O que ele realmente faz
([`demo_task.hpp`](demo_task.hpp)):

```cpp
void await_suspend(std::coroutine_handle<> handle) const {
    std::thread([handle, d = duracao]() mutable {
        std::this_thread::sleep_for(d);
        handle.resume();
    }).detach();                     // ← uma std::thread NOVA por espera
}
```

Cada `co_await SuspendFor{...}` cria uma thread e a põe para **dormir**: uma
thread **a mais**, não a menos. No demo 2 inteiro são **4 threads detached** (uma
em `buscar_dado(5)`, duas em `processar()`, uma em `exibir_mensagem`).
`SuspendFor` existe só para tornar o protocolo do Awaiter visível sem um
agendador. **A economia real só aparece no demo 4** — o cabeçalho de
[`demo_asio.hpp`](demo_asio.hpp) diz isso com todas as letras.

⚠ Como `resume()` é chamado da thread auxiliar, **tudo depois do `co_await` roda
nela**, inclusive o `fmt::print` de `buscar_dado`. Não na `main`. E `get_sync()`
— que existe porque `main` não é coroutine e não pode fazer `co_await` — é
`handle_.resume(); while (!handle_.done()) sleep_for(1ms);`: espera ativa que
**bloqueia** a thread chamadora.

**Onde este `Task<T>` fica devendo:** `await_suspend` devolve `void` e chama
`resume()`, ou seja, a *entrada* na cadeia não usa symmetric transfer, só a
*saída*. Uma cadeia de milhares de `co_await` ainda empilharia. Bom exercício:
troque o retorno por `coroutine_handle<>` e devolva `handle_`.

### Demo 3 — Internals: o que o compilador realmente gera

Comece pelo que você escreveu: `Generator<int> contar(int n) { while (true)
co_yield n++; }`.

**Etapa 1** — cada `co_yield` vira "avise o promise, marque o estado, volte":
`promise.yield_value(n++); __state = 1; return;` — e a retomada tem de voltar
**exatamente** para a linha seguinte a esse `return`.

**Etapa 2** — voltar para um ponto no meio da função exige um ponto de entrada:
nasce um `switch` no topo, e os locais migram para o frame.

```cpp
struct contar_frame { contar_promise promise; int n; int __state = 0; };
void contar_resume(contar_frame* f) {
    switch (f->__state) { case 0: goto label_0; case 1: goto label_1; }
    label_0:
        while (true) {
            f->promise.yield_value(f->n++);
            f->__state = 1;
            return;              // ← suspende
            label_1:;            // ← e o próximo resume() entra aqui
        }
}
```

**Etapa 3** — `handle.resume()` é, essencialmente, `contar_resume(frame)`. É isso:
sem mágica, sem thread, sem pilha escondida. Uma coroutine é uma **máquina de
estados** com os locais promovidos para um struct.

O demo 3.1 dirige esse ciclo à mão e imprime cada evento: `initial_suspend` roda
**na criação**, antes de qualquer `resume()`, e depois do último `resume()` o
`done()` fica `true` mas o frame **continua existindo** até o `destroy()` — a
prova prática do item 6. O demo 3.2 mostra `suspend_never`: `EagerTask` usa-o nos
dois pontos, e por isso `coroutine_eager()` roda inteira na linha da chamada.
⚠ Como `final_suspend()` também é `suspend_never`, o frame se autodestrói ao
terminar — e o `t.handle` que o demo imprime já está pendurado. Aqui é
inofensivo (só `done()` é lido), mas é o padrão que gera *use-after-free* em
produção. O demo 3.3 é o mecanismo do item 8 aplicado.

### Demo 4 — Asio: I/O assíncrono de verdade

Aqui a promessa do `co_await` se cumpre: quem espera é a coroutine, e a thread
vai atender outra.

```cpp
asio::awaitable<void> esperar_e_imprimir(std::string nome, milliseconds delay) {
    auto executor = co_await asio::this_coro::executor;
    asio::steady_timer timer(executor, delay);
    co_await timer.async_wait(asio::use_awaitable);   // suspende a COROUTINE
    fmt::print("  [{}] timer disparado após {}ms!\n", nome, delay.count());
}
asio::io_context io;
asio::co_spawn(io, esperar_e_imprimir("A", 300ms), asio::detached);
asio::co_spawn(io, esperar_e_imprimir("B", 100ms), asio::detached);
asio::co_spawn(io, esperar_e_imprimir("C", 200ms), asio::detached);
io.run();   // UMA thread atende as três
// medido — os lançamentos saem em A, B, C; os términos, na ordem dos prazos:
//   [B] timer disparado após 100ms!  [C] ... 200ms!  [A] ... 300ms!
```

Zero threads auxiliares. Compare com o demo 2, onde a mesma concorrência custou
uma thread por espera.

**Echo server, uma coroutine por conexão.** Este é o código **real** de
[`demo_asio.hpp`](demo_asio.hpp) — inclusive o `try/catch`, que **não é opcional**:

```cpp
asio::awaitable<void> echo_session(tcp::socket socket) {
    try {
        char buffer[1024];
        while (true) {
            std::size_t n = co_await socket.async_read_some(
                asio::buffer(buffer), asio::use_awaitable);
            co_await asio::async_write(
                socket, asio::buffer(buffer, n), asio::use_awaitable);
        }
    } catch (const std::exception&) {
        // cliente desconectou — a coroutine termina naturalmente
    }
}
```

⚠ **Por que o `try/catch` é obrigatório.** Com `asio::use_awaitable` os erros
chegam como **exceção**, não como `error_code` — e o fim normal de uma conexão é
o erro `asio::error::eof`. Ou seja, um cliente que se despede educadamente lança
uma exceção de dentro da coroutine. Como ela foi lançada com `asio::detached`
("ninguém está observando o resultado"), a exceção não tem para onde propagar e o
Asio chama `std::terminate`: **sem o `try/catch`, o processo morre quando o
primeiro cliente desconecta**. Se preferir `error_code` a exceções, o token é
`asio::as_tuple(asio::use_awaitable)`.

O listener aceita 2 conexões e lança uma sessão por cliente; os clientes são
outras duas coroutines no mesmo `io_context`. Medido: `[servidor] ouvindo na
porta 12399`, dois `cliente N conectado`, e os dois ecos —
`[cliente] enviou: 'ola mundo!' → recebeu echo: 'ola mundo!'`.

Cinco coroutines, uma thread, zero bloqueios. No modelo *thread-por-conexão*
seriam cinco threads. ⚠ A porta 12399 é fixa no código: se estiver ocupada, o
`tcp::acceptor` lança e o demo 4.2 aborta. É a única parte do executável que
depende do ambiente.

---

## Como validar

```bash
make configure && make build
./dist/bin/coroutines
```

Última linha esperada: `✓ Todos os demos de coroutines concluídos!`. Marcos que
provam cada demo, em ordem: `Fibonacci ≤ 1000: 0 1 1 2 3 5 8 13 21 34 55 89 144
233 377 610 987` e `Exceção capturada no chamador: valor proibido: 3` (demo 1);
`Soma final recebida no main: 30` (demo 2); `[promise] final_suspend — coroutine
terminou, aguardando destroy()` (demo 3);
`[cliente] enviou: 'ola mundo!' → recebeu echo: 'ola mundo!'` (demo 4).

**As duas flags do `meson.build`**, que costumam confundir:

- `-DASIO_HAS_CO_AWAIT` — sem esta macro o Asio standalone não expõe
  `asio::awaitable`, `use_awaitable` nem `co_spawn`, e o demo 4 não compila.
- `-Wno-subobject-linkage` — o GCC emite `-Wsubobject-linkage` para lambdas
  capturadas dentro de frames de coroutine (a lambda vira campo de um tipo que
  vive na heap). Falso-positivo conhecido; o motivo está comentado no próprio
  `meson.build`.

Requisitos: C++20, GCC ≥ 10 ou Clang ≥ 14, `fmt` e Asio standalone. Verificado
com **GCC 13.3.0**.

---

## Regras de ouro

| Situação | Faça |
|---|---|
| parâmetro de uma coroutine | **por valor**, sempre — nunca `const T&`, `string_view` ou ponteiro |
| lambda que é coroutine | capture por valor; melhor: use uma função nomeada |
| tipo de retorno próprio | RAII: mover, não copiar; `destroy()` no destrutor |
| ler o resultado após o fim | `final_suspend() → suspend_always` |
| `final_suspend()` | marque `noexcept` — exigência do padrão |
| cadeia longa de coroutines | devolva `coroutine_handle<>` de `await_suspend` |
| ninguém para retomar | devolva `std::noop_coroutine()`, nunca um handle nulo |
| esperar por I/O | `io_context` do Asio; **nunca** uma `std::thread` por espera |
| gerar uma sequência | `Generator<T>` com `co_yield` |
| um único valor assíncrono | `Task<T>` com `co_await` / `co_return` |
| `main` precisa do resultado | rode um `io_context`, não uma espera ativa |
| laço quente | lembre: cada chamada de coroutine é uma alocação de heap |

---

## Referências

Neste repositório: [`../concurrency2/`](../concurrency2/) (o que uma thread custa
de verdade) · [`../concurrency/`](../concurrency/) (`thread`, `future`, `async` —
o que o `co_await` substitui) · [`../cpp23/`](../cpp23/) (`std::generator`, a
versão padronizada deste `Generator<T>`; o README de lá aponta para cá) ·
[`../network/`](../network/) (Asio e `io_context` no estilo callback) ·
[`../http-client/`](../http-client/) (Asio num protocolo real) ·
[`../value_semantics/`](../value_semantics/) (tempo de vida — a armadilha nº 1) ·
[`../performance/`](../performance/) (como medir o custo do frame).

Fora dele:

- cppreference — [Coroutines (C++20)](https://en.cppreference.com/w/cpp/language/coroutines)
- Lewis Baker, série *C++ Coroutines* — <https://lewissbaker.github.io/> (em
  especial *Understanding operator co_await* e *Understanding Symmetric Transfer*)
- [P0912R5](https://wg21.link/p0912) — a proposta que entrou no C++20
- Asio — [C++20 Coroutines](https://think-async.com/Asio/asio-1.30.2/doc/asio/overview/composition/cpp20_coroutines.html)
