#include "physics_world.hpp"

namespace sim {

PhysicsWorld::PhysicsWorld() {
    // btSoftBodyRigidBodyCollisionConfiguration registra os algoritmos
    // de colisão para pares soft-rigid e soft-soft além dos rigid-rigid.
    config_     = std::make_unique<btSoftBodyRigidBodyCollisionConfiguration>();
    dispatcher_ = std::make_unique<btCollisionDispatcher>(config_.get());
    broadphase_ = std::make_unique<btDbvtBroadphase>();
    solver_     = std::make_unique<btSequentialImpulseConstraintSolver>();

    world_ = std::make_unique<btSoftRigidDynamicsWorld>(
        dispatcher_.get(),
        broadphase_.get(),
        solver_.get(),
        config_.get()
    );

    world_->setGravity(btVector3(0.f, -9.81f, 0.f));

    // btSoftBodyWorldInfo precisa ser inicializado com as mesmas
    // referências do mundo para que o solver de soft body funcione.
    soft_info_.m_broadphase       = broadphase_.get();
    soft_info_.m_dispatcher       = dispatcher_.get();
    soft_info_.m_gravity          = btVector3(0.f, -9.81f, 0.f);
    soft_info_.m_sparsesdf.Initialize();
}

PhysicsWorld::~PhysicsWorld() {
    if (!world_) return;

    // Remove constraints antes dos bodies
    for (int i = world_->getNumConstraints() - 1; i >= 0; --i)
        world_->removeConstraint(world_->getConstraint(i));

    // Remove soft bodies
    auto& sbs = world_->getSoftBodyArray();
    for (int i = sbs.size() - 1; i >= 0; --i)
        world_->removeSoftBody(sbs[i]);

    // Remove rigid bodies
    for (auto& rb : bodies_)
        world_->removeRigidBody(rb.get());
}

void PhysicsWorld::step(float dt) {
    world_->stepSimulation(dt, 10, 1.f / 60.f);
}

btRigidBody* PhysicsWorld::add_rigid(btCollisionShape* shape,
                                     btScalar mass,
                                     const btTransform& t,
                                     float restitution,
                                     float friction) {
    btVector3 inertia(0.f, 0.f, 0.f);
    if (mass > 0.f) shape->calculateLocalInertia(mass, inertia);

    auto state = std::make_unique<btDefaultMotionState>(t);
    btRigidBody::btRigidBodyConstructionInfo ci(mass, state.get(), shape, inertia);
    ci.m_restitution = restitution;
    ci.m_friction    = friction;

    auto rb = std::make_unique<btRigidBody>(ci);
    btRigidBody* ptr = rb.get();

    // Restringe ao plano 2D (XY): trava translação Z e rotação X/Y
    rb->setLinearFactor (btVector3(1.f, 1.f, 0.f));
    rb->setAngularFactor(btVector3(0.f, 0.f, 1.f));

    world_->addRigidBody(rb.get());

    states_.push_back(std::move(state));
    bodies_.push_back(std::move(rb));
    return ptr;
}

btSoftBody* PhysicsWorld::add_soft(btSoftBody* sb) {
    world_->addSoftBody(sb);
    return sb;
}

} // namespace sim
