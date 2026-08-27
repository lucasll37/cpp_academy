# collision — Física de Corpos Rígidos com Bullet3

## O que é este projeto?

Um motor de física resolvendo, passo a passo, o problema mais simples da mecânica: **uma
esfera cai e quica no chão**. Nada de renderização, nada de janela — só números impressos no
terminal, para que cada um deles possa ser conferido contra a fórmula do papel.

O cenário: esfera de raio 1 m e massa 1 kg, solta do repouso com o centro em y = 10 m,
gravidade −9,81 m/s², chão em y = 0, 180 passos de 1/60 s (3 segundos).

```bash
make build && ./dist/bin/collision
```

> ⚠ O alvo se chama **`collision`**, não `main`. Até esta revisão, `src/collision/meson.build`
> declarava `executable('main', ...)` — e `src/sandbox/meson.build` também. Os dois instalavam
> em `dist/bin/main`, e o binário desta física simplesmente nunca chegava lá. Se você tinha um
> `dist/bin/main` antigo, ele era do sandbox.

## Por que estudar isto?

Porque *simular* é o problema inverso de *resolver*. No papel você resolve o movimento uma vez,
em símbolos, e obtém `y(t)`. Um jogo, um simulador de robô, um digital twin ou uma animação
não têm essa opção: precisam responder "onde está tudo daqui a 16 milissegundos?" milhares de
vezes por segundo, para corpos cujas equações ninguém resolveu.

A resposta prática é sempre o mesmo pipeline, e ele é o mesmo no Bullet, no PhysX, no Havok, no
Box2D e no MuJoCo. **Aprender Bullet é aprender essa arquitetura**, não uma API. Trocar de
motor depois é trocar nomes de classe.

E porque é um dos raríssimos domínios em que você pode **provar** que o programa está certo:
a queda livre tem solução fechada. Este README usa isso o tempo todo — todo número citado aqui
saiu de uma execução real e é comparado com a fórmula.

## Antes de começar

Nenhum destes termos é pressuposto. Se você já sabe algum, pule.

### Corpo rígido

Um objeto que **não deforma**: a distância entre quaisquer dois pontos dele é constante. Isso
reduz o estado do objeto inteiro a 13 números — posição (3), orientação (4, como quaternion),
velocidade linear (3) e velocidade angular (3). É a simplificação que torna a simulação viável.
Pano, gelatina e água precisam de outros modelos (*soft body*, fluidos), e o Bullet os tem, mas
não é o que este demo faz.

### Estático vs. dinâmico

No Bullet, **massa = 0 significa "estático"**: o corpo participa das colisões, mas nenhuma
força o move. É como se tivesse massa infinita. O chão deste demo tem massa 0. A esfera tem
massa 1 kg e é dinâmica.

### Shape de colisão

A **forma geométrica** usada para detectar contato. Não é o desenho que aparece na tela — é o
volume matemático com que o motor calcula. Aqui: `btSphereShape` (uma esfera, definida só pelo
raio) e `btStaticPlaneShape` (um plano infinito, definido por uma normal e um deslocamento).

### AABB — *Axis-Aligned Bounding Box*

A menor caixa **alinhada aos eixos X, Y, Z** que contém o corpo inteiro. Guardar uma AABB são
6 floats (mín e máx em cada eixo). Testar se duas AABBs se sobrepõem são **6 comparações de
float** — e nada mais. É o teste mais barato que existe, e ele é *conservador*: se as AABBs não
se tocam, os corpos com certeza não se tocam. O contrário não vale (duas AABBs podem se
sobrepor com os corpos longe um do outro), e é justamente por isso que existe uma segunda fase.

### Impulso

Força multiplicada por tempo: **J = F·Δt = Δ(m·v)** — a variação da quantidade de movimento.

Numa colisão, a força é enorme e o tempo é minúsculo, e o produto é o que interessa. Bater com
20 000 N durante 0,5 ms produz o mesmo efeito que 10 N durante 1 s. Um motor de física
**nunca calcula a força de uma colisão**: ele aplica direto a mudança de velocidade. Daí o
nome do solver do Bullet, `btSequentialImpulseConstraintSolver`.

