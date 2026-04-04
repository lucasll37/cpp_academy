#include "sfml_widget.hpp"
#include <QShowEvent>
#include <QResizeEvent>
#include <cmath>

SFMLWidget::SFMLWidget(QWidget* parent)
    : QWidget(parent)
{
    // esses três atributos são obrigatórios para o SFML
    // renderizar dentro do widget sem conflito com o Qt
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    setMinimumSize(500, 400);

    // conecta o timer ao tick — nosso game loop
    connect(&timer_, &QTimer::timeout, this, &SFMLWidget::tick);
}

void SFMLWidget::showEvent(QShowEvent*) {
    if (!initialized_) {
        // winId() retorna o handle nativo do widget no SO
        // no Linux é um XID do X11
        // no Windows seria um HWND
        // o SFML cria uma RenderWindow usando esse handle
        // renderizando dentro do espaço do widget
        window_.create(sf::WindowHandle(winId()));

        // configura a bola
        ball_.setRadius(radius_);
        ball_.setFillColor(color_);
        ball_.setOrigin(radius_, radius_);
        ball_.setPosition(
            static_cast<float>(width())  / 2.f,
            static_cast<float>(height()) / 2.f
        );

        // velocidade inicial em diagonal
        velocity_ = { speed_, speed_ * 0.7f };

        timer_.start(1000 / 60);  // ~60fps
        initialized_ = true;
    }
}

void SFMLWidget::resizeEvent(QResizeEvent*) {
    if (initialized_) {
        window_.setSize({
            static_cast<unsigned>(width()),
            static_cast<unsigned>(height())
        });
    }
}

void SFMLWidget::paintEvent(QPaintEvent*) {
    // vazio intencionalmente
    // o SFML controla o rendering desta área via tick()
    // o Qt não deve tentar pintar nada aqui
}

void SFMLWidget::tick() {
    constexpr float dt = 1.f / 60.f;

    // move a bola
    ball_.move(velocity_ * dt);

    // colisão com as bordas — inverte a velocidade no eixo correspondente
    const auto pos = ball_.getPosition();
    const float r  = ball_.getRadius();
    const float w  = static_cast<float>(width());
    const float h  = static_cast<float>(height());

    if (pos.x - r < 0.f || pos.x + r > w) velocity_.x *= -1.f;
    if (pos.y - r < 0.f || pos.y + r > h) velocity_.y *= -1.f;

    // rendering
    window_.clear(sf::Color(30, 30, 30));
    window_.draw(ball_);
    window_.display();
}

void SFMLWidget::set_speed(float speed) {
    speed_ = speed;

    // mantém a direção atual, só ajusta a magnitude
    const float len = std::hypot(velocity_.x, velocity_.y);
    if (len > 0.f)
        velocity_ = (velocity_ / len) * speed_;
}

void SFMLWidget::set_radius(float radius) {
    radius_ = radius;
    ball_.setRadius(radius_);
    ball_.setOrigin(radius_, radius_);
}

void SFMLWidget::set_color(sf::Color color) {
    color_ = color;
    ball_.setFillColor(color_);
}

void SFMLWidget::reset() {
    ball_.setPosition(
        static_cast<float>(width())  / 2.f,
        static_cast<float>(height()) / 2.f
    );
    velocity_ = { speed_, speed_ * 0.7f };
}
