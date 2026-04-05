#include "simulation.hpp"
#include <BulletDynamics/ConstraintSolver/btPoint2PointConstraint.h>
#include <BulletCollision/CollisionDispatch/btCollisionWorld.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <fmt/core.h>
#include <cmath>
#include <algorithm>

namespace sim {

// ── Paletas ──────────────────────────────────────────────────
static const sf::Color C_SPHERE[] = {
    {100,200,255},{255,160,80},{120,240,140},{255,100,130},{220,180,255}};
static const sf::Color C_BOX[] = {
    {255,220,80},{80,200,200},{200,80,255},{255,140,140},{140,255,180}};
static const sf::Color C_CHASSIS {180, 80, 80};
static const sf::Color C_WHEEL   {220,160, 60};

// ── Helpers de shapes ────────────────────────────────────────
static btBoxShape* new_box(float hx, float hy, float hz, PhysicsWorld& pw) {
    auto s = std::make_unique<btBoxShape>(btVector3(hx,hy,hz));
    btBoxShape* p = s.get(); pw.take_shape(std::move(s)); return p;
}
static btSphereShape* new_sphere(float r, PhysicsWorld& pw) {
    auto s = std::make_unique<btSphereShape>(r);
    btSphereShape* p = s.get(); pw.take_shape(std::move(s)); return p;
}

// ── Transform helper ─────────────────────────────────────────
static btTransform make_t(float x, float y, float z = 0.f) {
    btTransform t; t.setIdentity(); t.setOrigin({x,y,z}); return t;
}

// ============================================================
// Constructor
// ============================================================
Simulation::Simulation()
    : window_(sf::VideoMode(800,600), "Bullet3 — física avançada",
              sf::Style::Close | sf::Style::Titlebar)
{
    window_.setFramerateLimit(60);
    font_loaded_ =
        font_.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf") ||
        font_.loadFromFile("/usr/share/fonts/TTF/DejaVuSansMono.ttf") ||
        font_.loadFromFile("/System/Library/Fonts/Menlo.ttc");

    build_scene();
    build_vehicle(-1.5f, 0.6f);

    // Corda pendurada no teto (ancora em (2, 6), cai até (2, 2))
    Rope rope;
    rope.build(physics_, 2.f, 5.5f, 2.f, 2.0f, 20);
    ropes_.push_back(rope);

    // Ragdoll inicial ao centro
    Ragdoll rd;
    rd.build(physics_, 0.f, 3.5f);
    ragdolls_.push_back(rd);
    n_ragdolls_++;
}

// ============================================================
// Cena estática: chão, paredes, plataformas
// ============================================================
void Simulation::build_scene() {
    // Chão (half = 6m de largura, 0.3m de espessura)
    physics_.add_rigid(new_box(6.f, 0.3f, 1.f, physics_), 0.f, make_t(0.f,-0.3f), 0.4f, 0.8f);

    // Parede esquerda
    physics_.add_rigid(new_box(0.3f, 5.f, 1.f, physics_), 0.f, make_t(-5.8f, 5.f), 0.3f, 0.7f);

    // Parede direita
    physics_.add_rigid(new_box(0.3f, 5.f, 1.f, physics_), 0.f, make_t( 5.8f, 5.f), 0.3f, 0.7f);

    // Plataforma inclinada — rotacionamos o transform
    {
        auto s = std::make_unique<btBoxShape>(btVector3(1.8f, 0.15f, 1.f));
        btBoxShape* sh = s.get(); physics_.take_shape(std::move(s));
        btTransform t; t.setIdentity();
        t.setOrigin({-1.5f, 2.5f, 0.f});
        btQuaternion q; q.setEulerZYX(0.f, 0.f, 0.35f); // ~20 graus
        t.setRotation(q);
        physics_.add_rigid(sh, 0.f, t, 0.3f, 0.6f);
    }

    // Plataforma direita — outra inclinação
    {
        auto s = std::make_unique<btBoxShape>(btVector3(1.5f, 0.15f, 1.f));
        btBoxShape* sh = s.get(); physics_.take_shape(std::move(s));
        btTransform t; t.setIdentity();
        t.setOrigin({2.8f, 3.8f, 0.f});
        btQuaternion q; q.setEulerZYX(0.f, 0.f, -0.25f);
        t.setRotation(q);
        physics_.add_rigid(sh, 0.f, t, 0.3f, 0.6f);
    }

    // Pilha inicial de caixas (torre)
    for (int i = 0; i < 6; ++i) {
        float y = 0.25f + i * 0.55f;
        auto* sh = new_box(0.25f, 0.25f, 0.25f, physics_);
        auto* rb  = physics_.add_rigid(sh, 1.2f, make_t(-3.f + (i%2)*0.05f, y), 0.2f, 0.6f);
        auto rect = std::make_unique<sf::RectangleShape>(sf::Vector2f(40.f, 40.f));
        rect->setOrigin(20.f, 20.f);
        rect->setFillColor(C_BOX[i % 5]);
        rect->setOutlineThickness(1.f);
        rect->setOutlineColor(sf::Color(255,255,255,40));
        entities_.push_back({rb, rect.get(), C_BOX[i % 5]});
        rects_.push_back(std::move(rect));
    }
}

// ============================================================
// btRaycastVehicle
// ============================================================
void Simulation::build_vehicle(float cx, float cy) {
    // ── Chassis ──────────────────────────────────────────────
    //
    // O chassis é um btBoxShape normal.
    // A forma deve ser um pouco menor que o visual para que
    // as rodas não penetrem nas paredes (margem de colisão padrão 0.04m).
    auto chassis_shape = std::make_unique<btBoxShape>(btVector3(0.7f, 0.2f, 0.3f));
    btBoxShape* cs = chassis_shape.get();
    physics_.take_shape(std::move(chassis_shape));

    vehicle_.chassis = physics_.add_rigid(cs, 3.0f, make_t(cx, cy), 0.1f, 0.5f);
    // Para o veículo, queremos que o chassis possa girar livremente
    vehicle_.chassis->setAngularFactor(btVector3(0.f, 0.f, 1.f));

    // ── Raycaster + Tuning ────────────────────────────────────
    //
    // btVehicleRaycaster faz raycasts para baixo a partir de cada roda
    // para detectar o solo — é isso que simula a suspensão.
    // A distância entre o ponto de contato e o chassis define
    // quanto a suspensão comprimiu.
    vehicle_.tuning    = std::make_unique<btRaycastVehicle::btVehicleTuning>();
    vehicle_.raycaster = std::make_unique<btDefaultVehicleRaycaster>(physics_.world());
    vehicle_.vehicle   = std::make_unique<btRaycastVehicle>(
        *vehicle_.tuning, vehicle_.chassis, vehicle_.raycaster.get()
    );

    // Registra o veículo como action (o mundo chama updateVehicle() a cada step)
    physics_.world()->addAction(vehicle_.vehicle.get());

    // Eixo correto para cada roda: Y negativo aponta "para baixo" no chassis
    vehicle_.vehicle->setCoordinateSystem(0, 1, 2); // right, up, forward

    // ── Adicionar rodas ───────────────────────────────────────
    //
    // addWheel(connectionPoint, wheelDirection, wheelAxle,
    //          suspensionRestLength, wheelRadius, tuning, isFrontWheel)
    //
    // connectionPoint: posição da roda no espaço local do chassis
    // wheelDirection:  direção da suspensão (para baixo = -Y)
    // wheelAxle:       eixo de rotação da roda (Z para 2D)
    // isFrontWheel:    se true, recebe steering

    btVector3 wheel_dir(0.f, -1.f, 0.f);
    btVector3 wheel_axle(0.f, 0.f, 1.f);
    float     susp_rest  = 0.3f;
    float     wheel_r    = 0.22f;

    struct WheelDef { float x; float y; bool front; };
    WheelDef wdefs[] = {
        {-0.55f, -0.05f, true },  // frente esquerdo
        { 0.55f, -0.05f, true },  // frente direito
        {-0.55f, -0.05f, false},  // trás esquerdo
        { 0.55f, -0.05f, false},  // trás direito
    };

    for (auto& wd : wdefs) {
        btWheelInfo& wi = vehicle_.vehicle->addWheel(
            btVector3(wd.x, wd.y, 0.f),
            wheel_dir, wheel_axle,
            susp_rest, wheel_r,
            *vehicle_.tuning, wd.front
        );

        // Parâmetros de suspensão (spring-damper)
        wi.m_suspensionStiffness       = 20.f;  // rigidez da mola
        wi.m_wheelsDampingRelaxation   = 2.3f;  // amortecimento ao relaxar
        wi.m_wheelsDampingCompression  = 4.4f;  // amortecimento ao comprimir
        wi.m_frictionSlip              = 1000.f; // fricção do pneu (alta = boa tração)
        wi.m_rollInfluence             = 0.1f;  // transferência de peso lateral

        // Forma visual (círculo SFML)
        sf::CircleShape cs_shape(wheel_r * SCALE);
        cs_shape.setOrigin(wheel_r * SCALE, wheel_r * SCALE);
        cs_shape.setFillColor(C_WHEEL);
        cs_shape.setOutlineThickness(2.f);
        cs_shape.setOutlineColor(sf::Color(60,60,60));
        vehicle_.wheel_shapes.push_back(cs_shape);
    }

    has_vehicle_ = true;
    fmt::println("Veículo criado em ({:.1f}, {:.1f})", cx, cy);
}

// ============================================================
// Spawn
// ============================================================
void Simulation::spawn_sphere(float sx, float sy) {
    auto bv = to_bullet_v(sx, sy);
    auto* sh = new_sphere(0.28f, physics_);
    auto* rb = physics_.add_rigid(sh, 1.f, make_t(bv.x, bv.y), 0.5f, 0.4f);

    float r = 0.28f * SCALE;
    auto c = std::make_unique<sf::CircleShape>(r);
    c->setOrigin(r, r);
    sf::Color col = C_SPHERE[n_spheres_ % 5];
    c->setFillColor(col);
    c->setOutlineThickness(1.f);
    c->setOutlineColor(sf::Color(255,255,255,50));

    entities_.push_back({rb, c.get(), col});
    circles_.push_back(std::move(c));
    n_spheres_++;
}

void Simulation::spawn_box(float sx, float sy) {
    auto bv = to_bullet_v(sx, sy);
    auto* sh = new_box(0.24f, 0.24f, 0.24f, physics_);
    auto* rb = physics_.add_rigid(sh, 1.5f, make_t(bv.x, bv.y), 0.3f, 0.6f);

    float side = 0.48f * SCALE;
    auto rect = std::make_unique<sf::RectangleShape>(sf::Vector2f(side, side));
    rect->setOrigin(side/2.f, side/2.f);
    sf::Color col = C_BOX[n_boxes_ % 5];
    rect->setFillColor(col);
    rect->setOutlineThickness(1.f);
    rect->setOutlineColor(sf::Color(255,255,255,50));

    entities_.push_back({rb, rect.get(), col});
    rects_.push_back(std::move(rect));
    n_boxes_++;
}

void Simulation::spawn_ragdoll(float sx, float sy) {
    auto bv = to_bullet_v(sx, sy);
    Ragdoll rd;
    rd.build(physics_, bv.x, bv.y + 1.5f);
    ragdolls_.push_back(rd);
    n_ragdolls_++;
    fmt::println("ragdoll #{} criado", n_ragdolls_);
}

// ============================================================
// Explosão radial
// ============================================================
void Simulation::explode(float sx, float sy) {
    auto bv    = to_bullet_v(sx, sy);
    btVector3 origin(bv.x, bv.y, 0.f);

    const float RADIUS   = 4.f;   // raio da explosão em metros
    const float STRENGTH = 25.f;  // força do impulso

    // Itera todos os rigid bodies no mundo
    int n = physics_.world()->getNumCollisionObjects();
    for (int i = 0; i < n; ++i) {
        btCollisionObject* obj = physics_.world()->getCollisionObjectArray()[i];
        btRigidBody* rb = btRigidBody::upcast(obj);
        if (!rb || rb->getMass() == 0.f) continue;  // ignora estáticos

        btVector3 rel = rb->getCenterOfMassPosition() - origin;
        float dist = rel.length();
        if (dist > RADIUS) continue;

        // Impulso radial com falloff linear
        float scale = STRENGTH * (1.f - dist / RADIUS) / std::max(dist, 0.1f);
        rb->activate(true);
        rb->applyCentralImpulse(rel.normalized() * scale);
        // Torque aleatório para girar os objetos
        rb->applyTorqueImpulse(btVector3(0.f, 0.f, scale * 0.3f));
    }

    // Explode ragdolls
    for (auto& rd : ragdolls_)
        rd.apply_impulse(btVector3(0.f, STRENGTH * 0.5f, 0.f), origin);

    // Flash visual
    flashes_.push_back({sx, sy, 0.4f});
    fmt::println("BOOM em ({:.1f}, {:.1f}) bullet", bv.x, bv.y);
}

// ============================================================
// Ray picking (arrastar objetos com o mouse)
// ============================================================
static btVector3 screen_to_ray_from(float sx, float sy) {
    auto bv = to_bullet_v(sx, sy);
    return btVector3(bv.x, bv.y, 1.f);
}
static btVector3 screen_to_ray_to(float sx, float sy) {
    auto bv = to_bullet_v(sx, sy);
    return btVector3(bv.x, bv.y, -1.f);
}

// ============================================================
// Events
// ============================================================
void Simulation::handle_events() {
    sf::Event ev;
    while (window_.pollEvent(ev)) {
        if (ev.type == sf::Event::Closed)
            window_.close();

        // ── Teclado ──────────────────────────────────────────
        if (ev.type == sf::Event::KeyPressed) {
            switch (ev.key.code) {
            case sf::Keyboard::Escape: window_.close(); break;
            case sf::Keyboard::P: paused_ = !paused_; break;
            case sf::Keyboard::R: {
                // Reset completo
                physics_  = PhysicsWorld{};
                entities_.clear(); circles_.clear(); rects_.clear();
                ragdolls_.clear(); ropes_.clear();
                n_spheres_ = n_boxes_ = n_ragdolls_ = 0;
                has_vehicle_ = false; picked_body_ = nullptr;
                build_scene();
                build_vehicle(-1.5f, 0.6f);
                Rope rope; rope.build(physics_, 2.f, 5.5f, 2.f, 2.0f, 20);
                ropes_.push_back(rope);
                Ragdoll rd; rd.build(physics_, 0.f, 3.5f);
                ragdolls_.push_back(rd); n_ragdolls_++;
                fmt::println("cena resetada");
                break;
            }
            default: break;
            }
        }

        // ── Mouse: clicar ────────────────────────────────────
        if (ev.type == sf::Event::MouseButtonPressed) {
            float mx = static_cast<float>(ev.mouseButton.x);
            float my = static_cast<float>(ev.mouseButton.y);

            if (ev.mouseButton.button == sf::Mouse::Left) {
                // Tenta fazer picking — ray cast no mundo
                btVector3 from = screen_to_ray_from(mx, my);
                btVector3 to   = screen_to_ray_to  (mx, my);
                btCollisionWorld::ClosestRayResultCallback cb(from, to);
                physics_.world()->rayTest(from, to, cb);

                if (cb.hasHit()) {
                    btRigidBody* rb = btRigidBody::upcast(
                        const_cast<btCollisionObject*>(cb.m_collisionObject));
                    if (rb && rb->getMass() > 0.f) {
                        picked_body_ = rb;
                        rb->activate(true);
                        rb->setDamping(0.8f, 0.8f); // amortece para não oscilar

                        // btPoint2PointConstraint: pivô fixo no espaço local do body
                        // É como segurar um objeto por um ponto com um fio rígido
                        btVector3 local = rb->getCenterOfMassTransform().inverse()
                            * cb.m_hitPointWorld;
                        auto* c = new btPoint2PointConstraint(*rb, local);
                        c->m_setting.m_impulseClamp = 30.f; // limita força máxima
                        c->m_setting.m_tau          = 0.001f; // resposta imediata
                        physics_.world()->addConstraint(c);
                        pick_constraint_ = c;
                        picked_dist_ = (cb.m_hitPointWorld - from).length();
                    }
                }
            }

            if (ev.mouseButton.button == sf::Mouse::Right)
                spawn_sphere(mx, my);
            if (ev.mouseButton.button == sf::Mouse::Middle)
                spawn_box(mx, my);
        }

        // ── Mouse: mover (arrastar) ───────────────────────────
        if (ev.type == sf::Event::MouseMoved && pick_constraint_) {
            float mx = static_cast<float>(ev.mouseMove.x);
            float my = static_cast<float>(ev.mouseMove.y);
            auto bv = to_bullet_v(mx, my);
            // Atualiza o pivot B (espaço mundial) da constraint
            pick_constraint_->setPivotB(btVector3(bv.x, bv.y, 0.f));
        }

        // ── Mouse: soltar ─────────────────────────────────────
        if (ev.type == sf::Event::MouseButtonReleased) {
            if (ev.mouseButton.button == sf::Mouse::Left && pick_constraint_) {
                physics_.world()->removeConstraint(pick_constraint_);
                delete pick_constraint_;
                pick_constraint_ = nullptr;
                if (picked_body_) {
                    picked_body_->setDamping(0.05f, 0.05f);
                    picked_body_ = nullptr;
                }
            }
        }

        // ── Scroll: explosão ──────────────────────────────────
        if (ev.type == sf::Event::MouseWheelScrolled) {
            explode(static_cast<float>(ev.mouseWheelScroll.x),
                    static_cast<float>(ev.mouseWheelScroll.y));
        }
    }

    // ── Veículo: teclas contínuas ─────────────────────────────
    if (has_vehicle_) {
        engine_force_ = 0.f;
        steering_     = 0.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))    engine_force_ = -15.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))  engine_force_ =  15.f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))  steering_     =  0.4f;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) steering_     = -0.4f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::E))
            spawn_ragdoll(ORIGIN_X, ORIGIN_Y - 3.f * SCALE);
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::B)) {
            auto mp = sf::Mouse::getPosition(window_);
            explode(static_cast<float>(mp.x), static_cast<float>(mp.y));
        }
    }
}

