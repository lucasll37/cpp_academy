#include "sfml_widget.hpp"
#include <QShowEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <array>

// ─────────────────────────────────────────────────────────────────────────────
// Constantes
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float PI       = 3.14159265f;
static constexpr float TWO_PI   = 2.f * PI;
static constexpr float HALF_PI  = PI / 2.f;
static constexpr int   TARGET_FPS = 60;

// Cores da paleta da simulação
static const sf::Color COL_BG       { 15,  15,  18, 255};
static const sf::Color COL_ROOM     { 40,  50,  70, 120};
static const sf::Color COL_ROOM_EDG { 60,  80, 120, 180};
static const sf::Color COL_FLOOR    { 30,  35,  50, 160};
static const sf::Color COL_AX_X     {220,  80,  80, 200};
static const sf::Color COL_AX_Y     { 80, 200,  80, 200};
static const sf::Color COL_AX_Z     { 80, 120, 220, 200};

static const std::array<sf::Color, 8> PALETTE = {{
    { 85, 120, 255, 255},
    {255,  85, 120, 255},
    { 85, 220, 160, 255},
    {255, 180,  50, 255},
    {210,  70, 255, 255},
    { 60, 200, 255, 255},
    {255, 130,  60, 255},
    {140, 255,  80, 255},
}};

static sf::Color random_color() {
    return PALETTE[rand() % PALETTE.size()];
}

// ─────────────────────────────────────────────────────────────────────────────
// Construtor
// ─────────────────────────────────────────────────────────────────────────────
SFMLWidget::SFMLWidget(QWidget* parent)
    : QWidget(parent)
    , physics_(5.f)  // quarto 10×10×10
{
    // Os três atributos obrigatórios para embutir SFML num QWidget
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    // Aceitar input de mouse
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(500, 400);

    connect(&timer_, &QTimer::timeout, this, &SFMLWidget::tick);

    // Spawna alguns objetos iniciais
    update_camera_matrix();
    for (int i = 0; i < 6; ++i) spawn_object(ShapeType::Sphere);
    for (int i = 0; i < 4; ++i) spawn_object(ShapeType::Box);
    for (int i = 0; i < 2; ++i) spawn_object(ShapeType::Cylinder);
}

// ─────────────────────────────────────────────────────────────────────────────
// showEvent: cria a sf::RenderWindow usando o handle nativo do widget
// ─────────────────────────────────────────────────────────────────────────────
void SFMLWidget::showEvent(QShowEvent*) {
    if (!initialized_) {
        window_.create(sf::WindowHandle(winId()));
        window_.setFramerateLimit(0);  // controlado pelo QTimer
        timer_.start(1000 / TARGET_FPS);
        initialized_ = true;
    }
}

void SFMLWidget::resizeEvent(QResizeEvent*) {
    if (initialized_)
        window_.setSize({(unsigned)width(), (unsigned)height()});
}

void SFMLWidget::paintEvent(QPaintEvent*) {
    // Vazio intencionalmente — SFML controla o rendering desta área
}

// ─────────────────────────────────────────────────────────────────────────────
// tick — game loop chamado pelo QTimer a ~60fps
// ─────────────────────────────────────────────────────────────────────────────
void SFMLWidget::tick() {
    if (!initialized_) return;

    // Processa eventos SFML (obrigatório para evitar "not responding" no X11)
    sf::Event evt;
    while (window_.pollEvent(evt)) {}

    // Avança física
    if (!paused_)
        physics_.step(1.f / TARGET_FPS);

    // Renderização
    window_.clear(COL_BG);
    draw_room();
    draw_objects();
    draw_axes();
    window_.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// Câmera esférica
// ─────────────────────────────────────────────────────────────────────────────
// Converte (theta, phi, radius) em vetores ortogonais da câmera.
// Usamos coordenadas esféricas:
//   pos = radius * (sin(phi)*sin(theta), cos(phi), sin(phi)*cos(theta))
//
// forward = -normalize(pos)   (aponta para o centro)
// right   = normalize(forward × world_up) ou fallback nos polos
// up      = forward × right   (perpendicular ao plano câmera)
void SFMLWidget::update_camera_matrix() {
    const float sp = std::sin(cam_phi_), cp = std::cos(cam_phi_);
    const float st = std::sin(cam_theta_), ct = std::cos(cam_theta_);

    cam_pos_ = { cam_radius_*sp*st, cam_radius_*cp, cam_radius_*sp*ct };

    // forward = -normalize(cam_pos)
    float len = std::sqrt(cam_pos_.x*cam_pos_.x +
                          cam_pos_.y*cam_pos_.y +
                          cam_pos_.z*cam_pos_.z);
    cam_forward_ = { -cam_pos_.x/len, -cam_pos_.y/len, -cam_pos_.z/len };

    // right = normalize(forward × world_up)
    // world_up = (0,1,0)
    Vec3f wy = {0.f, 1.f, 0.f};
    Vec3f r = {
        cam_forward_.y*wy.z - cam_forward_.z*wy.y,
        cam_forward_.z*wy.x - cam_forward_.x*wy.z,
        cam_forward_.x*wy.y - cam_forward_.y*wy.x
    };
    // fallback: near polo — usa world_z
    float rlen = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z);
    if (rlen < 1e-4f) { r = {0.f, 0.f, 1.f}; rlen = 1.f; }
    cam_right_ = { r.x/rlen, r.y/rlen, r.z/rlen };

    // up = right × forward
    cam_up_ = {
        cam_right_.y*cam_forward_.z - cam_right_.z*cam_forward_.y,
        cam_right_.z*cam_forward_.x - cam_right_.x*cam_forward_.z,
        cam_right_.x*cam_forward_.y - cam_right_.y*cam_forward_.x
    };
}

