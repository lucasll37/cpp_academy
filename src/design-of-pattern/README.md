# design-of-pattern — Padrões de Projeto GoF em C++ Moderno

## O que é este projeto?

Implementa **8 padrões de projeto** do livro "Design Patterns" (Gang of Four) usando C++ moderno. Cada padrão está em seu próprio subdiretório com exemplo real e comentado.

---

## Padrões implementados

| Padrão | Categoria | Subdiretório |
|---|---|---|
| **Chain of Responsibility** | Comportamental | `chain/` |
| **Command** | Comportamental | `command/` |
| **Decorator** | Estrutural | `decorator/` |
| **Factory Method** | Criacional | `factory/` |
| **Iterator** | Comportamental | `iterator/` |
| **Observer** | Comportamental | `observer/` |
| **State** | Comportamental | `state/` |
| **Strategy** | Comportamental | `strategy/` |

---

## Estrutura de arquivos

```
design-of-pattern/
├── chain/           ← cadeia de handlers para processamento de requisições
│   ├── handler.hpp
│   ├── auth_handler.hpp/cpp
│   ├── rate_limit_handler.hpp/cpp
│   ├── validation_handler.hpp/cpp
│   ├── business_handler.hpp/cpp
│   ├── request.hpp
│   └── main.cpp
├── command/         ← encapsula ações como objetos (undo/redo)
│   ├── command.hpp
│   ├── editor.hpp/cpp
│   ├── history.hpp/cpp
│   ├── insert_command.hpp/cpp
│   ├── replace_command.hpp/cpp
│   ├── delete_command.hpp/cpp
│   └── main.cpp
├── decorator/       ← adiciona comportamentos dinamicamente
│   ├── data_stream.hpp
│   ├── file_stream.hpp/cpp
│   ├── compression.hpp/cpp
│   ├── encryption.hpp/cpp
│   ├── logging.hpp/cpp
│   ├── stream_decorator.hpp
│   └── main.cpp
├── factory/         ← criação de objetos sem especificar classe concreta
│   ├── shape.hpp
│   ├── shape_factory.hpp
│   ├── circle.hpp/cpp
│   ├── square.hpp/cpp
│   └── main.cpp
├── iterator/        ← percorrer coleção sem expor representação interna
│   ├── iterator.hpp
│   ├── iterable.hpp
│   ├── song.hpp
│   ├── playlist.hpp
│   ├── playlist_iterators.hpp
│   └── main.cpp
├── observer/        ← notifica múltiplos objetos quando estado muda
│   ├── observer.hpp
│   ├── event_bus.hpp/cpp
│   ├── logger.hpp/cpp
│   ├── monitor.hpp/cpp
│   └── main.cpp
├── state/           ← altera comportamento quando estado interno muda
│   ├── tcp_state.hpp/cpp
│   ├── tcp_states.hpp/cpp
│   ├── tcp_connection.hpp/cpp
│   └── main.cpp
└── strategy/        ← família de algoritmos intercambiáveis em runtime
    ├── sorter.hpp
    ├── sort_context.hpp/cpp
    ├── bubble_sort.hpp/cpp
    ├── merge_sort.hpp/cpp
    ├── quick_sort.hpp/cpp
    └── main.cpp
```

---

## 1. Chain of Responsibility — Pipeline de Handlers

**Problema**: uma requisição precisa passar por múltiplas verificações em sequência.
**Solução**: encadear handlers; cada um processa ou repassa para o próximo.

```
Requisição → AuthHandler → RateLimitHandler → ValidationHandler → BusinessHandler
                │               │                   │                  │
           [não autenticado] [limite excedido]  [dados inválidos]  [processa]
                ↓               ↓                   ↓
             Rejeita         Rejeita             Rejeita
```

```cpp
struct Handler {
    Handler* next_ = nullptr;
    Handler* set_next(Handler* h) { next_ = h; return h; }
    virtual bool handle(const Request& req) = 0;
};

struct AuthHandler : Handler {
    bool handle(const Request& req) override {
        if (!req.has_token()) { /* rejeita */ return false; }
        return next_ ? next_->handle(req) : true;
    }
};

// Encadeamento
AuthHandler auth;
RateLimitHandler rate;
ValidationHandler valid;
BusinessHandler biz;

auth.set_next(&rate)->set_next(&valid)->set_next(&biz);
auth.handle(request); // percorre a cadeia
```