### Coeficiente de restituição (*e*)

Quanta velocidade sobrevive ao impacto: **v_saída = −e · v_entrada**.
`e = 0` → o corpo gruda; `e = 1` → quique perfeito, energia conservada para sempre.

> ⚠ O Bullet **multiplica** os coeficientes dos dois corpos em contato
> (`btManifoldResult::calculateCombinedRestitution` devolve
> `body0->getRestitution() * body1->getRestitution()`). Este demo põe 0.6 na esfera **e** 0.6 no
> chão, então o quique efetivo é **0.36**, não 0.6. A seção "Como validar" mostra os números
> batendo com 0.36 até a quarta casa.

### Tensor de inércia

Assim como a **massa** mede a resistência à aceleração *linear*, o **tensor de inércia** mede a
resistência à aceleração *angular*. Para uma esfera maciça de massa *m* e raio *r*, ele vale
(2/5)·m·r² em cada eixo — aqui, (2/5)·1·1² = 0,4.

> ⚠ Esquecer `calculateLocalInertia()` deixa a inércia em (0,0,0), e o Bullet interpreta zero
> como **"não gira"** (para corpos dinâmicos, o inverso da inércia é o que entra na conta; zero
> vira zero, e o corpo simplesmente não responde a torque). Sintoma clássico: a caixa desliza,
> mas nunca tomba.

### Integrador

O passo `estado(t) → estado(t + dt)`. Ver "Por que dt fixo", mais abaixo.

## Estrutura de arquivos

```
src/collision/
├── main.cpp      ← cenário completo: mundo, chão, esfera, loop, cleanup
├── meson.build   ← alvo `collision`
└── README.md
```

Um arquivo só, de propósito: aqui a lição é **a ordem em que as peças são montadas**, e
espalhá-las em headers esconderia exatamente isso.

## O problema: por que não basta um laço de colisão

A resposta ingênua para "quem está colidindo com quem" é testar todos os pares. Com *n* corpos
são **n(n−1)/2** testes por frame:

| corpos | pares por frame | a 60 FPS |
|---|---|---|
| 10 | 45 | 2 700/s |
| 100 | 4 950 | 297 000/s |
| 1 000 | **499 500** | **~30 milhões/s** |

E cada teste "de verdade" (esfera contra malha convexa, por exemplo) custa muito mais que uma
comparação de float. É inviável.

A saída é **filtrar barato antes de decidir caro** — a mesma ideia de um índice de banco de
dados. Daí as duas fases:

```
stepSimulation(dt, maxSubSteps, fixedTimeStep)
        │
        ├── broadphase ──── quais pares de AABB se sobrepõem?
        │                   (barato, conservador, descarta 99% dos pares)
        │
        ├── narrowphase ─── esses pares realmente se tocam? onde?
        │                   (caro; só roda no que o broadphase deixou passar)
        │
        ├── solver ──────── aplica impulsos nos pontos de contato
        │                   (Projected Gauss-Seidel, iterativo)
        │
        └── integração ──── v += a·dt ; x += v·dt  para cada corpo
```

O `btDbvtBroadphase` usado aqui mantém as AABBs numa **árvore binária dinâmica** (*Dynamic
Bounding Volume Tree*), que é atualizada em vez de reconstruída, e responde consultas em
O(log n) em vez de O(n). Custo por frame: O(n log n) em vez de O(n²). A alternativa,
`btAxisSweep3`, é melhor quando a cena tem limites fixos e conhecidos.

## Exemplo mínimo — gravidade em 25 linhas

Antes de olhar o `main.cpp` completo, este é o programa mais curto que faz um corpo cair. Sem
chão, sem quique, só a gravidade:

