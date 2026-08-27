# ecs — Entity Component System do zero

## O que é este projeto?

Uma implementação completa e comentada de um **Entity Component System (ECS)**, escrita do
zero, sem biblioteca nenhuma além do `fmt` para imprimir.

ECS é o jeito como game engines modernas (Unity DOTS, Bevy, Overwatch, muitos motores
próprios) organizam o mundo simulado. A ideia inteira cabe em três frases:

1. uma **entidade** é só um número inteiro — não tem dados nem métodos;
2. um **componente** é uma struct de dados puros, associada a uma entidade;
3. um **sistema** é uma função que percorre todas as entidades que têm certa combinação de
   componentes e faz alguma coisa com elas.

O que sai disso é um programa em que os dados ficam agrupados **por tipo** em arrays
contíguos, e não espalhados dentro de objetos — e é exatamente daí que vem o ganho de
desempenho. Este README explica esse mecanismo do começo.

```bash
make build      # compila tudo em build/  (usa o conanbuild.sh para achar o fmt)
make install    # copia os binários para dist/bin/
./dist/bin/ecs
```

> Se você só quer rodar sem instalar, o binário também fica em `build/src/ecs/ecs`.

---

## Antes de começar

Esta seção define **todos** os termos usados no resto do arquivo. Se algum já for familiar,
pule; nenhum deles será usado antes de aparecer aqui.

### Sobre memória

| Termo | O que é |
|---|---|
| **linha de cache** (*cache line*) | A CPU nunca lê 1 byte da RAM. Ela lê um bloco fixo, hoje quase sempre de **64 bytes**, e guarda esse bloco no cache. Pedir 1 byte custa o mesmo que pedir os 64. |
| **cache miss** | Quando o dado pedido não está no cache e a CPU precisa esperar a RAM. Custa da ordem de **100 ciclos**; um acerto no cache L1 custa ~4. |
| **prefetch** | O hardware observa o padrão de acessos. Se você percorre a memória em passos regulares para a frente, ele **adivinha** e busca as próximas linhas antes de você pedir. Só funciona com acesso previsível — isto é, sequencial. |
| **array contíguo** | Um bloco único de memória, sem buracos: `std::vector<T>`. Elemento `i` fica em `base + i*sizeof(T)`. É o layout que o prefetch entende. |
| **array denso** (*packed*) | Um array contíguo **sem posições vazias**: todos os slots de `0` a `n-1` valem. É o que este projeto mantém. |
| **array esparso** (*sparse*) | Uma tabela `chave → posição` usada para achar onde um item está no array denso. Aqui o papel é do `std::unordered_map<Entity, size_t>`. |
| **invalidação de referências** | Quando um `std::vector` cresce além da capacidade, ele aloca um bloco novo, copia/move tudo e libera o antigo. **Toda referência, ponteiro e iterador para os elementos antigos vira lixo.** Usá-los depois é *undefined behavior*. Isto aparece de novo, com um teste real, na seção de armadilhas. |

### Sobre C++

| Termo | O que é |
|---|---|
| **template variádico** | `template <typename... Ts>` — um template que aceita um número qualquer de tipos. |
| **parameter pack** | O `Ts` acima. Não é um tipo: é uma **lista** de tipos. Não dá para usá-lo sozinho; ele precisa ser *expandido*. |
| **expansão de pack** | Escrever `padrão...` repete o padrão uma vez por elemento, separado por vírgulas. Com `Ts = <Posicao, Velocidade>`, o texto `get<Ts>(e)...` vira `get<Posicao>(e), get<Velocidade>(e)`. |
| **fold expression** (C++17) | Expande um pack colando um **operador** entre os itens em vez de vírgulas. `(tem<Ts>(e) && ...)` com o mesmo pack vira `tem<Posicao>(e) && tem<Velocidade>(e)`. Com o pack vazio, `&&` dobra para `true`. |
| **perfect forwarding** | `Args&&... args` + `std::forward<Args>(args)...` repassa os argumentos preservando se eram lvalue ou rvalue — de modo que um temporário continue sendo movido, e não copiado, ao chegar no construtor lá no fim. |
| **type erasure** | Guardar objetos de tipos diferentes atrás de **uma interface comum**, esquecendo o tipo concreto, e recuperá-lo depois quando se sabe qual era. É o que permite `ComponentStore<Posicao>` e `ComponentStore<Saude>` — dois tipos sem parentesco — morarem no mesmo container. |
| **RTTI / `typeid` / `std::type_index`** | `typeid(T)` devolve um `std::type_info` que identifica o tipo `T` em tempo de execução. `std::type_info` não é copiável nem hasheável; `std::type_index` é um invólucro que **é** — então serve de chave de `unordered_map`. |
| **`std::unique_ptr<T>`** | Ponteiro que **possui** o objeto e o destrói ao sair de escopo. Usado aqui porque o `World` guarda ponteiros para uma classe base polimórfica (`IComponentStore`) e precisa que o `delete` chame o destrutor certo — daí o `virtual ~IComponentStore()`. |