// project3D: projeta um ponto 3D do mundo para pixel 2D no canvas SFML
// Implementa projeção perspectiva simples sem biblioteca externa.
//
// 1. Translada o ponto para o espaço de câmera (subtrai cam_pos)
// 2. Projeta nos eixos right/up/forward da câmera → coordenadas de view
// 3. Divide por profundidade (z de câmera) * fov_scale → NDC
// 4. Converte NDC para pixel (centro da tela = (W/2, H/2))
sf::Vector2f SFMLWidget::project3D(const btVector3& wp) const {
    // Vetor do ponto ao olho
    float dx = wp.x() - cam_pos_.x;
    float dy = wp.y() - cam_pos_.y;
    float dz = wp.z() - cam_pos_.z;

    // Coordenadas no espaço de câmera
    float vx =  dx*cam_right_.x   + dy*cam_right_.y   + dz*cam_right_.z;
    float vy = -(dx*cam_up_.x     + dy*cam_up_.y       + dz*cam_up_.z);
    float vz =  dx*cam_forward_.x + dy*cam_forward_.y  + dz*cam_forward_.z;

    if (vz < near_plane_) vz = near_plane_;

    float W = (float)width();
    float H = (float)height();
    float scale = (H / 2.f) / (fov_scale_ * vz);

    return {
        W/2.f + vx * scale,
        H/2.f + vy * scale
    };
}