```cpp
#include <btBulletDynamicsCommon.h>
#include <fmt/core.h>
#include <memory>

int main() {
    auto config     = std::make_unique<btDefaultCollisionConfiguration>();
    auto dispatcher = std::make_unique<btCollisionDispatcher>(config.get());
    auto broadphase = std::make_unique<btDbvtBroadphase>();
    auto solver     = std::make_unique<btSequentialImpulseConstraintSolver>();
    auto world      = std::make_unique<btDiscreteDynamicsWorld>(
        dispatcher.get(), broadphase.get(), solver.get(), config.get());
    world->setGravity(btVector3(0, -9.81f, 0));

    auto shape  = std::make_unique<btSphereShape>(1.0f);
    btTransform t;  t.setIdentity();  t.setOrigin(btVector3(0, 10, 0));
    auto motion = std::make_unique<btDefaultMotionState>(t);

    btVector3 inertia(0, 0, 0);
    shape->calculateLocalInertia(1.0f, inertia);
    auto body = std::make_unique<btRigidBody>(
        btRigidBody::btRigidBodyConstructionInfo(1.0f, motion.get(), shape.get(), inertia));

    world->addRigidBody(body.get());
    for (int i = 0; i < 10; ++i) {
        world->stepSimulation(1.f / 60.f, 1, 1.f / 60.f);
        btTransform p;  body->getMotionState()->getWorldTransform(p);
        fmt::println("step {:2}  y = {:.4f}", i, p.getOrigin().getY());
    }
    world->removeRigidBody(body.get());   // ⚠ antes que `body` seja destruído
}
```

Cinco objetos para montar o mundo, três para montar um corpo, uma chamada por frame. Todo o
resto do `main.cpp` é variação disso.

> ⚠ A ordem dos `unique_ptr` aqui é deliberada: eles são destruídos na **ordem inversa** da
> declaração, então `body`, `motion` e `shape` morrem **antes** de `world`. Por isso a linha
> `removeRigidBody` no fim é obrigatória — sem ela, `~btDiscreteDynamicsWorld` desreferencia um
> ponteiro pendurado. Ver "Cleanup", abaixo.

## Os demos, um a um

O `main.cpp` é um programa único dividido em cinco blocos numerados.

### 1 · O mundo

```cpp
// Registra qual algoritmo usar para cada PAR de shapes
// (esfera-esfera, caixa-esfera, convexo-malha, …)
auto* config     = new btDefaultCollisionConfiguration();

// Percorre os pares que o broadphase aprovou e chama o algoritmo certo.
// É aqui que o narrowphase acontece.
auto* dispatcher = new btCollisionDispatcher(config);

// Árvore dinâmica de AABBs — O(n log n) por frame
auto* broadphase = new btDbvtBroadphase();

// Resolve contatos e juntas por Projected Gauss-Seidel iterativo
auto* solver     = new btSequentialImpulseConstraintSolver();

auto* world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, config);
world->setGravity(btVector3(0, -9.81f, 0));
```

**Por que quatro objetos e não um?** Porque cada um é substituível. Trocar `btDbvtBroadphase`
por `btAxisSweep3` muda a estratégia de filtragem sem tocar em mais nada; trocar o solver muda
a qualidade/custo da resolução de contatos. É injeção de dependência aplicada a um motor de
física — o mesmo princípio de [`src/solid/dip/`](../solid/dip/README.md).

**Por que "Discrete"?** Porque o mundo avança em saltos de `dt`. Ele *não* olha o que aconteceu
entre um passo e o outro — ver "tunneling", em "Armadilhas".

### 2 · O chão (corpo estático)

```cpp
// Plano INFINITO: normal (0,1,0) = horizontal, deslocamento 0
auto* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);

btTransform groundTransform;
groundTransform.setIdentity();
groundTransform.setOrigin(btVector3(0, 0, 0));

auto* groundMotion = new btDefaultMotionState(groundTransform);

btRigidBody::btRigidBodyConstructionInfo groundCI(
    0,                   // massa 0 → ESTÁTICO
    groundMotion,
    groundShape,
    btVector3(0, 0, 0)   // inércia zero (irrelevante para estático)
);
auto* groundBody = new btRigidBody(groundCI);
world->addRigidBody(groundBody);
```

