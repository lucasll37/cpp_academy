#pragma once
#include "physics_world.hpp"
#include <array>

// ============================================================
// Ragdoll
//
// Corpo humano simplificado em 6 segmentos:
//   torso, cabeça, braço_e, braço_d, perna_e, perna_d
//
// Joints efetivamente criados em ragdoll.cpp — 5, todos do mesmo tipo:
//   cabeça-torso  → btConeTwistConstraint  (pescoço)
//   braço_e-torso → btConeTwistConstraint  (ombro esquerdo)
//   braço_d-torso → btConeTwistConstraint  (ombro direito)
//   perna_e-torso → btConeTwistConstraint  (quadril esquerdo)
//   perna_d-torso → btConeTwistConstraint  (quadril direito)
//
// NÃO há btHingeConstraint neste ragdoll: cotovelo e joelho exigiriam
// segmentos de antebraço e canela, que este modelo de 6 partes não tem.
//
// btConeTwistConstraint: junta de 3 graus de liberdade rotacionais.
//   swingSpan1/2: abertura do cone (swing frontal/lateral), em radianos.
//   twistSpan:    rotação ao redor do eixo do osso, em radianos.
//   Bom para ombro, quadril, pescoço.
// ============================================================

namespace sim {

struct Ragdoll {
    // Os 6 segmentos do corpo
    enum Segment { TORSO=0, HEAD, ARM_L, ARM_R, LEG_L, LEG_R, COUNT };

    btRigidBody* bodies[COUNT] = {};

    // Cria o ragdoll centrado em (cx, cy) no mundo
    void build(PhysicsWorld& pw, float cx, float cy);

    // Aplica impulso em todos os segmentos (para explosão)
    void apply_impulse(const btVector3& impulse, const btVector3& origin);
};

} // namespace sim
