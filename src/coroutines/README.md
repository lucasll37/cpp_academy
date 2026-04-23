# coroutines — Coroutines C++20

## O que é este projeto?

Guia progressivo sobre **coroutines em C++20**: desde o conceito básico de `co_yield` até integração com Asio para I/O assíncrono de verdade. Coroutines permitem escrever código assíncrono com aparência sequencial — sem callbacks, sem threads desnecessárias.

---

## Estrutura de arquivos

```
coroutines/
├── main.cpp              ← executa todos os demos em sequência
├── demo_generator.hpp    ← co_yield, Generator<T>, pipelines
├── demo_task.hpp         ← co_await, co_return, Task<T>, Awaitable
├── demo_internals.hpp    ← coroutine_handle, máquina de estados, symmetric transfer
└── demo_asio.hpp         ← timers, echo server, I/O assíncrono real
```

---

## Os três operadores de coroutine

| Operador | Significado | Análogo |
|---|---|---|
| `co_yield v` | Suspende e entrega `v` ao chamador | `yield` em Python |
| `co_await expr` | Suspende e aguarda `expr` completar | `await` em Python/JS |
| `co_return v` | Termina a coroutine com valor `v` | `return` normal |

Uma função é uma **coroutine** se usar qualquer um desses três operadores.

---

## Demo 1: Generator — co_yield

`co_yield` é para produzir sequências de valores preguiçosamente. A coroutine pausa a cada `co_yield`, entrega o valor, e continua da mesma linha quando pedido o próximo valor.

```cpp
Generator<int> fibonacci(long long limite) {
    long long a = 0, b = 1;
    while (a <= limite) {
        co_yield a;         // pausa aqui, entrega 'a', depois continua
        auto prox = a + b;
        a = b;
        b = prox;
    }
}

// Range-for funciona diretamente
for (auto n : fibonacci(1000)) {
    fmt::print("{} ", n);  // 0 1 1 2 3 5 8 13 21 34 55 ...
}
```

### Pipeline de generators

```cpp
auto numeros  = contar(0);
auto pares    = filtrar(std::move(numeros), [](int n){ return n % 2 == 0; });
auto dobrados = mapear(std::move(pares),   [](int n){ return n * 2; });
// Equivalente a: 0 → [0,2,4,6,...] → [0,4,8,12,...]
// Nenhum elemento é calculado antes de ser pedido
```

### Generator<T> — promise_type

Para que uma função seja uma coroutine geradora, o tipo de retorno precisa de `promise_type`:

```cpp
template <typename T>
struct Generator {
    struct promise_type {
        Generator get_return_object();     // cria o Generator
        std::suspend_always initial_suspend(); // lazy: não executa ao criar
        std::suspend_always final_suspend();   // não se auto-destrói ao terminar
        std::suspend_always yield_value(T v);  // o que fazer com co_yield v
        void return_void();                    // fim da função
        void unhandled_exception();            // propagação de exceções
    };
    // ...
};
```

---

## Demo 2: Task — co_await e co_return

`co_await` suspende a coroutine enquanto aguarda um resultado assíncrono — sem bloquear a thread.

```cpp
Task<int> buscar_dado(int id) {
    co_await SuspendFor{50ms};  // suspende, thread fica livre
    co_return id * 10;          // entrega o resultado
}

Task<int> processar() {
    int a = co_await buscar_dado(1); // aguarda sem bloquear
    int b = co_await buscar_dado(2); // encadeamento limpo
    co_return a + b;
}
```

### O protocolo Awaitable

Qualquer tipo pode ser usado com `co_await` se implementar três métodos:

```cpp
struct MeuAwaitable {
    bool await_ready();              // já tem resultado? pula suspensão
    void await_suspend(handle);      // o que fazer ao suspender
    ResultType await_resume();       // valor que co_await retorna
};
```

### Diferença de std::future

```cpp
// std::future — BLOQUEIA a thread
int resultado = future.get(); // thread parada aqui

// co_await — SUSPENDE a coroutine
int resultado = co_await task; // thread livre para outro trabalho
```

---

## Demo 3: Internals — o que o compilador gera

Uma coroutine é transformada em uma **máquina de estados**:

```
Função original (coroutine):          Equivalente gerado:
                                       struct Frame {
Generator<int> contar(int n) {           promise_type promise;
    while (true) co_yield n++;           int n;
}                                        int __state = 0;
                                       };

                                       void resume(Frame* f) {
                                         switch (f->__state) {
                                           case 0: goto L0;
                                           case 1: goto L1;
                                         }
                                         L0: while (true) {
                                           f->promise.yield_value(f->n++);
                                           f->__state = 1;
                                           return;  // suspende
                                           L1:;
                                         }
                                       }
```

### coroutine_handle<>

O handle é o mecanismo de controle:

```cpp
handle.resume();  // retoma a coroutine do ponto de suspensão
handle.done();    // terminou?
handle.destroy(); // libera o frame (evitar memory leak)
handle.address(); // ponteiro bruto (para void* APIs)
```

### Symmetric transfer

Evita stack overflow em cadeias longas de coroutines:

```cpp
// RUIM: cada resume() empilha um frame
void await_suspend(handle h) { outro_handle.resume(); } // h → outro → ...

// BOM: tail-call, stack constante
auto await_suspend(handle h) { return outro_handle; }   // goto direto
```

---

## Demo 4: Asio — I/O assíncrono real

Asio tem suporte nativo a coroutines. Com `asio::awaitable<T>`:

```cpp
// 3 coroutines concorrentes em UMA thread
asio::co_spawn(io, esperar_e_imprimir("A", 300ms), asio::detached);
asio::co_spawn(io, esperar_e_imprimir("B", 100ms), asio::detached);
asio::co_spawn(io, esperar_e_imprimir("C", 200ms), asio::detached);
io.run(); // dispara: B(100ms) → C(200ms) → A(300ms)
```

### Echo server com coroutines

```cpp
asio::awaitable<void> echo_session(tcp::socket socket) {
    char buffer[1024];
    while (true) {
        auto n = co_await socket.async_read_some(
            asio::buffer(buffer), asio::use_awaitable);
        co_await asio::async_write(
            socket, asio::buffer(buffer, n), asio::use_awaitable);
    }
}
```

Cada cliente tem sua coroutine. Uma thread gerencia todas — escalável a milhares de conexões sem overhead de criação de threads.

---

## Como compilar e executar

```bash
make configure
make build
./dist/bin/coroutines
```

**Requer**: C++20 (`-std=c++20`), Asio standalone com `ASIO_HAS_CO_AWAIT`.

---

## Quando usar cada primitiva

| Caso de uso | Use |
|---|---|
| Gerar sequência de valores | `Generator<T>` com `co_yield` |
| Operação assíncrona que retorna um valor | `Task<T>` com `co_await` |
| I/O de rede/disco sem bloquear thread | `asio::awaitable<T>` |
| Máquina de estados explícita | Coroutine com múltiplos `co_yield` |
| Substituir threads para concorrência leve | Pool de coroutines no `io_context` |