---

## 2. Command — Encapsulando Ações com Undo/Redo

**Problema**: implementar desfazer/refazer operações em um editor de texto.
**Solução**: cada operação vira um objeto Command com `execute()` e `undo()`.

```cpp
struct Command {
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo()    = 0;
};

struct InsertCommand : Command {
    Editor& editor_;
    std::string text_;
    int pos_;

    void execute() override { editor_.insert(pos_, text_); }
    void undo()    override { editor_.erase(pos_, text_.size()); }
};

struct History {
    std::stack<std::unique_ptr<Command>> stack_;

    void push(std::unique_ptr<Command> cmd) {
        cmd->execute();
        stack_.push(std::move(cmd));
    }

    void undo() {
        if (!stack_.empty()) {
            stack_.top()->undo();
            stack_.pop();
        }
    }
};
```

---

## 3. Decorator — Adicionando Comportamentos Dinamicamente

**Problema**: adicionar compressão, criptografia e logging a um stream de arquivo em qualquer combinação.
**Solução**: decorators envolvem o objeto base, adicionando comportamento antes/depois.

```cpp
struct DataStream {
    virtual ~DataStream() = default;
    virtual void write(const std::string& data) = 0;
    virtual std::string read() = 0;
};

struct FileStream : DataStream {
    void write(const std::string& data) override { /* escreve no arquivo */ }
};

struct StreamDecorator : DataStream {
    std::unique_ptr<DataStream> wrapped_;
    void write(const std::string& data) override { wrapped_->write(data); }
};

struct CompressionDecorator : StreamDecorator {
    void write(const std::string& data) override {
        wrapped_->write(compress(data)); // comprime antes de passar adiante
    }
};

struct EncryptionDecorator : StreamDecorator {
    void write(const std::string& data) override {
        wrapped_->write(encrypt(data)); // criptografa antes de passar
    }
};

// Composição flexível em runtime:
auto stream = std::make_unique<LoggingDecorator>(
    std::make_unique<CompressionDecorator>(
        std::make_unique<EncryptionDecorator>(
            std::make_unique<FileStream>("arquivo.bin")
        )
    )
);
stream->write("dados confidenciais"); // log → comprime → criptografa → arquivo
```

---

## 4. Factory Method — Criação Desacoplada

**Problema**: criar objetos polimórficos sem hardcode da classe concreta.
**Solução**: factory centraliza a criação; cliente usa a interface Shape.

```cpp
struct Shape {
    virtual ~Shape() = default;
    virtual void draw() const = 0;
    virtual double area() const = 0;
};

struct Circle : Shape {
    float radius_;
    void draw() const override { fmt::print("○ r={}\n", radius_); }
    double area() const override { return M_PI * radius_ * radius_; }
};

struct ShapeFactory {
    static std::unique_ptr<Shape> create(const std::string& type,
                                          const std::vector<double>& params) {
        if (type == "circle") return std::make_unique<Circle>(params[0]);
        if (type == "square") return std::make_unique<Square>(params[0]);
        throw std::invalid_argument("tipo desconhecido: " + type);
    }
};

auto shape = ShapeFactory::create("circle", {5.0});
shape->draw(); // não sabe se é Circle ou Square
```

---

## 5. Iterator — Percorrendo sem Expor Internos

**Problema**: percorrer uma playlist de músicas para frente e para trás.
**Solução**: iterator encapsula o ponteiro de percurso, a playlist não precisa ser exposta.

```cpp
template<typename T>
struct Iterator {
    virtual ~Iterator() = default;
    virtual bool hasNext() const = 0;
    virtual T& next() = 0;
};

struct Playlist {
    std::vector<Song> songs_;

    // Retorna um iterator sem expor o vetor interno
    std::unique_ptr<Iterator<Song>> forward_iterator();
    std::unique_ptr<Iterator<Song>> reverse_iterator();
};

auto it = playlist.forward_iterator();
while (it->hasNext()) {
    Song& s = it->next();
    fmt::print("{} — {}\n", s.title, s.artist);
}
```

---

## 6. Observer — Publicação/Assinatura