---

## Por que estudar isto?

### Problema 1 — a herança não fecha a conta

Modelando um jogo com hierarquia de classes, cada combinação de capacidades vira uma classe:

```cpp
class Entidade { };
class EntidadeMovel      : public Entidade { Posicao pos; Velocidade vel; };
class EntidadeComSaude   : public Entidade { Saude hp; };
class Jogador : public EntidadeMovel, public EntidadeComSaude { };
```

Com 3 capacidades já são 7 combinações possíveis; com 10, são 1023. E o pior: a decisão é
tomada **em tempo de compilação**. Fazer uma pedra passar a se mover exige uma classe nova.

Com ECS, a mesma coisa é um `push_back`:

```cpp
ecs::Entity pedra = world.criar();
world.adicionar<ecs::Posicao>(pedra, 9.0f, 9.0f);
// ... mais tarde, em runtime:
world.adicionar<ecs::Velocidade>(pedra, 1.0f, 0.0f);   // a pedra agora se move
world.remover<ecs::Velocidade>(pedra);                 // e agora parou
```

### Problema 2 — o layout de memória (o motivo real)

Este é o argumento que sustenta o ECS inteiro, e ele merece números.

Um `Posicao` tem dois `float`: **8 bytes**. Uma linha de cache tem 64 bytes. Logo, em um
`std::vector<Posicao>` cabem **8 posições por linha de cache**. Percorrer 1.000.000 de
posições custa `1.000.000 / 8 = 125.000` linhas trazidas da RAM — e o prefetch, vendo o
padrão sequencial, busca a maior parte delas antes de você precisar.

Agora guarde os mesmos objetos em um `std::unordered_map<Entity, Posicao>`: cada valor mora
em um nó alocado separadamente no heap, em endereço imprevisível. Percorrer o mapa é seguir
uma lista de ponteiros. No pior caso, **um cache miss por elemento**.

