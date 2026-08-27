#include "physics_world.hpp"
#include <cstdlib>   // rand
#include <cmath>     // std::sqrt

// ─────────────────────────────────────────────────────────────────────────────
// PhysicsWorld — implementação
//
// Detalhes do pipeline Bullet3:
//
//  btDefaultCollisionConfiguration
//      configura os algoritmos de colisão padrão (GJK/EPA, SAT).
//
//  btCollisionDispatcher
//      despacha pares de colisão para o algoritmo correto com base
//      nos tipos de shape (Box×Sphere, Sphere×Sphere, etc.).
//
//  btDbvtBroadphase
//      usa uma Bounding Volume Hierarchy dinâmica (DBVT) para
//      descartar rapidamente pares que não podem colidir (fase larga).
//      Alternativa: btAxisSweep3 (boa para mundos grandes e estáticos).
//
//  btSequentialImpulseConstraintSolver
//      resolve constraints e contatos com o método de impulso sequencial
//      (SI/PGS). É o solver padrão e mais rápido para jogos.
//
//  btDiscreteDynamicsWorld
//      combina tudo: integra a equação de movimento (Euler semi-implícito),
//      chama o broadphase, o dispatcher e o solver a cada step.
// ─────────────────────────────────────────────────────────────────────────────

PhysicsWorld::PhysicsWorld(float half_room)
    : half_room_(half_room)
    , config_     (std::make_unique<btDefaultCollisionConfiguration>())
    , dispatcher_ (std::make_unique<btCollisionDispatcher>(config_.get()))
    , broadphase_ (std::make_unique<btDbvtBroadphase>())
    , solver_     (std::make_unique<btSequentialImpulseConstraintSolver>())
    , world_      (std::make_unique<btDiscreteDynamicsWorld>(
                       dispatcher_.get(),
                       broadphase_.get(),
                       solver_.get(),
                       config_.get()))
{
    world_->setGravity(btVector3(0.f, -9.82f, 0.f));
    build_room();
}

// ─── step ────────────────────────────────────────────────────────────────────
// stepSimulation(dt, maxSubSteps, fixedTimeStep)
//   dt            : tempo real decorrido desde o último frame
//   maxSubSteps   : limite de sub-steps internos por frame
//                   (evita "spiral of death" em frames lentos)
//   fixedTimeStep : tempo interno de cada sub-step (1/60 s)
//
// O Bullet subdivide dt em passos de fixedTimeStep para estabilidade
// numérica. Se dt > maxSubSteps * fixedTimeStep, o excesso é descartado.
void PhysicsWorld::step(float dt) {
    world_->stepSimulation(dt, 10, 1.f / 60.f);
}

// ─── build_room ──────────────────────────────────────────────────────────────
// Seis planos estáticos definem o quarto. btStaticPlaneShape é infinito —
// não tem volume, não tem massa, nunca se move. É a forma mais eficiente
// para superfícies de chão/parede em Bullet.
void PhysicsWorld::build_room() {
    const float h = half_room_;
    // normal, constante (deslocamento ao longo da normal)
    add_static_plane({  0.f,  1.f,  0.f }, -h);  // chão
    add_static_plane({  0.f, -1.f,  0.f }, -h);  // teto
    add_static_plane({  1.f,  0.f,  0.f }, -h);  // parede esquerda
    add_static_plane({ -1.f,  0.f,  0.f }, -h);  // parede direita
    add_static_plane({  0.f,  0.f,  1.f }, -h);  // parede frontal
    add_static_plane({  0.f,  0.f, -1.f }, -h);  // parede traseira
}

void PhysicsWorld::add_static_plane(const btVector3& normal, float constant) {
    auto shape = std::make_unique<btStaticPlaneShape>(normal, constant);
    auto ms    = std::make_unique<btDefaultMotionState>();

    btRigidBody::btRigidBodyConstructionInfo ci(
        0.f,       // massa 0 → objeto estático
        ms.get(),
        shape.get()
    );
    ci.m_restitution = 0.85f;
    ci.m_friction    = 0.4f;

    auto body = std::make_unique<btRigidBody>(ci);
    world_->addRigidBody(body.get());

    wall_shapes_.push_back(std::move(shape));
    wall_states_.push_back(std::move(ms));
    wall_bodies_.push_back(std::move(body));
}

