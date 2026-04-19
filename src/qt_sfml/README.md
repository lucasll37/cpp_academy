# qt_sfml — Integração Qt + SFML (Widget customizado)

## O que é este projeto?

Demonstra como **embutir uma janela SFML dentro de um widget Qt**, combinando a rica interface de usuário do Qt (botões, sliders, layouts) com o poder de renderização em tempo real do SFML. Uma bola se move e quica dentro da área SFML enquanto o usuário controla velocidade, raio e cor via widgets Qt.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| `QWidget` customizado | Substituir o sistema de pintura do Qt pelo SFML |
| `winId()` + `sf::RenderWindow` | SFML renderiza dentro do widget Qt |
| `QTimer` como game loop | 60 FPS sem bloquear o event loop Qt |
| Signals & Slots | Qt conecta slider ao `SFMLWidget` |
| Atributos de widget | `WA_PaintOnScreen`, `WA_OpaquePaintEvent` |
| Coordenadas Y invertido | Conversão entre sistemas de coordenadas Qt e SFML |

---

## Estrutura de arquivos

```
qt_sfml/
├── main.cpp          ← cria QApplication e MainWindow
├── mainwindow.hpp/cpp ← janela principal: layouts, sliders, botões
├── sfml_widget.hpp/cpp ← widget SFML: game loop, física simples, renderização
└── meson.build
```

---

## O truque central — SFMLWidget

```cpp
// SFMLWidget herda de QWidget mas renderiza com SFML

class SFMLWidget : public QWidget {
    Q_OBJECT

    sf::RenderWindow window_;  // janela SFML dentro do widget
    sf::CircleShape  ball_;    // a bola animada
    sf::Vector2f     velocity_;
    QTimer           timer_;   // game loop: dispara 60x/segundo

protected:
    // CRÍTICO: retornar nullptr diz ao Qt para não criar um QPainter
    // O SFML assume o controle total desta área
    QPaintEngine* paintEngine() const override { return nullptr; }

    void showEvent(QShowEvent*) override {
        // O SFML precisa do handle nativo do widget para abrir sua janela
        // winId() retorna XID no Linux, HWND no Windows
        if (!initialized_) {
            window_.create(static_cast<sf::WindowHandle>(winId()));
            timer_.start(1000 / 60); // 60 FPS
            initialized_ = true;
        }
    }

    void paintEvent(QPaintEvent*) override {
        tick(); // Qt chama paintEvent → delegamos para o SFML
    }
};
```

---

## Os três atributos obrigatórios

```cpp
SFMLWidget::SFMLWidget(QWidget* parent) : QWidget(parent) {
    // Sem esses atributos, o Qt tenta pintar sobre a área do SFML,
    // causando flickering ou sobrescrevendo o conteúdo SFML.

    setAttribute(Qt::WA_PaintOnScreen);     // Qt não pinta sobre este widget
    setAttribute(Qt::WA_OpaquePaintEvent);  // Qt não limpa o fundo antes de pintar
    setAttribute(Qt::WA_NoSystemBackground); // Qt não preenche com cor de fundo
}
```

---

## O game loop com QTimer

```cpp
// QTimer em vez de while(true) — NÃO bloqueia o event loop Qt!
timer_.setInterval(1000 / 60); // 60 FPS
connect(&timer_, &QTimer::timeout, this, &SFMLWidget::tick);
timer_.start();

void SFMLWidget::tick() {
    // Física simples: movimento + rebatimento nas bordas
    ball_.move(velocity_ * (1.0f / 60.0f));

    sf::Vector2u size = window_.getSize();
    sf::FloatRect bounds = ball_.getGlobalBounds();

    // Rebate nas paredes
    if (bounds.left < 0 || bounds.left + bounds.width > size.x)
        velocity_.x *= -1.f;
    if (bounds.top < 0 || bounds.top + bounds.height > size.y)
        velocity_.y *= -1.f;

    // Renderiza
    window_.clear(sf::Color(20, 20, 30));
    window_.draw(ball_);
    window_.display();
}
```

---

## `MainWindow` — widgets Qt de controle

```cpp
// MainWindow.cpp
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    auto* central = new QWidget(this);
    auto* layout  = new QHBoxLayout(central);

    // Área SFML — painel principal
    sfml_widget_ = new SFMLWidget(this);
    sfml_widget_->setMinimumSize(640, 480);
    layout->addWidget(sfml_widget_, 1);

    // Painel de controles Qt
    auto* controls = new QVBoxLayout();
    layout->addLayout(controls);

    // Slider de velocidade
    auto* spd_slider = new QSlider(Qt::Vertical, this);
    spd_slider->setRange(50, 500);
    spd_slider->setValue(200);
    connect(spd_slider, &QSlider::valueChanged, [this](int v) {
        sfml_widget_->set_speed(static_cast<float>(v));
    });

    // Botão reset
    auto* btn_reset = new QPushButton("Reset", this);
    connect(btn_reset, &QPushButton::clicked, sfml_widget_, &SFMLWidget::reset);
}
```

---

## Signals & Slots — comunicação Qt

```
QSlider::valueChanged(int) ──signal──► SFMLWidget::set_speed(float)
QPushButton::clicked()     ──signal──► SFMLWidget::reset()
QTimer::timeout()          ──signal──► SFMLWidget::tick()
```

```cpp
// Conectar slider de raio
connect(radius_slider, &QSlider::valueChanged, [this](int v) {
    sfml_widget_->set_radius(static_cast<float>(v));
});

// Implementação no SFMLWidget
void SFMLWidget::set_radius(float r) {
    radius_ = r;
    ball_.setRadius(r);
    ball_.setOrigin(r, r); // centro do círculo como origem
}
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja qt_sfml
./bin/qt_sfml
```

---

## Dependências

- `Qt6` (ou Qt5) — framework GUI
- `SFML` — renderização 2D