// Retorna profundidade de câmera normalizada para efeito de depth-cue
float SFMLWidget::depth_scale(const btVector3& wp) const {
    float dx = wp.x() - cam_pos_.x;
    float dy = wp.y() - cam_pos_.y;
    float dz = wp.z() - cam_pos_.z;
    float vz = dx*cam_forward_.x + dy*cam_forward_.y + dz*cam_forward_.z;
    // mapeia [near, far] → [1, 0.3]
    float t = std::max(0.f, std::min(1.f, (vz - 2.f) / 25.f));
    return 1.f - 0.7f * t;
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_room — wireframe do quarto e chão
// ─────────────────────────────────────────────────────────────────────────────
void SFMLWidget::draw_room() {
    const float h = physics_.half_room();

    // 8 vértices do cubo
    std::array<btVector3, 8> v = {{
        {-h,-h,-h}, { h,-h,-h}, { h,-h, h}, {-h,-h, h},
        {-h, h,-h}, { h, h,-h}, { h, h, h}, {-h, h, h}
    }};

    // 12 arestas (pares de índices)
    static const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},  // fundo
        {4,5},{5,6},{6,7},{7,4},  // topo
        {0,4},{1,5},{2,6},{3,7}   // pilares
    };

    for (auto& e : edges) {
        auto p1 = project3D(v[e[0]]);
        auto p2 = project3D(v[e[1]]);
        sf::Vertex line[2] = {
            {p1, COL_ROOM_EDG},
            {p2, COL_ROOM_EDG}
        };
        window_.draw(line, 2, sf::Lines);
    }

    // Preenchimento semitransparente do chão (y = -h)
    std::array<sf::Vertex, 4> floor_verts;
    const int fi[4] = {0,1,2,3};
    for (int i = 0; i < 4; ++i)
        floor_verts[i] = {project3D(v[fi[i]]), COL_FLOOR};
    window_.draw(floor_verts.data(), 4, sf::Quads);
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_objects — projeta e desenha cada PhysicsObject
// ─────────────────────────────────────────────────────────────────────────────
void SFMLWidget::draw_objects() {
    // Ordena por profundidade (painter's algorithm: mais longe primeiro)
    struct Entry {
        float       depth;
        const PhysicsObject* obj;
    };
    std::vector<Entry> entries;
    entries.reserve(physics_.objects().size());

    for (auto& uptr : physics_.objects()) {
        btTransform t;
        uptr->motion_state->getWorldTransform(t);
        btVector3 pos = t.getOrigin();
        float dx = pos.x() - cam_pos_.x;
        float dy = pos.y() - cam_pos_.y;
        float dz = pos.z() - cam_pos_.z;
        float depth = dx*cam_forward_.x + dy*cam_forward_.y + dz*cam_forward_.z;
        entries.push_back({depth, uptr.get()});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b){ return a.depth > b.depth; });

    for (auto& entry : entries) {
        const auto* obj = entry.obj;
        btTransform t;
        obj->motion_state->getWorldTransform(t);
        btVector3 pos = t.getOrigin();

        auto proj = project3D(pos);
        float ds  = depth_scale(pos);

        // Tamanho projetado baseado em half_extent e fov_scale
        float W = (float)width(), H = (float)height();
        float cam_z = entry.depth < near_plane_ ? near_plane_ : entry.depth;
        float scale = (H / 2.f) / (fov_scale_ * cam_z);
        float r_px  = obj->half_extent * scale;
        r_px = std::max(3.f, r_px);

        // Cor com depth-cue
        sf::Color col = obj->color;
        col.r = (sf::Uint8)(col.r * ds);
        col.g = (sf::Uint8)(col.g * ds);
        col.b = (sf::Uint8)(col.b * ds);

        // Borda mais escura
        sf::Color border = col;
        border.r /= 2; border.g /= 2; border.b /= 2;

        switch (obj->type) {
            case ShapeType::Sphere: {
                // Esfera: círculo cheio + highlight branco
                sf::CircleShape circle(r_px);
                circle.setOrigin(r_px, r_px);
                circle.setPosition(proj);
                circle.setFillColor(col);
                circle.setOutlineThickness(1.5f);
                circle.setOutlineColor(border);
                window_.draw(circle);

                // Highlight (brilho)
                float hl_r = r_px * 0.35f;
                sf::CircleShape hl(hl_r);
                hl.setOrigin(hl_r, hl_r);
                hl.setPosition(proj.x - r_px*0.25f, proj.y - r_px*0.25f);
                sf::Color hl_col(255,255,255, (sf::Uint8)(80*ds));
                hl.setFillColor(hl_col);
                window_.draw(hl);
                break;
            }
            case ShapeType::Box: {
                // Caixa: quadrado rotacionado com a orientação do rigidbody
                // Extrai ângulo de rotação no eixo Y da matriz de rotação Bullet
                btMatrix3x3 rot = t.getBasis();
                // Projeta os dois eixos do box no plano da câmera para simular rotação 2D
                btVector3 ax = rot.getColumn(0) * obj->half_extent;
                btVector3 ay = rot.getColumn(1) * obj->half_extent;

                // 4 cantos do quadrado no espaço 3D
                btVector3 corners[4] = {
                    pos + ax + ay,
                    pos - ax + ay,
                    pos - ax - ay,
                    pos + ax - ay
                };
                sf::ConvexShape box;
                box.setPointCount(4);
                for (int i = 0; i < 4; ++i)
                    box.setPoint(i, project3D(corners[i]));
                box.setFillColor(col);
                box.setOutlineThickness(1.5f);
                box.setOutlineColor(border);
                window_.draw(box);
                break;
            }
            case ShapeType::Cylinder: {
                // Cilindro: elipse com o eixo de rotação
                btMatrix3x3 rot = t.getBasis();
                btVector3 ax = rot.getColumn(0) * obj->half_extent;
                btVector3 ay = rot.getColumn(1) * (obj->half_extent * 1.6f);
                btVector3 az = rot.getColumn(2) * obj->half_extent;

                // Topo e fundo do cilindro
                btVector3 top    = pos + ay;
                btVector3 bottom = pos - ay;
                auto ptop    = project3D(top);
                auto pbottom = project3D(bottom);

                // Quadrilátero lateral
                btVector3 tl = top    + ax;
                btVector3 tr = top    - ax;
                btVector3 bl = bottom + ax;
                btVector3 br = bottom - ax;

                sf::ConvexShape body;
                body.setPointCount(4);
                body.setPoint(0, project3D(tl));
                body.setPoint(1, project3D(tr));
                body.setPoint(2, project3D(br));
                body.setPoint(3, project3D(bl));
                body.setFillColor(col);
                body.setOutlineThickness(1.5f);
                body.setOutlineColor(border);
                window_.draw(body);

                // Tampa do topo
                float rp = std::max(2.f, r_px * 0.4f);
                sf::CircleShape cap(rp);
                cap.setOrigin(rp, rp);
                cap.setPosition(ptop);
                sf::Color top_col = col;
                top_col.r = std::min(255, (int)(top_col.r * 1.3f));
                top_col.g = std::min(255, (int)(top_col.g * 1.3f));
                top_col.b = std::min(255, (int)(top_col.b * 1.3f));
                cap.setFillColor(top_col);
                window_.draw(cap);
                break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// draw_axes — mini eixos de orientação no canto inferior esquerdo
// ─────────────────────────────────────────────────────────────────────────────
void SFMLWidget::draw_axes() {
    const float cx = 50.f, cy = (float)height() - 50.f;
    const float len = 30.f;

    // Projeta os três eixos unitários no espaço de câmera para desenhar
    auto draw_axis = [&](sf::Color col, float wx, float wy, float wz) {
        float px = wx*cam_right_.x + wy*cam_right_.y + wz*cam_right_.z;
        float py = -(wx*cam_up_.x  + wy*cam_up_.y    + wz*cam_up_.z);
        sf::Vertex line[2] = {
            {{cx, cy}, col},
            {{cx + px*len, cy + py*len}, col}
        };
        window_.draw(line, 2, sf::Lines);
    };

    draw_axis(COL_AX_X, 1,0,0);
    draw_axis(COL_AX_Y, 0,1,0);
    draw_axis(COL_AX_Z, 0,0,1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse: orbitar câmera com arrastar, lançar objetos com clique
// ─────────────────────────────────────────────────────────────────────────────
void SFMLWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        mouse_dragging_ = true;
        mouse_last_     = e->pos();
    }
}

void SFMLWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!mouse_dragging_) return;
    QPoint delta = e->pos() - mouse_last_;
    cam_theta_ -= delta.x() * 0.008f;
    cam_phi_    = std::max(0.1f, std::min(PI - 0.1f,
                          cam_phi_ - delta.y() * 0.008f));
    mouse_last_ = e->pos();
    update_camera_matrix();
}

void SFMLWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton)
        mouse_dragging_ = false;
}