**Problema**: notificar Logger e Monitor sempre que um evento ocorre no sistema.
**Solução**: EventBus mantém lista de observers e notifica todos na publicação.

```cpp
struct Observer {
    virtual void update(const std::string& event, const std::string& data) = 0;
};

struct EventBus {
    std::unordered_map<std::string, std::vector<Observer*>> subscribers_;

    void subscribe(const std::string& event, Observer* obs) {
        subscribers_[event].push_back(obs);
    }

    void publish(const std::string& event, const std::string& data) {
        for (auto* obs : subscribers_[event])
            obs->update(event, data);
    }
};

// Subscribers
struct Logger  : Observer { void update(const std::string& e, const std::string& d) override { log(e, d); } };
struct Monitor : Observer { void update(const std::string& e, const std::string& d) override { display(e, d); } };

EventBus bus;
Logger  logger;
Monitor monitor;

bus.subscribe("usuario.login", &logger);
bus.subscribe("usuario.login", &monitor);
bus.publish("usuario.login", "user=lucas"); // notifica logger E monitor
```

---

## 7. State — Máquina de Estados TCP

**Problema**: uma conexão TCP tem comportamentos diferentes dependendo do estado (Listen, Established, Closed).
**Solução**: cada estado é um objeto; TCPConnection delega para o estado atual.

```cpp
struct TCPState {
    virtual void connect(TCPConnection&)    = 0;
    virtual void acknowledge(TCPConnection&) = 0;
    virtual void close(TCPConnection&)      = 0;
};

struct ListenState : TCPState {
    void connect(TCPConnection& conn) override {
        fmt::print("Listen → SYN recebido\n");
        conn.change_state(std::make_unique<EstablishedState>());
    }
    void close(TCPConnection&) override { /* não faz nada no estado Listen */ }
};

struct TCPConnection {
    std::unique_ptr<TCPState> state_;

    void change_state(std::unique_ptr<TCPState> s) { state_ = std::move(s); }
    void connect()     { state_->connect(*this); }
    void close()       { state_->close(*this); }
};

TCPConnection conn;
conn.connect(); // Listen → Established
conn.close();   // Established → Closed
```

---

## 8. Strategy — Algoritmos Intercambiáveis

**Problema**: um contexto de ordenação precisa usar diferentes algoritmos dependendo dos dados.
**Solução**: cada algoritmo é uma estratégia trocável em runtime.

```cpp
struct Sorter {
    virtual ~Sorter() = default;
    virtual void sort(std::vector<int>& data) = 0;
    virtual std::string name() const = 0;
};

struct BubbleSort : Sorter {
    void sort(std::vector<int>& data) override { /* O(n²) */ }
    std::string name() const override { return "bubble_sort"; }
};

struct MergeSort : Sorter {
    void sort(std::vector<int>& data) override { /* O(n log n) */ }
    std::string name() const override { return "merge_sort"; }
};

struct SortContext {
    std::unique_ptr<Sorter> strategy_;

    void set_strategy(std::unique_ptr<Sorter> s) { strategy_ = std::move(s); }

    void sort(std::vector<int>& data) {
        fmt::print("Usando: {}\n", strategy_->name());
        strategy_->sort(data);
    }
};

SortContext ctx;
ctx.set_strategy(std::make_unique<MergeSort>());
ctx.sort(dados); // usa merge sort

ctx.set_strategy(std::make_unique<QuickSort>());
ctx.sort(dados); // troca para quick sort em runtime
```

---

## Quando usar cada padrão

| Padrão | Use quando... |
|---|---|
| **Chain** | Uma requisição deve ser processada por uma sequência de handlers |
| **Command** | Precisa de undo/redo, filas de tarefas, ou logging de operações |
| **Decorator** | Quer adicionar comportamentos a objetos sem herança |
| **Factory** | Não quer que o código dependa de classes concretas |
| **Iterator** | Precisa percorrer uma coleção sem expor sua estrutura |
| **Observer** | Vários objetos devem ser notificados de mudanças de estado |
| **State** | Objeto muda comportamento com seu estado interno |
| **Strategy** | Família de algoritmos intercambiáveis |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja design_pattern_chain design_pattern_command
# ... (um target por padrão)
./bin/design_pattern_strategy
./bin/design_pattern_observer
```