> ⚠ `btStaticPlaneShape` só serve para corpos **estáticos**, e o nome não é decorativo: um plano
> infinito tem volume infinito. Não existe centro de massa, não existe tensor de inércia finito,
> não existe "acelerar o plano". Se você precisa de um chão que se mova, use um `btBoxShape`
> bem achatado.

### 3 · A esfera (corpo dinâmico)

```cpp
auto* sphereShape = new btSphereShape(1.0f);   // raio = 1 m

btTransform sphereTransform;
sphereTransform.setIdentity();
sphereTransform.setOrigin(btVector3(0, 10, 0));

btScalar mass = 1.0f;
btVector3 localInertia(0, 0, 0);
sphereShape->calculateLocalInertia(mass, localInertia);   // (2/5)·m·r² = 0.4

auto* sphereMotion = new btDefaultMotionState(sphereTransform);
btRigidBody::btRigidBodyConstructionInfo sphereCI(
    mass, sphereMotion, sphereShape, localInertia);
auto* sphereBody = new btRigidBody(sphereCI);

sphereBody->setRestitution(0.6f);
groundBody->setRestitution(0.6f);              // ⚠ combinados = 0.6 × 0.6 = 0.36

world->addRigidBody(sphereBody);
```

> ⚠ **A posição é a do CENTRO de massa.** A esfera "em y = 10" tem sua base em y = 9, e vai
> repousar em **y = 1**, não em y = 0. É a primeira confusão de todo mundo ao ler a saída — e a
> última linha do programa imprime exatamente `1.0000`.

#### `btMotionState`: por que não ler a pose direto do corpo?

Por três razões concretas:

1. **O Bullet só sincroniza corpos ATIVOS.** Um corpo adormecido (ver "Armadilhas") não gera
   chamada nenhuma ao seu `motionState`. Renderizar por *callback* significa não fazer trabalho
   nenhum por objeto parado — e uma cena típica tem 95% dos objetos parados.
2. **Ele interpola.** Quando o `dt` do render não bate com o `fixedTimeStep` da física, o Bullet
   entrega ao `motionState` uma transformação *interpolada*, suave, em vez do último estado
   discreto. É por isso que a coluna `y` deste demo e a coluna `vy` (lida direto de
   `getLinearVelocity()`) estão defasadas em exatamente um sub-passo — mais sobre isso em
   "Como validar".
3. **Ele é o ponto de acoplamento com o renderer.** Você deriva de `btMotionState` e escreve
   `setWorldTransform` direto no seu objeto de cena. A física empurra; o render não precisa
   perguntar.

### 4 · O laço

```cpp
const float dt    = 1.0f / 60.0f;
const int   steps = 180;              // 3 segundos

for (int i = 0; i < steps; ++i) {
    world->stepSimulation(dt, 1, dt);

    btTransform t;
    sphereBody->getMotionState()->getWorldTransform(t);

    float y  = t.getOrigin().getY();
    float vy = sphereBody->getLinearVelocity().getY();
    // …imprime a cada 10 frames
}
```

`stepSimulation(timeStep, maxSubSteps, fixedTimeStep)`:

| parâmetro | significado |
|---|---|
| `timeStep` | quanto tempo real passou desde o frame anterior |
| `fixedTimeStep` | o `dt` interno **fixo** com que a física realmente avança (padrão 1/60) |
| `maxSubSteps` | quantos sub-passos internos são permitidos se `timeStep > fixedTimeStep` |

Passando `timeStep == fixedTimeStep` e `maxSubSteps = 1`, como aqui, a simulação fica
**determinística**: mesma entrada, mesma saída, sempre. É o que permite comparar a saída com a
fórmula.

#### Por que `dt` fixo interno?

Porque a **estabilidade do integrador** depende de `dt`, e um `dt` que varia com a carga da
máquina faria a física mudar de comportamento conforme o FPS.

O Bullet integra com **Euler semi-implícito** (também chamado *symplectic Euler*). A diferença
para o Euler explícito é a ordem de duas linhas — e é a diferença entre um simulador estável e
um que explode:

