# memory — Estratégias de Alocação de Memória em C++

## O que é este projeto?

Guia completo sobre **alocadores de memória** em C++: desde o comportamento de `new`/`delete` até alocadores customizados (pool, arena) e os alocadores polimórficos do C++17 (`std::pmr`).

---

## Por que estudar alocação de memória?

`new`/`delete` são convenientes mas têm custo:
- Chamada ao sistema operacional (syscall)
- Busca por bloco livre na heap
- Fragmentação de memória ao longo do tempo
- Custo não determinístico (varia com o estado da heap)

Para aplicações de alta performance (jogos, sistemas de tempo real, roteadores), alocadores customizados podem dar **10x–100x de speedup** nas alocações.

---

## Estrutura de arquivos

```
memory/
├── main.cpp                  ← executa todos os demos
├── demo_new_delete.hpp       ← comportamento interno de new/delete
├── demo_pool_allocator.hpp   ← pool de blocos de tamanho fixo
├── demo_arena_allocator.hpp  ← arena: aloca tudo, libera tudo
└── demo_pmr.hpp              ← std::pmr — alocadores polimórficos C++17
```

---

## Demo 1: `new`/`delete` — o que realmente acontece

```cpp
// new T equivale a:
// 1. void* p = operator new(sizeof(T))  → aloca bytes (pode lançar bad_alloc)
// 2. new(p) T(args...)                  → chama construtor (placement new)

// delete p equivale a:
// 1. p->~T()                            → chama destrutor
// 2. operator void(p)                   → libera bytes

// Sobrescrevendo operator new globalmente (para diagnóstico)
std::size_t total_alocado = 0;
std::size_t num_alocacoes = 0;

void* operator new(std::size_t size) {
    total_alocado += size;
    ++num_alocacoes;
    return std::malloc(size);
}

void operator delete(void* p) noexcept {
    std::free(p);
}
```

### Placement new — construir em memória já alocada

```cpp
// Aloca buffer
alignas(MyObject) std::byte buf[sizeof(MyObject)];

// Constrói objeto no buffer (sem alocação!)
MyObject* obj = new(buf) MyObject(42);

// Destrói explicitamente (sem liberar memória)
obj->~MyObject();
// Buffer ainda válido — pode reusar
```

---

## Demo 2: Pool Allocator — blocos de tamanho fixo

### Problema que resolve

Muitas alocações/liberações de objetos do mesmo tamanho causam **fragmentação**:

```
Heap antes: [livre: 100 bytes contínuos]
Aloca A (20 bytes): [A(20)][livre(80)]
Aloca B (20 bytes): [A(20)][B(20)][livre(60)]
Libera A:           [livre(20)][B(20)][livre(60)]
Aloca C (40 bytes): FALHA! Não há bloco contínuo de 40 bytes!
                    (total livre = 80, mas fragmentado)
```

### Solução: pool de blocos fixos

```cpp
template<typename T, std::size_t PoolSize>
class PoolAllocator {
    // Buffer contínuo para PoolSize objetos
    alignas(T) std::byte storage_[sizeof(T) * PoolSize];

    // Free list: singly-linked list de blocos livres
    struct FreeNode { FreeNode* next; };
    FreeNode* free_list_ = nullptr;

public:
    PoolAllocator() {
        // Inicializa a free list encadeando todos os blocos
        for (std::size_t i = 0; i < PoolSize; ++i) {
            auto* node = reinterpret_cast<FreeNode*>(storage_ + i * sizeof(T));
            node->next = free_list_;
            free_list_ = node;
        }
    }

    T* allocate() {
        if (!free_list_) throw std::bad_alloc{};
        FreeNode* node = free_list_;
        free_list_ = node->next;
        return reinterpret_cast<T*>(node); // O(1)!
    }

    void deallocate(T* p) {
        p->~T();
        auto* node = reinterpret_cast<FreeNode*>(p);
        node->next = free_list_;
        free_list_ = node; // O(1)!
    }
};
```

### Vantagens do Pool Allocator:

| Característica | `new`/`delete` | Pool Allocator |
|---|---|---|
| Tempo de alocação | O(n) amortizado, variável | O(1) constante |
| Fragmentação | Pode ocorrer | Zero (blocos fixos) |
| Overhead por objeto | 8–16 bytes (metadata) | Zero |
| Cache locality | Ruim (disperso) | Excelente (contínuo) |

---

## Demo 3: Arena Allocator — aloca tudo, libera tudo

```cpp
class ArenaAllocator {
    std::vector<std::byte> buffer_;
    std::size_t offset_ = 0;

public:
    explicit ArenaAllocator(std::size_t size) : buffer_(size) {}

    void* allocate(std::size_t size, std::size_t align = alignof(std::max_align_t)) {
        // Alinha o offset atual
        std::size_t aligned = (offset_ + align - 1) & ~(align - 1);
        if (aligned + size > buffer_.size()) throw std::bad_alloc{};

        void* ptr = buffer_.data() + aligned;
        offset_ = aligned + size;
        return ptr; // O(1)!
    }

    void reset() { offset_ = 0; } // "libera" TUDO — O(1)!
    // Sem free individual!
};

// Uso clássico: lógica de frame em jogos
ArenaAllocator frame_arena(1024 * 1024); // 1 MB por frame

while (true) {
    frame_arena.reset();  // zera tudo — O(1)
    // ... alocações do frame ...
    // Objetos destruídos implicitamente no reset()
}
```

### Vantagens da Arena:

- **Zero overhead** por alocação (só incrementa um pointer)
- **Localidade de cache** perfeita (todos os objetos contíguos)
- **Liberação O(1)**: reset de um contador vs percorrer toda a heap
- **Sem fragmentação**: alocações lineares, liberação em bloco

### Limitação:

Não é possível liberar objetos individualmente — ou todos ou nenhum.

---

## Demo 4: `std::pmr` — Alocadores Polimórficos (C++17)

`std::pmr` (Polymorphic Memory Resource) permite trocar o alocador de qualquer container STL sem mudar o tipo do container.

```cpp
#include <memory_resource>

// Recursos de memória padrão
std::pmr::monotonic_buffer_resource arena(1024 * 1024); // como ArenaAllocator
std::pmr::unsynchronized_pool_resource pool;            // pool de diferentes tamanhos
std::pmr::synchronized_pool_resource thread_safe_pool;  // thread-safe

// Usando com containers
std::byte buf[4096];
std::pmr::monotonic_buffer_resource local_arena(buf, sizeof(buf));

// vector que aloca em local_arena (na stack!)
std::pmr::vector<int> v(&local_arena);
v.push_back(1);
v.push_back(2);
// Nenhuma alocação na heap!

// map com arena de frame
std::pmr::map<std::string, int> m(&local_arena);
m["chave"] = 42;
```

### Cadeia de recursos

```cpp
// Se local_arena esgotar, usa new_delete_resource como fallback
std::pmr::new_delete_resource fallback;  // usa operator new/delete
std::pmr::monotonic_buffer_resource arena(
    buf, sizeof(buf),
    &fallback  // fallback quando a arena esgota
);
```

---

## Quando usar cada um?

| Cenário | Alocador recomendado |
|---|---|
| Uso geral | `new`/`delete` (padrão) |
| Muitos objetos iguais com alta frequência | Pool Allocator |
| Objetos temporários de frame (jogos) | Arena / monotonic_buffer |
| Protótipo rápido de alocador customizado | `std::pmr` |
| Múltiplos containers com mesmo alocador | `std::pmr` |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja memory
./bin/memory
```

**Requer C++17** para `std::pmr`.