Medido nesta máquina (`g++ -O2`, 1.000.000 de elementos, inseridos em ordem embaralhada
para simular um heap fragmentado de verdade — o programa de medição está na seção
[Como validar](#como-validar)):

```
soma de 1.000.000 de Posicao.x
  std::vector<Posicao>                 :  0,72 ms
  std::unordered_map<Entity, Posicao>  : 20,7 ms      ← ~29× mais lento
```

Mesma quantidade de dados, mesmo resultado, mesma complexidade assintótica (O(n) nos dois
casos). A diferença é **inteiramente** de layout de memória.

> É por isso que `component_store.hpp` começa dizendo que `unordered_map<Entity, T>` é *"o
> que NÃO fazemos"*. O `unordered_map` continua no projeto, mas só como **índice**
> (`Entity → posição`), consultado uma vez por entidade — nunca como o container que os
> sistemas percorrem.

---

## Estrutura de arquivos

```
src/ecs/
├── main.cpp             ← 5 demos executáveis
├── entity.hpp           ← Entity (uint32_t) + EntityManager (free list de IDs)
├── component_store.hpp  ← ComponentStore<T>: array denso + índice esparso
├── components.hpp       ← os 12 tipos de componente (dados puros)
├── world.hpp            ← World: fachada, type erasure e query<Ts...>
├── systems.hpp          ← as 7 funções de sistema (lógica pura)
├── meson.build
└── README.md
```

Nenhum arquivo `.cpp` além do `main.cpp`: tudo é template ou função pequena, então mora em
cabeçalho.

---

## O exemplo mínimo, colável

Este programa compila e roda. Ele usa só `world.hpp` e `components.hpp` — nem o `fmt` é
preciso:

```cpp
#include "world.hpp"        // src/ecs/world.hpp
#include "components.hpp"   // src/ecs/components.hpp
#include <cstdio>

int main() {
    ecs::World w;

    ecs::Entity nave = w.criar();                    // 1) a entidade: só um número
    w.adicionar<ecs::Posicao>(nave, 0.0f, 0.0f);     // 2) os dados
    w.adicionar<ecs::Velocidade>(nave, 3.0f, 4.0f);

    ecs::Entity pedra = w.criar();                   // pedra NÃO tem Velocidade
    w.adicionar<ecs::Posicao>(pedra, 9.0f, 9.0f);

    // 3) o sistema: roda só em quem tem Posicao E Velocidade
    w.query<ecs::Posicao, ecs::Velocidade>(
        [](ecs::Entity, ecs::Posicao& p, ecs::Velocidade& v) {
            p.x += v.dx;
            p.y += v.dy;
        });

    std::printf("nave  = (%.0f, %.0f)\n",
                w.get<ecs::Posicao>(nave).x,  w.get<ecs::Posicao>(nave).y);
    std::printf("pedra = (%.0f, %.0f)\n",
                w.get<ecs::Posicao>(pedra).x, w.get<ecs::Posicao>(pedra).y);
}
```

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Isrc/ecs /tmp/minimo.cpp -o /tmp/minimo && /tmp/minimo
```

```
nave  = (3, 4)
pedra = (9, 9)
```

A pedra não se moveu porque a `query` **não a visitou**. Esse "não visitou" é o ECS inteiro:
a lista de tipos no `query<...>` é o filtro.

---

## Como funciona por dentro

### `Entity` — só um `uint32_t`

```cpp
// entity.hpp
using Entity = uint32_t;
constexpr Entity ENTITY_NULL = UINT32_MAX;   // sentinela de "entidade inválida"

class EntityManager {
public:
    explicit EntityManager(uint32_t max_entities = 10000)
        : max_(max_entities), proximo_(0) {}

    Entity criar() {
        if (!livres_.empty()) {              // reaproveita um ID devolvido
            Entity id = livres_.front();
            livres_.pop();
            vivos_.push_back(id);
            return id;
        }
        if (proximo_ >= max_)                // ⚠ o limite é real e LANÇA
            throw std::runtime_error("EntityManager: limite de entidades atingido");
        Entity id = proximo_++;
        vivos_.push_back(id);
        return id;
    }

    void destruir(Entity e) {
        auto it = std::find(vivos_.begin(), vivos_.end(), e);
        if (it != vivos_.end()) {
            vivos_.erase(it);
            livres_.push(e);                 // devolve o ID para a fila
        }
    }

private:
    uint32_t            max_;
    uint32_t            proximo_;
    std::vector<Entity> vivos_;
    std::queue<Entity>  livres_;             // free list, em ordem FIFO
};
```

Três fatos que a leitura casual perde:

- **o primeiro ID é `0`**, não `1` — `proximo_` começa em zero;
- o construtor impõe um **teto de 10.000 entidades** por padrão. Estourar não retorna erro:
  **lança** `std::runtime_error`. Passe outro valor em `World w{100000};` se precisar;
- `destruir` e `existe` fazem `std::find` em um `std::vector` — **O(n)**. Aceitável para um
  projeto didático; um ECS de produção usa um bitset ou a técnica de índice+geração descrita
  [mais abaixo](#o-que-falta-para-um-ecs-de-produção).

### `ComponentStore<T>` — array denso + índice esparso

Este é o coração do projeto. Três containers trabalhando juntos:

```cpp
// component_store.hpp
template <typename T>
class ComponentStore : public IComponentStore {
    std::vector<T>                          componentes_; // ← os dados, CONTÍGUOS
    std::vector<Entity>                     entidades_;   // quem está em cada slot
    std::unordered_map<Entity, std::size_t> indice_;      // entity → slot
};
```

```
slot:          0     1     2     3
componentes_: [T ]  [T ]  [T ]  [T ]   ← contíguo; é isto que os sistemas percorrem
entidades_:   [e2]  [e0]  [e5]  [e1]   ← paralelo ao de cima, mesmo índice
indice_:      e0→1, e1→3, e2→0, e5→2   ← consultado UMA vez, em get()/tem()
```

`componentes_` e `entidades_` são **paralelos**: o componente do slot `i` pertence à entidade
`entidades_[i]`. O `indice_` só existe para responder "onde está a entidade `e`?".

#### Adicionar

```cpp
template <typename... Args>
T& adicionar(Entity e, Args&&... args) {
    if (indice_.count(e))
        throw std::runtime_error("ComponentStore: entidade já tem este componente");

    indice_[e] = componentes_.size();                     // o slot é o fim do array
    entidades_.push_back(e);
    componentes_.emplace_back(std::forward<Args>(args)...); // constrói IN-PLACE
    return componentes_.back();
}
```

`emplace_back` com perfect forwarding constrói o `T` **dentro** do vector, sem criar um
temporário para depois copiar. É por isso que `world.adicionar<Posicao>(e, 0.0f, 0.0f)` passa
os argumentos do construtor, e não um `Posicao` pronto.

Adicionar duas vezes o mesmo tipo na mesma entidade **lança**. É proposital: silenciosamente
sobrescrever seria pior.

#### Remover mantendo o array denso — *swap com o último*

Apagar do meio de um `vector` com `erase` desloca todo o resto: O(n). O truque padrão é
**mover o último elemento para o buraco** e encolher: O(1).

```cpp
void remover(Entity e) override {
    if (!indice_.count(e)) return;

    std::size_t slot   = indice_[e];
    std::size_t ultimo = componentes_.size() - 1;

    if (slot != ultimo) {
        componentes_[slot] = std::move(componentes_[ultimo]);
        Entity e_ultimo    = entidades_[ultimo];
        entidades_[slot]   = e_ultimo;
        indice_[e_ultimo]  = slot;          // ← o passo que todo mundo esquece
    }

    componentes_.pop_back();
    entidades_.pop_back();
    indice_.erase(e);
}
```

Passo a passo, removendo a entidade que está no slot 1 de um store com quatro itens:

```
antes       slot:  0    1    2    3
            comp: [A]  [B]  [C]  [D]
            ents: [eA] [eB] [eC] [eD]
            ind :  eA→0  eB→1  eC→2  eD→3

1) move o último para o buraco     comp: [A]  [D]  [C]  [D]
2) atualiza o índice de quem mudou  ind :  eD→1
3) pop_back                        comp: [A]  [D]  [C]
                                   ents: [eA] [eD] [eC]
                                   ind :  eA→0  eC→2  eD→1   (eB removido)