```cpp
// Euler EXPLÍCITO — usa a velocidade ANTIGA para mover
x += v * dt;
v += a * dt;      // energia cresce sem limite: uma órbita vira espiral para fora

// Euler SEMI-IMPLÍCITO — atualiza a velocidade primeiro, move com a NOVA
v += a * dt;
x += v * dt;      // energia oscila em torno do valor certo, mas não diverge
```

Os dois têm o mesmo custo e a mesma ordem de erro local. Só o segundo conserva
aproximadamente a energia ao longo de milhões de passos, e é por isso que praticamente todo
motor de física de tempo real usa esta forma.

Este demo permite ver o semi-implícito diretamente: a posição após *n* passos é exatamente

```
y_n = 10 − g·dt²·n(n+1)/2
```

O `n(n+1)/2` — e não `n²/2` — é a assinatura do semi-implícito. Ver "Como validar".

### 5 · Cleanup

```cpp
world->removeRigidBody(sphereBody);      // 1. tirar do mundo
world->removeRigidBody(groundBody);

delete sphereBody;   delete sphereMotion;   delete sphereShape;   // 2. destruir corpos
delete groundBody;   delete groundMotion;   delete groundShape;

delete world;        delete solver;                                // 3. destruir o mundo
delete broadphase;   delete dispatcher;     delete config;
```

O Bullet **não faz ownership de nada**. Todo `new` acima é seu para destruir. Verificando na
fonte (`BulletCollision/CollisionDispatch/btCollisionWorld.cpp`), o destrutor do mundo é:

```cpp
btCollisionWorld::~btCollisionWorld()
{
    for (int i = 0; i < m_collisionObjects.size(); i++)
    {
        btCollisionObject* collisionObject = m_collisionObjects[i];
        btBroadphaseProxy* bp = collisionObject->getBroadphaseHandle();
        if (bp) { /* limpa o proxy do broadphase */ }
    }
}
```

Ele percorre os corpos **para desreferenciá-los**, e não chama `delete` em nenhum.

> ⚠ Daí a ordem — e o motivo é o oposto do que muita gente escreve. **Não** é "deletar o mundo
> antes dos corpos quebra". É: **deletar um corpo que ainda está registrado no mundo** deixa um
> ponteiro pendurado em `m_collisionObjects`, e o `delete world` seguinte o desreferencia →
> *use-after-free*. `removeRigidBody()` primeiro, `delete` depois.

> ⚠ Este bloco existe para você ver o custo. A alternativa correta está em
> [`../sfml_bullet/physics_world.hpp`](../sfml_bullet/physics_world.hpp): tudo em
> `std::unique_ptr`, e o destrutor da classe faz **uma** coisa —
> ```cpp
> PhysicsWorld::~PhysicsWorld() {
>     if (!world_) return;                                  // objeto foi movido
>     for (auto& rb : rigid_bodies_) world_->removeRigidBody(rb.get());
> }
> ```
> — porque o corpo do destrutor roda **antes** da destruição dos membros. Os onze `delete`
> somem, e nenhum caminho de exceção pode pular a limpeza. É RAII aplicado a uma API em C:
> ver [`../smart-pointers/README.md`](../smart-pointers/README.md).

## Saída real

Copiada de uma execução (`./dist/bin/collision`), sem edição:

```
  step    time (s)         y (m)      vy (m/s)
----------------------------------------------
     0       0.000       10.0000       -0.1635
    10       0.167        9.8501       -1.7985
    20       0.333        9.4278       -3.4335
    30       0.500        8.7329       -5.0685
    40       0.667        7.7655       -6.7035
    50       0.833        6.5256       -8.3385
    60       1.000        5.0133       -9.9735
    70       1.167        3.2284      -11.6085
    80       1.333        1.1710      -13.2435   ← último frame antes do contato
    90       1.500        1.5773        3.2962   ← já quicou; subindo
   100       1.667        2.0040        1.6612
   110       1.833        2.1582        0.0262   ← ápice do quique
   120       2.000        2.0400       -1.6088
   130       2.167        1.6492       -3.2438
   140       2.333        0.9859        1.8662   ← segundo contato (note y < 1: penetrou)
   150       2.500        1.1744        0.2312
   160       2.667        1.0903       -1.4038
   170       2.833        1.0000        0.0000   ← repouso, centro em y = 1 (= raio)
```