void SFMLWidget::wheelEvent(QWheelEvent* e) {
    cam_radius_ -= e->angleDelta().y() * 0.02f;
    cam_radius_  = std::max(5.f, std::min(35.f, cam_radius_));
    update_camera_matrix();
}

// ─────────────────────────────────────────────────────────────────────────────
// API pública
// ─────────────────────────────────────────────────────────────────────────────
void SFMLWidget::spawn_object(ShapeType type) {
    const float h   = physics_.half_room() * 0.6f;
    float px = ((float)rand()/RAND_MAX * 2 - 1) * h;
    float py = ((float)rand()/RAND_MAX * 2 - 1) * h;
    float pz = ((float)rand()/RAND_MAX * 2 - 1) * h;

    float speed = 4.f + (float)rand()/RAND_MAX * 6.f;
    float vx = ((float)rand()/RAND_MAX * 2 - 1) * speed;
    float vy = ((float)rand()/RAND_MAX * 2 - 1) * speed;
    float vz = ((float)rand()/RAND_MAX * 2 - 1) * speed;

    float he = 0.3f + (float)rand()/RAND_MAX * 0.5f;

    physics_.add_object(type, he,
                        {px, py, pz},
                        {vx, vy, vz},
                        random_color());
}

void SFMLWidget::clear_objects() { physics_.clear_objects(); }
void SFMLWidget::explode()       { physics_.explode(20.f); }
void SFMLWidget::set_gravity(float gy) { physics_.set_gravity(gy); }
void SFMLWidget::set_paused(bool p) { paused_ = p; }
int  SFMLWidget::object_count() const {
    return (int)physics_.objects().size();
}

void SFMLWidget::set_restitution(float r) {
    restitution_ = r;
    // Aplica a todos os corpos existentes via user data não-padrão.
    // Em Bullet, a restitution por contato é a média dos dois corpos.
    // Recria os contact materials não é direto — a forma mais simples
    // é ajustar a propriedade no btRigidBody diretamente.
    for (auto& obj : physics_.objects())
        obj->body->setRestitution(r);
}
