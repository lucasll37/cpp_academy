#pragma once
#include <QWidget>
#include <QTimer>
#include <SFML/Graphics.hpp>

// ============================================================
// SFMLWidget
//
// Herda de QWidget mas renderiza com SFML por dentro.
//
// O truque central: SFML aceita um WindowHandle externo.
// winId() retorna o handle nativo do widget no sistema
// operacional (XID no Linux, HWND no Windows).
// Passamos esse handle para sf::RenderWindow — o SFML
// renderiza dentro do espaço do widget, sem criar uma
// janela separada.
//
// Três atributos são obrigatórios para isso funcionar:
//   WA_PaintOnScreen    — Qt não tenta pintar sobre o widget
//   WA_OpaquePaintEvent — Qt não limpa o fundo antes de pintar
//   WA_NoSystemBackground — Qt não preenche com cor de fundo
//
// O game loop é um QTimer disparando 60x por segundo.
// NÃO usamos while(true) — isso travaria o event loop do Qt
// e a UI pararia de responder a cliques e sliders.
// ============================================================

class SFMLWidget : public QWidget {
    Q_OBJECT

public:
    explicit SFMLWidget(QWidget* parent = nullptr);
    ~SFMLWidget() override = default;

    // chamados pelo MainWindow via signals/slots
    void set_speed(float speed);
    void set_radius(float radius);
    void set_color(sf::Color color);

public slots:
    void reset();

protected:
    // retornar nullptr diz ao Qt para não criar um QPainter
    // o SFML assume o controle total do rendering desta área
    QPaintEngine* paintEngine() const override { return nullptr; }

    void showEvent  (QShowEvent*)   override;
    void resizeEvent(QResizeEvent*) override;
    void paintEvent (QPaintEvent*)  override;

private slots:
    void tick();  // chamado pelo QTimer — um frame do game loop

private:
    sf::RenderWindow window_;     // renderiza dentro do widget
    sf::CircleShape  ball_;       // a bola que se move
    sf::Vector2f     velocity_;   // direção e magnitude do movimento
    QTimer           timer_;      // dispara tick() a 60fps
    bool             initialized_ = false;

    float      speed_  = 200.f;
    float      radius_ = 30.f;
    sf::Color  color_  = sf::Color(100, 200, 255);
};