```

> **⚠ A ordem dos componentes muda.** Depois de qualquer remoção, `D` está antes de `C`.
> Se algum sistema depender da ordem de iteração (desenho por camada, resolução de
> prioridade), ele **vai quebrar de forma intermitente**. A saída é ordenar explicitamente,
> ou guardar a ordem em um componente próprio — nunca confiar na ordem do store.

### `World` — a fachada, e onde mora o type erasure

O `World` precisa guardar `ComponentStore<Posicao>`, `ComponentStore<Saude>`,
`ComponentStore<Tag>`… tipos que não têm nenhum parentesco entre si. Um `std::unordered_map`
exige **um** tipo de valor. A solução é o type erasure:

```cpp
// 1) uma interface SEM template, com só o que dá para fazer sem saber o tipo
struct IComponentStore {
    virtual ~IComponentStore() = default;
    virtual void remover(Entity e) = 0;
    virtual bool tem(Entity e) const = 0;
};

// 2) o store concreto herda dela
template <typename T>
class ComponentStore : public IComponentStore { /* ... */ };

// 3) o container guarda ponteiros para a interface — o tipo foi "apagado"
std::unordered_map<std::type_index, std::unique_ptr<IComponentStore>> stores_;

// 4) e é recuperado quando se sabe qual era
template <typename T>
ComponentStore<T>& store_de() {
    auto key = std::type_index(typeid(T));      // chave = identidade do tipo T
    auto it  = stores_.find(key);
    if (it == stores_.end())
        stores_[key] = std::make_unique<ComponentStore<T>>();   // cria sob demanda
    return static_cast<ComponentStore<T>&>(*stores_[key]);      // ← ver aviso
}
```

O `static_cast` para baixo (*downcast*) é seguro **porque a chave garante o tipo**: a entrada
sob `type_index(typeid(Posicao))` só pode ter sido criada como `ComponentStore<Posicao>`, na
linha logo acima. Se o invariante fosse quebrado, seria UB — `dynamic_cast` seria a versão
verificada, e mais cara.

O `virtual ~IComponentStore()` **não é decorativo**: sem ele, o `delete` feito pelo
`unique_ptr<IComponentStore>` chamaria o destrutor errado e vazaria os três vectors do store
concreto. Isto também é UB.

### `query<Ts...>` — a expansão do pack, escrita por extenso

```cpp
template <typename... Ts, typename Func>
void query(Func&& func) {
    auto& primeiro_store = store_de<first_t<Ts...>>();   // store do PRIMEIRO tipo
    const auto& ents = primeiro_store.entidades();

    for (std::size_t i = 0; i < ents.size(); ++i) {
        Entity e = ents[i];
        if (tem_todos<Ts...>(e))
            func(e, get<Ts>(e)...);
    }
}

template <typename... Ts>
bool tem_todos(Entity e) const {
    return (tem<Ts>(e) && ...);       // fold expression, C++17
}
```

Com `Ts = <Posicao, Velocidade>`, o compilador escreve literalmente:

```cpp
if (tem<Posicao>(e) && tem<Velocidade>(e))          // ← de (tem<Ts>(e) && ...)
    func(e, get<Posicao>(e), get<Velocidade>(e));   // ← de get<Ts>(e)...
