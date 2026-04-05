#include "physics_world.hpp"

namespace sim {

// ============================================================
// ESCALA
//
// Bullet trabalha bem com unidades em metros.
// Nossa tela tem ~800px de largura, então mapeamos:
//   1 metro Bullet = SCALE pixels SFML
//
// Na prática: convertemos posição Bullet → pixel antes de desenhar.
// ============================================================

PhysicsWorld::PhysicsWorld() {
    config_     = std::make_unique<btDefaultCollisionConfiguration>();
    dispatcher_ = std::make_unique<btCollisionDispatcher>(config_.get());
    broadphase_ = std::make_unique<btDbvtBroadphase>();
    solver_     = std::make_unique<btSequentialImpulseConstraintSolver>();

    world_ = std::make_unique<btDiscreteDynamicsWorld>(
        dispatcher_.get(),
        broadphase_.get(),
        solver_.get(),
        config_.get()
    );

    // gravidade no eixo Y negativo (cai para baixo)
    world_->setGravity(btVector3(0.f, -9.81f, 0.f));
}

PhysicsWorld::~PhysicsWorld() {
    if (!world_) return;  // objeto foi movido — nada a limpar
    for (auto& rb : rigid_bodies_) {
        world_->removeRigidBody(rb.get());
    }
}

void PhysicsWorld::step(float dt) {
    // stepSimulation(timeStep, maxSubSteps, fixedTimeStep)
    //
    // Bullet usa um dt interno fixo (1/60s por padrão) para
    // estabilidade numérica. Se dt > fixedTimeStep, o Bullet
    // subdivide o passo em até maxSubSteps sub-passos.
    //
    // maxSubSteps = 10 dá margem para frames lentos sem explodir.
    world_->stepSimulation(dt, 10, 1.f / 60.f);
}

BodyHandle PhysicsWorld::add_body(const BodyDef& def) {
    // ── 1. Criar shape ────────────────────────────────────────
    //
    // btSphereShape e btBoxShape são shapes convexas.
    // Convexo = algoritmo de colisão mais eficiente (GJK/EPA).
    // O Bullet usa o half-extent internamente para caixas.
    std::unique_ptr<btCollisionShape> shape;

    if (def.type == ShapeType::Sphere) {
        shape = std::make_unique<btSphereShape>(def.half_extent);
    } else {
        // btBoxShape recebe half-extents nos três eixos.
        // Para 2D simulado, Z = half_extent também (cubo).
        shape = std::make_unique<btBoxShape>(
            btVector3(def.half_extent, def.half_extent, def.half_extent)
        );
    }

    // ── 2. Motion state ───────────────────────────────────────
    //
    // btMotionState é a ponte entre Bullet e o render engine.
    // O Bullet chama setWorldTransform() quando o corpo se move,
    // permitindo interpolação visual sem travar a simulação.
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(def.x, def.y, 0.f));

    auto motion = std::make_unique<btDefaultMotionState>(transform);

    // ── 3. Inércia local ──────────────────────────────────────
    //
    // Para massa > 0 (corpo dinâmico), precisamos calcular o
    // tensor de inércia da shape. Para massa = 0, é sempre (0,0,0).
    btVector3 inertia(0.f, 0.f, 0.f);
    if (def.mass > 0.f) {
        shape->calculateLocalInertia(def.mass, inertia);
    }

    // ── 4. Rigid body ─────────────────────────────────────────
    btRigidBody::btRigidBodyConstructionInfo ci(
        def.mass, motion.get(), shape.get(), inertia
    );
    ci.m_restitution = def.restitution;
    ci.m_friction    = def.friction;

    auto rb = std::make_unique<btRigidBody>(ci);

    // Para simulação 2D: congela translação e rotação em Z.
    // Isso evita que os corpos "saiam" do plano XY.
    rb->setLinearFactor(btVector3(1.f, 1.f, 0.f));
    rb->setAngularFactor(btVector3(0.f, 0.f, 1.f));

    // ── 5. Registrar ──────────────────────────────────────────
    BodyHandle handle{
        rb.get(),
        shape.get(),
        def.type,
        def.half_extent
    };

    world_->addRigidBody(rb.get());

    // Transfere ownership para os vetores internos
    shapes_.push_back(std::move(shape));
    motion_states_.push_back(std::move(motion));
    rigid_bodies_.push_back(std::move(rb));

    bodies_.push_back(handle);
    return handle;
}

} // namespace sim