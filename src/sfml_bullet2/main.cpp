#include "simulation.hpp"

// ============================================================
// SFML + BULLET3 — simulação de física avançada
//
// Recursos do Bullet demonstrados:
//
//   btSoftRigidDynamicsWorld
//     Suporta rigid bodies e soft bodies na mesma cena.
//     Requer btSoftBodyRigidBodyCollisionConfiguration.
//
//   btConeTwistConstraint
//     Joint de 3 graus de liberdade (ombro, quadril, pescoço).
//     Parâmetros: swingSpan1/2 (abertura do cone), twistSpan.
//
//   btHingeConstraint (indiretamente via ConeTwist nos ragdolls)
//     Joint de 1 grau (cotovelo, joelho).
//     Limites: setLimit(lower, upper).
//
//   btRaycastVehicle
//     Veículo com suspensão spring-damper por raycast.
//     Rodas com tração, steering e freio independentes.
//     Parâmetros: suspensionStiffness, dampingRelaxation,
//                 dampingCompression, frictionSlip, rollInfluence.
//
//   btSoftBody (corda via btSoftBodyHelpers::CreateRope)
//     Nós, links, material (kLST, kAST, kDP).
//     Âncora fixa (setMass=0) e âncora em rigid body (appendAnchor).
//     generateBendingConstraints para resistência à curvatura.
//
//   applyCentralImpulse + applyTorqueImpulse
//     Explosão radial com falloff de distância.
//
//   btPoint2PointConstraint
//     Ray picking: arrastar objetos com o mouse em tempo real.
//     setPivotB atualiza o ponto alvo frame a frame.
//
//   setLinearFactor / setAngularFactor
//     Restringe graus de liberdade para simulação 2D.
// ============================================================

int main() {
    sim::Simulation simulation;
    simulation.run();
    return 0;
}
