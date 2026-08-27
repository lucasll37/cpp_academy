#pragma once
#include "physics_world.hpp"
#include "ragdoll.hpp"
#include "rope.hpp"
#include <SFML/Graphics.hpp>
#include <BulletDynamics/Vehicle/btRaycastVehicle.h>
#include <vector>
#include <memory>

// ============================================================
// Simulation
//
// Orquestra todos os subsistemas:
//   - PhysicsWorld  (btSoftRigidDynamicsWorld)
//   - Ragdoll       (constraints articuladas)
//   - Rope          (btSoftBody)
//   - btRaycastVehicle
//   - Explosões radiais via applyCentralImpulse
//   - Ray picking   (clique+arrasto move objetos)
//   - Spawning de esferas, caixas e ragdolls
//
// COORDENADAS:
//   SCALE = 80px/m
//   Origem Bullet → (ORIGIN_X, ORIGIN_Y) em pixels SFML
//   Y invertido: sfml_y = -bullet_y * SCALE + ORIGIN_Y
// ============================================================

namespace sim {

constexpr float SCALE    = 80.f;
constexpr float ORIGIN_X = 400.f;
constexpr float ORIGIN_Y = 540.f;

inline sf::Vector2f to_sfml(float bx, float by) {
    return { bx * SCALE + ORIGIN_X, -by * SCALE + ORIGIN_Y };
}
inline sf::Vector2f to_bullet_v(float sx, float sy) {
    return { (sx - ORIGIN_X) / SCALE, -(sy - ORIGIN_Y) / SCALE };
}

// Par corpo físico ↔ forma visual SFML
struct Entity {
    btRigidBody*    body;
    sf::Shape*      visual;
    sf::Color       color;
};

struct VehicleEntity {
    std::unique_ptr<btRaycastVehicle::btVehicleTuning> tuning;
    std::unique_ptr<btDefaultVehicleRaycaster>          raycaster;
    std::unique_ptr<btRaycastVehicle>                   vehicle;
    btRigidBody*                                        chassis = nullptr;
    // Shapes das rodas para render
    std::vector<sf::CircleShape>                        wheel_shapes;
};

class Simulation {
public:
    Simulation();
    void run();

private:
    void build_scene();
    void build_vehicle(float cx, float cy);
    void spawn_sphere(float sx, float sy);
    void spawn_box(float sx, float sy);
    void spawn_ragdoll(float sx, float sy);
    void explode(float sx, float sy);

    void handle_events();
    void update(float dt);
    void render();

    void sync_entities();
    void sync_vehicle();
    void draw_soft_bodies();
    void draw_ragdolls();
    void draw_hud();
    void draw_explosion_flash(float sx, float sy);

    sf::RenderWindow  window_;
    sf::Font          font_;
    bool              font_loaded_ = false;

    PhysicsWorld      physics_;

    // Corpos dinâmicos genéricos
    std::vector<Entity>                           entities_;
    std::vector<std::unique_ptr<sf::CircleShape>> circles_;
    std::vector<std::unique_ptr<sf::RectangleShape>> rects_;

    // Ragdolls
    std::vector<Ragdoll> ragdolls_;

    // Cordas
    std::vector<Rope> ropes_;

    // Veículo
    VehicleEntity vehicle_;
    bool has_vehicle_ = false;

    // Ray picking: corpo sendo arrastado
    btRigidBody*      picked_body_  = nullptr;
    btPoint2PointConstraint* pick_constraint_ = nullptr;
    float             picked_dist_  = 0.f;

    // Estado
    sf::Clock clock_;
    bool      paused_ = false;
    int       n_spheres_ = 0, n_boxes_ = 0, n_ragdolls_ = 0;

    // Flash de explosão (posição + timer)
    struct Flash { float x, y, ttl; };
    std::vector<Flash> flashes_;

    // Acelerador/freio do veículo
    float engine_force_  = 0.f;
    float steering_      = 0.f;
};

} // namespace sim
