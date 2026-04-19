# ecs — Entity Component System do Zero em C++17

## O que é este projeto?

Implementa um **Entity Component System (ECS)** completo do zero em C++17. ECS é o padrão arquitetural dominante em game engines (Unity, Unreal, Bevy) por ser extremamente eficiente em cache e permitir composição dinâmica de comportamentos sem herança profunda.

---

## Por que ECS?

### Problema com herança clássica:

```cpp
// Hierarquia rígida — precisa decidir em tempo de compilação
class Entidade { };
class EntidadeMovel : public Entidade { Posicao pos; Velocidade vel; };
class EntidadeComSaude : public Entidade { Saude hp; };
class Jogador : public EntidadeMovel, public EntidadeComSaude { }; // explosão combinatória
```

### Solução com ECS:
```cpp
// Composição dinâmica em runtime — sem herança
Entity jogador = world.criar();
world.adicionar<Posicao>(jogador, 0.f, 0.f);
world.adicionar<Velocidade>(jogador, 5.f, 0.f);
world.adicionar<Saude>(jogador, 100, 100);

// Transforma em entidade voadora sem criar nova classe:
world.adicionar<Voando>(jogador);
```

---

## Os três pilares do ECS

| Pilar | Responsabilidade | Neste projeto |
|---|---|---|
| **Entity** | Identificador único (só um número) | `uint32_t` com reciclagem |
| **Component** | Dados puros, sem lógica | `Posicao`, `Velocidade`, `Saude`, etc. |
| **System** | Lógica que opera sobre components | `movimento()`, `colisao()`, `dano()` |

---

## Estrutura de arquivos

```
ecs/
├── main.cpp            ← 5 demos mostrando ECS em ação
├── entity.hpp          ← Entity (uint32_t) + EntityManager com reciclagem
├── component_store.hpp ← armazenamento tipado de componentes
├── world.hpp           ← fachada central: entidades + componentes + queries
├── components.hpp      ← todos os tipos de componente (dados puros)
└── systems.hpp         ← todas as funções de sistema (lógica pura)
```

---

## `Entity` — apenas um número

```cpp
using Entity = uint32_t;

class EntityManager {
    uint32_t proximo_id_ = 1;
    std::vector<Entity> reciclados_; // IDs de entidades destruídas
    std::unordered_set<Entity> vivos_;

public:
    Entity criar() {
        Entity e;
        if (!reciclados_.empty()) {
            e = reciclados_.back();  // reutiliza ID
            reciclados_.pop_back();
        } else {
            e = proximo_id_++;
        }
        vivos_.insert(e);
        return e;
    }

    void destruir(Entity e) {
        vivos_.erase(e);
        reciclados_.push_back(e); // devolve para o pool
    }
};
```

A **reciclagem de IDs** evita crescimento infinito do espaço de IDs em jogos de longa duração (projéteis que nascem e morrem a cada frame).

---

## Components — dados puros

```cpp
// components.hpp — nenhum método, só dados

struct Posicao    { float x, y; };
struct Velocidade { float dx, dy; };
struct Saude      { int hp_atual, hp_max; };
struct Colisao    { float raio; };
struct Dano       { int dano_base; float alcance; };
struct Renderavel { char glyph; };
struct Tag        { std::string nome; };

// Componentes de marcação (sem dados — apenas presença)
struct Jogador {};
struct Inimigo {};
struct Voando  {};

// Componente com lógica de tempo
struct Tempo_de_vida {
    float duracao_inicial;
    float restante;
};
```

---

## `ComponentStore<T>` — armazenamento por tipo

```cpp
template<typename T>
class ComponentStore : public IComponentStore {
    std::unordered_map<Entity, T> dados_;
    std::vector<Entity> entidades_; // para iteração eficiente

public:
    template<typename... Args>
    T& adicionar(Entity e, Args&&... args) {
        entidades_.push_back(e);
        return dados_.emplace(e, T{std::forward<Args>(args)...}).first->second;
    }

    T& get(Entity e)       { return dados_.at(e); }
    bool tem(Entity e) const { return dados_.count(e) > 0; }
    void remover(Entity e)   { dados_.erase(e); /* remove de entidades_ */ }
};
```

---

## `World` — a fachada central