Como as amostras são de 10 em 10 passos, o instante exato do impacto cai entre duas linhas.
A próxima seção mostra como localizá-lo.

## Como validar

Este demo tem uma propriedade rara: **é conferível no papel**. Três verificações.

### 1 · A queda livre bate com a fórmula, dígito a dígito

Antes do primeiro contato não há colisão nenhuma — só o integrador. Para Euler semi-implícito
com `v₀ = 0`, `y₀ = 10`, `g = 9,81`, `dt = 1/60`:

```
y_n = 10 − g·dt²·n(n+1)/2
```

```bash
python3 -c '
g, dt = 9.81, 1/60
for n in range(0, 90, 10):
    print(f"{n:>6} {10 - g*dt*dt*n*(n+1)/2:>10.4f}")'
```

| step | fórmula | programa |
|---|---|---|
| 10 | 9.8501 | 9.8501 |
| 30 | 8.7329 | 8.7329 |
| 50 | 6.5256 | 6.5256 |
| 70 | 3.2284 | 3.2284 |
| 80 | 1.1710 | 1.1710 |

Batem em todas as casas impressas. E o `n(n+1)` — em vez do `n²` do papel — é a *assinatura* do
integrador semi-implícito: o erro do método é exatamente meio passo de velocidade, ou
`g·dt·t/2`. Comparando com a solução contínua `y(t) = 10 − 4,905·t²`, em t = 0,833 s a fórmula
contínua dá 6,594 e o programa dá 6,5256 — 1% de diferença, e agora você sabe de onde ela vem.

Repare também que `y` e `vy` estão defasados de um sub-passo: em `step 10`,
`vy = −1,7985 = −11·g·dt`, ou seja, a velocidade **depois** de 11 passos, enquanto `y` é a
posição **depois** de 10. É o `btMotionState` interpolando, exatamente como descrito no bloco 3.

### 2 · O instante do impacto

O centro toca o plano quando y = raio = 1, ou seja, após uma queda de 9 m:

```
t = √(2·9 / 9,81) ≈ 1,3546 s   →   step ≈ 1,3546 · 60 ≈ 81,3
```

E de fato: no step 80 (t = 1,333 s) a esfera ainda está em y = 1,1710; no step 90 já está
subindo. Aplicando a fórmula discreta, `y₈₀ = 1,1710` e `y₈₁ = 0,9503` — o contato acontece
entre os passos 80 e 81. ✔

### 3 · O quique bate com *e* = 0,36 (e não 0,6)

Velocidade no instante do impacto: `vy` no step 80 é **−13,2435 m/s** (= −81·g·dt).
Com o coeficiente combinado 0,6 × 0,6 = 0,36:

```
v_saída = 0,36 · 13,2435 = 4,7677 m/s
h_ápice = v² / (2g) = 4,7677² / 19,62 = 1,1584 m
y_ápice = 1 + 1,1584 = 2,1584 m
```

O programa imprime **2.1582** no step 110. Quatro casas de acordo — e se você tivesse assumido
*e* = 0,6, teria previsto um ápice em 4,22 m e concluído erradamente que o Bullet estava
quebrado.

### 4 · O corpo adormece (e o número de steps importa)

Aumente `steps` de 180 para 600 e imprima `sphereBody->isActive()`:

```cpp
const int steps = 600;
// dentro do laço:
fmt::print("ativo={} ", sphereBody->isActive());
```

Medido nesta máquina: a esfera para em torno do **step 175** e o Bullet a **desativa no step
284** (t = 4,733 s) — cerca de 2 s parada, que é o `gDeactivationTime` padrão. A partir daí ela
some do solver e do broadphase ativo, e `getMotionState()` deixa de ser chamado.

