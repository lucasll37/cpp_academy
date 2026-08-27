#include "simulation.hpp"

// ============================================================
// SFML + BULLET3 — simulação de física 2D interativa
//
// Conceitos demonstrados:
//
//   Bullet3:
//     - Pipeline completo do mundo físico
//     - Corpos estáticos (chão, paredes) e dinâmicos (esferas, caixas)
//     - btSphereShape e btBoxShape (shapes convexas)
//     - Restrição de graus de liberdade para simulação 2D
//       (setLinearFactor / setAngularFactor)
//     - Restitution (elasticidade) e friction por corpo
//     - Gerenciamento manual de ownership (Bullet não usa smart pointers)
//
//   SFML:
//     - Janela, event loop, frame rate cap
//     - sf::CircleShape e sf::RectangleShape como proxies visuais
//     - Conversão de coordenadas: Y invertido SFML ↔ Bullet
//     - Sincronização posição + rotação a cada frame
//
//   Integração:
//     - getWorldTransform via MotionState para ler posição do Bullet
//     - btQuaternion → ângulo de rotação para SFML
//     - Escala métrica: 1 metro Bullet = 80 pixels SFML
//     - Delta-time variável com clamp para evitar instabilidade
// ============================================================

int main() {
    sim::Simulation simulation;
    simulation.run();
    return 0;
}
