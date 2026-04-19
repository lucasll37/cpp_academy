# sfml_bullet — Física 2D Interativa com SFML + Bullet3

## O que é este projeto?

Demonstra como combinar **SFML** (renderização 2D) com **Bullet3** (motor de física 3D usado em modo 2D) para criar uma simulação física visual e interativa. Esferas e caixas caem com gravidade, colidem entre si e com o chão.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| Restrição 2D no Bullet3D | `setLinearFactor` e `setAngularFactor` para mover só em XY |
| `btSphereShape` e `btBoxShape` | Shapes convexas mais comuns |
| `btMotionState` → posição SFML | Sincronização física → visual a cada frame |
| `btQuaternion` → ângulo | Rotação 3D para ângulo 2D em graus |
| Delta-time variável | Medir tempo real entre frames e passar para Bullet |
| Escala métrica | 1 metro Bullet = N pixels SFML |

---

## Estrutura de arquivos

```
sfml_bullet/
├── main.cpp              ← entry point, cria e roda Simulation
├── simulation.hpp/cpp    ← game loop: poll events, step, render
├── physics_world.hpp/cpp ← wrapper Bullet com helpers add_sphere/add_box
└── meson.build
```

---

## Restrição 2D — o detalhe mais importante

Bullet é um motor 3D. Para simular física 2D:

```cpp
btRigidBody* body = /* ... criar corpo ... */;

// Só se move nos eixos X e Y (Z fixo)
body->setLinearFactor(btVector3(1, 1, 0));

// Só rota ao redor do eixo Z (como se fosse 2D)
body->setAngularFactor(btVector3(0, 0, 1));

// Resultado: comportamento perfeitamente 2D
// com a física 3D de alta qualidade do Bullet
```

---

## `PhysicsWorld` — wrapper de alto nível

```cpp
class PhysicsWorld {
public:
    PhysicsWorld() {
        // Setup padrão do Bullet
        config_     = new btDefaultCollisionConfiguration();
        dispatcher_ = new btCollisionDispatcher(config_);
        broadphase_ = new btDbvtBroadphase();
        solver_     = new btSequentialImpulseConstraintSolver();
        world_      = new btDiscreteDynamicsWorld(
            dispatcher_, broadphase_, solver_, config_);
        world_->setGravity(btVector3(0, -20.f, 0)); // mais forte para parecer 2D

        create_boundaries(); // chão e paredes invisíveis
    }

    btRigidBody* add_sphere(float x, float y, float radius, float mass = 1.f) {
        auto* shape = new btSphereShape(radius);
        btVector3 inertia(0,0,0);
        shape->calculateLocalInertia(mass, inertia);

        auto* state = new btDefaultMotionState(
            btTransform(btQuaternion::getIdentity(), btVector3(x, y, 0)));
        auto* body  = new btRigidBody(mass, state, shape, inertia);

        body->setLinearFactor(btVector3(1, 1, 0));  // 2D!
        body->setAngularFactor(btVector3(0, 0, 1)); // 2D!
        body->setRestitution(0.5f);   // quica um pouco
        body->setFriction(0.7f);

        world_->addRigidBody(body);
        // armazena para destruição posterior
        return body;
    }

    btRigidBody* add_box(float x, float y, float hw, float hh, float mass = 1.f) {
        auto* shape = new btBoxShape(btVector3(hw, hh, 0.5f)); // delgado em Z
        // ... mesmo padrão ...
    }

    void step(float dt) {
        // Clamp: evita instabilidade se um frame demorar muito
        dt = std::min(dt, 0.05f);
        world_->stepSimulation(dt, 10, 1.f/120.f);
    }
};
```

---

## Conversão de coordenadas

```
Bullet:  Y cresce para CIMA,  centro = (0, 0), unidade = metros
SFML:    Y cresce para BAIXO, (0,0) = canto superior esquerdo, unidade = pixels

const float SCALE = 80.f; // 1 metro = 80 pixels

Bullet → SFML:
  px = bullet_x * SCALE + window_width  / 2
  py = window_height / 2 - bullet_y * SCALE

SFML → Bullet (para clique do mouse):
  bx = (px - window_width  / 2) / SCALE
  by = (window_height / 2 - py) / SCALE
```

---

## O game loop — `Simulation`

```cpp
void Simulation::run() {
    sf::RenderWindow window(sf::VideoMode(800, 600), "SFML + Bullet3");
    window.setFramerateLimit(60);
    sf::Clock clock;

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();

        // Eventos
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::MouseButtonPressed) {
                // Clique esquerdo: cria esfera onde clicou
                auto pos = sfml_to_bullet(event.mouseButton.x,
                                          event.mouseButton.y,
                                          window.getSize());
                physics_.add_sphere(pos.x, pos.y, 0.3f);
            }
        }

        // Física
        physics_.step(dt);

        // Renderização
        window.clear(sf::Color(20, 20, 30));
        render_bodies(window);
        window.display();
    }
}

void Simulation::render_bodies(sf::RenderWindow& win) {
    for (auto& [body, radius] : spheres_) {
        btTransform t;
        body->getMotionState()->getWorldTransform(t);
        btVector3 pos   = t.getOrigin();
        btQuaternion rot = t.getRotation();

        // Converte posição
        sf::Vector2f sfml_pos = bullet_to_sfml(pos, win.getSize());

        // Converte rotação: quaternion → ângulo em radianos → graus
        float angle_rad = rot.getAngle();
        float angle_deg = angle_rad * (180.f / M_PI);

        sf::CircleShape circle(radius * SCALE);
        circle.setOrigin(radius * SCALE, radius * SCALE);
        circle.setPosition(sfml_pos);
        circle.setRotation(angle_deg);
        circle.setFillColor(sf::Color(100, 180, 255, 200));
        circle.setOutlineColor(sf::Color::White);
        circle.setOutlineThickness(1.f);
        win.draw(circle);
    }
}
```

---

## Como interagir

```
Clique esquerdo    → cria esfera no ponto clicado
Clique direito     → cria caixa no ponto clicado
Tecla R           → reinicia a simulação
Tecla ESC         → fecha
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja sfml_bullet
./bin/sfml_bullet
```

---

## Dependências

- `SFML` — renderização 2D, janela, eventos
- `Bullet3` — motor de física
