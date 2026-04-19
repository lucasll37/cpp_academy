# concurrency — Multithreading e Concorrência em C++

## O que é este projeto?

Guia progressivo sobre **programação concorrente** em C++ moderno. Cobre desde a criação básica de threads até padrões avançados como thread pool e produtor/consumidor, passando por todos os primitivos de sincronização da STL.

---

## Estrutura de arquivos

```
concurrency/
├── main.cpp              ← executa todos os demos em sequência
├── demo_threads.hpp      ← criação, join, detach, RAII guard
├── demo_mutex.hpp        ← mutex, lock_guard, unique_lock, shared_mutex
├── demo_condition.hpp    ← condition_variable, wait/notify
├── demo_futures.hpp      ← future, promise, async, packaged_task
├── demo_atomics.hpp      ← atomic<T>, memory ordering
└── demo_patterns.hpp     ← thread pool, produtor/consumidor
```

---

## Demo 1: Threads — Criação e Lifecycle

```cpp
#include <thread>
#include <fmt/core.h>

// Criação básica
std::thread t([]{ fmt::print("thread rodando!\n"); });

// join: bloqueia até a thread terminar (você DEVE fazer join ou detach)
t.join();

// detach: thread roda independentemente (daemon thread)
// Atenção: se o processo terminar, a thread é destruída abruptamente
std::thread t2([]{ /* work */ });
t2.detach();

// Passando argumentos
void trabalho(int id, std::string nome) { /* ... */ }
std::thread t3(trabalho, 42, "worker");
t3.join();
```

### RAII Guard — nunca esquecer o join

```cpp
// Problema: se uma exceção ocorrer entre criar a thread e o join, ela nunca é joinada
// Isso causa std::terminate()!

// Solução: RAII guard
class ThreadGuard {
    std::thread& t_;
public:
    explicit ThreadGuard(std::thread& t) : t_(t) {}
    ~ThreadGuard() { if (t_.joinable()) t_.join(); }
};

std::thread t([]{ /* work */ });
ThreadGuard guard(t); // join automático mesmo com exceção
```

---

## Demo 2: Mutex — Exclusão Mútua

### Race condition (problema)

```cpp
int contador = 0;

// ERRADO: duas threads incrementando sem sincronização
// contador++ é leitura + modificação + escrita → não é atômico!
std::thread t1([&]{ for(int i=0; i<1000; i++) contador++; });
std::thread t2([&]{ for(int i=0; i<1000; i++) contador++; });
t1.join(); t2.join();
// resultado: qualquer valor entre 1000 e 2000, não determinístico
```

### `lock_guard` — lock/unlock automático (RAII)

```cpp
std::mutex mtx;
int contador = 0;

auto incrementar = [&]{
    for (int i = 0; i < 1000; i++) {
        std::lock_guard<std::mutex> lock(mtx);  // lock ao criar
        contador++;
    }                                            // unlock ao destruir
};

std::thread t1(incrementar), t2(incrementar);
t1.join(); t2.join();
// resultado: sempre 2000
```

### `unique_lock` — lock flexível

```cpp
std::mutex mtx;

std::unique_lock<std::mutex> lock(mtx);  // lock imediato
lock.unlock();  // unlock explícito possível
lock.lock();    // re-lock possível
// lock.try_lock() — não bloqueia, retorna bool
```

### `shared_mutex` — readers/writer lock

```cpp
std::shared_mutex rw_mtx;
std::string dados = "inicial";

// Múltiplos leitores simultâneos
auto ler = [&]{
    std::shared_lock<std::shared_mutex> lock(rw_mtx); // lock de leitura
    fmt::print("lendo: {}\n", dados);
};

// Apenas um escritor por vez, exclusivo
auto escrever = [&](std::string novo){
    std::unique_lock<std::shared_mutex> lock(rw_mtx); // lock de escrita
    dados = novo;
};
```

---

## Demo 3: `condition_variable` — Sincronização por Estado

```cpp
std::mutex mtx;
std::condition_variable cv;
bool pronto = false;
std::queue<int> fila;

// Produtor
auto produtor = [&]{
    {
        std::lock_guard<std::mutex> lock(mtx);
        fila.push(42);
        pronto = true;
    }
    cv.notify_one(); // acorda UM consumidor (notify_all acorda todos)
};

// Consumidor
auto consumidor = [&]{
    std::unique_lock<std::mutex> lock(mtx);
    // wait libera o mutex e dorme. Quando acordar, reaquire o mutex.
    // A lambda é a condição: se falsa, volta a dormir (protege de spurious wakeups)
    cv.wait(lock, [&]{ return pronto; });
    
    int val = fila.front();
    fila.pop();
    fmt::print("recebido: {}\n", val);
};
```

