# collision — Simulação de Física com Bullet3

## O que é este projeto?

Demonstra como configurar e usar o motor de física **Bullet Physics** para simular colisões 3D com corpos rígidos. O cenário é simples mas completo: uma esfera cai de y=10 e colide com um plano em y=0, quicando com coeficiente de restituição 0.6.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| Pipeline de física | Broadphase → Narrowphase → Solver → Integração |
| Corpos estáticos vs dinâmicos | massa = 0 vs massa > 0 |
| Shapes de colisão | `btStaticPlaneShape`, `btSphereShape` |
| `btMotionState` | Interface entre Bullet e o engine de renderização |
| Coeficiente de restituição | Controla o "quique" (0 = sem, 1 = perfeito) |
| Cleanup manual | Bullet não possui garbage collector |

---

## Estrutura de arquivos

```
collision/
├── main.cpp      ← cenário completo de queda livre com colisão
└── meson.build
```

---

## Pipeline de um frame no Bullet

```
dynamicsWorld->stepSimulation(dt, maxSubSteps, fixedTimeStep)
                     │
          ┌──────────┼──────────┐──────────┐
          │          │          │          │
     Broadphase  Narrowphase  Solver  Integração
          │          │          │          │
    Detecta pares  Colisão   Resolve   Atualiza
    de AABB que   exata     contatos  posição e
    se sobrepõem  entre     (impulsos, velocidade
    (rápido)      pares     fricção)  de cada corpo
```

---

## Componentes do mundo físico

### 1. Configuração

```cpp
// Registra os algoritmos padrão para cada par de shapes
btDefaultCollisionConfiguration* config = new btDefaultCollisionConfiguration();

// Itera os pares candidatos e executa o algoritmo correto (narrowphase)
btCollisionDispatcher* dispatcher = new btCollisionDispatcher(config);

// Estrutura de aceleração para broadphase
// btDbvtBroadphase = Dynamic AABB Tree → O(n log n), ótimo para cenas dinâmicas
// Alternativa: btAxisSweep3 para cenas com bounds fixos
btDbvtBroadphase* broadphase = new btDbvtBroadphase();

// Resolve contatos e constraints usando Projected Gauss-Seidel
btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver();

// O mundo que une tudo
btDiscreteDynamicsWorld* world = new btDiscreteDynamicsWorld(
    dispatcher, broadphase, solver, config
);
world->setGravity(btVector3(0, -9.81f, 0));
```

### 2. Corpo estático (chão)

```cpp
// btStaticPlaneShape: plano infinito, normal (0,1,0), offset 0
btStaticPlaneShape* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);

btTransform groundTransform;
groundTransform.setIdentity();

btDefaultMotionState* groundMotion = new btDefaultMotionState(groundTransform);

// massa = 0, inércia = (0,0,0) → corpo ESTÁTICO, nunca se move
btRigidBody::btRigidBodyConstructionInfo groundCI(
    0,              // massa 0 = estático
    groundMotion,
    groundShape,
    btVector3(0, 0, 0)  // inércia zero
);
btRigidBody* groundBody = new btRigidBody(groundCI);
world->addRigidBody(groundBody);
```

### 3. Corpo dinâmico (esfera)

```cpp
btSphereShape* sphereShape = new btSphereShape(1.0f); // raio = 1 metro

btTransform sphereTransform;
sphereTransform.setIdentity();
sphereTransform.setOrigin(btVector3(0, 10, 0)); // começa em y=10

btScalar mass = 1.0f; // 1 kg

// Tensor de inércia: para esfera = (2/5) * m * r²
btVector3 localInertia(0, 0, 0);
sphereShape->calculateLocalInertia(mass, localInertia);

btDefaultMotionState* sphereMotion = new btDefaultMotionState(sphereTransform);
btRigidBody::btRigidBodyConstructionInfo sphereCI(mass, sphereMotion, sphereShape, localInertia);
btRigidBody* sphereBody = new btRigidBody(sphereCI);

// Coeficiente de restituição: 0.6 = 60% da velocidade é recuperada no impacto
sphereBody->setRestitution(0.6f);
groundBody->setRestitution(0.6f);

world->addRigidBody(sphereBody);
```

---

## Loop de simulação

```cpp
const float dt    = 1.0f / 60.0f; // passo de tempo: 60 FPS
const int   steps = 180;          // 3 segundos de simulação

for (int i = 0; i < steps; ++i) {
    // stepSimulation(timeStep, maxSubSteps, fixedTimeStep):
    //   - fixedTimeStep: dt interno para estabilidade numérica (padrão 1/60)
    //   - maxSubSteps: se timeStep > fixedTimeStep, subdivide (até maxSubSteps vezes)
    //   - passando maxSubSteps=1 e dt=fixedTimeStep → determinístico
    world->stepSimulation(dt, 1, dt);

    btTransform t;
    sphereBody->getMotionState()->getWorldTransform(t);

    float y  = t.getOrigin().getY();   // posição vertical
    float vy = sphereBody->getLinearVelocity().getY(); // velocidade vertical
}
```

### Por que dt fixo interno?

Bullet usa um passo de tempo fixo internamente para **estabilidade numérica**. Integradores de Euler explícitos (usados em física de jogos) divergem se o dt varia muito. O `stepSimulation` aceita um dt variável externo e subdivide internamente conforme necessário.

---

## Cleanup — A ordem importa!

```cpp
// 1. Remover corpos do mundo ANTES de deletar
world->removeRigidBody(sphereBody);
world->removeRigidBody(groundBody);

// 2. Deletar corpos e seus componentes
delete sphereBody;   delete sphereMotion;   delete sphereShape;
delete groundBody;   delete groundMotion;   delete groundShape;

// 3. Deletar o mundo e seus subsistemas
delete world;        delete solver;
delete broadphase;   delete dispatcher;     delete config;
```

Bullet não possui smart pointers nem garbage collector. A ordem de deleção é crítica — deletar o mundo antes dos corpos causa acesso a memória inválida.

---

## Saída esperada

```
  step    time (s)        y (m)      vy (m/s)
----------------------------------------------
     0       0.000      10.0000        0.0000
    10       0.167       9.8635       -1.6350
    20       0.333       9.4542       -3.2700
    ...
    50       0.833       6.4127       -8.1750   ← acelerando
    ...
    90       1.500       1.0000      -14.1...   ← prestes a colidir
    ...
   100       1.667       0.9...       +8.5...   ← quicou! velocidade invertida
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja collision
./bin/collision
```

---

## Dependências

- `bullet3` — motor de física 3D
- `fmt` — formatação de saída

---

## Extensões possíveis

- Adicionar múltiplos corpos e observar colisões entre eles
- Usar `btBoxShape` ou `btCylinderShape`
- Adicionar `btHingeConstraint` ou `btSliderConstraint` para articulações
- Visualizar com SFML (veja o projeto `sfml_bullet`)
