# sfml_bullet — Física 2D interativa: costurando Bullet3 a SFML

## O que é este projeto?

Uma janela 800×600 em que você **clica e nascem corpos** — esfera com o botão esquerdo, caixa
com o direito — que caem, colidem, quicam e giram. A física é do **Bullet3** (motor 3D, aqui
restrito ao plano XY); o desenho é da **SFML**.

O assunto real não é física nem gráficos, é a **costura**: dois mundos com unidades diferentes
(metros × pixels), eixos verticais opostos (Y para cima × Y para baixo) e relógios diferentes
(passo fixo do solver × quadro de duração variável).

```bash
make configure          # só na primeira vez (Conan + Meson)
make build
./dist/bin/sfml_bullet
```

Só este alvo:

```bash
source build/conanbuild.sh
meson compile -C build sfml_bullet
./build/src/sfml_bullet/sfml_bullet
```

O alvo se chama `sfml_bullet` — é o nome em [`meson.build`](meson.build), linha
`executable('sfml_bullet', ...)`; `make install` o copia para `dist/bin/sfml_bullet`.

> **Pré-requisito recomendado:** [`../collision/README.md`](../collision/README.md) monta o
> pipeline do Bullet num programa de console, sem gráficos. Este README repete as definições
> essenciais para ser autossuficiente, mas aquele é o passo anterior natural.

---

## Antes de começar: os conceitos que você precisa

Não se presume nada além de C++ básico (funções, classes, ponteiros, `std::vector`).

### A SFML

**SFML** (Simple and Fast Multimedia Library) dá quatro coisas: uma **janela**, uma **fila de
eventos**, um **desenhador 2D** e áudio. Nada mais — ela não sabe o que é gravidade, massa ou
colisão; para ela um círculo é uma cor e uma posição em pixels.

| Tipo | O que é |
|---|---|
| `sf::RenderWindow` | a janela; recebe `clear()`, `draw()`, `display()` |
| `sf::Event` + `pollEvent` | fila de eventos: cada chamada tira um ou devolve `false` |
| `sf::CircleShape` / `sf::RectangleShape` | formas desenháveis, em **pixels** |
| `sf::Clock` | `restart()` devolve o tempo desde a chamada anterior |

`setOrigin(r, r)` merece nota: por padrão a posição de uma forma é o **canto superior
esquerdo** da sua caixa envolvente. `setOrigin` move esse ponto de referência; pondo-o no
centro, `setPosition` passa a significar "centro do círculo" — que é o que o Bullet reporta.
Sem isso, todo corpo aparece deslocado meio diâmetro.

### Motor de física e corpo rígido

Um **motor de física** responde 60 vezes por segundo: *dadas posições, velocidades, massas e
formas de N objetos, onde eles estarão daqui a `dt` segundos?*

Um **corpo rígido** é o objeto que ele simula: um sólido que **não deforma**. Todo o seu estado
cabe em poucos números — posição, orientação, velocidade linear, velocidade angular, massa.
Como não deforma, mover o corpo é mover um sistema de coordenadas; é essa simplificação que
torna a simulação barata. **Massa 0 significa "infinitamente pesado"**, isto é, **estático**:
colide, mas nenhuma força o move. É assim que se faz chão e parede.

### O pipeline de um passo

```
  world.stepSimulation(dt, maxSubSteps, fixedTimeStep)
        │
        ├─ 1. BROADPHASE  "quais pares PODEM estar se tocando?"
        │     Compara só caixas alinhadas aos eixos (AABB), o que é barato. O
        │     btDbvtBroadphase mantém uma árvore de AABBs: ~O(n log n) em vez
        │     dos O(n²) pares. Saída: PARES CANDIDATOS.
        ├─ 2. NARROWPHASE "esses pares REALMENTE se tocam? onde?"
        │     Algoritmo exato sobre a forma verdadeira (GJK/EPA, para convexos).
        │     Saída: PONTOS DE CONTATO — posição, normal, penetração.
        ├─ 3. SOLVER      "que impulso tira os contatos da interpenetração sem
        │     violar atrito e restituição?" O btSequentialImpulseConstraintSolver
        │     resolve os contatos um a um, em várias passadas, até convergir.
        └─ 4. INTEGRAÇÃO   v += a·dt ;  x += v·dt
```

**AABB** = *Axis-Aligned Bounding Box*, a menor caixa alinhada aos eixos que contém a forma.
Comparar duas AABBs são seis comparações de `float`; comparar duas formas reais é um algoritmo
iterativo. Daí a divisão em duas fases.

