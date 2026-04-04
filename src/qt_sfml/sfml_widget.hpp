#pragma once
#include <QWidget>
#include <QTimer>
#include <SFML/Graphics.hpp>

class SFMLWidget : public QWidget {
    Q_OBJECT

public:
    explicit SFMLWidget(QWidget* parent = nullptr);
    ~SFMLWidget() override = default;

    void set_speed(float speed);
    void set_radius(float radius);
    void reset();

protected:
    // necessário para o Qt não pintar por cima do SFML
    QPaintEngine* paintEngine() const override { return nullptr; }
    void showEvent(QShowEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void paintEvent(QPaintEvent*) override;

private slots:
    void tick();

private:
    sf::RenderWindow  window_;
    sf::CircleShape   ball_;
    sf::Vector2f      velocity_;
    QTimer            timer_;
    bool              initialized_ = false;

    float speed_  = 200.f;
    float radius_ = 30.f;
};