// ============================================================
// Update
// ============================================================
void Simulation::update(float dt) {
    if (paused_) return;

    // Aplica controles do veículo
    if (has_vehicle_) {
        auto& v = *vehicle_.vehicle;
        // Rodas traseiras (índices 2 e 3) recebem tração
        v.applyEngineForce(engine_force_, 2);
        v.applyEngineForce(engine_force_, 3);
        // Rodas dianteiras (0 e 1) recebem steering
        v.setSteeringValue(steering_, 0);
        v.setSteeringValue(steering_, 1);
        // Freio de mão nas rodas dianteiras quando sem input
        float brake = (engine_force_ == 0.f) ? 0.5f : 0.f;
        v.setBrake(brake, 0);
        v.setBrake(brake, 1);
    }

    float clamped = std::min(dt, 0.05f);
    physics_.step(clamped);

    // Atualiza flashes
    for (auto& f : flashes_) f.ttl -= dt;
    flashes_.erase(std::remove_if(flashes_.begin(), flashes_.end(),
        [](const Flash& f){ return f.ttl <= 0.f; }), flashes_.end());
}

// ============================================================
// Sync: Bullet → SFML
// ============================================================
void Simulation::sync_entities() {
    for (auto& e : entities_) {
        btTransform t;
        e.body->getMotionState()->getWorldTransform(t);
        float bx = t.getOrigin().getX();
        float by = t.getOrigin().getY();
        e.visual->setPosition(to_sfml(bx, by));

        btScalar angle = t.getRotation().getAngle();
        btVector3 axis = t.getRotation().getAxis();
        float deg = static_cast<float>(angle) * (180.f / 3.14159f);
        if (axis.getZ() < 0.f) deg = -deg;
        e.visual->setRotation(-deg);
    }
}