```

O `first_t<Ts...>` é um **helper privado do próprio `World`** — um alias não consegue pegar
"o primeiro do pack" sozinho, é preciso uma struct com especialização parcial:

```cpp
template <typename... Ts> struct first_type;                        // declarada
template <typename T, typename... Rest>
struct first_type<T, Rest...> { using type = T; };                  // especializada

template <typename... Ts>
using first_t = typename first_type<Ts...>::type;
```

Copiar o corpo de `query` para outro lugar **não compila** sem levar junto esse helper.

#### ⚠ O custo da query e a ordem dos tipos

`query<A, B>` percorre o store de **A** e testa `tem<B>` em cada um. Logo o custo é
`tamanho_do_store<A>` consultas ao índice — e **a ordem dos tipos importa**:

| Consulta | Iterações | Comentário |
|---|---|---|
| `query<Posicao, Jogador>` | uma por entidade com `Posicao` (todas) | testa `Jogador` milhares de vezes |
| `query<Jogador, Posicao>` | uma por entidade com `Jogador` (uma) | testa `Posicao` uma vez |

**Regra: ponha primeiro o componente mais raro.** Em `systems.hpp`, `ia()` faz
`query<Posicao, Jogador>` — a ordem cara. Está assim de propósito, para ser visível.

Note também que esta implementação **não escolhe o menor store automaticamente** (o comentário
"menor iteração possível" no fonte descreve a intenção, não o que o código faz). Comparar os
`contagem()` dos stores e iterar o menor é um exercício excelente, e são umas 10 linhas.

### Componentes — dados puros

```cpp
// components.hpp — sem herança, sem virtual, sem lógica de negócio
struct Posicao    { float x = 0, y = 0;  /* + construtores */ };
struct Velocidade { float dx = 0, dy = 0; };
struct Colisao    { float raio = 1.0f; bool colidindo = false; };
struct Renderavel { char simbolo = '?'; float escala = 1.0f; bool visivel = true; };
struct Tag        { std::string nome; };
struct Jogador    { int score = 0; int nivel = 1; };
struct Inimigo    { float agressividade = 1.0f; bool alertado = false; };
```

`Saude` tem `vivo()` e `ratio()`, que são *acessores* sobre os próprios campos — não regras
de jogo. A fronteira é essa: **se o método precisa olhar outra entidade, ele é um sistema.**

`Jogador` e `Inimigo` funcionam como *tag components*: o que importa é a **presença** deles
na entidade, não os valores.

### Sistemas — funções, não objetos

```cpp
// systems.hpp
void movimento(World& world, float dt) {
    world.query<Posicao, Velocidade>([&](Entity, Posicao& pos, Velocidade& vel) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    });
}
```

Um sistema não guarda estado. Precisou de estado persistente? A recomendação do próprio
`systems.hpp` é criar um **componente singleton**: uma entidade única que representa o mundo
(placar, dificuldade, clima) e um `query` sobre ela. Assim o estado continua sendo dado no
`World`, salvável e inspecionável como todo o resto.

---

## Os demos, um a um

Os cinco demos são o `main.cpp`. Toda saída abaixo foi copiada de `./dist/bin/ecs`.

### Demo 1 — entidade é ID, componente é dado

Cria três entidades com combinações diferentes e consulta com `tem<T>` e `get<T>`.

```
  IDs: jogador=0, inimigo=1, particula=2
  entidades criadas: 3
  posição do jogador: (0, 0)
  jogador tem Saude?     true
  particula tem Saude?   false
  inimigo tem Jogador?   false
```

Repare no `IDs: 0, 1, 2` — a numeração começa em **zero**.

### Demo 2 — a query como filtro

Quatro entidades, quatro combinações. Três consultas sobre o mesmo mundo:

```
  total de entidades: 4

  query<Posicao, Velocidade>:
    e1 → pos(1,1) vel(1,0)
    e2 → pos(2,2) vel(0,1)

  query<Posicao>:
    e1 → pos(1,1)
    e2 → pos(2,2)
    e3 → pos(3,3)

  query<Posicao, Velocidade, Saude>:
    e2 → HP 100/100
