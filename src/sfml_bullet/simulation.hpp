#pragma once
#include "physics_world.hpp"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

// ============================================================
// Simulation
//
// Camada de integração entre PhysicsWorld (Bullet) e SFML.
//
// Responsabilidades:
//   - manter a janela SFML
//   - gerenciar o game loop (event → update → render)
//   - converter coordenadas Bullet ↔ SFML
//   - sincronizar formas SFML com o estado do Bullet
//   - spawnar corpos via clique do mouse
//
// COORDENADAS:
//   Bullet: Y sobe ↑    (origem no centro do mundo)
//   SFML:   Y desce ↓   (origem no canto superior esquerdo)
//
//   Para converter:
//     sfml_x =  bullet_x * SCALE + OFFSET_X
//     sfml_y = -bullet_y * SCALE + OFFSET_Y   ← inversão do Y
// ============================================================

namespace sim {

constexpr float SCALE    = 80.f;   // 1 metro Bullet = 80 pixels SFML
constexpr float OFFSET_X = 400.f;  // origem Bullet → centro da janela
constexpr float OFFSET_Y = 550.f;  // chão em y=550px

// Converte posição Bullet → pixel SFML
inline sf::Vector2f to_sfml(float bx, float by) {
    return { bx * SCALE + OFFSET_X, -by * SCALE + OFFSET_Y };
}

// Converte posição pixel SFML → Bullet
inline sf::Vector2f to_bullet(float sx, float sy) {
    return { (sx - OFFSET_X) / SCALE, -(sy - OFFSET_Y) / SCALE };
}

// Par: corpo físico + forma visual
struct Entity {
    BodyHandle   handle;
    sf::Shape*   visual;   // ponteiro não-owning — owned pelos vetores abaixo
};

class Simulation {
public:
    Simulation();
    void run();

private:
    void handle_events();
    void update(float dt);
    void render();

    // Spawna uma esfera ou caixa na posição do mouse
    void spawn_sphere(float sx, float sy);
    void spawn_box   (float sx, float sy);

    // Cria as paredes e o chão estáticos
    void create_boundaries();

    // Sincroniza posição/rotação Bullet → forma SFML
    void sync_entities();

    // Desenha HUD com instrucoes
    void draw_hud();

    sf::RenderWindow window_;
    sf::Font         font_;
    bool             font_loaded_ = false;

    PhysicsWorld physics_;

    // Ownership das formas SFML
    std::vector<std::unique_ptr<sf::CircleShape>> circles_;
    std::vector<std::unique_ptr<sf::RectangleShape>> rects_;

    // Pares corpo+visual (apenas corpos dinâmicos)
    std::vector<Entity> entities_;

    // Estado da simulação
    sf::Clock  clock_;
    int        sphere_count_ = 0;
    int        box_count_    = 0;
    bool       paused_       = false;
};

} // namespace sim