void Simulation::sync_vehicle() {
    if (!has_vehicle_) return;
    auto& v = *vehicle_.vehicle;
    v.updateVehicle(1.f / 60.f); // força sync das rodas

    for (int i = 0; i < v.getNumWheels(); ++i) {
        const btWheelInfo& wi = v.getWheelInfo(i);
        btTransform wt = wi.m_worldTransform;
        float bx = wt.getOrigin().getX();
        float by = wt.getOrigin().getY();
        vehicle_.wheel_shapes[i].setPosition(to_sfml(bx, by));

        btScalar a = wt.getRotation().getAngle();
        btVector3 ax = wt.getRotation().getAxis();
        float deg = static_cast<float>(a) * (180.f / 3.14159f);
        if (ax.getZ() < 0.f) deg = -deg;
        vehicle_.wheel_shapes[i].setRotation(-deg);
    }
}

// ============================================================
// Desenha soft bodies (cordas)
// ============================================================
void Simulation::draw_soft_bodies() {
    for (auto& rope : ropes_) {
        if (!rope.body) continue;
        const auto& nodes = rope.body->m_nodes;
        if (nodes.size() < 2) continue;

        sf::VertexArray va(sf::LineStrip, nodes.size());
        for (int i = 0; i < (int)nodes.size(); ++i) {
            float bx = nodes[i].m_x.getX();
            float by = nodes[i].m_x.getY();
            auto sp = to_sfml(bx, by);
            va[i].position = sp;
            // Gradiente de cor ao longo da corda
            float t = static_cast<float>(i) / (nodes.size() - 1);
            va[i].color = sf::Color(
                static_cast<sf::Uint8>(200 + 55*t),
                static_cast<sf::Uint8>(180 - 80*t),
                static_cast<sf::Uint8>(60),
                220
            );
        }
        window_.draw(va);

        // Pontos nos nós
        for (int i = 0; i < (int)nodes.size(); ++i) {
            float bx = nodes[i].m_x.getX();
            float by = nodes[i].m_x.getY();
            sf::CircleShape dot(2.5f);
            dot.setOrigin(2.5f, 2.5f);
            dot.setFillColor(sf::Color(255,200,80,180));
            dot.setPosition(to_sfml(bx, by));
            window_.draw(dot);
        }
    }
}