```

`e4` tem só `Velocidade` e nunca aparece: como o primeiro tipo pedido é sempre `Posicao`, a
query itera o store de `Posicao`, onde `e4` não está. Acrescentar um tipo à lista só reduz o
resultado — a query é sempre um **E** lógico.

### Demo 3 — o loop de simulação (5 frames)

O demo grande: 1 jogador, 1 inimigo e 3 projéteis com `Tempo_de_vida` de 1,5 s, `dt = 0,5 s`.
Sete sistemas rodam em ordem fixa a cada frame — `ia`, `aceleracao`, `movimento`, `colisao`,
`dano`, `tempo_de_vida`, `renderizar`.

```
  entidades iniciais: 5

  --- frame 1 (t=0.5s) ---
    [ia] inimigo alertado! (dist=80.0)
    [colisao] 'jogador' ↔ 'proj-0' (dist=5.0)
    [render] @@(5,0) E@(76,0) *@(10,0) *@(10,10) *@(10,20)
  entidades vivas: 5

  --- frame 2 (t=1.0s) ---
    [render] @@(10,0) E@(72,0) *@(20,0) *@(20,10) *@(20,20)
  entidades vivas: 5

  --- frame 3 (t=1.5s) ---
    [ttl] entidade 'proj-0' expirou
    [ttl] entidade 'proj-1' expirou
    [ttl] entidade 'proj-2' expirou
    [render] @@(15,0) E@(68,0)
  entidades vivas: 2
```

Três coisas para observar:

1. **A ordem dos sistemas é a lógica do jogo.** `ia` roda antes de `dano` porque o inimigo
   precisa estar alertado para atacar. Trocar duas linhas do loop muda o comportamento
   observável — e é exatamente por isso que a ordem fica explícita no `main.cpp`, e não
   escondida dentro dos sistemas.
2. **Nenhuma linha `[dano]` aparece.** O inimigo é alertado a 80 unidades (`RAIO_DETECCAO`),
   mas `Dano::alcance` dele é 6,0 e ele nunca chega tão perto em 5 frames. É a saída correta;
   se você diminuir a distância inicial, as linhas de dano aparecem.
3. **Os três projéteis somem no frame 3** (`1,5 s / 0,5 s = 3 frames`) e a contagem cai de 5
   para 2. Foi `systems::tempo_de_vida` que os destruiu — e o modo como ele faz isso é a
   armadilha seguinte.

### Demo 4 — composição dinâmica

```
  tem Velocidade? false
  após adicionar Velocidade: true
  pos após movimento: (5, 3)
  após remover Velocidade: false
  pos após remover vel: (5, 3) (não mudou)
```

A mesma entidade ganha e perde comportamento em runtime, sem classe nova, sem `if`, sem
downcast. Depois do `remover<Velocidade>`, `systems::movimento` simplesmente não a vê mais.

### Demo 5 — reciclagem de IDs, e o perigo dela

```
  criados: 0, 1, 2
  destruído: 1
  entidades vivas: 2
  novo ID: 1 (reutilizou 1)
  e2 ainda existe? true
  novo existe?     true
```

A reciclagem evita que o espaço de IDs cresça sem fim em um jogo que cria e mata projéteis a
cada frame. Mas olhe a penúltima linha com atenção:

> `e2` foi **destruída**, e mesmo assim `world.existe(e2)` responde `true`.

Não é bug de implementação: `e2` vale `1`, e o número `1` está vivo de novo — pertencendo
agora a **outra** entidade. Qualquer código que tenha guardado `e2` (um alvo de mísseil, uma
referência de UI) passa a operar em cima de uma entidade que nunca escolheu. É a falha
clássica do assunto, e a solução padrão está [na última seção](#o-que-falta-para-um-ecs-de-produção).

---

## ⚠ As três armadilhas

### 1. `get<T>()` devolve uma referência — não a guarde

`get<T>` devolve `T&`, que aponta **para dentro** do `std::vector` do store. Adicionar mais
um componente do mesmo tipo pode fazer o vector realocar, e a referência antiga vira lixo:

```cpp
ecs::World w;
ecs::Entity a = w.criar();
w.adicionar<ecs::Posicao>(a, 1.0f, 1.0f);

ecs::Posicao& p = w.get<ecs::Posicao>(a);   // referência para dentro do vector

for (int i = 0; i < 100; ++i) {             // força a realocação
    ecs::Entity e = w.criar();
    w.adicionar<ecs::Posicao>(e, 0.0f, 0.0f);
}