Isso é uma otimização enorme (uma cena com mil caixas empilhadas custa quase nada depois que
assenta), mas confunde quem não espera: um corpo adormecido **não reage** a uma força aplicada
por código. Ou você o acorda com `activate()`, ou desliga a otimização com
`setActivationState(DISABLE_DEACTIVATION)`.

### 5 · Compilar isolado, com todos os avisos

```bash
export PKG_CONFIG_PATH=$PWD/build
FMTP=$(sed -n 's|^prefix=||p' build/fmt.pc)
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic \
    $(pkg-config --cflags bullet) -I$FMTP/include \
    src/collision/main.cpp -o /tmp/collision \
    $(pkg-config --libs bullet) -L$FMTP/lib -lfmtd
/tmp/collision
```

Estado atual: compila **sem nenhum aviso** com `-Wall -Wextra -Wpedantic`.

## Armadilhas

| Armadilha | Sintoma | Causa e correção |
|---|---|---|
| Posição pelo centro | "a esfera parou flutuando em y = 1" | é o raio. O centro repousa em y = raio |
| Restituição multiplicada | quique menor que o esperado | o Bullet multiplica: 0.6 × 0.6 = 0.36 |
| `calculateLocalInertia` esquecido | o corpo desliza mas nunca tomba | inércia (0,0,0) → o corpo não responde a torque |
| *Tunneling* | uma bala atravessa a parede | `btDiscreteDynamicsWorld` só testa posições **discretas**. Se o corpo anda mais que a espessura do obstáculo em um `dt`, não há frame em que eles se sobreponham. Correção: `setCcdMotionThreshold()` + `setCcdSweptSphereRadius()` (*continuous collision detection*), ou `dt` menor |
| *Sleeping* | o corpo ignora uma força aplicada | está desativado. `activate()` ou `setActivationState(DISABLE_DEACTIVATION)` |
| `delete` antes de `removeRigidBody` | crash no fim do programa | ponteiro pendurado em `m_collisionObjects` |
| Plano infinito dinâmico | comportamento sem sentido | `btStaticPlaneShape` só vale com massa 0 |

## Para onde ir depois

- **múltiplos corpos** — empilhe dez esferas e veja o solver iterativo trabalhando;
- **outras shapes** — `btBoxShape`, `btCylinderShape`, `btCapsuleShape`, `btConvexHullShape`;
- **juntas** — `btHingeConstraint` (dobradiça), `btSliderConstraint` (trilho),
  `btPoint2PointConstraint` (rótula);
- **desenhar** — [`../sfml_bullet/`](../sfml_bullet/README.md) põe este mesmo mundo numa janela
  SFML, com `unique_ptr` no lugar dos `delete`;
- **cenas complexas** — [`../sfml_bullet2/`](../sfml_bullet2/README.md) traz *ragdoll* e corda.

## Referências

- [`../sfml_bullet/`](../sfml_bullet/README.md) — o mesmo pipeline com RAII e renderização
- [`../sfml_bullet2/`](../sfml_bullet2/README.md) — juntas, ragdoll, corda
- [`../smart-pointers/`](../smart-pointers/README.md) — por que `unique_ptr` no lugar dos `delete`
- [`../solid/dip/`](../solid/dip/README.md) — inversão de dependência, o padrão dos quatro
  subsistemas plugáveis do mundo Bullet
- [`../performance/`](../performance/README.md) — localidade de cache, o outro motivo de o
  broadphase ser rápido
- *Bullet 2.83 Physics SDK Manual* — o PDF oficial, distribuído junto com o código-fonte do
  Bullet (`docs/Bullet_User_Manual.pdf`)
- <https://github.com/bulletphysics/bullet3> — código-fonte; a wiki tem os artigos sobre
  *anatomy of a collision*, CCD e *activation states*
- Erin Catto, *Iterative Dynamics with Temporal Coherence* (GDC 2005) — a origem do solver
  de impulsos sequenciais usado aqui
