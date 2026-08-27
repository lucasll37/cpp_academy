#pragma once
#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h>
#include <BulletDynamics/Vehicle/btRaycastVehicle.h>
#include <memory>
#include <vector>

// ============================================================
// PhysicsWorld
//
// Usa btSoftRigidDynamicsWorld em vez de btDiscreteDynamicsWorld
// para suportar tanto rigid bodies quanto soft bodies na mesma
// simulação. A diferença principal é a collision configuration:
//
//   btDefaultCollisionConfiguration         → só rigid
//   btSoftBodyRigidBodyCollisionConfiguration → rigid + soft
//
// O btSoftBodyWorldInfo é um bloco de parâmetros compartilhado
// por todos os btSoftBody — gravity, water density, air density,
// dispatcher e broadphase ficam aqui para que o soft body solver
// possa acessá-los durante a simulação.
// ============================================================

namespace sim {

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(PhysicsWorld&&)            = default;
    PhysicsWorld& operator=(PhysicsWorld&&) = default;

    void step(float dt);

    // Acesso direto ao mundo (necessário para Vehicle tuner e SoftBody)
    btSoftRigidDynamicsWorld* world()    { return world_.get(); }
    btSoftBodyWorldInfo&      soft_info(){ return soft_info_; }

    // Factory helpers — criam corpos e os adicionam ao mundo
    btRigidBody* add_rigid(btCollisionShape* shape,
                           btScalar mass,
                           const btTransform& t,
                           float restitution = 0.3f,
                           float friction    = 0.5f);

    btSoftBody* add_soft(btSoftBody* sb);

    // Ownership de shapes e bodies (Bullet não gerencia)
    void take_shape(std::unique_ptr<btCollisionShape> s)  { shapes_.push_back(std::move(s)); }
    void take_body (std::unique_ptr<btRigidBody> b)       { bodies_.push_back(std::move(b)); }
    void take_state(std::unique_ptr<btDefaultMotionState> s){ states_.push_back(std::move(s)); }

private:
    std::unique_ptr<btSoftBodyRigidBodyCollisionConfiguration> config_;
    std::unique_ptr<btCollisionDispatcher>                     dispatcher_;
    std::unique_ptr<btDbvtBroadphase>                          broadphase_;
    std::unique_ptr<btSequentialImpulseConstraintSolver>       solver_;
    std::unique_ptr<btSoftRigidDynamicsWorld>                  world_;
    btSoftBodyWorldInfo                                        soft_info_;

    std::vector<std::unique_ptr<btCollisionShape>>     shapes_;
    std::vector<std::unique_ptr<btRigidBody>>          bodies_;
    std::vector<std::unique_ptr<btDefaultMotionState>> states_;
};

} // namespace sim