p.x = 42.0f;                                // ⚠ heap-use-after-free
```

Compilado com `-fsanitize=address`, esse programa aborta com
`AddressSanitizer: heap-use-after-free` — a linha exata do `p.x = 42.0f`.

**Regra: trate o resultado de `get<T>` como válido só até a próxima modificação do `World`.**
Use dentro do escopo e descarte; se precisar guardar algo, guarde a `Entity` e refaça o
`get`.

### 2. Não destrua entidades dentro de uma `query`

`World::destruir` chama `remover` em todos os stores, que faz *swap com o último* e
`pop_back`. Fazer isso enquanto o laço da `query` está percorrendo `entidades_` embaralha o
array debaixo do iterador: entidades são puladas ou visitadas duas vezes.

O padrão correto — e é o que `systems::tempo_de_vida` faz — é **coletar primeiro, destruir
depois**:

```cpp
void tempo_de_vida(World& world, float dt) {
    std::vector<Entity> expiradas;                       // 1. coleta

    world.query<Tempo_de_vida>([&](Entity e, Tempo_de_vida& tdv) {
        tdv.restante -= dt;
        if (tdv.restante <= 0.0f)
            expiradas.push_back(e);                      //    ← só marca
    });

    for (Entity e : expiradas)                           // 2. destrói FORA da query
        world.destruir(e);
}
```

A generalização disso, em ECS de produção, chama-se **command buffer**: as mudanças
estruturais (criar, destruir, adicionar, remover) são enfileiradas durante o frame e
aplicadas em um ponto de sincronização.

### 3. Modificar o *mesmo* store de dentro da query

`systems::dano` faz uma `query` aninhada dentro de outra. Funciona porque nenhuma das duas
adiciona ou remove componentes — só **lê e escreve campos** de componentes existentes, o que
não mexe no tamanho de nenhum vector. `world.adicionar<T>` dentro de uma `query<T, ...>` é
uma variante da armadilha 1 e da 2 ao mesmo tempo.

---

## ⚠ Quando NÃO usar ECS

O ECS não é gratuito. Três situações em que ele piora o código:

1. **Poucas entidades.** Abaixo de algumas centenas, tudo cabe no cache de qualquer jeito e a
   vantagem de layout desaparece. O que sobra é a indireção: um `unordered_map` de stores, um
   índice por acesso, um `type_index` por chamada. Um `std::vector<Inimigo>` simples ganha.
2. **Lógica muito acoplada a uma entidade só.** A regra "o chefe da fase 3 faz X quando Y"
   não tem paralelismo nem repetição para explorar. Num ECS ela precisa virar um componente e
   um sistema que roda para **uma** entidade — burocracia pura.
3. **Depuração passo a passo.** Perguntar *"o que aconteceu com esta entidade?"* não tem
   resposta em um lugar só: o comportamento dela está espalhado por N sistemas, e não existe
   um breakpoint natural. Motores de produção compensam isso com um inspetor de entidades
   próprio — que você teria de escrever.

A herança clássica continua certa quando o número de combinações é **pequeno e conhecido**.
ECS ganha quando é grande, muda em runtime, e há muitas entidades iguais.

---

## O que falta para um ECS de produção

Este projeto implementa a família de ECS chamada **sparse set** (a mesma do
[EnTT](https://github.com/skypjack/entt)). Falta o seguinte:

**Geração/versão no `Entity`.** É a resposta ao demo 5. Em vez de o `Entity` ser um índice
puro, ele é dividido em campos — por exemplo 20 bits de índice e 12 bits de geração. Cada vez
que o índice é reciclado, a geração incrementa. Um handle antigo tem geração `3` e o slot
agora está na geração `4`, então `existe()` responde `false` corretamente. Custa uma
comparação e resolve o problema inteiro.

**Archetypes / chunks.** A outra família de ECS. Em vez de um array por *tipo de componente*,
existe um bloco de memória por *combinação de componentes* (o "arquétipo"). Todas as entidades
com exatamente `{Posicao, Velocidade, Saude}` moram juntas, em chunks de tamanho fixo. Uma
query vira "percorra os chunks cujo arquétipo contém estes tipos" — **sem nenhum teste por
entidade**, ao contrário do `tem_todos` daqui. Em compensação, adicionar um componente move a
entidade de arquétipo, o que é caro. É o desenho do Unity DOTS e do [flecs](https://www.flecs.dev/).

**Sistemas em paralelo.** A tabela clássica de vantagens do ECS promete paralelismo, e este
projeto é **inteiramente single-thread** — não há uma linha de `std::thread` no diretório. O
que falta não é criar threads: é cada sistema **declarar** quais componentes lê e quais
escreve, para o escalonador provar que dois sistemas não conflitam e rodá-los juntos. Sem essa
declaração, rodar sistemas em paralelo é *data race* — que é UB, não "valor errado" (veja
[`../concurrency2/`](../concurrency2/)).

**Serialização e reflexão.** Salvar o jogo exige percorrer os stores por tipo em runtime.
Com `type_index` como chave você tem a identidade do tipo, mas não o nome nem os campos —
seria preciso um registro de tipos explícito.

---

## Como validar

```bash
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)

