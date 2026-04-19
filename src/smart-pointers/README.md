# smart-pointers — Gerenciamento Moderno de Memória em C++

## O que é este projeto?

Guia completo de **smart pointers** em C++ moderno: `unique_ptr`, `shared_ptr`, `weak_ptr` e padrões práticos de uso. Eliminar `new`/`delete` manual é a forma mais eficaz de prevenir memory leaks e use-after-free.

---

## Por que smart pointers?

```cpp
// Antes (C++03): propenso a leaks
void ruim() {
    Widget* w = new Widget();
    if (alguma_condicao) return; // LEAK! Widget nunca é deletado
    throw SomeException();       // LEAK! Widget nunca é deletado
    delete w;                    // só chega aqui no caso feliz
}

// Depois (C++11): seguro sempre
void bom() {
    auto w = std::make_unique<Widget>();
    if (alguma_condicao) return; // OK: destrutor chamado automaticamente
    throw SomeException();       // OK: destrutor chamado automaticamente
}                                // OK: destrutor chamado automaticamente
```

---

## Estrutura de arquivos

```
smart-pointers/
├── main.cpp           ← executa todos os demos
├── demo_unique.hpp    ← unique_ptr: propriedade exclusiva
├── demo_shared.hpp    ← shared_ptr: propriedade compartilhada
├── demo_weak.hpp      ← weak_ptr: referência não-proprietária
└── demo_patterns.hpp  ← padrões práticos: RAII, pimpl, factory
```

---

## Demo 1: `unique_ptr` — Propriedade Exclusiva

`unique_ptr` representa **um único dono** de um recurso. Quando o `unique_ptr` é destruído, o objeto também é.

```cpp
// Criação — preferir make_unique (C++14)
auto p = std::make_unique<Widget>(42);
// equivale a: unique_ptr<Widget> p(new Widget(42));
// vantagem do make_unique: exception-safe, sem new explícito

// Acesso
p->metodo();     // como ponteiro normal
(*p).metodo();   // desreferência

// Sem cópia — único dono!
auto q = p;      // ERRO DE COMPILAÇÃO

// Transferência de posse (move)
auto q = std::move(p);
// Agora q é o dono. p == nullptr.
// p->metodo() aqui seria UB!

// Verifica se tem objeto
if (p) { /* p tem objeto */ }
if (p != nullptr) { /* idem */ }

// Libera sem deletar (transfere para ponteiro cru — cuidado!)
Widget* raw = p.release(); // p == nullptr, você é responsável pelo delete
delete raw;

// Deleter customizado
auto closer = [](FILE* f){ fclose(f); };
std::unique_ptr<FILE, decltype(closer)> file(fopen("x.txt", "r"), closer);
// FILE fechado automaticamente no fim do escopo
```

---

## Demo 2: `shared_ptr` — Propriedade Compartilhada

`shared_ptr` usa **contagem de referências**. O objeto é destruído quando o último `shared_ptr` é destruído.

```cpp
// Criação — preferir make_shared (aloca objeto + contador em um bloco)
auto p = std::make_shared<Widget>(42);

// Cópia: incrementa contador
auto q = p;
auto r = p;
// contador = 3

fmt::print("{}\n", p.use_count()); // 3

// Destruição: decrementa contador
q.reset(); // contador = 2
{
    auto s = p; // contador = 3
}              // s destruído → contador = 2
// Quando use_count() chega a 0 → Widget é destruído

// Pointer aritmético do shared_ptr
p.get();        // ponteiro cru (não transfere posse)
p.use_count();  // conta de referências
p.unique();     // true se use_count() == 1 (C++20: removed)
```

### `make_shared` vs `new`

```cpp
// Com new: duas alocações (objeto + bloco de controle)
shared_ptr<Widget> p(new Widget()); // EVITAR

// Com make_shared: uma única alocação
auto p = make_shared<Widget>();     // PREFERIR
// Mais eficiente em memória e cache-friendly
// Também exception-safe
```

---

## Demo 3: `weak_ptr` — Referência Não-Proprietária

`weak_ptr` **observa** um objeto gerenciado por `shared_ptr` sem aumentar a contagem de referências. Resolve **ciclos de referência** e implementa **caches/observers**.

```cpp
auto shared = std::make_shared<Widget>(42);
std::weak_ptr<Widget> weak = shared; // não incrementa use_count

// weak.use_count() NÃO inclui weak_ptrs — conta apenas shared_ptrs
fmt::print("{}\n", shared.use_count()); // 1 (não 2!)

// Para usar o objeto via weak_ptr: DEVE converter para shared_ptr
if (auto locked = weak.lock()) {    // lock: tenta obter shared_ptr
    locked->metodo(); // objeto existe
} else {
    // objeto foi destruído
}

// Verificar sem usar
if (!weak.expired()) { /* objeto ainda existe */ }
```

### Quebrando ciclos de referência

```cpp
// PROBLEMA: ciclo de referência — nenhum dos dois é destruído!
struct No {
    shared_ptr<No> proximo;  // ciclo!
};
auto a = make_shared<No>();
auto b = make_shared<No>();
a->proximo = b;
b->proximo = a; // ciclo: a→b→a, use_count nunca chega a 0

// SOLUÇÃO: weak_ptr quebra o ciclo
struct No {
    weak_ptr<No> proximo;  // não aumenta use_count
};
// Agora quando a e b saem de escopo → use_count chega a 0 → destruídos
```

---

## Demo 4: Padrões Modernos

### RAII Guard com `unique_ptr`

```cpp
// Guard genérico para qualquer recurso com função de "fechamento"
template<typename T, typename Deleter>
auto make_guard(T* resource, Deleter deleter) {
    return std::unique_ptr<T, Deleter>(resource, deleter);
}

// Uso:
auto file    = make_guard(fopen("x.txt", "r"), fclose);
auto mutex   = make_guard(&mtx, [](auto* m){ m->unlock(); });
auto socket  = make_guard(socket_fd, close_socket);
// Todos os recursos fechados automaticamente ao sair do escopo
```

### Pimpl (Pointer to Implementation)

```cpp
// Separa interface de implementação — reduz dependências de compilação

// widget.hpp — apenas declara Impl (forward declaration)
class Widget {
public:
    Widget();
    ~Widget();
    void show();
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_; // tamanho fixo (um ponteiro)
};

// widget.cpp — implementação completa
struct Widget::Impl {
    std::string titulo;
    int         x, y;
    // ... muitos campos ...
};

Widget::Widget() : pimpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default; // unique_ptr cuida da destruição
void Widget::show() { /* usa pimpl_->titulo, etc. */ }
```

### Factory com `unique_ptr`

```cpp
// Factory que retorna unique_ptr → propriedade clara de quem cria
std::unique_ptr<Shape> criar_shape(const std::string& tipo) {
    if (tipo == "circulo")   return std::make_unique<Circulo>();
    if (tipo == "quadrado")  return std::make_unique<Quadrado>();
    return nullptr;
}

auto shape = criar_shape("circulo");
shape->desenhar(); // polimorfico
// destruído automaticamente
```

---

## Regras de ouro

| Situação | Use |
|---|---|
| Único dono do objeto | `unique_ptr` |
| Propriedade compartilhada | `shared_ptr` |
| Observar sem possuir | `weak_ptr` |
| Ponteiro sem posse para função | `T*` ou `T&` |
| Criar `unique_ptr` | `make_unique<T>()` |
| Criar `shared_ptr` | `make_shared<T>()` |
| Nunca mais | `new` / `delete` manual |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja smart_pointers
./bin/smart_pointers
# Saída: "Todos os demos concluídos sem leaks!"
```
