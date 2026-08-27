# qt_sfml — SFML desenhando dentro de um widget Qt

## O que é este projeto?

Um aplicativo Qt de verdade — menu, barra de status, painel lateral com sliders e botões — em
que **uma região da janela é pintada pelo SFML**, e não pelo Qt. Dentro dessa região uma bola
quica; os controles Qt mudam a velocidade, o raio e a cor dela em tempo real.

```bash
make configure          # só na primeira vez (Conan + Meson)
make build              # compila tudo
./dist/bin/qt_academy   # ⚠ o alvo se chama qt_academy, não qt_sfml
```

> ⚠ **O nome do binário não é o nome do diretório.** `src/qt_sfml/meson.build` declara
> `executable('qt_academy', ...)`; para compilar só ele, `meson compile -C build qt_academy`.
>
> ⚠ **Sessão Wayland:** a técnica deste subprojeto é X11. Se a sua sessão for Wayland, force o
> plugin XCB — o Qt roda sobre XWayland e tudo funciona:
> `QT_QPA_PLATFORM=xcb ./dist/bin/qt_academy`. Sem isso o Qt tenta o plugin `wayland` e,
> dependendo do ambiente, morre com `Failed to create wl_display`. A seção
> [Portabilidade](#portabilidade-a-parte-frágil) explica por quê.

---

## Antes de começar: o vocabulário do Qt

Esta seção define **tudo** o que o resto do README usa; nada aqui pressupõe mais que C++ básico.

### 1. Widget

**Widget** é qualquer coisa retangular que aparece na tela e recebe eventos: um botão, um
rótulo, um slider, uma janela inteira. Todos herdam de `QWidget`. Widgets formam uma **árvore** —
um widget contém outros, que ocupam pedaços da sua área:

```
QMainWindow  (a janela)
├── QMenuBar          "Simulation"
├── centralwidget     ← área principal; aqui vive o SFMLWidget
│   └── SFMLWidget    ← a região pintada pelo SFML
├── QDockWidget       "Controles" — o painel lateral
│   ├── QSlider       sliderSpeed, sliderRadius
│   └── QPushButton   btnColor, btnReset
└── QStatusBar        a faixa de mensagens embaixo
```

`QWidget` é a classe base de todo controle visual; `QMainWindow` é uma janela pronta com menu,
barra de status e áreas de *dock*; `centralwidget` é o widget do meio dela, onde vai o conteúdo
principal.

### 2. Pai, filho e quem chama `delete`

Todo widget pode receber um **pai** no construtor: `new QSlider(pai)`. O Qt guarda o filho numa
lista e, **quando o pai é destruído, ele destrói todos os filhos**. Por isso o código deste
subprojeto tem vários `new` e **nenhum `delete`** — e mesmo assim não vaza:

```cpp
sfml_ = new SFMLWidget(ui_.centralwidget);        // dono: centralwidget
auto* layout = new QHBoxLayout(ui_.centralwidget);
```

Quando a `MainWindow` morre, ela destrói o `centralwidget`, que destrói o `SFMLWidget` e o
layout. É RAII com o dono explicitado no construtor, no lugar de um `unique_ptr`.

> ⚠ Um `QObject` **com** pai é do pai. Um `QObject` **sem** pai é seu — aí use `std::unique_ptr`
> ou uma variável local, como `MainWindow window;` no `main.cpp`. Nunca dê `delete` num filho
> vivo, e nunca coloque um objeto com pai dentro de um `unique_ptr`: os dois vão destruí-lo.

### 3. Event loop

Um programa gráfico não tem "começo, meio e fim". Ele fica parado esperando **eventos** — mouse,
teclado, redimensionamento, temporizador — e reage a cada um. Esse laço é o **event loop**, e no
Qt ele é `app.exec()`:

```
                ┌───────────── app.exec() ──────────────┐
 SO ───────────►│  fila: [ev1][ev2][ev3]…               │
 (clique,       │     │ pega o primeiro                 │
  tecla,        │     ▼                                 │
  timer)        │  entrega ao widget: paintEvent,       │
                │  mousePressEvent, slot conectado      │
                │     └──── volta a esperar ────────────┤
                └───────────────────────────────────────┘
```

> ⚠ **Bloquear o event loop congela a interface.** Com um `while (true)` seu rodando, o laço
> acima não roda: o botão não pinta o estado "pressionado", o slider não anda, a janela não
> fecha. É por isso que este projeto **não** tem game loop `while(true)` — o "loop" é um `QTimer`
> que devolve o controle ao Qt entre dois quadros (seção *O game loop*).

### 4. Signal e slot

O Qt precisa dizer "quando o slider mudar, avise alguém" sem que o slider conheça esse alguém. A
solução é o par **signal/slot**, o padrão *observador* embutido no Qt:

- **signal** — função que você **declara e nunca implementa**; o moc escreve o corpo. Emiti-la
  significa "isto aconteceu": `valueChanged(int)`, `clicked()`, `timeout()`.
- **slot** — função normal, que você implementa, apta a ser ligada a um signal.
- **`connect(origem, signal, destino, slot)`** — liga os dois.

Exemplo mínimo, completo:

```cpp
#include <QApplication>
#include <QPushButton>
#include <QDebug>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QPushButton botao("Clique");
    // o botão não sabe o que vai acontecer; só emite clicked()
    QObject::connect(&botao, &QPushButton::clicked, &botao, [] { qDebug() << "clicou"; });
    botao.show();
    return app.exec();
}
```

O que a sintaxe de ponteiro-para-membro (`&QPushButton::clicked`) dá, e um `std::function`
guardado à mão não dá:

| | `connect` do Qt | um `std::function` seu |
|---|---|---|
| erro de tipo no argumento | **erro de compilação** | erro em tempo de execução, ou nada |
| um evento, vários interessados | N slots por signal, sem esforço | você mantém um `vector` |
| o destino morre antes | conexão **se desfaz sozinha** | ponteiro pendurado, *crash* |
| destino em outra thread | o Qt enfileira a chamada na thread do destino | você escreve isso |

### 5. `Q_OBJECT`

Signals e slots não existem em C++. Para existirem, a classe precisa de uma tabela de metadados
em tempo de execução (nome dos métodos, assinatura, índices). Quem **declara** essa tabela é a
macro `Q_OBJECT`, na primeira linha do corpo da classe:

```cpp
class SFMLWidget : public QWidget {
    Q_OBJECT            // ← sem isto, nada de signals/slots nesta classe
private slots:
    void tick();
};
```

`Q_OBJECT` apenas declara; quem **define** é o moc.

### 6. As três ferramentas: moc, uic, rcc

O Qt vem com geradores de código que rodam **antes** do compilador:

| Ferramenta | Entrada | Saída | Para quê |
|---|---|---|---|
| **moc** (*meta-object compiler*) | `.hpp` com `Q_OBJECT` | `moc_<nome>.cpp` | implementa a tabela de metaobjetos: signals, slots, `qobject_cast` |
| **uic** (*user interface compiler*) | `.ui` (XML do Qt Designer) | `ui_<nome>.h` | uma classe `Ui::X` que cria os widgets em código |
| **rcc** (*resource compiler*) | `.qrc` | `.cpp` | embute ícones/arquivos no binário — **não usado aqui** |

O `meson.build` deste subprojeto chama as duas primeiras, em ordem:

```meson
ui_files  = qt6.compile_ui(sources: ['mainwindow.ui'])          # uic
moc_files = qt6.compile_moc(headers: ['mainwindow.hpp',         # moc
                                      'sfml_widget.hpp'],
                            include_directories: include_directories('.'))

executable('qt_academy',
    ['main.cpp', 'mainwindow.cpp', 'sfml_widget.cpp', ui_files, moc_files],
    dependencies: [qt_dep, sfml_dep],
    include_directories: include_directories('.'),
    install: true)
```

> ⚠ **Esquecer um header em `compile_moc` produz o erro de link mais famoso do Qt:**
> `undefined reference to 'vtable for SFMLWidget'`. A classe tem `Q_OBJECT` (que declara métodos
> virtuais do metaobjeto), ninguém gerou o `moc_sfml_widget.cpp` que os define, e o linker não
> acha a vtable. Criou classe nova com `Q_OBJECT`? Acrescente o header à lista de `headers:`.

### 7. O arquivo `.ui` e a convenção `on_<objeto>_<sinal>`

`mainwindow.ui` é XML — o formato do **Qt Designer**, o editor visual de interfaces. O uic o
traduz para `ui_mainwindow.h`, que contém uma classe `Ui::MainWindow` cujo `setupUi(QMainWindow*)`
faz **todos os `new`** e dá nome a cada objeto (`sliderSpeed`, `btnReset`, …). Depois de criar os
widgets, o `setupUi` gerado termina com `QMetaObject::connectSlotsByName(MainWindow);`.

Essa função varre os slots do objeto procurando o padrão **`on_<objectName>_<nomeDoSinal>`** e
conecta sozinha:

```
slot                              conecta-se automaticamente a
─────────────────────────────────────────────────────────────────
on_sliderSpeed_valueChanged(int)  sliderSpeed  → valueChanged(int)
on_btnReset_clicked()             btnReset     → clicked()
on_actionQuit_triggered()         actionQuit   → triggered()
```

É por isso que `mainwindow.cpp` não tem **nenhum** `connect` para os controles: os nomes fazem o
serviço.

> ⚠ **Não chame `connectSlotsByName` você mesmo depois do `setupUi`.** Ela **não** usa
> `Qt::UniqueConnection`: a segunda chamada conecta tudo de novo e cada slot passa a rodar
> **duas vezes por evento** — `on_btnColor_clicked` abriria o diálogo de cor duas vezes seguidas.
> Este subprojeto tinha exatamente essa chamada redundante; ela foi removida, e o comentário no
> lugar dela explica o porquê.

### 8. Handle nativo (`winId`)

Quem cria janelas de verdade é o sistema operacional, e cada janela nativa tem um identificador:
**XID** no X11, **HWND** no Windows, `NSView*` no macOS. Isso é o **handle**.

`QWidget::winId()` devolve o handle nativo do widget (criando um, se ainda não existir). E o SFML
aceita construir uma `sf::RenderWindow` **sobre um handle já existente**:

```cpp
window_.create(sf::WindowHandle(winId()));   // "desenhe dentro daquela janela ali"
```

É o encaixe inteiro deste subprojeto: o Qt fornece o retângulo, o SFML pinta dentro dele.

### 9. Paint engine, back buffer e o "piscar"

O caminho normal de pintura do Qt:

```
Qt precisa repintar
   └─► cria um QPainter apoiado num PAINT ENGINE (raster, OpenGL…)
         └─► pinta num BACK BUFFER fora da tela
               └─► copia (blit) o buffer para a tela, de uma vez
```

O **paint engine** transforma comandos (`drawLine`, `fillRect`) em pixels. O **back buffer** é a
cópia fora da tela — é ele que evita ver a janela sendo desenhada aos pedaços.

Se o SFML pinta direto na janela nativa **e** o Qt pinta a mesma região pelo caminho acima, os
dois se atropelam: sai um quadro do SFML, o Qt copia seu buffer por cima, sai outro quadro… o
olho vê **piscar** (*flickering*). Daí o widget precisar dizer ao Qt: "não pinte aqui".

---

## Por que estudar isto?

O problema é concreto: **SFML e Qt são bons em coisas opostas.**

| | SFML sozinho | Qt sozinho | Qt + SFML (aqui) |
|---|---|---|---|
| desenhar 60 quadros/s | ✅ é o objetivo dele | possível, mas fora do caminho natural | ✅ |
| botão, slider, menu, atalho | ❌ não existe; você desenha à mão | ✅ prontos e nativos | ✅ |
| diálogo de cor / de arquivo do sistema | ❌ | ✅ `QColorDialog`, `QFileDialog` | ✅ |
| barra de status, painel acoplável | ❌ | ✅ | ✅ |
| editar a interface num editor visual | ❌ | ✅ Qt Designer | ✅ |

Qualquer ferramenta que junte "viewport que anima" com "painel de controle" cai nesse encaixe —
editor de fases, ferramenta de arte, painel de robótica —, e o padrão (**um `QWidget` cujo handle
nativo é entregue a um renderizador externo**) é o mesmo que embute OpenGL cru, Vulkan ou um
motor de jogo dentro de uma interface Qt.

---

## Estrutura de arquivos

```
qt_sfml/
├── README.md          ← este arquivo
├── main.cpp           ← QApplication, MainWindow, app.exec() — 29 linhas
├── mainwindow.hpp     ← classe MainWindow: Ui::MainWindow + os slots on_*
├── mainwindow.cpp     ← setupUi, insere o SFMLWidget, implementa os slots
├── mainwindow.ui      ← XML do Qt Designer: menu, dock, sliders, botões
│                        (o uic gera ui_mainwindow.h — NUNCA edite o gerado)
├── sfml_widget.hpp    ← classe SFMLWidget: o widget pintado pelo SFML
├── sfml_widget.cpp    ← atributos, showEvent, resizeEvent, tick, setters
└── meson.build        ← as três etapas: uic → moc → executable
```

---

## O programa, parte por parte

### 1. `main.cpp` — o event loop

```cpp
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);   // sem isto, nenhum widget pode existir
    app.setApplicationName("Academy");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("cpp_academy");

    MainWindow window;              // na pilha: sem pai, o dono é o main
    window.show();
    return app.exec();              // event loop; só retorna ao fechar a janela
}
```

### 2. Os três atributos, e o paint engine nulo

```cpp
SFMLWidget::SFMLWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_PaintOnScreen);       // "esta área é pintada direto na tela,
                                              //  por outra pessoa"
    setAttribute(Qt::WA_OpaquePaintEvent);    // "eu cubro cada pixel; não limpe antes"
    setAttribute(Qt::WA_NoSystemBackground);  // "não preencha com a cor de fundo"

    setMinimumSize(500, 400);
    connect(&timer_, &QTimer::timeout, this, &SFMLWidget::tick);   // o game loop
}
```

E, no header: `QPaintEngine* paintEngine() const override { return nullptr; }`

| Linha | O que o Qt deixa de fazer |
|---|---|
| `WA_PaintOnScreen` | deixa de pintar num back buffer e copiar; a região é da janela nativa |
| `WA_OpaquePaintEvent` | deixa de apagar a região antes de mandar repintar |
| `WA_NoSystemBackground` | deixa de preencher a região com a cor de fundo do tema |
| `paintEngine() → nullptr` | deixa de conseguir criar um `QPainter` para este widget |

O `nullptr` é a garantia mais forte das quatro: sem paint engine, **nenhum** `QPainter` pode ser
construído sobre o widget. Uma tentativa gera o aviso `QWidget::paintEngine: Should no longer be
called` e não desenha nada — nunca uma cor por cima do SFML.

### 3. `showEvent` — criar a janela SFML no momento certo

```cpp
void SFMLWidget::showEvent(QShowEvent*) {
    if (!initialized_) {
        window_.create(sf::WindowHandle(winId()));   // ← SFML dentro do widget

        ball_.setRadius(radius_);                    // radius_ = 30
        ball_.setFillColor(color_);                  // (100, 200, 255)
        ball_.setOrigin(radius_, radius_);           // origem no CENTRO do círculo
        ball_.setPosition(width() / 2.f, height() / 2.f);

        velocity_ = { speed_, speed_ * 0.7f };       // diagonal, 200 px/s
        timer_.start(1000 / 60);                     // 16 ms ≈ 60 quadros/s
        initialized_ = true;
    }
}
```

Por que no `showEvent` e não no construtor: `winId()` só devolve um handle **válido e do tamanho
certo** depois que o widget entra na hierarquia de uma janela que vai aparecer. E a guarda
`initialized_` importa porque `showEvent` roda toda vez que o widget volta a ficar visível
(minimizar/restaurar) — sem ela, cada exibição criaria uma `RenderWindow` nova.

`setOrigin(r, r)` põe o ponto de referência no centro do círculo; sem isso `setPosition`
posicionaria o canto superior esquerdo da caixa e toda a colisão ficaria deslocada de um raio.

### 4. `paintEvent` **vazio** — e por quê

```cpp
void SFMLWidget::paintEvent(QPaintEvent*) {
    // vazio intencionalmente
    // o SFML controla o rendering desta área via tick()
}
```

> ⚠ **Não delegue o desenho ao `paintEvent`.** É tentador escrever `void paintEvent(...) {
> tick(); }` — e está errado. `paintEvent` roda **quando o Qt decide** repintar a região (janela
> exposta, redimensionada, descoberta); com os três atributos acima o Qt considera que a região
> não é dele e praticamente nunca pede repintura. A animação travaria e só andaria quando você
> mexesse na janela. A **única** fonte de quadros aqui é o `QTimer`.

### 5. O game loop: o `QTimer` que substitui o `while(true)`

```cpp
connect(&timer_, &QTimer::timeout, this, &SFMLWidget::tick);   // no construtor
timer_.start(1000 / 60);                                       // no showEvent
```

```cpp
void SFMLWidget::tick() {
    constexpr float dt = 1.f / 60.f;
    ball_.move(velocity_ * dt);                  // física: posição += v · dt

    const auto  pos = ball_.getPosition();
    const float r   = ball_.getRadius();
    const float w   = static_cast<float>(width());
    const float h   = static_cast<float>(height());

    if (pos.x - r < 0.f || pos.x + r > w) velocity_.x *= -1.f;   // rebate
    if (pos.y - r < 0.f || pos.y + r > h) velocity_.y *= -1.f;

    window_.clear(sf::Color(30, 30, 30));
    window_.draw(ball_);
    window_.display();                           // troca os buffers do SFML
}
```

```
QTimer (16 ms)  ──timeout()──►  SFMLWidget::tick()
      ▲                                │  move, colide, desenha
      └────── app.exec() volta a ◄─────┘
              processar eventos (cliques, sliders, resize)
```

Entre dois `tick()` o Qt tem 16 ms livres para o resto — é o que mantém a interface viva.

> ⚠ `dt` é **fixo** em 1/60 s, não medido. Se um quadro atrasar, a bola anda menos: o tempo do
> jogo desacelera junto com a máquina. É a escolha certa para um demo determinístico; para
> animação atrelada ao relógio de parede, meça o intervalo com `QElapsedTimer`.

### 6. `resizeEvent` — janela **e** view

```cpp
void SFMLWidget::resizeEvent(QResizeEvent*) {
    if (!initialized_) return;
    const auto w = static_cast<unsigned>(width());
    const auto h = static_cast<unsigned>(height());

    window_.setSize({w, h});                        // 1) o destino
    window_.setView(sf::View(sf::FloatRect(         // 2) o mundo mapeado nele
        0.f, 0.f, static_cast<float>(w), static_cast<float>(h))));
}
```

A **view** do SFML é o retângulo do mundo que a janela mostra. `setSize` muda só o destino; se a
view continuar com o retângulo antigo, o SFML **estica** o desenho para caber — e a bola passa a
bater visualmente longe da borda, porque a física do `tick()` usa `width()`/`height()` em pixels
reais.

> ⚠ Num programa SFML normal quem faz esse acerto é o tratamento de `sf::Event::Resized`. Aqui
> esse evento **nunca chega** (próxima seção), então o ajuste é feito à mão pelo `resizeEvent`.

### 7. ⚠ A armadilha nº 1: a janela SFML **não recebe eventos**

```
X11 / Windows  ──evento de mouse/teclado──►  a janela é do QT
                                                  │
                                      Qt entrega ao widget:
                                      mousePressEvent, keyPressEvent…

sf::RenderWindow criada sobre winId()  ──►  NÃO recebe nada
                                            window_.pollEvent(e) → sempre false
```

A `sf::RenderWindow` foi criada sobre um handle que **pertence ao Qt**; quem está inscrito para
receber os eventos daquela janela nativa é o Qt. Na prática:

- `while (window_.pollEvent(e)) { ... }` nunca entra no laço — **não escreva isso**;
- `sf::Mouse::isButtonPressed` / `sf::Keyboard::isKeyPressed` funcionam (consultam o estado
  global do dispositivo, não a fila da janela), mas ignoram foco.

O jeito certo é sobrescrever os manipuladores do **Qt** no `SFMLWidget` (as coordenadas de
`e->position()` já são as do widget, iguais às do SFML):

```cpp
void SFMLWidget::mousePressEvent(QMouseEvent* e) override {
    ball_.setPosition(static_cast<float>(e->position().x()),
                      static_cast<float>(e->position().y()));
}
```

Para receber teclado, o widget ainda precisa aceitar foco: `setFocusPolicy(Qt::StrongFocus)`.

### 8. `set_speed` — preservar a direção com `std::hypot`

O slider entrega um número de 50 a 600. A implementação ingênua seria:

```cpp
// ❌ errado: joga a bola de volta na diagonal original a cada movimento do slider
void SFMLWidget::set_speed(float s) { velocity_ = { s, s * 0.7f }; }
```

O código real separa **direção** de **magnitude**:

```cpp
void SFMLWidget::set_speed(float speed) {
    speed_ = speed;
    const float len = std::hypot(velocity_.x, velocity_.y);   // magnitude atual
    if (len > 0.f)
        velocity_ = (velocity_ / len) * speed_;               // normaliza e reescala
}
```

`velocity_ / len` é o vetor **unitário** na direção atual; multiplicá-lo por `speed_` devolve a
mesma direção com o novo comprimento:

```
v = (120, 84)   →   len = hypot(120, 84) = 146.5
v/len = (0.82, 0.57)          ← só direção, comprimento 1
(v/len) * 300 = (246, 172)    ← nova magnitude, mesma direção
```

`std::hypot(x, y)` em vez de `std::sqrt(x*x + y*y)` evita estouro e perda de precisão quando um
dos termos é muito grande ou muito pequeno. A guarda `len > 0.f` evita divisão por zero.

### 9. `MainWindow` — Designer, `setupUi` e os slots automáticos

```cpp
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    ui_.setupUi(this);                  // cria menu, dock, sliders, botões, status bar

    // SFMLWidget é classe nossa: o Designer não a conhece. Inserimos à mão.
    sfml_ = new SFMLWidget(ui_.centralwidget);
    auto* layout = new QHBoxLayout(ui_.centralwidget);
    layout->setContentsMargins(0, 0, 0, 0);   // sem margem: o SFML ocupa tudo
    layout->addWidget(sfml_);

    statusBar()->showMessage("Pronto — use os controles para ajustar a simulação");
}
```

Os slots são ligados **pelo nome**, sem um `connect` sequer:
```cpp
void MainWindow::on_sliderSpeed_valueChanged(int value) {
    ui_.labelSpeed->setText(QString("%1 px/s").arg(value));
    sfml_->set_speed(static_cast<float>(value));
}

void MainWindow::on_btnColor_clicked() {
    QColor color = QColorDialog::getColor(QColor(100, 200, 255), this,
                                          "Escolher cor da bola");
    if (color.isValid()) {                       // ⚠ inválido = usuário cancelou
        sfml_->set_color(sf::Color(static_cast<sf::Uint8>(color.red()),
                                   static_cast<sf::Uint8>(color.green()),
                                   static_cast<sf::Uint8>(color.blue())));
        statusBar()->showMessage(QString("Cor alterada para %1").arg(color.name()));
    }
}

void MainWindow::on_actionReset_triggered() { sfml_->reset(); }   // menu Simulation
void MainWindow::on_actionQuit_triggered()  { close(); }          // Ctrl+Q
```

O que vem de `mainwindow.ui` (e portanto do `setupUi`):
| Objeto | Tipo | Configuração no `.ui` |
|---|---|---|
| `menuSimulation` | `QMenu` | itens **Resetar** e **Sair** |
| `actionQuit` | `QAction` | atalho `Ctrl+Q` |
| `dockControls` | `QDockWidget` | título "Controles", acoplado à direita |
| `sliderSpeed` | `QSlider` | 50 … 600, valor inicial 200 |
| `sliderRadius` | `QSlider` | 10 … 100, valor inicial 30 |
| `btnColor` / `btnReset` | `QPushButton` | "Escolher cor" / "Resetar posição" |
| `labelSpeed` / `labelRadius` | `QLabel` | mostram o valor atual |

> ⚠ **`QColorDialog::getColor` devolve cor inválida quando o usuário cancela.** Sem o
> `if (color.isValid())`, a bola ficaria preta a cada cancelamento (um `QColor` inválido converte
> para preto).

> ⚠ **Erro de digitação no nome do slot é silencioso.** `on_sliderSpead_valueChanged` compila,
> linka e nunca é chamado — é o preço de conectar por convenção de nome. Para inspecionar as
> conexões: `QT_LOGGING_RULES="qt.core.qobject.connect=true"`.

---

## Exemplo mínimo — um arquivo, sem `.ui`, sem moc

O menor programa que prova a técnica. Sem bola, sem sliders. Ele **não tem `Q_OBJECT`**: em vez
de um slot, o `QTimer` é ligado a uma lambda — e sem `Q_OBJECT` não é preciso rodar o moc.

```cpp
// mini_qt_sfml.cpp
#include <QApplication>
#include <QWidget>
#include <QTimer>
#include <SFML/Graphics.hpp>

class MiniSFML : public QWidget {
public:
    explicit MiniSFML(QWidget* parent = nullptr) : QWidget(parent) {
        setAttribute(Qt::WA_PaintOnScreen);      // outra pessoa pinta aqui
        setAttribute(Qt::WA_OpaquePaintEvent);   // não limpe antes
        setAttribute(Qt::WA_NoSystemBackground); // não pinte o fundo
        resize(400, 300);
        connect(&timer_, &QTimer::timeout, this, [this] { frame(); });
    }

protected:
    QPaintEngine* paintEngine() const override { return nullptr; }
    void paintEvent(QPaintEvent*) override {}   // vazio: quem pinta é o frame()

    void showEvent(QShowEvent*) override {
        if (started_) return;
        window_.create(sf::WindowHandle(winId()));  // ← o truque inteiro
        timer_.start(16);                           // ~60 quadros por segundo
        started_ = true;
    }

private:
    void frame() {
        hue_ = (hue_ + 1) % 256;
        window_.clear(sf::Color(static_cast<sf::Uint8>(hue_), 40, 120));
        window_.display();
    }

    sf::RenderWindow window_;
    QTimer           timer_;
    bool             started_ = false;
    int              hue_     = 0;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MiniSFML w;
    w.show();
    return app.exec();
}
```

Compilar fora do Meson, com o `pkg-config` que o `make configure` já deixou em `build/`:

```bash
export PKG_CONFIG_PATH=$PWD/build
g++ -std=c++20 -fPIC mini_qt_sfml.cpp -o /tmp/mini_qt_sfml \
    $(pkg-config --cflags --libs Qt6Widgets sfml-graphics)
QT_QPA_PLATFORM=xcb /tmp/mini_qt_sfml
```

Aparece uma janela de 400×300 cujo fundo varre o vermelho continuamente. Agora **comente** `setAttribute(Qt::WA_PaintOnScreen);` e rode de novo: o Qt volta a gerenciar a
região, e a animação passa a piscar ou a sumir conforme o driver. É a demonstração mais direta do
que os atributos fazem.

---

## Portabilidade: a parte frágil

`WA_PaintOnScreen` + `paintEngine() == nullptr` é, na prática, uma técnica **X11**:

| Plataforma | Situação |
|---|---|
| Linux/X11 (ou XWayland) | ✅ funciona — é o caso testado aqui |
| Linux/Wayland nativo | ⚠ o compositor não expõe subjanelas assim; use `QT_QPA_PLATFORM=xcb` |
| Windows | ⚠ costuma funcionar (o `HWND` do widget é janela de verdade), com ressalvas de DPI |
| macOS | ❌ `WA_PaintOnScreen` é documentado como sem efeito no Cocoa |

**A alternativa portátil**: renderizar num `sf::RenderTexture` (fora da tela) e mostrar a textura
dentro de um `QOpenGLWidget`. Custa uma cópia por quadro e ganha em portabilidade. Este
subprojeto ensina uma **técnica de encaixe**, com prazo de validade; o que não expira é o
mecanismo — quem pinta, quem recebe evento e quem é dono da janela.

---

## Saída esperada

`./dist/bin/qt_academy` (com `QT_QPA_PLATFORM=xcb` se a sessão for Wayland) abre uma janela de
900×600 chamada **"Academy — Qt + SFML"**:

```
┌───────────────────────────────────────────────────────────────┐
│ Simulation                                                    │ ← menu (Resetar, Sair Ctrl+Q)
├──────────────────────────────────────┬────────────────────────┤
│                                      │  Controles        [x]  │ ← dock à direita
│      fundo cinza escuro (30,30,30)   │  ┌ Velocidade ───────┐ │
│                                      │  │    200 px/s       │ │
│              ●  ← bola azul-clara    │  │ ────●──────────── │ │
│                 (100,200,255), r=30  │  └───────────────────┘ │
│                 quicando na diagonal │  ┌ Raio ───── 30 px ─┐ │
│                                      │  │ ──●────────────── │ │
│                                      │  └───────────────────┘ │
│                                      │  [ Escolher cor ]      │
├──────────────────────────────────────┴────────────────────────┤
│ Pronto — use os controles para ajustar a simulação            │ ← status bar
└───────────────────────────────────────────────────────────────┘
```

Abaixo de "Escolher cor" vem o botão **Resetar posição**. No terminal, o único texto normal é o
aviso do SFML sobre *vertical sync*.

## Como validar

| # | O que fazer | O que tem de acontecer |
|---|---|---|
| 1 | observar a bola por alguns segundos | quica nas quatro bordas, sem sair da área nem piscar |
| 2 | arrastar `sliderSpeed` de 200 para 600 | **acelera mantendo a direção** — não muda de rumo (é o `hypot`) |
| 4 | **Escolher cor** → escolher → OK | a bola muda de cor; a status bar mostra `Cor alterada para #rrggbb` |
| 6 | **Resetar posição** | a bola volta ao centro; status bar: `Posição resetada` |
| 8 | `Ctrl+Q` | a janela fecha e o processo termina com código 0 |
| 9 | arrastar a borda e agrandar a janela | a área SFML acompanha **sem esticar**; a bola rebate na borda nova (é o `setView`) |
| 10 | com a bola andando, arrastar o dock "Controles" para o outro lado | a animação **não trava** — o event loop está livre |

O item 10 é o teste do `QTimer`: com um `while(true)` no lugar dele, arrastar o dock congelaria a
janela. Conferir o build e o código gerado:

```bash
make build
ldd ./dist/bin/qt_academy | grep -E "Qt6|sfml"                    # linkou com os dois
grep -n "connectSlotsByName" build/src/qt_sfml/qt_academy.p/ui_mainwindow.h
grep -n "on_sliderSpeed_valueChanged" build/src/qt_sfml/qt_academy.p/moc_mainwindow.cpp
```

As duas últimas linhas mostram, no código gerado, os dois mecanismos da seção 7: a chamada de
`connectSlotsByName` que o uic escreve, e o nome do slot como **texto** dentro da tabela do moc.

Exercícios: (1) implemente `mousePressEvent` para reposicionar a bola — antes, confirme que
`window_.pollEvent()` no `tick()` nunca dispara; (2) troque o `dt` fixo por `QElapsedTimer`;
(3) comente `'sfml_widget.hpp'` da lista `headers:` do `meson.build` e leia o erro de vtable.

---

## Referências

Neste repositório:

- [`../qt_sfml2/`](../qt_sfml2/) — o mesmo encaixe, com física da Bullet3 por trás
- [`../sfml_bullet/`](../sfml_bullet/) e [`../sfml_bullet2/`](../sfml_bullet2/) — SFML puro, sem Qt
- [`../collision/`](../collision/) — a matemática de colisão que o `tick()` faz de forma ingênua
- [`../smart-pointers/`](../smart-pointers/) — posse em C++, para comparar com o pai/filho do Qt

Fontes canônicas:

- *Qt Widgets* (Qt 6): `QWidget`, `QMainWindow`, `QDockWidget`, `QWidget::winId()`,
  `QWidget::paintEngine()` e a lista de `Qt::WidgetAttribute`
- *Signals & Slots*, *Using the Meta-Object Compiler (moc)*, *Using Qt Designer* e
  *Automatic Connections* (`connectSlotsByName`) — manual do Qt
- SFML 2.6, tutorial *Integrating SFML into your own window* — documenta que uma janela criada
  sobre handle externo não recebe eventos