// ─── add_object ──────────────────────────────────────────────────────────────
void PhysicsWorld::add_object(ShapeType type, float half_extent,
                               const btVector3& position,
                               const btVector3& initial_velocity,
                               sf::Color color)
{
    auto obj = std::make_unique<PhysicsObject>();
    obj->type        = type;
    obj->color       = color;
    obj->half_extent = half_extent;

    // ── Shape ────────────────────────────────────────────────────────────────
    // btBoxShape aceita half-extents (metade de cada dimensão)
    // btSphereShape aceita o raio
    // btCylinderShape aceita half-extents (raio, meia altura, raio)
    switch (type) {
        case ShapeType::Box:
            obj->shape = std::make_unique<btBoxShape>(
                btVector3(half_extent, half_extent, half_extent));
            break;
        case ShapeType::Sphere:
            obj->shape = std::make_unique<btSphereShape>(half_extent);
            break;
        case ShapeType::Cylinder:
            obj->shape = std::make_unique<btCylinderShape>(
                btVector3(half_extent, half_extent * 1.6f, half_extent));
            break;
    }

    // ── Inércia ──────────────────────────────────────────────────────────────
    // O Bullet calcula o tensor de inércia automaticamente a partir da shape.
    // Para objetos dinâmicos (massa > 0) é obrigatório fornecer a inércia.
    const float mass = 1.f;
    btVector3 inertia(0.f, 0.f, 0.f);
    obj->shape->calculateLocalInertia(mass, inertia);

    // ── MotionState ──────────────────────────────────────────────────────────
    // btDefaultMotionState guarda a transformação (posição + orientação).
    // O Bullet atualiza o MotionState a cada step — nós lemos a posição
    // de lá para renderizar, sem precisar acessar o btRigidBody diretamente.
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(position);
    obj->motion_state = std::make_unique<btDefaultMotionState>(transform);

    btRigidBody::btRigidBodyConstructionInfo ci(
        mass, obj->motion_state.get(), obj->shape.get(), inertia);
    ci.m_restitution    = 0.85f;
    ci.m_friction       = 0.4f;
    ci.m_linearDamping  = 0.01f;
    ci.m_angularDamping = 0.01f;

    obj->body = std::make_unique<btRigidBody>(ci);
    obj->body->setLinearVelocity(initial_velocity);

    // Spin angular aleatório para ficar visualmente interessante
    obj->body->setAngularVelocity(btVector3(
        (float)(rand() % 6 - 3),
        (float)(rand() % 6 - 3),
        (float)(rand() % 6 - 3)));

    world_->addRigidBody(obj->body.get());
    objects_.push_back(std::move(obj));
}

// ─── clear_objects ───────────────────────────────────────────────────────────
void PhysicsWorld::clear_objects() {
    for (auto& obj : objects_)
        world_->removeRigidBody(obj->body.get());
    objects_.clear();
}

// ─── explode ─────────────────────────────────────────────────────────────────
void PhysicsWorld::explode(float force) {
    for (auto& obj : objects_) {
        btVector3 pos = obj->body->getWorldTransform().getOrigin();
        // direção radial a partir do centro
        btVector3 dir = pos.length() > 0.01f ? pos.normalized() : btVector3(0,1,0);
        obj->body->activate(true);
        obj->body->setLinearVelocity(dir * force);
    }
}

// ─── set_gravity ─────────────────────────────────────────────────────────────
void PhysicsWorld::set_gravity(float gy) {
    world_->setGravity(btVector3(0.f, gy, 0.f));
    // reativa todos os corpos para que respondam à nova gravidade imediatamente
    for (auto& obj : objects_)
        obj->body->activate(true);
}
