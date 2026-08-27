#pragma once
#include "physics_world.hpp"

// ============================================================
// Rope
//
// btSoftBody como corda:
//   - Cadeia de N nós ligados por links lineares
//   - btSoftBodyHelpers::CreateRope cria a topologia
//   - Nó 0 fixado ao mundo via setMass(0, 0) — ancora
//   - Parâmetros de material: kLST (stiffness), kDP (damping)
//
// A corda pode interagir com rigid bodies:
//   appendAnchor(node, rigidBody) conecta um nó a um body,
//   fazendo-o arrastar a corda junto ao se mover.
// ============================================================

namespace sim {

struct Rope {
    btSoftBody* body = nullptr;

    // Cria corda de (x0,y0) até (x1,y1) com `segments` subdivisões
    // O nó 0 fica ancorado; o último nó fica livre (ou ancorado a `attach`)
    void build(PhysicsWorld& pw,
               float x0, float y0,
               float x1, float y1,
               int segments,
               btRigidBody* attach = nullptr);
};

} // namespace sim
