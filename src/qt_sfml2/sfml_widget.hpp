#pragma once

// ============================================================
// SFMLWidget
//
// Herda de QWidget e embarca um sf::RenderWindow usando o
// handle nativo do widget (winId()), exatamente como no
// qt_sfml original — mas agora renderiza uma cena 3D
// projetada com câmera perspectiva manual.
//
// Responsabilidades:
//   - Manter e avançar o PhysicsWorld (Bullet3)
//   - Projetar os corpos 3D em 2D com câmera esférica
//   - Desenhar com SFML (sf::CircleShape, sf::RectangleShape)
//   - Processar mouse: orbitar câmera, lançar objetos
//
// NÃO contém lógica de UI (sliders, botões) — isso fica em
// MainWindow que chama os métodos públicos abaixo via slots.
// ============================================================

#include <QWidget>
#include <QTimer>
#include <QMouseEvent>
#include <SFML/Graphics.hpp>
#include "physics_world.hpp"

class SFMLWidget : public QWidget {
    Q_OBJECT

public:
    explicit SFMLWidget(QWidget* parent = nullptr);
    ~SFMLWidget() override = default;

    // ── API pública chamada pelo MainWindow via slots ──────────────────────

    // Adiciona um objeto na posição central com velocidade aleatória
    void spawn_object(ShapeType type);

    // Remove todos os objetos dinâmicos
    void clear_objects();

    // Explosão radial central
    void explode();

    // Altera gravidade (gy negativo = para baixo)
    void set_gravity(float gy);

    // Altera elasticidade global (0=mole, 1=borracha)
    void set_restitution(float r);

    // Liga/desliga pausa da simulação
    void set_paused(bool p);

    // Retorna quantidade de objetos na cena
    int object_count() const;

protected:
    QPaintEngine* paintEngine() const override { return nullptr; }

    void showEvent  (QShowEvent*)   override;
    void resizeEvent(QResizeEvent*) override;
    void paintEvent (QPaintEvent*)  override;

    void mousePressEvent  (QMouseEvent*) override;
    void mouseMoveEvent   (QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent       (QWheelEvent*) override;

private slots:
    void tick();

private:
    // ── Projeção 3D → 2D ──────────────────────────────────────────────────
    // Câmera esférica parametrizada por (theta, phi, radius).
    // project3D converte um ponto 3D do mundo para pixel 2D no canvas SFML.
    sf::Vector2f project3D(const btVector3& world_pos) const;

    // Retorna um fator de "escala de profundidade" (0..1) para simular
    // profundidade de campo no tamanho/brilho dos objetos desenhados.
    float depth_scale(const btVector3& world_pos) const;

    // Constrói a matriz de view a partir dos parâmetros da câmera
    void update_camera_matrix();

    // Desenha as arestas do quarto (wireframe)
    void draw_room();

    // Desenha todos os objetos físicos projetados
    void draw_objects();

    // Desenha um eixo de orientação (mini-axes no canto inferior esquerdo)
    void draw_axes();

    // ── Estado ────────────────────────────────────────────────────────────
    sf::RenderWindow window_;
    QTimer           timer_;
    bool             initialized_ = false;
    bool             paused_      = false;

    PhysicsWorld     physics_;

    // ── Câmera esférica ───────────────────────────────────────────────────
    float cam_theta_  = 0.5f;   // azimute (radianos)
    float cam_phi_    = 0.8f;   // elevação (radianos, 0=topo, π=fundo)
    float cam_radius_ = 22.f;   // distância ao centro

    // Matriz de view 3×3 (linhas = eixos right, up, forward da câmera)
    // Calculada por update_camera_matrix() sempre que theta/phi mudam.
    struct Vec3f { float x, y, z; };
    Vec3f cam_right_, cam_up_, cam_forward_, cam_pos_;

    // Parâmetros de projeção perspectiva
    float fov_scale_  = 1.0f;   // tan(FOV/2) — menor = mais zoom
    float near_plane_ = 0.1f;

    // ── Interação de mouse ────────────────────────────────────────────────
    bool  mouse_dragging_ = false;
    QPoint mouse_last_;

    float restitution_ = 0.85f;
};