// ============================================================
// Desenha ragdolls (cápsulas simplificadas como retângulos)
// ============================================================
void Simulation::draw_ragdolls() {
    // Dimensões visuais de cada segmento (pixels)
    struct SegDef { float w; float h; sf::Color color; };
    static const SegDef DEFS[Ragdoll::COUNT] = {
        {34.f, 55.f, {180,120, 80}},  // TORSO
        {30.f, 30.f, {220,180,130}},  // HEAD (círculo)
        {12.f, 38.f, {160,100, 70}},  // ARM_L
        {12.f, 38.f, {160,100, 70}},  // ARM_R
        {15.f, 42.f, {140, 90, 60}},  // LEG_L
        {15.f, 42.f, {140, 90, 60}},  // LEG_R
    };

    for (auto& rd : ragdolls_) {
        for (int i = 0; i < Ragdoll::COUNT; ++i) {
            btRigidBody* rb = rd.bodies[i];
            if (!rb) continue;

            btTransform t;
            rb->getMotionState()->getWorldTransform(t);
            float bx = t.getOrigin().getX();
            float by = t.getOrigin().getY();
            auto sp = to_sfml(bx, by);

            btScalar angle = t.getRotation().getAngle();
            btVector3 axis = t.getRotation().getAxis();
            float deg = static_cast<float>(angle) * (180.f / 3.14159f);
            if (axis.getZ() < 0.f) deg = -deg;

            const auto& def = DEFS[i];

            if (i == Ragdoll::HEAD) {
                sf::CircleShape cs(def.w / 2.f);
                cs.setOrigin(def.w/2.f, def.w/2.f);
                cs.setFillColor(def.color);
                cs.setOutlineThickness(1.5f);
                cs.setOutlineColor(sf::Color(255,255,255,40));
                cs.setPosition(sp);
                window_.draw(cs);
            } else {
                sf::RectangleShape rs({def.w, def.h});
                rs.setOrigin(def.w/2.f, def.h/2.f);
                rs.setFillColor(def.color);
                rs.setOutlineThickness(1.5f);
                rs.setOutlineColor(sf::Color(255,255,255,40));
                rs.setPosition(sp);
                rs.setRotation(-deg);
                window_.draw(rs);
            }
        }
    }
}

