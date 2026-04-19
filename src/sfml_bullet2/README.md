# sfml_bullet2 — Ragdoll e Cordas com SFML + Bullet3

## O que é este projeto?

Extensão do `sfml_bullet` com dois sistemas físicos avançados: um **ragdoll** (corpo articulado com juntas) e uma **corda** (cadeia de corpos rígidos conectados). Demonstra o uso de constraints do Bullet3.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| `btHingeConstraint` | Junta de 1 DOF (dobradiça): joelho, cotovelo |
| `btConeTwistConstraint` | Junta de 3 DOF: ombro, quadril, pescoço |
| `btPoint2PointConstraint` | Junta de bola (ball-and-socket) |
| Ragdoll anatomy | Modelar corpo humano como grafo de corpos + juntas |
| Corda física | Cadeia de cilindros com ponto2ponto constraints |
| Impulso de explosão | Aplicar força instantânea em todos os segmentos |

---

## Estrutura de arquivos

```
sfml_bullet2/
├── main.cpp              ← cria janela SFML + simulation
├── simulation.hpp/cpp    ← game loop com ragdoll + corda
├── physics_world.hpp/cpp ← wrapper Bullet com add_constraint
├── ragdoll.hpp/cpp       ← corpo humano em 7 segmentos
└── rope.hpp/cpp          ← corda como cadeia de cilindros
```

---

## Ragdoll — `ragdoll.hpp`

```cpp
struct Ragdoll {
    enum Segment { TORSO=0, HEAD, ARM_L, ARM_R, LEG_L, LEG_R, COUNT };
    btRigidBody* bodies[COUNT] = {};

    void build(PhysicsWorld& pw, float cx, float cy) {
        // Dimensões em metros (escala humana simplificada)
        auto torso = pw.add_capsule(cx,      cy,      0.25f, 0.5f, 5.0f); // 0.5x1.0m
        auto head  = pw.add_sphere (cx,      cy+0.8f, 0.2f,        1.0f); // esfera r=0.2m
        auto arm_l = pw.add_capsule(cx-0.5f, cy+0.3f, 0.1f, 0.3f, 1.5f); // braço esq
        auto arm_r = pw.add_capsule(cx+0.5f, cy+0.3f, 0.1f, 0.3f, 1.5f);
        auto leg_l = pw.add_capsule(cx-0.2f, cy-0.7f, 0.12f,0.4f, 2.0f);
        auto leg_r = pw.add_capsule(cx+0.2f, cy-0.7f, 0.12f,0.4f, 2.0f);

        bodies[TORSO] = torso;  bodies[HEAD]  = head;
        bodies[ARM_L] = arm_l;  bodies[ARM_R] = arm_r;
        bodies[LEG_L] = leg_l;  bodies[LEG_R] = leg_r;

        // ── Juntas ──────────────────────────────────────────────────────
        //
        // btConeTwistConstraint: junta de 3 DOF (cone + torção)
        // Boa para: ombro, quadril, pescoço
        // swingSpan1/2: abertura máxima do cone
        // twistSpan: rotação ao redor do eixo do osso
        add_cone_twist(pw, torso, head,    /* pos no torso */ btVector3(0,  0.5f, 0),
                                           /* pos na cabeça */ btVector3(0, -0.2f, 0),
                                           0.2f, 0.2f, 0.1f); // swings + twist

        // btHingeConstraint: junta de 1 DOF (dobradiça)
        // Boa para: cotovelo, joelho
        // Parâmetros: eixo de rotação em cada corpo + limites angulares
        add_hinge(pw, torso, arm_l,
            btVector3(-0.4f, 0.3f, 0),    // pivot no torso
            btVector3(0.3f,  0.0f, 0),    // pivot no braço
            btVector3(0,0,1),             // eixo do torso
            btVector3(1,0,0),             // eixo do braço
            -M_PI/4, M_PI/2);            // limite: −45° a +90°
    }

    // Explosão: aplica impulso em todos os segmentos
    void apply_impulse(const btVector3& impulse, const btVector3& origin) {
        for (auto* b : bodies) {
            if (!b) continue;
            btVector3 dir = b->getWorldTransform().getOrigin() - origin;
            dir.normalize();
            b->applyCentralImpulse(impulse.length() * dir);
        }
    }
};
```

---

## Tipos de Constraints

### `btHingeConstraint` — 1 grau de liberdade

```
Exemplos: joelho, cotovelo, mandíbula, porta

Parâmetros:
  - bodyA, bodyB: os dois corpos conectados
  - pivotA, pivotB: ponto de pivot em cada corpo (espaço local)
  - axisA, axisB: eixo de rotação em cada corpo
  - low, high: limites angulares em radianos
```

### `btConeTwistConstraint` — 3 graus de liberdade

```
Exemplos: ombro, quadril, pescoço, bola em soquete

Parâmetros:
  - frameA, frameB: transformações locais em cada corpo
  - swingSpan1: abertura máxima lateral (radianos)
  - swingSpan2: abertura máxima frontal (radianos)
  - twistSpan: torção máxima ao redor do eixo (radianos)
```

### `btPoint2PointConstraint` — bola e soquete

```
Exemplos: segmentos de corda, pendulo

Parâmetros:
  - pivotA, pivotB: pontos de conexão em cada corpo
  - Sem limites angulares (gira livremente em todos os eixos)
```

---

## Corda — `rope.hpp`

```cpp
class Rope {
    std::vector<btRigidBody*> links_;

public:
    void build(PhysicsWorld& pw, float x, float y, int n_links, float link_length) {
        float mass = 0.5f;

        for (int i = 0; i < n_links; ++i) {
            float lx = x;
            float ly = y - i * link_length;

            auto* link = pw.add_capsule(lx, ly, 0.05f, link_length/2, mass);
            link->setLinearFactor(btVector3(1, 1, 0));   // 2D
            link->setAngularFactor(btVector3(0, 0, 1));  // 2D
            link->setDamping(0.1f, 0.1f); // amortecimento para estabilidade

            if (i == 0) {
                // Primeiro elo: fixo (massa 0 = estático)
                link->setMassProps(0, btVector3(0,0,0));
            }

            if (i > 0) {
                // Conecta ao elo anterior com bola e soquete
                btVector3 pivot(0, link_length/2, 0); // topo do elo atual
                pw.world()->addConstraint(
                    new btPoint2PointConstraint(
                        *links_.back(), *link,
                        btVector3(0, -link_length/2, 0), // base do anterior
                        pivot),                            // topo do atual
                    true // desativa colisão entre elos adjacentes
                );
            }

            links_.push_back(link);
        }
    }
};
```

---

## Explosão — atingindo o ragdoll

```cpp
// Quando o usuário clica com o botão direito:
btVector3 explosion_origin(click_x, click_y, 0);
btVector3 impulse(0, 500, 0); // 500 Ns para cima
ragdoll_.apply_impulse(impulse, explosion_origin);
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja sfml_bullet2
./bin/sfml_bullet2
```

### Controles

```
Clique esquerdo   → cria esfera
Clique direito    → explode o ragdoll no ponto clicado
Tecla C          → cria nova corda
Tecla R          → reseta tudo
ESC              → fecha
```

---

## Dependências

- `SFML` — renderização 2D
- `Bullet3` — motor de física com constraints
