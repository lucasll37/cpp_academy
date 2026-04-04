#include "sfml_widget.hpp"
#include <QShowEvent>
#include <QResizeEvent>

SFMLWidget::SFMLWidget(QWidget* parent)
    : QWidget(parent)
{
    // diz ao Qt para não limpar o fundo — SFML faz isso
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    setMinimumSize(600, 400);

    // timer dispara 60x por segundo — nosso game loop
    connect(&timer_, &QTimer::timeout, this, &SFMLWidget::tick);
}

void SFMLWidget::showEvent(QShowEvent*) {
    if (!initialized_) {
        // cria a RenderWindow SFML usando o handle nativo do widget Qt
        // esse é o truque central — SFML renderiza dentro do widget
        window_.create(sf::WindowHandle(winId()));

        // configura a bola
        ball_.setRadius(radius_);
        ball_.setFillColor(sf::Color(100, 200, 255));
        ball_.setOrigin(radius_, radius_);
        ball_.setPosition(
            width()  / 2.f,
            height() / 2.f
        );

        velocity_ = {speed_, speed_ * 0.7f};

        timer_.start(1000 / 60);  // ~60fps
        initialized_ = true;
    }
}

void SFMLWidget::resizeEvent(QResizeEvent*) {
    if (initialized_)
        window_.setSize({
            (unsigned)width(),
            (unsigned)height()
        });
}

void SFMLWidget::paintEvent(QPaintEvent*) {
    // vazio — SFML controla o rendering, não o Qt
}

void SFMLWidget::tick() {
    float dt = 1.f / 60.f;

    // move a bola
    ball_.move(velocity_ * dt);

    // colisão com as bordas
    auto pos = ball_.getPosition();
    float r  = ball_.getRadius();

    if (pos.x - r < 0 || pos.x + r > width())  velocity_.x *= -1;
    if (pos.y - r < 0 || pos.y + r > height()) velocity_.y *= -1;

    // render
    window_.clear(sf::Color(30, 30, 30));
    window_.draw(ball_);
    window_.display();
}

void SFMLWidget::set_speed(float speed) {
    speed_ = speed;
    // mantém a direção, ajusta a magnitude
    float len = std::hypot(velocity_.x, velocity_.y);
    if (len > 0) velocity_ = velocity_ / len * speed_;
}

void SFMLWidget::set_radius(float radius) {
    radius_ = radius;
    ball_.setRadius(radius_);
    ball_.setOrigin(radius_, radius_);
}

void SFMLWidget::reset() {
    ball_.setPosition(width() / 2.f, height() / 2.f);
    velocity_ = {speed_, speed_ * 0.7f};
}