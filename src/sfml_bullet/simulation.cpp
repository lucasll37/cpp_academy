#include "simulation.hpp"
#include <fmt/core.h>
#include <cmath>

namespace sim {

// Paleta de cores para os objetos
static const sf::Color SPHERE_COLORS[] = {
    {100, 200, 255},  // azul claro
    {255, 160, 80 },  // laranja
    {120, 240, 140},  // verde
    {255, 100, 130},  // rosa
    {220, 180, 255},  // lilás
};
static const sf::Color BOX_COLORS[] = {
    {255, 220,  80},  // amarelo
    {80,  200, 200},  // ciano
    {200,  80, 255},  // roxo
    {255, 140, 140},  // salmão
    {140, 255, 180},  // menta
};
static constexpr int N_COLORS = 5;

// ============================================================
// Construtor
// ============================================================
Simulation::Simulation()
    : window_(sf::VideoMode(800, 600), "Bullet3 + SFML — física 2D",
              sf::Style::Close | sf::Style::Titlebar)
{
    window_.setFramerateLimit(60);

    // Tenta carregar fonte do sistema
    // Falha silenciosa — HUD fica sem texto mas simulação continua
    if (font_.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf") ||
        font_.loadFromFile("/usr/share/fonts/TTF/DejaVuSansMono.ttf") ||
        font_.loadFromFile("/System/Library/Fonts/Menlo.ttc"))
    {
        font_loaded_ = true;
    }

    create_boundaries();
}

// ============================================================
// Boundaries — chão e paredes laterais (corpos estáticos)
// ============================================================
void Simulation::create_boundaries() {
    // Dimensões do mundo em metros Bullet
    //   Janela: 800×600 px
    //   SCALE = 80 px/m → 10m × 7.5m
    //   OFFSET_X = 400px → centro em x=0
    //   OFFSET_Y = 550px → chão em y=0 Bullet

    // Chão em y = 0 (0.5m abaixo do ponto de referência)
    physics_.add_body({ ShapeType::Box,
        0.f, -0.5f,   // posição centro do chão
        5.1f,         // half_extent: 10.2m de largura (cobre a tela)
        0.f,          // massa = 0 → estático
        0.4f, 0.6f
    });

    // Parede esquerda  (x ≈ -5m, que mapeia para x=0px)
    physics_.add_body({ ShapeType::Box,
        -5.1f, 3.75f,
        0.5f,
        0.f,
        0.2f, 0.6f
    });

    // Parede direita  (x ≈ +5m, que mapeia para x=800px)
    physics_.add_body({ ShapeType::Box,
        5.1f, 3.75f,
        0.5f,
        0.f,
        0.2f, 0.6f
    });

    // Desenhamos as paredes diretamente (são estáticas — não entram em entities_)
}

// ============================================================
// Spawn de esfera
// ============================================================
void Simulation::spawn_sphere(float sx, float sy) {
    auto bp = to_bullet(sx, sy);
    float radius_m = 0.3f;  // 0.3 metros = 24px

    BodyDef def{
        ShapeType::Sphere,
        bp.x, bp.y,
        radius_m,
        1.0f,   // massa
        0.5f,   // restitution (quique moderado)
        0.4f    // friction
    };

    auto handle = physics_.add_body(def);

    // Forma visual SFML
    auto circle = std::make_unique<sf::CircleShape>(radius_m * SCALE);
    circle->setOrigin(radius_m * SCALE, radius_m * SCALE);
    circle->setFillColor(SPHERE_COLORS[sphere_count_ % N_COLORS]);
    circle->setOutlineThickness(1.5f);
    circle->setOutlineColor(sf::Color(255, 255, 255, 60));

    entities_.push_back({ handle, circle.get() });
    circles_.push_back(std::move(circle));

    sphere_count_++;
    fmt::println("sphere #{} em ({:.1f}, {:.1f}) bullet", sphere_count_, bp.x, bp.y);
}

// ============================================================
// Spawn de caixa
// ============================================================
void Simulation::spawn_box(float sx, float sy) {
    auto bp = to_bullet(sx, sy);
    float half_m = 0.28f;

    BodyDef def{
        ShapeType::Box,
        bp.x, bp.y,
        half_m,
        1.5f,   // um pouco mais pesada
        0.3f,
        0.6f
    };

    auto handle = physics_.add_body(def);

    float side_px = half_m * 2.f * SCALE;
    auto rect = std::make_unique<sf::RectangleShape>(sf::Vector2f(side_px, side_px));
    rect->setOrigin(side_px / 2.f, side_px / 2.f);
    rect->setFillColor(BOX_COLORS[box_count_ % N_COLORS]);
    rect->setOutlineThickness(1.5f);
    rect->setOutlineColor(sf::Color(255, 255, 255, 60));

    entities_.push_back({ handle, rect.get() });
    rects_.push_back(std::move(rect));

    box_count_++;
    fmt::println("box #{} em ({:.1f}, {:.1f}) bullet", box_count_, bp.x, bp.y);
}

// ============================================================
// Sincroniza posição e rotação Bullet → SFML
// ============================================================
void Simulation::sync_entities() {
    for (auto& e : entities_) {
        btTransform t;
        e.handle.body->getMotionState()->getWorldTransform(t);

        float bx = t.getOrigin().getX();
        float by = t.getOrigin().getY();

        auto sp = to_sfml(bx, by);
        e.visual->setPosition(sp);

        // Rotação: Bullet usa quaternions, precisamos do ângulo em torno de Z
        // btQuaternion → ângulo em radianos → graus para SFML
        btScalar angle = t.getRotation().getAngle();
        btVector3 axis = t.getRotation().getAxis();

        // Se o eixo aponta para -Z, invertemos o sinal
        float angle_deg = static_cast<float>(angle) * (180.f / 3.14159f);
        if (axis.getZ() < 0.f) angle_deg = -angle_deg;

        // SFML: ângulo em graus, sentido horário = positivo
        // Bullet: ângulo anti-horário positivo → invertemos
        e.visual->setRotation(-angle_deg);
    }
}

// ============================================================
// HUD
// ============================================================
void Simulation::draw_hud() {
    if (!font_loaded_) return;

    auto make_text = [&](const std::string& s, float x, float y,
                         unsigned size = 13,
                         sf::Color color = sf::Color(220, 220, 220)) {
        sf::Text t(s, font_, size);
        t.setFillColor(color);
        t.setPosition(x, y);
        window_.draw(t);
    };

    // Painel semi-transparente
    sf::RectangleShape panel({220.f, 105.f});
    panel.setPosition(8.f, 8.f);
    panel.setFillColor(sf::Color(0, 0, 0, 140));
    panel.setOutlineThickness(1.f);
    panel.setOutlineColor(sf::Color(255, 255, 255, 40));
    window_.draw(panel);

    make_text("BULLET3 + SFML",          16.f, 14.f, 14, sf::Color(120, 200, 255));
    make_text("Clique esq: esfera",       16.f, 36.f);
    make_text("Clique dir: caixa",        16.f, 54.f);
    make_text("P: pausar/retomar",        16.f, 72.f);
    make_text("R: resetar",               16.f, 90.f);

    // Contadores no canto direito
    sf::RectangleShape panel2({170.f, 65.f});
    panel2.setPosition(622.f, 8.f);
    panel2.setFillColor(sf::Color(0, 0, 0, 140));
    panel2.setOutlineThickness(1.f);
    panel2.setOutlineColor(sf::Color(255, 255, 255, 40));
    window_.draw(panel2);

    make_text(fmt::format("Esferas : {}", sphere_count_), 630.f, 14.f);
    make_text(fmt::format("Caixas  : {}", box_count_),    630.f, 34.f);
    make_text(fmt::format("Total   : {}", sphere_count_ + box_count_), 630.f, 54.f);

    if (paused_) {
        sf::Text pt("PAUSADO", font_, 28);
        pt.setFillColor(sf::Color(255, 200, 80, 220));
        pt.setPosition(330.f, 280.f);
        window_.draw(pt);
    }
}

// ============================================================
// Event handling
// ============================================================
void Simulation::handle_events() {
    sf::Event ev;
    while (window_.pollEvent(ev)) {
        if (ev.type == sf::Event::Closed) {
            window_.close();
        }

        if (ev.type == sf::Event::MouseButtonPressed) {
            float sx = static_cast<float>(ev.mouseButton.x);
            float sy = static_cast<float>(ev.mouseButton.y);

            if (ev.mouseButton.button == sf::Mouse::Left) {
                spawn_sphere(sx, sy);
            } else if (ev.mouseButton.button == sf::Mouse::Right) {
                spawn_box(sx, sy);
            }
        }

        if (ev.type == sf::Event::KeyPressed) {
            if (ev.key.code == sf::Keyboard::P) {
                paused_ = !paused_;
                fmt::println("simulação {}", paused_ ? "pausada" : "retomada");
            }
            if (ev.key.code == sf::Keyboard::R) {
                // Recria a simulação do zero
                // (simples: reinicia o objeto inteiro via placement)
                physics_  = PhysicsWorld{};
                entities_.clear();
                circles_.clear();
                rects_.clear();
                sphere_count_ = 0;
                box_count_    = 0;
                create_boundaries();
                fmt::println("simulação resetada");
            }
            if (ev.key.code == sf::Keyboard::Escape) {
                window_.close();
            }
        }
    }
}

// ============================================================
// Update
// ============================================================
void Simulation::update(float dt) {
    if (paused_) return;

    // Clampa dt para evitar explosões numéricas em frames muito lentos
    float clamped = std::min(dt, 0.05f);
    physics_.step(clamped);
    sync_entities();
}

// ============================================================
// Render
// ============================================================
void Simulation::render() {
    window_.clear(sf::Color(25, 25, 35));

    // Chão visual (retângulo cinza escuro)
    sf::RectangleShape ground({810.f, 20.f});
    ground.setPosition(-5.f, 550.f);
    ground.setFillColor(sf::Color(70, 70, 90));
    window_.draw(ground);

    // Paredes visuais
    sf::RectangleShape wall_l({12.f, 560.f});
    wall_l.setPosition(0.f, 0.f);
    wall_l.setFillColor(sf::Color(60, 60, 80));
    window_.draw(wall_l);

    sf::RectangleShape wall_r({12.f, 560.f});
    wall_r.setPosition(788.f, 0.f);
    wall_r.setFillColor(sf::Color(60, 60, 80));
    window_.draw(wall_r);

    // Entidades dinâmicas
    for (auto& e : entities_) {
        window_.draw(*e.visual);
    }

    draw_hud();
    window_.display();
}

// ============================================================
// Loop principal
// ============================================================
void Simulation::run() {
    fmt::println("Simulação iniciada.");
    fmt::println("  Clique esquerdo  → spawna esfera");
    fmt::println("  Clique direito   → spawna caixa");
    fmt::println("  P                → pausar/retomar");
    fmt::println("  R                → resetar");
    fmt::println("  Escape           → sair");

    clock_.restart();

    while (window_.isOpen()) {
        float dt = clock_.restart().asSeconds();

        handle_events();
        update(dt);
        render();
    }

    fmt::println("Simulação encerrada. Total de objetos criados: {}",
        sphere_count_ + box_count_);
}

} // namespace sim
