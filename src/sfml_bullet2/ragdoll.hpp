#pragma once
#include "physics_world.hpp"
#include <array>

// ============================================================
// Ragdoll
//
// Corpo humano simplificado em 7 segmentos:
//   torso, cabeça, braço_e, braço_d, perna_e, perna_d
//
// Joints:
//   cabeça-torso    → btConeTwistConstraint  (gira em 3 eixos)
//   braço-torso     → btConeTwistConstraint  (ombro: 3 eixos)
//   perna-torso     → btConeTwistConstraint  (quadril: 3 eixos)
//   antebraço-braço → btHingeConstraint      (cotovelo: 1 eixo)
//   canela-coxa     → btHingeConstraint      (joelho:   1 eixo)
//
// btHingeConstraint: junta de 1 grau de liberdade (dobradiça).
//   Parâmetros: dois frames locais em cada corpo, limites em radianos.
//   Bom para joelho, cotovelo, mandíbula.
//
// btConeTwistConstraint: junta de 3 graus.
//   swingSpan1/2: abertura do cone (swing lateral/frontal).
//   twistSpan:    rotação ao redor do eixo do osso.
//   Bom para ombro, quadril, pescoço.
// ============================================================

namespace sim {

struct Ragdoll {
    // Os 7 segmentos do corpo
    enum Segment { TORSO=0, HEAD, ARM_L, ARM_R, LEG_L, LEG_R, COUNT };

    btRigidBody* bodies[COUNT] = {};

    // Cria o ragdoll centrado em (cx, cy) no mundo
    void build(PhysicsWorld& pw, float cx, float cy);

    // Aplica impulso em todos os segmentos (para explosão)
    void apply_impulse(const btVector3& impulse, const btVector3& origin);
};

} // namespace sim