### Restituição e atrito — e a regra que quase ninguém sabe

**Restituição** é a elasticidade do choque: 0 = a energia some (massa de modelar), 1 = o corpo
volta à altura de onde caiu. **Atrito** é a resistência ao deslizamento no contato.

⚠ Ambos são propriedades de **um** corpo, mas a colisão envolve **dois**. O Bullet os combina
**multiplicando**: `restituição_efetiva = restA × restB`. Está medido na seção
[Números medidos](#números-medidos): esfera com `0.5` sobre chão com `0.4` quica como se fosse
`0.20`, não `0.45`. Um corpo com restituição 0 **anula o quique de tudo que o encostar**.

### `btMotionState` — a ponte para o renderizador

O Bullet não desenha. `btMotionState` é a interface pela qual ele **avisa** onde o corpo está:
`getWorldTransform(t)` preenche um `btTransform` com posição + orientação. Ler dali, e não do
transform interno do corpo, tem uma vantagem: para corpos dinâmicos o Bullet devolve o estado
**interpolado** para o instante do quadro, e não o do último passo fixo — o movimento fica
suave mesmo com física a 60 Hz e tela a 144 Hz.

### Graus de liberdade — e como fazer 2D num motor 3D

**Grau de liberdade (DOF)** é cada número independente que descreve a configuração do corpo.
Em 3D são **seis**:

```
        y                TRANSLAÇÃO (3)        ROTAÇÃO (3)
        │                1. mover em x         4. girar em torno de x (pitch)
        │  ↻ yaw         2. mover em y         5. girar em torno de y (yaw)
        └────── x        3. mover em z         6. girar em torno de z (roll)
       ╱
      z  ↻ roll — o único giro visível no plano da tela
```

Uma simulação 2D em XY usa exatamente **três**: mover em x, mover em y, girar em torno de z.
Os outros precisam ser **zerados**, senão um contato levemente descentrado empurra o corpo
para fora da tela ou o tomba de lado. O Bullet zera DOFs multiplicando as velocidades por um
fator, eixo a eixo ([`physics_world.cpp:99-100`](physics_world.cpp)):

```cpp
rb->setLinearFactor (btVector3(1.f, 1.f, 0.f));  // x ✔  y ✔  z ✘
rb->setAngularFactor(btVector3(0.f, 0.f, 1.f));  // pitch ✘  yaw ✘  roll ✔
```

Por que não usar um motor 2D? Porque assim você herda de graça solver, atrito, estabilidade de
pilha e *broadphase* de nível industrial — e volta a 3D apagando duas linhas.

### Quaternion — por que a rotação não são três ângulos

Rotações 3D não cabem em três ângulos sem defeito: a composição não comuta e existe o *gimbal
lock*, em que dois eixos se alinham e um grau de liberdade some. O Bullet usa **quaternions**:
quatro números que codificam *"gire θ radianos em torno do eixo unitário `a`"*.

Extrair um ângulo 2D disso tem armadilha:

```cpp
btScalar  angle = t.getRotation().getAngle();   // sempre em [0, 2π) — NUNCA negativo
btVector3 axis  = t.getRotation().getAxis();    // o eixo em torno do qual se gira
```

Girar **−30° em torno de +Z** é a mesma rotação que **+30° em torno de −Z**. `getAngle()`
sozinho perde o sinal, e o corpo giraria sempre para o mesmo lado. Por isso é obrigatório
olhar o eixo ([`simulation.cpp:166-167`](simulation.cpp)):

```cpp
float angle_deg = static_cast<float>(angle) * (180.f / 3.14159f);
if (axis.getZ() < 0.f) angle_deg = -angle_deg;   // recupera o sinal perdido
```

### Game loop, delta-time, passo fixo e substeps

**Game loop** é o `while` que, enquanto a janela existir, lê eventos, avança o estado e
desenha. **Delta-time (`dt`)** é o tempo real desde o quadro anterior. Ele **varia**: o SO
escalona outros processos, a janela é arrastada, uma alocação demora — o `dt` de 16 ms vira 90.

Só que a física **odeia `dt` variável**: a integração numérica só é estável com passos pequenos
e regulares. O Bullet desacopla os dois relógios:

```
   dt real do quadro = 0.033 s        fixedTimeStep = 1/60 = 0.0167 s
   ├────────────────────────────┤
   ├─────────────┤├─────────────┤     → 2 substeps internos; resto guardado
```

`stepSimulation(dt, maxSubSteps, fixedTimeStep)` consome `dt` em fatias de `fixedTimeStep`, no
máximo `maxSubSteps` delas; o que sobra fica para o próximo quadro.

⚠ **A espiral da morte.** `dt` cresce → mais substeps → o quadro custa mais → `dt` cresce mais
→ o programa congela. Duas defesas, ambas presentes: `maxSubSteps = 10` é o teto duro
([`physics_world.cpp:47`](physics_world.cpp)) e o `dt` é limitado antes de entrar
([`simulation.cpp:276`](simulation.cpp)):

```cpp
float clamped = std::min(dt, 0.05f);   // no máximo 50 ms de física por quadro
```

O preço é honesto: num quadro de 200 ms a simulação avança 50 ms — o mundo fica em câmera lenta
em vez de explodir.

### Escala métrica — por que 1 metro e não 1 pixel

As tolerâncias internas do Bullet são constantes absolutas calibradas para **metros**: margem
de contato de poucos centímetros, limiar de "dormir" em fração de m/s, passo de 1/60 s. Se você
simular a janela em pixels, uma esfera de "raio 24" é um objeto de 24 metros: a margem de
contato vira invisível, os corpos afundam uns nos outros e a pilha treme. Mantenha tudo entre
~0,05 m e ~10 m e converta só na hora de desenhar ([`simulation.hpp:30-32`](simulation.hpp)):

```cpp
constexpr float SCALE    = 80.f;   // 1 metro Bullet = 80 pixels SFML
constexpr float OFFSET_X = 400.f;  // x=0 do Bullet no meio da janela
constexpr float OFFSET_Y = 550.f;  // y=0 do Bullet na linha do chão desenhado
```

A janela de 800×600 px passa a valer 10 m × 7,5 m — escala humana.

---

## Por que estudar isto?

| Sem esta costura | Com esta costura |
|---|---|
| você escreve `y += v*dt` à mão e descobre que empilhar 3 caixas é problema de pesquisa | o solver resolve pilha, atrito e quique com qualidade industrial |
| a lógica vive em pixels, e a física muda de comportamento ao trocar a resolução | a física vive em metros; a resolução é só o `SCALE` do desenho |
| um quadro lento faz os objetos atravessarem o chão | `fixedTimeStep` + `maxSubSteps` + *clamp* seguram a simulação |
| física e render no mesmo objeto: trocar a SFML exige reescrever a física | `PhysicsWorld` não inclui um único header de SFML |

A lição de arquitetura vale mais que a de física: **quem calcula não desenha, e quem desenha
não calcula.**

---

## Estrutura de arquivos

```
src/sfml_bullet/
├── main.cpp             ← 3 linhas: cria Simulation e chama run()
├── physics_world.hpp    ← BodyDef, BodyHandle, PhysicsWorld — SÓ Bullet, zero SFML
├── physics_world.cpp    ← pipeline, gravidade, add_body em 5 etapas, step()
├── simulation.hpp       ← SCALE/OFFSET, to_sfml/to_bullet, Entity, Simulation
├── simulation.cpp       ← janela, eventos, sincronização, render, HUD
├── meson.build          ← executable('sfml_bullet', ...)
├── src_meson.build      ← ÓRFÃO: cópia antiga de src/meson.build, ninguém lê
└── README.md
```

⚠ `src_meson.build` não é lido por ninguém. Quem registra este subprojeto é
[`../meson.build`](../meson.build), na linha `subdir('sfml_bullet')`.

---

## Arquitetura: quem sabe o quê

```
 ┌──────────────────────────────────────────────────────────────┐
 │ main.cpp    sim::Simulation simulation;  simulation.run();   │
 └──────────────────────────────┬───────────────────────────────┘
 ┌──────────────────────────────▼───────────────────────────────┐
 │ Simulation (simulation.hpp/cpp) — A PONTE                    │
 │   • janela SFML, eventos, HUD, formas visuais                │
 │   • converte metros ↔ pixels e inverte o Y                   │
 │   • decide QUANDO chamar a física (pausa, clamp de dt)       │
 │   inclui <SFML/Graphics.hpp>  E  "physics_world.hpp"         │
 └──────────────────────────────┬───────────────────────────────┘
                 add_body(BodyDef) / step(dt) / bodies()
 ┌──────────────────────────────▼───────────────────────────────┐
 │ PhysicsWorld (physics_world.hpp/cpp) — SÓ FÍSICA             │
 │   inclui <btBulletDynamicsCommon.h>, <vector>, <memory>      │
 │   NÃO inclui NADA de SFML — nem sabe o que é um pixel        │
 └──────────────────────────────────────────────────────────────┘
```

A seta é de mão única. Trocar a SFML por Dear ImGui, por ncurses ou por nada (um teste
automatizado) mexe em **um** arquivo. Os [Números medidos](#números-medidos) abaixo foram
produzidos exatamente assim: um `main` de console que inclui `physics_world.hpp` e não toca em
gráfico algum.

---

## Exemplo mínimo

O menor programa que faz Bullet e SFML conversarem. Compila sozinho com SFML 2.6 e Bullet 3.25:

```cpp
#include <SFML/Graphics.hpp>
#include <btBulletDynamicsCommon.h>
#include <algorithm>   // std::min

int main() {
    // 1. o mundo físico: as quatro peças + o mundo que as une
    btDefaultCollisionConfiguration     config;
    btCollisionDispatcher               dispatcher(&config);
    btDbvtBroadphase                    broadphase;
    btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &config);
    world.setGravity(btVector3(0.f, -9.81f, 0.f));           // m/s²

    // 2. chão estático (massa 0): plano infinito em y = 0
    btStaticPlaneShape   forma_chao(btVector3(0.f, 1.f, 0.f), 0.f);
    btDefaultMotionState ms_chao(btTransform::getIdentity());
    btRigidBody chao(btRigidBody::btRigidBodyConstructionInfo(
        0.f, &ms_chao, &forma_chao, btVector3(0, 0, 0)));
    chao.setRestitution(1.f);
    world.addRigidBody(&chao);

    // 3. esfera dinâmica de raio 0,3 m largada de y = 3 m
    btSphereShape forma_bola(0.3f);
    btVector3 inercia(0, 0, 0);
    forma_bola.calculateLocalInertia(1.f, inercia);
    btTransform t0;  t0.setIdentity();  t0.setOrigin(btVector3(0.f, 3.f, 0.f));
    btDefaultMotionState ms_bola(t0);
    btRigidBody::btRigidBodyConstructionInfo ci(1.f, &ms_bola, &forma_bola, inercia);
    ci.m_restitution = 0.8f;
    btRigidBody bola(ci);
    bola.setLinearFactor (btVector3(1.f, 1.f, 0.f));         // 2D
    bola.setAngularFactor(btVector3(0.f, 0.f, 1.f));         // 2D
    world.addRigidBody(&bola);

    // 4. janela SFML e o proxy visual da esfera
    sf::RenderWindow win(sf::VideoMode(400, 300), "Bullet + SFML mínimo");
    win.setFramerateLimit(60);
    constexpr float SCALE = 80.f, OFF_X = 200.f, OFF_Y = 280.f;   // px/m e origem
    sf::CircleShape visual(0.3f * SCALE);
    visual.setOrigin(0.3f * SCALE, 0.3f * SCALE);
    visual.setFillColor(sf::Color(100, 200, 255));

    sf::Clock relogio;
    while (win.isOpen()) {
        sf::Event ev;
        while (win.pollEvent(ev))
            if (ev.type == sf::Event::Closed) win.close();

        float dt = relogio.restart().asSeconds();              // dt REAL do quadro
        world.stepSimulation(std::min(dt, 0.05f), 10, 1.f / 60.f);

        btTransform t;
        bola.getMotionState()->getWorldTransform(t);           // lê o Bullet
        visual.setPosition( t.getOrigin().getX() * SCALE + OFF_X,
                           -t.getOrigin().getY() * SCALE + OFF_Y);  // Y invertido

        win.clear(sf::Color(25, 25, 35));
        win.draw(visual);
        win.display();
    }
    world.removeRigidBody(&bola);    // tirar do mundo antes de destruir
    world.removeRigidBody(&chao);
}
```

Um quinto disso é o mundo, um quinto são os corpos, e o resto gira em torno da **linha do Y
invertido**. É essa linha que o projeto inteiro elabora.

---

## Módulo 1 — `PhysicsWorld`: o Bullet embrulhado

### O problema: seis argumentos posicionais

```cpp
add_sphere(0.f, 5.f, 0.3f, 1.f, 0.5f, 0.7f);   // qual é qual, mesmo?
```

Ninguém lembra a ordem, e trocar dois `float` compila em silêncio. A solução aqui é uma
**struct de parâmetros** ([`physics_world.hpp:29-36`](physics_world.hpp)):

```cpp
enum class ShapeType { Sphere, Box };

struct BodyDef {
    ShapeType type;
    float     x, y;           // posição inicial (pixels convertidos)
    float     half_extent;    // raio ou meia-largura da caixa
    float     mass;           // 0 = estático
    float     restitution;    // elasticidade [0, 1]
    float     friction;
};
```

Com *designated initializers* do C++20 a chamada se autoexplica e a ordem deixa de importar
para quem lê:

```cpp
physics_.add_body({ .type = ShapeType::Sphere, .x = 0.f, .y = 5.f,
                    .half_extent = 0.3f, .mass = 1.f,
                    .restitution = 0.5f, .friction = 0.4f });
```

O retorno é um `BodyHandle` — leve, copiável, **sem posse**
([`physics_world.hpp:39-44`](physics_world.hpp)): `btRigidBody*`, `btCollisionShape*`, o
`ShapeType` e o `half_extent`.

### As cinco etapas de `add_body`

[`physics_world.cpp:50-119`](physics_world.cpp), nesta ordem:

1. **forma** — `btSphereShape(raio)` ou `btBoxShape(half-extents)`; ambas **convexas**, o que
   permite ao narrowphase usar GJK/EPA.
2. **motion state** — `btDefaultMotionState(transform)` com a posição inicial.
3. **inércia local** — `shape->calculateLocalInertia(mass, inertia)`. A massa diz quanto custa
   acelerar em linha reta; o **tensor de inércia** diz quanto custa acelerar em rotação, e
   depende da forma. Com massa 0 fica `(0,0,0)`, que o Bullet lê como "imóvel".
4. **corpo rígido** — `btRigidBodyConstructionInfo` recebe massa, motion state, forma e
   inércia; `m_restitution` e `m_friction` são preenchidos antes de construir. Em seguida vêm
   as duas linhas do 2D.
5. **registro** — `world_->addRigidBody(...)` e então a posse migra para os vetores internos
   por `std::move`.

### Gestão de memória: a evolução em relação a `collision`

O Bullet é uma API C++98 e **não** libera nada. Em
[`../collision/main.cpp`](../collision/main.cpp) isso vira uma sequência de `delete` manuais no
fim do `main`, na ordem exata inversa da criação — e um `return` antecipado vaza tudo.

Aqui a posse é declarativa ([`physics_world.hpp:64-78`](physics_world.hpp)):

```cpp
// Ordem de criação importa — destruição é na ordem inversa
std::unique_ptr<btDefaultCollisionConfiguration>     config_;
std::unique_ptr<btCollisionDispatcher>               dispatcher_;
std::unique_ptr<btDbvtBroadphase>                    broadphase_;
std::unique_ptr<btSequentialImpulseConstraintSolver> solver_;
std::unique_ptr<btDiscreteDynamicsWorld>             world_;

std::vector<std::unique_ptr<btRigidBody>>          rigid_bodies_;
std::vector<std::unique_ptr<btDefaultMotionState>> motion_states_;
std::vector<std::unique_ptr<btCollisionShape>>     shapes_;
```

Membros são destruídos na **ordem inversa da declaração** — regra da linguagem, não convenção.
`world_` é o último declarado, logo morre primeiro, e o dispatcher que ele usa ainda está vivo
nesse instante. O `delete` na ordem certa virou uma **ordem de declaração** conferida pelo
compilador. Três consequências:

- **A cópia some.** `unique_ptr` não é copiável, logo `PhysicsWorld` também não é — e está
  certo: dois donos do mesmo mundo seriam um *double free*.
- **O move precisa ser pedido.** Declarar um destrutor **suprime** a geração automática do
  move. Por isso ([`physics_world.hpp:51-53`](physics_world.hpp)):

  ```cpp
  PhysicsWorld(PhysicsWorld&&)            = default;
  PhysicsWorld& operator=(PhysicsWorld&&) = default;
  ```

  Não é enfeite: a tecla **R** faz `physics_ = PhysicsWorld{}`
  ([`simulation.cpp:253`](simulation.cpp)). Sem essas linhas, não compila.
- **O destrutor tem de sobreviver ao objeto movido.** Após um *move* os ponteiros da origem
  ficam nulos e o destrutor dela ainda roda — daí
  ([`physics_world.cpp:33`](physics_world.cpp)): `if (!world_) return;`. Sem essa guarda, cada
  `R` desreferenciaria um ponteiro nulo.

Aprofundamento: [`../smart-pointers/README.md`](../smart-pointers/README.md) e
[`../value_semantics/README.md`](../value_semantics/README.md).

---

## Módulo 2 — `Simulation`: a ponte

### Conversão de coordenadas

```
Bullet: Y para CIMA ↑, metros        SFML: Y para BAIXO ↓, pixels, origem no
                                           canto superior esquerdo
  y                                    ┌───────────────────► x
  ▲   ●                                │        ●
  └──────► x                           ▼ y
```

```cpp
inline sf::Vector2f to_sfml(float bx, float by) {          // simulation.hpp:35
    return { bx * SCALE + OFFSET_X, -by * SCALE + OFFSET_Y };
}
inline sf::Vector2f to_bullet(float sx, float sy) {        // simulation.hpp:40
    return { (sx - OFFSET_X) / SCALE, -(sy - OFFSET_Y) / SCALE };
}
```

O sinal negativo em `by` é a inversão do eixo. `OFFSET_Y = 550` põe o zero do Bullet na linha
do chão desenhado, **não** no meio da janela: `y = 0` m aparece em `y = 550` px.

### O quadro, passo a passo

```
run()  [simulation.cpp:316]
  ├─ dt = clock_.restart().asSeconds()      ← tempo real do quadro anterior
  ├─ handle_events()  [227]  fechar · clique esq → esfera · clique dir → caixa
  │                          · P pausa · R reset · Esc sai
  ├─ update(dt)       [272]  se pausado → RETORNA sem tocar na física
  │                          clamped = min(dt, 0.05); physics_.step(clamped)
  │                          sync_entities()          → Bullet ⇒ formas SFML
  └─ render()         [284]  clear · chão · paredes · entidades · HUD · display
```

A pausa está em `update`, **não** em `render`: com a simulação parada a janela continua sendo
redesenhada 60 vezes por segundo, então ainda responde a arrastar, a `Esc` e ao próprio `P`.

### `sync_entities` — o coração da costura

```cpp
struct Entity {                     // simulation.hpp:45
    BodyHandle handle;
    sf::Shape* visual;   // ponteiro não-owning — owned pelos vetores abaixo
};
```

A posse das formas está em `std::vector<std::unique_ptr<sf::CircleShape>> circles_` e no
equivalente para retângulos. `Entity::visual` é `sf::Shape*` **de propósito**: círculo e
retângulo compartilham a base `sf::Shape`, o que permite um único laço para os dois.

⚠ Guardar `unique_ptr` num `vector` e ponteiros crus para o **conteúdo** é seguro — o vetor
realoca o array de ponteiros, mas os objetos apontados não se movem. Guardar
`std::vector<sf::CircleShape>` e ponteiros para os elementos seria um erro: a primeira
realocação invalidaria todos.

```cpp
for (auto& e : entities_) {                        // simulation.cpp:150
    btTransform t;
    e.handle.body->getMotionState()->getWorldTransform(t);

    auto sp = to_sfml(t.getOrigin().getX(), t.getOrigin().getY());
    e.visual->setPosition(sp);

    btScalar  angle = t.getRotation().getAngle();
    btVector3 axis  = t.getRotation().getAxis();

    float angle_deg = static_cast<float>(angle) * (180.f / 3.14159f);
    if (axis.getZ() < 0.f) angle_deg = -angle_deg;
    e.visual->setRotation(-angle_deg);
}
```

Há **duas** inversões de sinal, e confundi-las é a pegadinha clássica:

| Linha | Por quê |
|---|---|
| `if (axis.getZ() < 0.f) angle_deg = -angle_deg;` | `getAngle()` nunca é negativo; o sentido está no **eixo** |
| `setRotation(-angle_deg)` | com o Y da tela invertido, o positivo do Bullet (anti-horário) vira horário na SFML |

### Fronteiras, spawn e HUD

`create_boundaries()` ([`simulation.cpp:48`](simulation.cpp)) é método de **`Simulation`**, não
de `PhysicsWorld`: as fronteiras são detalhe *desta* cena. Cria três corpos de massa 0 que
**não** entram em `entities_` — `render()` desenha retângulos fixos no lugar deles. Confira os
números medidos antes de confiar nessa geometria.

`spawn_sphere` / `spawn_box` ([`simulation.cpp:85`/`117`](simulation.cpp)) convertem o pixel do
clique com `to_bullet`, criam o corpo, criam a forma com `setOrigin` no centro e guardam o par
em `entities_`; as cores saem de duas paletas de cinco, indexadas por `contador % 5`.

`draw_hud()` desenha dois painéis translúcidos e depende de uma fonte carregada no construtor
([`simulation.cpp:35-40`](simulation.cpp)), com três caminhos alternativos.

⚠ A falha é **silenciosa**: sem nenhuma dessas fontes, `draw_hud()` retorna na primeira linha e
a simulação roda sem texto algum — nem painéis, nem contadores, nem o aviso `PAUSADO`. Se você
não vê texto, não é bug de render: instale `fonts-dejavu-core`.

---

## Números medidos

Medidos com um `main` de console que inclui `physics_world.hpp` (sem SFML), replica
`create_boundaries()` e roda em passos de 1/60 s.

### 1. `btBoxShape` recebe half-extents nos **três** eixos

[`physics_world.cpp:63-65`](physics_world.cpp) usa o mesmo `half_extent` para x, y e z — toda
caixa deste projeto é um **cubo**. Para as caixas do usuário (`0.28` m) é o desejado; para as
fronteiras, não:

| Corpo | `BodyDef` | Volume real | Em pixels |
|---|---|---|---|
| chão | centro (0; −0,5), `half_extent` 5,1 | cubo de 10,2 m de aresta | face de cima em **y = 182 px** |
| parede esq. | centro (−5,1; 3,75), `half_extent` 0,5 | cubo de 1 m | x −48…32 px, y 210…290 px |
| parede dir. | centro (5,1; 3,75), `half_extent` 0,5 | cubo de 1 m | x 768…848 px, y 210…290 px |

O comentário em [`simulation.cpp:58`](simulation.cpp) diz *"10.2m de largura (cobre a tela)"* —
a largura está certa, mas a **altura** também virou 10,2 m.

### 2. Onde os corpos realmente param

Três esferas criadas em pontos diferentes, 20 s de simulação:

```
clique em (60, 100)   → repouso em (60,0 ; 158,0) px    bullet y = 4,90 m
clique em (760, 100)  → repouso em (760,0 ; 158,0) px   bullet y = 4,90 m
clique em (400, 60)   → repouso em (400,0 ; 158,0) px   bullet y = 4,90 m
```

Todas param com o **centro em y = 158 px**, isto é, a borda inferior em 182 px — exatamente
sobre a face de cima do cubo. O chão **desenhado** está em y = 550 px, 368 px abaixo.

⚠ **É isto que você vê ao rodar**: os corpos assentam sobre uma superfície invisível no terço
superior da janela, não sobre o retângulo cinza. Erro de geometria da cena, não do motor — o
Bullet faz exatamente o que foi mandado.

**Exercício (correção mínima).** Dê meias-extensões independentes à caixa:

```cpp
// physics_world.hpp — acrescentar ao BodyDef
float half_y = 0.f;   // 0 → usa half_extent (cubo), como hoje

// physics_world.cpp — na criação da forma
const float hy = (def.half_y > 0.f) ? def.half_y : def.half_extent;
shape = std::make_unique<btBoxShape>(btVector3(def.half_extent, hy, def.half_extent));
```

Em `create_boundaries()`: chão com `half_y = 0.25` centrado em `y = -0.25`; paredes com
`half_y = 4.0` centradas em `y = 3.5`. O repouso passa para `y ≈ 526 px` (centro da esfera,
raio 24 px, sobre o chão em 550 px).

### 3. Restituição é multiplicada, não somada

Esfera (`restitution` 0,5) largada de `y = 7` m sobre o chão (`restitution` 0,4):

```
queda:                    7,000 m → repouso em 4,900 m   (2,100 m)
pico após o 1º quique:    5,0145 m                       (0,1145 m acima do repouso)
razão de alturas:         0,1145 / 2,100 = 5,45 %
restituição efetiva:      √0,0545 = 0,233
```

`0,5 × 0,4 = 0,20`; o resto é viés do solver. Se o Bullet fizesse a média (0,45), o quique
teria ~0,43 m — quase quatro vezes mais alto.

### 4. Nota de honestidade sobre os comentários do código

O cabeçalho de [`main.cpp:15`](main.cpp) diz *"Gerenciamento manual de ownership (Bullet não
usa smart pointers)"*. A primeira metade descreve o subprojeto `collision`; **este** já usa
`unique_ptr` em tudo. Continua verdade a segunda metade: a **biblioteca** não usa smart
pointers — nós é que os pusemos por cima.

---

## Controles

| Entrada | Efeito | Código |
|---|---|---|
| Clique **esquerdo** | esfera (raio 0,3 m = 24 px, massa 1,0) no ponto clicado | [`simulation.cpp:239`](simulation.cpp) |
| Clique **direito** | caixa (meia-aresta 0,28 m, massa 1,5) no ponto clicado | [`simulation.cpp:241`](simulation.cpp) |
| **P** | pausa / retoma — congela só `update`, a janela segue viva | [`simulation.cpp:246`](simulation.cpp) |
| **R** | reset: `physics_ = PhysicsWorld{}`, limpa formas/contadores, recria fronteiras | [`simulation.cpp:250`](simulation.cpp) |
| **Esc** / botão de fechar | fecha a janela | [`simulation.cpp:262`/`230`](simulation.cpp) |

---

## Saída esperada

No terminal, ao subir:

```
Simulação iniciada.
  Clique esquerdo  → spawna esfera
  Clique direito   → spawna caixa
  P                → pausar/retomar
  R                → resetar
  Escape           → sair
```

A linha `Setting vertical sync not supported`, quando aparece, vem da SFML e é inofensiva.

Na janela (800×600, fundo `RGB 25,25,35`): um retângulo cinza de 20 px em `y = 550` (o chão
**desenhado**) e duas faixas verticais nas bordas; painel translúcido à esquerda com o título e
os quatro atalhos; painel à direita com `Esferas`, `Caixas` e `Total`; ao clicar, corpos
coloridos nascem, caem, giram e **param a cerca de 158 px do topo** (veja
[Números medidos](#números-medidos)); com `P`, a palavra `PAUSADO` em âmbar no centro.

Cada clique imprime no terminal, por exemplo:

```
sphere #1 em (0.0, 3.1) bullet
box #1 em (1.5, 2.4) bullet
simulação pausada
simulação resetada
```

Ao sair: `Simulação encerrada. Total de objetos criados: N`.

---

## Como validar

```bash
source build/conanbuild.sh
meson compile -C build sfml_bullet      # sem warnings sob -Wall -Wextra -Wpedantic
./dist/bin/sfml_bullet
```

1. **A costura funciona** — clique em qualquer lugar: o corpo nasce **sob o cursor**. Se nascer
   deslocado, o culpado é `to_bullet` ou o `setOrigin`.
2. **A rotação tem sinal** — solte várias caixas umas sobre as outras e observe uma tombar. Ela
   precisa girar nos **dois** sentidos ao longo do tempo; se girar sempre para o mesmo lado, o
   teste de `axis.getZ()` se perdeu.
3. **A pausa não congela a janela** — com `P` acionado, arraste a janela e aperte `Esc`.
4. **O reset não estoura** — segure `R` por alguns segundos: cada toque destrói um mundo
   inteiro por *move-assignment* (é a guarda `if (!world_) return;` trabalhando).
5. **Sem fonte, sem HUD** — renomeie temporariamente a `DejaVuSansMono.ttf`: a simulação segue
   rodando, sem nenhum texto.

---

## Referências

Subprojetos irmãos:

- [`../collision/README.md`](../collision/README.md) — o pipeline do Bullet em console, sem
  gráficos. O passo **anterior**.
- [`../sfml_bullet2/README.md`](../sfml_bullet2/README.md) — o passo **seguinte**:
  *constraints*, ragdoll e cordas.
- [`../qt_sfml/README.md`](../qt_sfml/README.md) — a mesma SFML embutida num widget Qt.
- [`../qt_sfml2/README.md`](../qt_sfml2/README.md) — Qt + SFML + Bullet juntos.
- [`../smart-pointers/README.md`](../smart-pointers/README.md) e
  [`../value_semantics/README.md`](../value_semantics/README.md) — a base de `unique_ptr`,
  *move* e regra dos 0/3/5 usados aqui.

Fontes externas canônicas:

- Bullet Physics — *User Manual*:
  <https://github.com/bulletphysics/bullet3/blob/master/docs/Bullet_User_Manual.pdf>
- Bullet Physics — código e exemplos: <https://github.com/bulletphysics/bullet3>
- SFML 2.6 — tutoriais e referência: <https://www.sfml-dev.org/tutorials/2.6/>
- Glenn Fiedler, *Fix Your Timestep!* — a referência sobre passo fixo e espiral da morte:
  <https://gafferongames.com/post/fix_your_timestep/>

Versões neste repositório: **SFML 2.6.1** e **Bullet3 3.25** (`conanfile.py`).

---

> **Nota de fidelidade.** Todos os trechos de código deste README são recortes literais dos
> arquivos citados, com as linhas indicadas. As únicas exceções, marcadas como tais, são o
> *Exemplo mínimo* e o *Exercício*. Se o código mudar e este README não, ele passa a estar
> errado — corrija-o junto.