// ============================================================
// Flash de explosão
// ============================================================
void Simulation::draw_explosion_flash(float sx, float sy) {
    // Círculo que expande e some
    static float timer = 0.f; (void)timer;
}

// ============================================================
// HUD
// ============================================================
void Simulation::draw_hud() {
    // Painel fundo
    sf::RectangleShape panel({230.f, 185.f});
    panel.setPosition(8.f, 8.f);
    panel.setFillColor(sf::Color(0,0,0,150));
    panel.setOutlineThickness(1.f);
    panel.setOutlineColor(sf::Color(255,255,255,30));
    window_.draw(panel);

    if (!font_loaded_) return;
    auto txt = [&](const std::string& s, float x, float y,
                   sf::Color c={210,210,210}, unsigned sz=12) {
        sf::Text t(s, font_, sz);
        t.setFillColor(c); t.setPosition(x, y);
        window_.draw(t);
    };

    txt("BULLET3 FÍSICA AVANÇADA", 16, 14, {120,200,255}, 13);
    txt("Clique esq: arrastar obj", 16, 34);
    txt("Clique dir: esfera",       16, 52);
    txt("Clique meio: caixa",       16, 70);
    txt("Scroll: EXPLOSÃO",         16, 88, {255,120,80});
    txt("Setas: veículo",           16, 106);
    txt("E: novo ragdoll",          16, 124);
    txt("B: explosão no cursor",    16, 142, {255,120,80});
    txt("P: pausar  R: reset",      16, 160);

    // Contadores
    sf::RectangleShape p2({190.f, 80.f});
    p2.setPosition(602.f, 8.f);
    p2.setFillColor(sf::Color(0,0,0,150));
    p2.setOutlineThickness(1.f);
    p2.setOutlineColor(sf::Color(255,255,255,30));
    window_.draw(p2);

    txt(fmt::format("Esferas : {}", n_spheres_), 610, 14);
    txt(fmt::format("Caixas  : {}", n_boxes_),   610, 32);
    txt(fmt::format("Ragdolls: {}", n_ragdolls_),610, 50);
    txt(fmt::format("Cordas  : {}", (int)ropes_.size()), 610, 68);

    if (paused_) {
        sf::Text pt("PAUSADO", font_, 30);
        pt.setFillColor(sf::Color(255,200,80,230));
        pt.setPosition(310, 275);
        window_.draw(pt);
    }
}

