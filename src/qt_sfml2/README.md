# qt_sfml2 — Qt + SFML + Bullet3 (Física Interativa)

## O que é este projeto?

Extensão do `qt_sfml` que adiciona **simulação de física com Bullet3** ao widget SFML dentro do Qt. O usuário pode criar corpos físicos via interface Qt e observar colisões e gravidade em tempo real.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| Integração tripla | Qt (UI) + SFML (render) + Bullet (física) |
| `PhysicsWorld` wrapper | Encapsula Bullet dentro de uma classe C++ segura |
| Sincronização física→visual | Posição Bullet → posição SFML a cada frame |
| Coordenadas mundo físico | Metros (Bullet) ↔ Pixels (SFML) |
| Criação dinâmica de corpos | Clique na tela cria novo corpo físico |

---

## Estrutura de arquivos

```
qt_sfml2/
├── main.cpp              ← QApplication + MainWindow
├── mainwindow.hpp/cpp    ← controles Qt + conexão com SFMLWidget
├── sfml_widget.hpp/cpp   ← game loop: Bullet step + SFML render
├── physics_world.hpp/cpp ← wrapper de alto nível sobre Bullet
└── meson.build
```

---

## `PhysicsWorld` — wrapper sobre Bullet

```cpp
class PhysicsWorld {
    btDefaultCollisionConfiguration*     config_;
    btCollisionDispatcher*               dispatcher_;
    btDbvtBroadphase*                    broadphase_;
    btSequentialImpulseConstraintSolver* solver_;
    btDiscreteDynamicsWorld*             world_;

    // Armazena todos os objetos criados para destruição correta
    std::vector<btRigidBody*>      bodies_;
    std::vector<btCollisionShape*> shapes_;
    std::vector<btMotionState*>    states_;

public:
    PhysicsWorld() {
        config_     = new btDefaultCollisionConfiguration();
        dispatcher_ = new btCollisionDispatcher(config_);
        broadphase_ = new btDbvtBroadphase();
        solver_     = new btSequentialImpulseConstraintSolver();
        world_      = new btDiscreteDynamicsWorld(
            dispatcher_, broadphase_, solver_, config_);
        world_->setGravity(btVector3(0, -9.81f, 0));
    }

    // Adiciona esfera dinâmica em posição (x, y) em metros
    btRigidBody* add_sphere(float x, float y, float radius, float mass = 1.0f) {
        auto* shape  = new btSphereShape(radius);
        auto* state  = new btDefaultMotionState(
            btTransform(btQuaternion::getIdentity(), btVector3(x, y, 0)));
        btVector3 inertia(0,0,0);
        shape->calculateLocalInertia(mass, inertia);
        auto* body   = new btRigidBody(mass, state, shape, inertia);

        // Restrição 2D: só se move em XY, só rota em Z
        body->setLinearFactor(btVector3(1, 1, 0));
        body->setAngularFactor(btVector3(0, 0, 1));

        world_->addRigidBody(body);
        bodies_.push_back(body);
        shapes_.push_back(shape);
        states_.push_back(state);
        return body;
    }

    void step(float dt) {
        world_->stepSimulation(dt, 10, 1.0f/120.0f);
    }
};
```

---

## Conversão de coordenadas — física vs tela

```
Sistema Bullet:     Y cresce para CIMA,  unidade: metros
Sistema SFML:       Y cresce para BAIXO, unidade: pixels

Escala: 1 metro = 80 pixels

Conversão:
  pixel_x = (meter_x) * 80.0f + (window_width / 2)
  pixel_y = (window_height / 2) - (meter_y) * 80.0f
```

```cpp
sf::Vector2f bullet_to_sfml(btVector3 pos, sf::Vector2u window_size) {
    const float SCALE = 80.0f;
    return {
        pos.getX() * SCALE + window_size.x / 2.0f,
        window_size.y / 2.0f - pos.getY() * SCALE
    };
}

btVector3 sfml_to_bullet(sf::Vector2i pixel, sf::Vector2u window_size) {
    const float SCALE = 80.0f;
    return {
        (pixel.x - window_size.x / 2.0f) / SCALE,
        (window_size.y / 2.0f - pixel.y) / SCALE,
        0.0f
    };
}
```

---

## Game loop — física + renderização

```cpp
void SFMLWidget::tick() {
    float dt = clock_.restart().asSeconds();
    dt = std::min(dt, 0.05f); // clamp: evita instabilidade em frames lentos

    // 1. Avança a física
    physics_.step(dt);

    // 2. Lê eventos SFML (mouse, teclado)
    sf::Event event;
    while (window_.pollEvent(event)) {
        if (event.type == sf::Event::MouseButtonPressed)
            on_click(event.mouseButton.x, event.mouseButton.y);
    }

    // 3. Limpa a tela
    window_.clear(sf::Color(15, 15, 25));

    // 4. Renderiza cada corpo físico
    for (auto& [body, shape] : renderables_) {
        btTransform t;
        body->getMotionState()->getWorldTransform(t);

        auto pos = bullet_to_sfml(t.getOrigin(), window_.getSize());
        auto angle = t.getRotation(); // btQuaternion

        shape.setPosition(pos);
        // Converte quaternion → ângulo em graus para SFML
        float angle_deg = angle.getAngle() * (180.0f / M_PI);
        shape.setRotation(angle_deg);
        window_.draw(shape);
    }

    window_.display();
}
```

---

## Diferença para `qt_sfml`

| Aspecto | `qt_sfml` | `qt_sfml2` |
|---|---|---|
| Física | Manual (rebatimento simples) | Bullet3 completo |
| Gravidade | Não | Sim (−9.81 m/s²) |
| Colisões | Bordas da tela | Entre todos os corpos |
| Interatividade | Slider de velocidade | Clique cria corpos |
| Complexidade | Intro | Intermediário |

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja qt_sfml2
./bin/qt_sfml2
# Clique na área SFML para criar esferas que caem com física real
```

---

## Dependências

- `Qt6` (ou Qt5) — framework GUI
- `SFML` — renderização 2D
- `Bullet3` — motor de física 3D