```cpp
class World {
    EntityManager entities_;
    // type_index → ComponentStore (type erasure via IComponentStore*)
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStore>> stores_;

public:
    Entity criar()            { return entities_.criar(); }
    void   destruir(Entity e) { /* remove componentes + entidade */ }

    template<typename T, typename... Args>
    T& adicionar(Entity e, Args&&... args) {
        return store_de<T>().adicionar(e, std::forward<Args>(args)...);
    }

    template<typename T> T& get(Entity e)        { return store_de<T>().get(e); }
    template<typename T> bool tem(Entity e) const { /* ... */ }
    template<typename T> void remover(Entity e)   { store_de<T>().remover(e); }

    // QUERY — o poder do ECS
    template<typename... Ts, typename Func>
    void query(Func&& func) {
        // Itera pelo store do primeiro tipo
        // Para cada entidade, verifica se tem TODOS os tipos pedidos
        auto& primeiro = store_de<first_t<Ts...>>();
        for (Entity e : primeiro.entidades()) {
            if ((tem<Ts>(e) && ...))  // fold expression C++17
                func(e, get<Ts>(e)...);
        }
    }
};
```

### Type Erasure — como guardar stores de tipos diferentes no mesmo map

```cpp
// Interface base sem templates
struct IComponentStore {
    virtual ~IComponentStore() = default;
    virtual void remover(Entity e) = 0;
    virtual bool tem(Entity e) const = 0;
};

// Implementação tipada — invisível externamente
template<typename T>
class ComponentStore : public IComponentStore { /* ... */ };

// No World: armazenamos por std::type_index
std::unordered_map<std::type_index, std::unique_ptr<IComponentStore>> stores_;

// Acesso tipado: static_cast de volta ao tipo correto
template<typename T>
ComponentStore<T>& store_de() {
    auto key = std::type_index(typeid(T));
    return static_cast<ComponentStore<T>&>(*stores_[key]);
}
```

---

## Systems — lógica pura

```cpp
// systems.hpp — funções que operam sobre componentes via queries

namespace systems {

void movimento(World& w, float dt) {
    w.query<Posicao, Velocidade>([&](Entity, Posicao& p, Velocidade& v) {
        p.x += v.dx * dt;
        p.y += v.dy * dt;
    });
}

void tempo_de_vida(World& w, float dt) {
    std::vector<Entity> mortos;
    w.query<Tempo_de_vida>([&](Entity e, Tempo_de_vida& t) {
        t.restante -= dt;
        if (t.restante <= 0.f) mortos.push_back(e);
    });
    for (Entity e : mortos) w.destruir(e);
}

void colisao(World& w) {
    // Detecta colisões entre entidades com Posicao + Colisao
    // Marca Colidindo em ambas as partes
}

} // namespace systems
```

---

## Loop de simulação — os 5 demos

### Demo 1: Entidade = ID, Componente = dados
Cria jogador, inimigo e partícula com componentes diferentes. Mostra `tem<T>()` e `get<T>()`.

### Demo 2: Query — só entidades certas
```cpp
// Só e1 e e2 têm Posicao + Velocidade
world.query<Posicao, Velocidade>([&](Entity e, Posicao& p, Velocidade& v) {
    fmt::print("{}: ({},{}) → ({},{})\n", world.get<Tag>(e).nome,
               p.x, p.y, v.dx, v.dy);
});
```

### Demo 3: Loop de simulação (5 frames)
```cpp
for (int frame = 1; frame <= 5; ++frame) {
    systems::ia(world);           // inimigos perseguem jogador
    systems::aceleracao(world, DT);
    systems::movimento(world, DT); // Posicao += Velocidade * dt
    systems::colisao(world);       // detecta colisões
    systems::dano(world);          // aplica dano aos colididos
    systems::tempo_de_vida(world, DT); // projéteis expiram e são destruídos
    systems::renderizar(world);    // imprime estado
}
```

### Demo 4: Composição dinâmica
```cpp
world.adicionar<Velocidade>(e, 5.f, 3.f); // adiciona comportamento em runtime
systems::movimento(world, 1.f);
world.remover<Velocidade>(e);              // remove: entidade para de se mover
```

### Demo 5: Reciclagem de IDs
```cpp
Entity e1 = world.criar(); // ID: 1
world.destruir(e1);        // ID 1 devolvido ao pool
Entity e2 = world.criar(); // ID: 1 (reutilizado!)
```

---

## Vantagens do ECS

| Vantagem | Detalhe |
|---|---|
| **Cache efficiency** | Componentes do mesmo tipo ficam juntos em memória |
| **Sem herança** | Sem explosão combinatória de classes |
| **Composição dinâmica** | Adicione/remova comportamentos em runtime |
| **Paralelismo fácil** | Sistemas independentes podem rodar em paralelo |
| **Testabilidade** | Cada sistema é uma função pura testável isoladamente |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja ecs
./bin/ecs
```