### Por que a lambda no `wait`?

**Spurious wakeups**: o sistema operacional pode acordar uma thread sem motivo (especificação do POSIX). A lambda garante que o processamento só ocorre quando a condição é de fato verdadeira.

---

## Demo 4: `future` & `promise` — Comunicação Assíncrona

### `std::async` — forma mais simples

```cpp
// Executa a lambda em uma thread separada (ou pode ser lazy)
std::future<int> resultado = std::async(std::launch::async, []{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 42;
});

// ... faz outro trabalho enquanto espera ...

int val = resultado.get(); // bloqueia até ter o resultado
```

### `promise` / `future` — comunicação manual entre threads

```cpp
std::promise<int> promessa;
std::future<int> futuro = promessa.get_future();

std::thread t([&]{
    // trabalho...
    promessa.set_value(100); // entrega o resultado
    // ou: promessa.set_exception(std::make_exception_ptr(e));
});

int val = futuro.get(); // bloqueia e recebe o resultado
t.join();
```

### `packaged_task` — embrulhar função em task com future

```cpp
std::packaged_task<int(int, int)> task([](int a, int b){ return a + b; });
std::future<int> fut = task.get_future();

std::thread t(std::move(task), 10, 20);
t.join();

fmt::print("resultado: {}\n", fut.get()); // 30
```

---

## Demo 5: Atomics & Memory Model

### `std::atomic<T>` — operações atômicas lock-free

```cpp
std::atomic<int> contador{0};

// Operações atômicas (sem mutex necessário)
contador.fetch_add(1);              // atômico ++
contador.fetch_sub(1);              // atômico --
contador.store(42);                 // escrita atômica
int val = contador.load();          // leitura atômica
contador.exchange(100);             // write e retorna valor anterior
int expected = 42;
contador.compare_exchange_strong(expected, 99); // CAS
```

### Memory ordering

Controla como as operações atômicas se relacionam com outras leituras/escritas:

| Ordering | Descrição | Custo |
|---|---|---|
| `memory_order_relaxed` | Sem garantias de ordenação | Mínimo |
| `memory_order_acquire` | Leituras não passam deste ponto para cima | Médio |
| `memory_order_release` | Escritas não passam deste ponto para baixo | Médio |
| `memory_order_seq_cst` | Ordem total, consistente entre todas as threads | Máximo (padrão) |

```cpp
std::atomic<bool> flag{false};
int dados = 0;

// Thread 1 (produtor)
dados = 42;                           // escrita normal
flag.store(true, memory_order_release); // "publica" os dados

// Thread 2 (consumidor)
while (!flag.load(memory_order_acquire)); // espera a publicação
// acquire garante que "dados = 42" está visível aqui
fmt::print("{}\n", dados); // sempre 42
```

---

## Demo 6: Padrões — Thread Pool & Produtor/Consumidor

### Thread Pool

```cpp
class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    explicit ThreadPool(size_t n) {
        for (size_t i = 0; i < n; ++i)
            workers_.emplace_back([this]{
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this]{ return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task(); // executa fora do lock
                }
            });
    }

    template<typename F>
    void enqueue(F&& f) {
        { std::lock_guard<std::mutex> lock(mtx_); tasks_.push(std::forward<F>(f)); }
        cv_.notify_one();
    }

    ~ThreadPool() {
        { std::lock_guard<std::mutex> lock(mtx_); stop_ = true; }
        cv_.notify_all();
        for (auto& t : workers_) t.join();
    }
};
```

---

## Armadilhas comuns

| Problema | Causa | Solução |
|---|---|---|
| **Race condition** | Acesso não sincronizado a dados compartilhados | mutex ou atomic |
| **Deadlock** | Duas threads esperam uma pela outra | `std::lock(m1, m2)` ou hierarquia |
| **Thread não joinada** | `std::thread` destruído sem join/detach | RAII guard |
| **Spurious wakeup** | CV acordada sem notificação | Sempre use predicado no `wait` |
| **ABA problem** | CAS vê valor antigo que voltou | Versioned CAS, hazard pointers |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja concurrency
./bin/concurrency
```

**Requer**: compilação com `-pthread` (configurado no meson.build).