# 1. warnings
g++ -std=c++17 -Wall -Wextra -Wpedantic -I$FMTP/include \
    src/ecs/main.cpp -o /tmp/ecs -L$FMTP/lib -lfmtd

# 2. sanitizers (o que pega o uso de referência inválida)
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -I$FMTP/include \
    src/ecs/main.cpp -o /tmp/ecssan -L$FMTP/lib -lfmtd && /tmp/ecssan

# 3. o exemplo mínimo, sem fmt nenhum
g++ -std=c++17 -Wall -Wextra -Wpedantic -Isrc/ecs /tmp/minimo.cpp -o /tmp/minimo
```

Estado atual: compila **sem nenhum warning** com `-Wall -Wextra -Wpedantic` e roda com
**0 achados sob ASan+UBSan**. O subprojeto compila tanto em C++17 quanto no `cpp_std=c++20`
que o `meson.build` da raiz define.

O que se espera ver ao rodar `./dist/bin/ecs`: cinco blocos numerados, a contagem de entidades
caindo de **5 para 2** no frame 3 do demo 3, e a última linha `✓ ECS demo concluído!`.

### A medição de cache, reproduzível

O programa que produziu os `0,72 ms × 20,7 ms` da seção
[Por que estudar isto](#problema-2--o-layout-de-memória-o-motivo-real):

```cpp
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <random>
#include <cstdio>

struct Posicao { float x, y; };

int main() {
    const int N = 1'000'000;
    std::vector<Posicao> denso(N);
    std::unordered_map<unsigned, Posicao> mapa;

    std::vector<unsigned> ids(N);
    for (int i = 0; i < N; ++i) ids[i] = i;
    std::mt19937 rng(42);
    std::shuffle(ids.begin(), ids.end(), rng);   // heap fragmentado, como na vida real

    for (int i = 0; i < N; ++i) {
        mapa[ids[i]] = Posicao{float(i), float(i)};
        denso[i]     = Posicao{float(i), float(i)};
    }

    auto t0 = std::chrono::steady_clock::now();
    double s1 = 0; for (auto& p  : denso) s1 += p.x;
    auto t1 = std::chrono::steady_clock::now();
    double s2 = 0; for (auto& kv : mapa)  s2 += kv.second.x;
    auto t2 = std::chrono::steady_clock::now();

    std::printf("vector    : %.2f ms (soma %.0f)\n",
                std::chrono::duration<double, std::milli>(t1 - t0).count(), s1);
    std::printf("unord_map : %.2f ms (soma %.0f)\n",
                std::chrono::duration<double, std::milli>(t2 - t1).count(), s2);
}
```

```bash
g++ -std=c++17 -O2 /tmp/bench.cpp -o /tmp/bench && /tmp/bench
```

As duas somas têm de dar o **mesmo** valor — é a prova de que o trabalho foi idêntico e só o
layout mudou. Os números absolutos variam por máquina; a razão entre eles é o que interessa.

---

## Referências

Dentro deste repositório:

- [`../performance/`](../performance/) — cache, localidade e AoS vs SoA, medidos
- [`../layout/`](../layout/) — como os bytes de uma struct são realmente arrumados
- [`../type_erasure/`](../type_erasure/) — a técnica do `IComponentStore`, tratada a fundo
- [`../memory/`](../memory/) — alocação, arenas e `std::pmr`
- [`../templates/`](../templates/) — parameter packs, fold expressions e perfect forwarding
- [`../concurrency2/`](../concurrency2/) — o que "rodar sistemas em paralelo" exigiria
- [`../undefined_behavior/`](../undefined_behavior/) — ASan/UBSan e as armadilhas acima

Fora dele:

- [EnTT](https://github.com/skypjack/entt) — a implementação de referência de ECS *sparse
  set* em C++. A série de artigos do autor, *ECS back and forth*, explica cada decisão de
  desenho: <https://skypjack.github.io/2019-02-14-ecs-baf-part-1/>
- [flecs](https://www.flecs.dev/) — ECS baseado em archetypes, com documentação excelente
  sobre a diferença entre as duas famílias
- Mike Acton, *Data-Oriented Design and C++* (CppCon 2014) — a palestra de onde vem o
  argumento de layout de memória deste README: <https://www.youtube.com/watch?v=rX0ItVEVjHc>
- Adam Martin, *Entity Systems are the future of MMOG development* (2007) — a série de
  artigos que popularizou o termo: <https://t-machine.org/index.php/2007/09/03/entity-systems-are-the-future-of-mmog-development-part-1/>
- Unity, *Entities* (DOTS) — a documentação de archetypes e chunks:
  <https://docs.unity3d.com/Packages/com.unity.entities@latest>