// ============================================================
// Render
// ============================================================
void Simulation::render() {
    window_.clear(sf::Color(22, 22, 32));

    // Chão
    sf::RectangleShape ground({800.f, 24.f});
    ground.setPosition(0.f, ORIGIN_Y - 0.f * SCALE);
    ground.setFillColor(sf::Color(60,62,75));
    window_.draw(ground);

    // Paredes
    sf::RectangleShape wl({12.f, 600.f}); wl.setPosition(0.f,0.f);
    wl.setFillColor(sf::Color(50,52,65)); window_.draw(wl);
    sf::RectangleShape wr({12.f, 600.f}); wr.setPosition(788.f,0.f);
    wr.setFillColor(sf::Color(50,52,65)); window_.draw(wr);

    // Entidades genéricas
    sync_entities();
    for (auto& e : entities_) window_.draw(*e.visual);

    // Veículo
    sync_vehicle();
    if (has_vehicle_) {
        // Chassis
        btTransform ct;
        vehicle_.chassis->getMotionState()->getWorldTransform(ct);
        float bx = ct.getOrigin().getX();
        float by = ct.getOrigin().getY();
        sf::RectangleShape chassis_rect({112.f, 32.f});
        chassis_rect.setOrigin(56.f, 16.f);
        chassis_rect.setFillColor(C_CHASSIS);
        chassis_rect.setOutlineThickness(2.f);
        chassis_rect.setOutlineColor(sf::Color(220,100,100));
        chassis_rect.setPosition(to_sfml(bx, by));
        btScalar a = ct.getRotation().getAngle();
        btVector3 ax = ct.getRotation().getAxis();
        float deg = static_cast<float>(a) * (180.f/3.14159f);
        if (ax.getZ() < 0.f) deg = -deg;
        chassis_rect.setRotation(-deg);
        window_.draw(chassis_rect);

        // Rodas
        for (auto& ws : vehicle_.wheel_shapes) window_.draw(ws);
    }

    // Ragdolls
    draw_ragdolls();

    // Cordas (por cima de tudo)
    draw_soft_bodies();

    // Flashes de explosão
    for (auto& f : flashes_) {
        float r = (0.4f - f.ttl) / 0.4f;   // 0→1 ao longo da vida
        float radius = 20.f + r * 120.f;
        float alpha  = (1.f - r) * 255.f;
        sf::CircleShape blast(radius);
        blast.setOrigin(radius, radius);
        blast.setPosition(f.x, f.y);
        blast.setFillColor(sf::Color(255, 160, 40,
            static_cast<sf::Uint8>(alpha * 0.3f)));
        blast.setOutlineThickness(2.f);
        blast.setOutlineColor(sf::Color(255, 220, 100,
            static_cast<sf::Uint8>(alpha)));
        window_.draw(blast);
    }

    // Indicador de picking
    if (pick_constraint_) {
        btVector3 piv = pick_constraint_->getPivotInB();
        auto sp = to_sfml(piv.getX(), piv.getY());
        sf::CircleShape dot(5.f); dot.setOrigin(5.f, 5.f);
        dot.setFillColor(sf::Color(255, 255, 100, 200));
        dot.setPosition(sp);
        window_.draw(dot);
    }

    draw_hud();
    window_.display();
}

// ============================================================
// Loop principal
// ============================================================
void Simulation::run() {
    fmt::println("=== Bullet3 Física Avançada ===");
    fmt::println("  Clique esq        : arrastar objetos");
    fmt::println("  Clique dir        : spawn esfera");
    fmt::println("  Clique meio       : spawn caixa");
    fmt::println("  Scroll do mouse   : EXPLOSÃO");
    fmt::println("  Setas             : controle do veículo");
    fmt::println("  E                 : spawnar ragdoll");
    fmt::println("  B                 : explosão no cursor");
    fmt::println("  P                 : pausar/retomar");
    fmt::println("  R                 : resetar cena");
    fmt::println("  Esc               : sair");

    clock_.restart();
    while (window_.isOpen()) {
        float dt = clock_.restart().asSeconds();
        handle_events();
        update(dt);
        render();
    }
}

} // namespace sim
