# qt_sfml2 — Sandbox 3D com Qt, SFML e Bullet

## O que é este projeto?

Uma **caixa cúbica em wireframe** com esferas, caixas e cilindros quicando dentro, dentro
de uma janela Qt de verdade (menus, dock de controles, atalhos). A física é do **Bullet**,
os pixels são do **SFML**, a interface é do **Qt** — e a projeção 3D→2D é escrita à mão,
sem OpenGL e sem nenhuma biblioteca de matemática.

```bash
make configure            # só na primeira vez
source build/conanbuild.sh
meson compile -C build qt_sfml2
./dist/bin/qt_sfml2
```

O binário se chama exatamente `qt_sfml2` — é o nome do alvo em [`meson.build`](meson.build)
e o arquivo instalado em `dist/bin/`.

**Controles reais** (extraídos de `sfml_widget.cpp` e de [`mainwindow.ui`](mainwindow.ui)):

| Entrada | Efeito |
|---|---|
| **Arrastar** com o botão esquerdo sobre a cena | **orbita a câmera** (muda `cam_theta_` e `cam_phi_`) |
| **Roda do mouse** | zoom — aproxima/afasta a câmera (`cam_radius_`, limitado a 5…35 m) |
| Botões **Esfera / Caixa / Cilindro** no dock | criam um objeto em posição e velocidade aleatórias |
| Teclas **1 / 2 / 3** | os mesmos três, via ações de menu |
| Combo **Gravidade** | Normal (−9,82) · Zero-G (0) · Fraca (−2) · Lunar (−1,62) · Pesada (−20) |
| Slider **Elasticidade** | restituição de todos os objetos, de 0,00 a 1,00 |
| Botão **💥 Explosão** | lança todos os objetos para longe do centro |
| Botão **⏸ Pausar** / tecla **Espaço** | congela a física (a câmera continua orbitando) |
| Botão **🗑 Limpar** | remove todos os objetos |
| Menu *Simulação → Reiniciar* | limpa e recria os 12 objetos iniciais |
| **Ctrl+Q** | sai |

> ⚠ **Clicar na cena NÃO cria objetos.** O clique orbita a câmera. Objetos nascem apenas
> pelos botões do dock e pelos atalhos 1/2/3. A razão está explicada em
> "A armadilha central" mais abaixo, e é o motivo pedagógico deste subprojeto existir.

---

## Antes de começar: os conceitos que você precisa

Nada além de C++ básico é pressuposto. Cada termo usado adiante é definido aqui.

### O lado Qt

**QWidget** — qualquer retângulo na tela: um botão, um rótulo, a janela inteira. Widgets
formam uma árvore; o pai destrói os filhos.

**Event loop** — Qt não tem `while (rodando) { ... }` escrito por você. `app.exec()` entra
num laço que dorme até chegar um evento do sistema (clique, tecla, redimensionamento,
timer) e o entrega ao widget certo. Seu código só roda em resposta a eventos. Se você
bloquear dentro de um handler, a interface inteira congela.

**QTimer** — a forma de "rodar algo periodicamente" sem bloquear: ele injeta um evento
`timeout` no laço a cada N milissegundos. É assim que este projeto tem um *game loop* a
60 Hz sem travar a UI.

**signals & slots** — o mecanismo de notificação do Qt. Um objeto **emite** um sinal
(`clicked()`), zero ou mais **slots** são chamados. O emissor não sabe quem escuta:

```
[QPushButton btnExplode] ──clicked()──▶ [MainWindow::on_btnExplode_clicked()]
       emissor                                        receptor
```

**`Q_OBJECT`** — a macro que toda classe com sinais/slots precisa declarar. Ela é apenas um
marcador; o código real é gerado por outro programa.

**moc** (*Meta-Object Compiler*) — lê o cabeçalho, encontra `Q_OBJECT`, e **gera um .cpp
extra** com as tabelas que permitem chamar métodos pelo nome em tempo de execução. C++ não
tem reflexão; o moc a fabrica. Em [`meson.build`](meson.build):

```meson
moc_files = qt6.compile_moc(headers: ['./mainwindow.hpp', './sfml_widget.hpp'], ...)
```

> ⚠ Esqueceu o cabeçalho na lista do `compile_moc`? O erro não é de compilação — é de
> **link**: `undefined reference to vtable for MainWindow`. É a assinatura desse erro.

**.ui e uic** — [`mainwindow.ui`](mainwindow.ui) é um XML que descreve a janela (menus,
dock, grupos, botões, slider). O **uic** (*User Interface Compiler*) o traduz em um header
`ui_mainwindow.h` com uma struct `Ui::MainWindow` cheia de ponteiros. `ui_.setupUi(this)`
constrói tudo.

**`connectSlotsByName`** — a mágica que `setupUi` faz no fim: para cada widget do `.ui`, ela
procura métodos chamados `on_<nomeDoWidget>_<nomeDoSinal>` e os conecta **automaticamente**.
Por isso não existe nenhum `connect(...)` para os botões neste projeto:

```
widget no .ui:  name="btnExplode"        sinal: clicked()
slot em C++:    void on_btnExplode_clicked()
                    └───┬────┘ └──┬───┘
                   nome exato   sinal
```

> ⚠ Um erro de digitação no nome quebra a conexão **em silêncio**: o botão simplesmente não
> faz nada, sem erro de compilação. No máximo aparece um aviso em tempo de execução.

### O lado SFML dentro do Qt

**`sf::RenderWindow`** normalmente cria a própria janela do sistema. Mas ela também aceita
um *handle* nativo já existente e desenha **dentro** dele:

```cpp
window_.create(sf::WindowHandle(winId()));   // winId() = handle nativo do QWidget
```

Para que os dois não briguem pelo mesmo retângulo, três atributos e uma sobrescrita:

| O quê | Por quê |
|---|---|
| `setAttribute(Qt::WA_PaintOnScreen)` | Qt desenha direto na janela nativa, sem buffer próprio |
| `setAttribute(Qt::WA_OpaquePaintEvent)` | Qt não precisa limpar o fundo — o SFML pinta tudo |
| `setAttribute(Qt::WA_NoSystemBackground)` | evita o flash de fundo antes do primeiro frame |
| `QPaintEngine* paintEngine() const override { return nullptr; }` | desliga o pintor do Qt para este widget |

E `paintEvent` fica **vazio de propósito**: quem desenha é o `tick()` do `QTimer`.

### O lado Bullet

**Corpo rígido** — objeto que não deforma; todo o estado é posição, orientação, velocidade
linear e angular. `btRigidBody` = uma forma (`btCollisionShape`) + onde ele está
(`btMotionState`) + massa. **Massa 0 significa estático**, não leve.

**`btStaticPlaneShape(normal, constant)`** — um plano **infinito**, definido pela sua
normal e por um deslocamento ao longo dela. Não tem volume, não tem massa e nunca se move;
é a forma mais barata para chão e paredes.

**Restituição** — o quique, de 0 (massa de modelar) a 1 (borracha ideal). É o que o slider
controla.

**`stepSimulation(dt, maxSubSteps, fixedStep)`** — avança a física. `dt` é o tempo real;
internamente ele é fatiado em passos fixos de `fixedStep`, porque passo fixo é o que torna
a simulação estável e reproduzível.

**Broadphase / narrowphase / solver** — as três fases de um passo:

```
BROADPHASE  (btDbvtBroadphase)      quais pares estão PERTO?  → árvore de AABB
NARROWPHASE (btCollisionDispatcher) esses pares se TOCAM? onde?
SOLVER      (btSequentialImpulse…)  que impulsos separam tudo sem violar as regras?
```

### O lado 3D — a matemática que este projeto escreve à mão

**Câmera esférica (orbital)** — em vez de guardar "posição + para onde olha", guarda três
números e olha **sempre para a origem**:

```
        y
        │        θ (theta) = azimute: rotação em torno do eixo vertical
        │        φ (phi)   = elevação: 0 = polo norte, π = polo sul
   ●    │            r     = distância até o centro
  câmera│
    ╲ φ │                 pos = r · ( sinφ·sinθ , cosφ , sinφ·cosθ )
     ╲  │
      ╲ │        ┌───────┐
       ╲│        │ cubo  │       arrastar o mouse muda θ e φ
────────●────────└───────┘       a roda muda r
       origem
```

**Base da câmera (right / up / forward)** — três vetores unitários perpendiculares entre si
que definem "direita", "cima" e "frente" **do ponto de vista da câmera**. Com eles, medir a
coordenada de um ponto no espaço da câmera é só fazer produtos escalares.

**Projeção perspectiva** — a razão de coisas distantes parecerem menores é uma única
divisão: divide-se a posição lateral pela profundidade.

```
     tela                      olho ●
      │                            ╱│╲
   ───┼─── objeto perto  ─────────╱ │ ╲
      │      (grande)            ╱  │  ╲
   ───┼─── objeto longe ────────╱   │   ╲
      │      (pequeno)
                                 x_tela = x_camera / z_camera
```

**Near plane** — a profundidade mínima. Um ponto com z ≤ 0 está atrás da câmera; dividir
por ele produz coordenadas absurdas ou infinitas. Grampeia-se z num valor pequeno positivo
(`near_plane_ = 0.1f`).

**Algoritmo do pintor** — sem *z-buffer*, a única forma de acertar o que aparece na frente
é **desenhar do mais distante para o mais próximo**, como um pintor cobrindo camadas. É
exatamente o que `draw_objects()` faz: ordena por profundidade antes de desenhar.

**Depth cueing** — pista visual de profundidade: o que está longe fica **menor e mais
escuro**. Aqui, `depth_scale()` devolve um fator entre 1,0 (perto) e 0,3 (longe) que
multiplica as componentes de cor.

---

## Por que estudar isto?

Porque este subprojeto é sobre **fazer três bibliotecas com modelos de mundo incompatíveis
conviverem sem que nenhuma saiba da existência das outras**:

| Biblioteca | O que ela acha que controla | Como se cede o controle |
|---|---|---|
| **Qt** | o laço principal e todos os eventos de entrada | `app.exec()` manda; o resto vira slot |
| **Bullet** | o avanço do tempo, em passos fixos | um `QTimer` chama `stepSimulation` |
| **SFML** | uma janela inteira e seus eventos | recebe um `winId()` emprestado e **não** recebe eventos |

E porque cada uma dessas cessões tem um preço que só aparece em código:

| Sem entender isto | Com isto |
|---|---|
| `pollEvent` num SFML embarcado — o clique nunca chega, e você depura pelo lado errado | a entrada vem toda dos handlers do Qt |
| desenhar 3D exige OpenGL, GLM, uma engine | 60 linhas de trigonometria projetam qualquer cena |
| a UI congela enquanto a física roda | `QTimer` a 60 Hz + timer de status a 4 Hz |
| lógica de física, desenho e botões no mesmo arquivo | três camadas, cada uma ignorando as outras duas |

---

## Estrutura de arquivos

```
src/qt_sfml2/
├── main.cpp              ← 25 linhas: QApplication, MainWindow, exec()
├── mainwindow.ui         ← 8,5 KB de XML: menus, dock "Controles", 3 grupos de
│                            botões, combo de gravidade, slider, labels de status.
│                            Compilado pelo uic → ui_mainwindow.h
├── mainwindow.hpp        ← camada UI. NÃO inclui Bullet nem SFML. Só slots
├── mainwindow.cpp           on_<widget>_<sinal> e o timer de status a 4 Hz
├── sfml_widget.hpp       ← camada render+integração: embarca a sf::RenderWindow,
├── sfml_widget.cpp          473 linhas — quase todas de projeção 3D→2D, desenho
│                            de sólidos com primitivas 2D, e handlers de mouse
├── physics_world.hpp     ← camada física: só Bullet. Sala de 6 planos estáticos,
├── physics_world.cpp        PhysicsObject (corpo + cor + tipo), explode, gravidade
├── meson.build           ← qt6.compile_ui + qt6.compile_moc + executable('qt_sfml2')
└── README.md
```

---

## Arquitetura: três camadas, zero vazamento

```
┌──────────────────────────────────────────────────────────────┐
│ MainWindow            "NÃO conhece Bullet3, NÃO conhece SFML" │
│   slots on_btnAddSphere_clicked, on_comboGravity_…, …         │
└───────────────────────────┬──────────────────────────────────┘
                            │ só chama métodos públicos:
                            │ spawn_object, clear_objects, explode,
                            │ set_gravity, set_restitution,
                            │ set_paused, object_count
┌───────────────────────────▼──────────────────────────────────┐
│ SFMLWidget            só renderização + projeção + mouse      │
│   project3D, depth_scale, update_camera_matrix, draw_*        │
└───────────────────────────┬──────────────────────────────────┘
                            │ só lê objects(), half_room()
┌───────────────────────────▼──────────────────────────────────┐
│ PhysicsWorld          só Bullet — nenhuma linha de Qt         │
│   add_object, clear_objects, explode, set_gravity, step       │
└──────────────────────────────────────────────────────────────┘
```

A única concessão a esse isolamento é deliberada e vale a pena entender:

```cpp
struct PhysicsObject {
    std::unique_ptr<btCollisionShape>  shape;
    std::unique_ptr<btMotionState>     motion_state;
    std::unique_ptr<btRigidBody>       body;

    ShapeType   type;          // ← metadado de RENDER dentro da struct de FÍSICA
    sf::Color   color;         // ←
    float       half_extent;   // ←
};
```

Por que cor e tipo moram junto do corpo físico? Porque a alternativa é pior: um segundo
vetor paralelo de dados visuais, indexado pela mesma posição, que precisa ser mantido em
sincronia em toda inserção e remoção — um par de vetores que podem divergir é uma fonte
inesgotável de bugs. Guardar três campos triviais junto do corpo é o **acoplamento mínimo**
que faz o renderizador saber o que desenhar sem consultar mais ninguém. `PhysicsWorld`
continua sem incluir Qt, e inclui apenas `<SFML/Graphics/Color.hpp>` — um header de tipo
de dado, não de renderização.

---

## A armadilha central: o `pollEvent` que não recebe nada

Este é o erro clássico de quem integra Qt e SFML, e a linha mais importante do projeto:

```cpp
// sfml_widget.cpp, dentro de tick()
sf::Event evt;
while (window_.pollEvent(evt)) {}     // ← laço VAZIO, de propósito
```

> ⚠ **Um `sf::RenderWindow` criado sobre o `winId()` de um QWidget nunca recebe eventos de
> entrada.** A janela nativa pertence ao Qt; é o Qt que a registra no sistema e consome
> teclado e mouse. O `pollEvent` do SFML devolverá, na prática, apenas eventos internos —
> **nunca** um `MouseButtonPressed` seu. Escrever
> `if (evt.type == sf::Event::MouseButtonPressed) spawn(...)` aqui compila, roda, e
> simplesmente **não funciona** — e você vai depurar a física, quando o problema é o dono
> da fila de eventos.
>
> O laço vazio existe por um motivo real: **drenar a fila**. Se ninguém consumir os eventos
> que chegam, a fila cresce e, no X11, a aplicação acaba marcada como "não responde".

Toda a entrada vem, então, dos handlers virtuais do `QWidget`:

```cpp
void SFMLWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) { mouse_dragging_ = true; mouse_last_ = e->pos(); }
}

void SFMLWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!mouse_dragging_) return;
    QPoint delta = e->pos() - mouse_last_;
    cam_theta_ -= delta.x() * 0.008f;                          // arrastar horizontal → azimute
    cam_phi_    = std::max(0.1f, std::min(PI - 0.1f,           // arrastar vertical → elevação
                          cam_phi_ - delta.y() * 0.008f));
    mouse_last_ = e->pos();
    update_camera_matrix();
}

void SFMLWidget::wheelEvent(QWheelEvent* e) {
    cam_radius_ -= e->angleDelta().y() * 0.02f;
    cam_radius_  = std::max(5.f, std::min(35.f, cam_radius_)); // zoom limitado
    update_camera_matrix();
}
```

O grampeamento de `cam_phi_` em `[0.1, π − 0.1]` não é estética: **exatamente** no polo, o
vetor `forward` fica paralelo ao `world_up`, o produto vetorial que define o `right` dá zero
e a base da câmera colapsa. (Há um *fallback* em `update_camera_matrix()` para o caso
`rlen < 1e-4`, mas evitar o polo é a defesa de verdade.)

---

## Exemplo mínimo: um cubo e uma câmera que orbita

Antes de olhar as 473 linhas de `sfml_widget.cpp`, este é o núcleo — sem Bullet, sem
MainWindow, sem física. Cabe em ~35 linhas e já dá para orbitar:

```cpp
class MiniWidget : public QWidget {
    Q_OBJECT
public:
    MiniWidget() {
        setAttribute(Qt::WA_PaintOnScreen);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);
        connect(&timer_, &QTimer::timeout, this, [this]{ redraw(); });
    }
protected:
    QPaintEngine* paintEngine() const override { return nullptr; }
    void showEvent(QShowEvent*) override {
        win_.create(sf::WindowHandle(winId()));
        timer_.start(16);
    }
    void mousePressEvent(QMouseEvent* e) override { last_ = e->pos(); }
    void mouseMoveEvent (QMouseEvent* e) override {
        theta_ -= (e->pos().x() - last_.x()) * 0.01f;
        last_ = e->pos();
    }
private:
    sf::Vector2f project(float x, float y, float z) {
        // câmera em (r·sinθ, 3, r·cosθ) olhando para a origem — versão simplificada
        float cx = 12.f * std::sin(theta_), cz = 12.f * std::cos(theta_);
        float fx = -cx / 12.f, fz = -cz / 12.f;      // forward (já normalizado)
        float rx =  fz,        rz = -fx;             // right = forward × (0,1,0)
        float dx = x - cx, dy = y - 3.f, dz = z - cz;
        float vx = dx*rx + dz*rz;                    // coordenada lateral
        float vz = dx*fx + dz*fz;                    // profundidade
        if (vz < 0.1f) vz = 0.1f;                    // near plane
        float s = (height() / 2.f) / vz;             // ← a divisão perspectiva
        return { width()/2.f + vx*s, height()/2.f - dy*s };
    }
    void redraw() {
        win_.clear(sf::Color(15,15,18));
        const float h = 3.f;
        btVector3 v[8] = {{-h,-h,-h},{h,-h,-h},{h,-h,h},{-h,-h,h},
                          {-h, h,-h},{h, h,-h},{h, h,h},{-h, h,h}};
        static const int e[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},
                                     {6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        for (auto& ed : e) {
            sf::Vertex l[2] = {
                { project(v[ed[0]].x(), v[ed[0]].y(), v[ed[0]].z()), sf::Color::White },
                { project(v[ed[1]].x(), v[ed[1]].y(), v[ed[1]].z()), sf::Color::White }};
            win_.draw(l, 2, sf::Lines);
        }
        win_.display();
    }
    sf::RenderWindow win_; QTimer timer_; QPoint last_; float theta_ = 0.f;
};
```

Um cubo em wireframe que gira quando você arrasta o mouse. **Tudo o mais neste subprojeto é
elaboração disto.**

---

## A projeção 3D→2D, linha por linha

### Passo 1 — a posição da câmera, de (θ, φ, r)

```cpp
const float sp = std::sin(cam_phi_),   cp = std::cos(cam_phi_);
const float st = std::sin(cam_theta_), ct = std::cos(cam_theta_);
cam_pos_ = { cam_radius_*sp*st, cam_radius_*cp, cam_radius_*sp*ct };
```

Valores iniciais: `cam_theta_ = 0.5`, `cam_phi_ = 0.8`, `cam_radius_ = 22` — a câmera
começa acima e à frente da sala de 10×10×10 m.

### Passo 2 — a base right / up / forward

```cpp
cam_forward_ = -normalize(cam_pos_);                 // aponta para a origem
Vec3f r      = cam_forward_ × (0,1,0);               // produto vetorial com o "cima" do mundo
if (|r| < 1e-4) r = (0,0,1);                         // fallback nos polos
cam_right_   = normalize(r);
cam_up_      = cam_right_ × cam_forward_;
```

Como a câmera olha **sempre para a origem**, `forward` é só a direção oposta à posição.
O truque de sempre: `right` é perpendicular a `forward` e ao "cima do mundo"; `up` é o que
sobra.

### Passo 3 — mundo → espaço da câmera

```cpp
float dx = wp.x() - cam_pos_.x;   // vetor do olho até o ponto
float dy = wp.y() - cam_pos_.y;
float dz = wp.z() - cam_pos_.z;

float vx =  dx*cam_right_.x   + dy*cam_right_.y   + dz*cam_right_.z;   // lateral
float vy = -(dx*cam_up_.x     + dy*cam_up_.y      + dz*cam_up_.z);     // vertical, INVERTIDO
float vz =  dx*cam_forward_.x + dy*cam_forward_.y + dz*cam_forward_.z; // profundidade
```

Cada linha é um **produto escalar**: "quanto deste vetor aponta na direção do eixo `right`?"
— e essa é literalmente a coordenada nesse eixo, porque a base é ortonormal.

O sinal negativo em `vy` é a conversão de convenção: no mundo, Y cresce para cima; em
pixels, Y cresce para baixo.

### Passo 4 — divisão perspectiva e conversão para pixel

```cpp
if (vz < near_plane_) vz = near_plane_;                 // nunca dividir por ≤ 0
float scale = (height() / 2.f) / (fov_scale_ * vz);     // ← o coração da perspectiva
return { width()/2.f + vx*scale, height()/2.f + vy*scale };
```

`fov_scale_` é `tan(FOV/2)`: **menor = mais zoom** (campo de visão estreito, telefoto),
**maior = grande-angular**. O projeto usa 1,0, equivalente a um FOV vertical de 90°.

A escala usa a **altura** da janela, não a largura. Isso é proposital: mantém a projeção
isotrópica (um círculo continua círculo) e faz o FOV vertical ser constante enquanto
redimensionar horizontalmente apenas revela mais cena.

### Passo 5 — depth cueing

```cpp
float t = clamp((vz - 2.f) / 25.f, 0.f, 1.f);
return 1.f - 0.7f * t;                 // 1,0 a 2 m  →  0,3 a 27 m
```

Esse fator multiplica as três componentes de cor. Objetos ao fundo ficam 70% mais escuros —
uma pista de profundidade que a projeção sozinha não dá.

---

## Desenhar sólidos 3D com primitivas 2D do SFML

O SFML não sabe o que é um cubo. Ele sabe desenhar linhas, círculos e polígonos convexos.
Toda a cena é montada com três primitivas:

**A sala** — 8 vértices, 12 arestas, cada uma projetada e desenhada como `sf::Lines`, mais
um `sf::Quads` semitransparente para o chão:

```cpp
std::array<btVector3, 8> v = {{ {-h,-h,-h}, {h,-h,-h}, {h,-h,h}, {-h,-h,h},
                                {-h, h,-h}, {h, h,-h}, {h, h,h}, {-h, h,h} }};
static const int edges[12][2] = { {0,1},{1,2},{2,3},{3,0},      // face de baixo
                                  {4,5},{5,6},{6,7},{7,4},      // face de cima
                                  {0,4},{1,5},{2,6},{3,7} };    // pilares
```

**Esfera** → `sf::CircleShape` de raio `half_extent * scale`, mais um segundo círculo
branco translúcido deslocado para cima e à esquerda, que simula o brilho especular.
A projeção de uma esfera é sempre um círculo, o que torna este o caso fácil.

**Caixa** → `sf::ConvexShape` de 4 pontos. Os dois primeiros vetores-coluna da matriz de
rotação do corpo, escalados por `half_extent`, dão os eixos locais do quadrado; os 4 cantos
são `pos ± ax ± ay`, cada um projetado individualmente:

```cpp
btMatrix3x3 rot = t.getBasis();
btVector3 ax = rot.getColumn(0) * obj->half_extent;
btVector3 ay = rot.getColumn(1) * obj->half_extent;
btVector3 corners[4] = { pos+ax+ay, pos-ax+ay, pos-ax-ay, pos+ax-ay };
```

> ⚠ Isto desenha **uma face**, não um cubo. Um cubo de verdade precisaria das 6 faces com
> descarte das que estão de costas (*backface culling*). O atalho funciona visualmente
> porque a face acompanha a orientação real do corpo, e o resultado lê como um sólido
> girando — mas não se engane sobre o que está na tela.

**Cilindro** → o quadrilátero lateral (mesmos eixos da caixa, com o eixo Y esticado por
1,6, batendo com o `btCylinderShape(he, he*1.6, he)`) mais um `sf::CircleShape` clareado
em 30% como tampa superior.

**Mini-eixos** → três linhas de 30 px no canto inferior esquerdo, projetadas **apenas pela
rotação** (sem translação nem divisão perspectiva): X vermelho, Y verde, Z azul. Servem
para você saber para onde a câmera está apontando enquanto orbita.

### A ordenação: algoritmo do pintor

```cpp
// profundidade = projeção do vetor olho→objeto sobre o forward da câmera
float depth = dx*cam_forward_.x + dy*cam_forward_.y + dz*cam_forward_.z;
...
std::sort(entries.begin(), entries.end(),
          [](const Entry& a, const Entry& b){ return a.depth > b.depth; });
```

Ordem **decrescente**: o mais distante primeiro, para ser coberto pelos próximos.

> ⚠ Ordenar por **centro** é uma aproximação. Dois objetos que se interpenetram, ou um
> comprido atravessando outro, podem ser desenhados na ordem errada e "piscar" quando a
> câmera gira. Só um *z-buffer* — que exige por-pixel, ou seja, OpenGL — resolve de vez.
> Para objetos convexos, separados e de tamanho parecido, como aqui, o algoritmo do pintor
> é exato o bastante.

---

## A física: sala fechada, 12 objetos, gravidade ajustável

`PhysicsWorld(float half_room = 5.f)` — `SFMLWidget` constrói com `physics_(5.f)`, ou seja,
uma sala **10 × 10 × 10 m**. As paredes são 6 planos infinitos:

```cpp
add_static_plane({ 0, 1, 0}, -h);  // chão
add_static_plane({ 0,-1, 0}, -h);  // teto
add_static_plane({ 1, 0, 0}, -h);  // esquerda
add_static_plane({-1, 0, 0}, -h);  // direita
add_static_plane({ 0, 0, 1}, -h);  // frente
add_static_plane({ 0, 0,-1}, -h);  // fundo
```

Cada uma com massa 0, `restitution = 0.85`, `friction = 0.4`.

A API real, em `physics_world.hpp`:

```cpp
void add_object(ShapeType type, float half_extent,
                const btVector3& position,
                const btVector3& initial_velocity,
                sf::Color color);
void clear_objects();
void explode(float force);
void set_gravity(float gy);
void step(float dt);
const std::vector<std::unique_ptr<PhysicsObject>>& objects() const;
float half_room() const;
```

`ShapeType` tem **três** valores: `{ Box, Sphere, Cylinder }` — e há um botão para cada um.

Os objetos criados por `SFMLWidget::spawn_object`: posição aleatória dentro de ±60% da
sala, velocidade aleatória de 4 a 10 m/s em cada eixo, `half_extent` entre 0,3 e 0,8 m,
cor sorteada de uma paleta de 8, massa fixa 1 kg, restituição 0,85, atrito 0,4,
amortecimento 0,01 e uma velocidade angular aleatória para o objeto girar enquanto voa.

**A cena inicial tem 12 objetos**: 6 esferas, 4 caixas e 2 cilindros — criados no
construtor do `SFMLWidget`, e recriados pelo menu *Simulação → Reiniciar*. É por isso que o
`.ui` traz `Objetos: 12` como texto inicial do label.

### Os valores reais dos controles

Gravidade, em `mainwindow.cpp` — note que "Normal" é **−9,82**, não −9,81:

```cpp
static constexpr float GRAVITY_VALUES[] = {
    -9.82f,   // Normal
     0.0f,    // Zero-G
    -2.0f,    // Fraca
    -1.62f,   // Lunar
   -20.0f,    // Pesada
};
```

Passo de simulação: `world_->stepSimulation(dt, 10, 1.f / 60.f)` — sub-passo de **1/60 s**.

Explosão:

```cpp
void PhysicsWorld::explode(float force) {
    for (auto& obj : objects_) {
        btVector3 pos = obj->body->getWorldTransform().getOrigin();
        btVector3 dir = pos.length() > 0.01f ? pos.normalized() : btVector3(0,1,0);
        obj->body->activate(true);
        obj->body->setLinearVelocity(dir * force);   // ⚠ SUBSTITUI a velocidade
    }
}
```

> ⚠ Isto **não é um impulso**. `setLinearVelocity` descarta o movimento anterior e impõe
> exatamente 20 m/s radiais (`SFMLWidget::explode()` chama com `20.f`), independentemente
> da massa. Uma explosão fisicamente correta usaria `applyCentralImpulse`, que soma ao
> movimento existente e divide pela massa. O atalho é intencional — dá um efeito uniforme e
> previsível — mas saiba a diferença. Note também a guarda `pos.length() > 0.01f`: sem ela,
> um objeto exatamente no centro produziria `NaN` em `normalized()`, e um único NaN
> contamina o solver e faz a cena inteira desaparecer.

Restituição: o slider (0…100) vira `value / 100.f` e é aplicada com
`obj->body->setRestitution(r)` **a todos os objetos existentes**.

> ⚠ Duas consequências que surpreendem. Primeira: as **paredes continuam com 0,85** — o
> Bullet combina a restituição dos dois corpos em contato, então pôr o slider em 0,00 não
> deixa o quique em zero contra as paredes. Segunda: objetos criados **depois** de mexer no
> slider nascem com 0,85 (o valor de `add_object`); o slider só age sobre quem já existe.
> Mova o slider de novo para reaplicar.

---

## Os dois timers, e o que o contador de FPS realmente mede

```
SFMLWidget::timer_    a cada 1000/60 ms  →  tick(): física + desenho completo
MainWindow::status_timer_  a cada 250 ms →  update_status(): atualiza dois QLabel
```

Separar os dois é um padrão útil: **não faz sentido reescrever um `QLabel` 60 vezes por
segundo**. Texto que muda rápido demais é ilegível, e cada `setText` dispara relayout. 4 Hz
é o suficiente para números de status.

> ⚠ **O "FPS" exibido não é o FPS do render.** `MainWindow` cria um *terceiro* `QTimer` a
> 1000/60 ms cujo único trabalho é `++frame_count_`; `update_status()` divide essa contagem
> pelo tempo decorrido. Ou seja, ele mede a frequência de um timer que não desenha nada.
> Enquanto o event loop der conta, o número marcará ~60 independentemente de o desenho
> estar lento. Para medir o render de verdade, o incremento teria de estar dentro de
> `SFMLWidget::tick()`.

> ⚠ **A física avança com passo fixo, não com o tempo real:** `tick()` chama
> `physics_.step(1.f / TARGET_FPS)` — sempre 1/60 s, independentemente de quanto tempo
> realmente passou. Se o sistema engasgar, a simulação roda em **câmera lenta** em vez de
> perder frames. É uma escolha legítima (determinismo total), mas é o oposto do
> `sfml_bullet2`, que passa o `dt` medido. Compare os dois.

---

## Diferenças para o `qt_sfml`

| Aspecto | [qt_sfml](../qt_sfml/README.md) | qt_sfml2 |
|---|---|---|
| **Renderização** | 2D direto | **3D projetado à mão, câmera esférica orbital** |
| Física | nenhuma / animação simples | Bullet — `btDiscreteDynamicsWorld` |
| Interface | janela simples | `.ui` com menus, dock, combo, slider, atalhos |
| Mouse | — | arrastar orbita, roda dá zoom |
| Criação de objetos | — | botões do dock e teclas 1/2/3 |
| Foco pedagógico | embarcar SFML num QWidget | fazer três bibliotecas coexistirem, e projetar 3D sem OpenGL |

---

## Como validar

```bash
source build/conanbuild.sh
meson compile -C build qt_sfml2
./dist/bin/qt_sfml2
```

**Saída esperada na tela**: uma janela de 1100×700 intitulada
`Qt + SFML + Bullet3 — Simulação 3D`, com barra de menus (*Simulação*, *Adicionar*), a cena
escura à esquerda e um dock **Controles** à direita contendo os grupos *Adicionar Objeto*,
*Física*, *Câmera*, *Ações* e *Status*. Na cena: o cubo da sala em wireframe azulado, o
chão em quadrilátero semitransparente, 12 objetos coloridos quicando, e três mini-eixos
(vermelho/verde/azul) no canto inferior esquerdo.

> O terminal pode imprimir `Setting vertical sync not supported` e, em WSL/Wayland,
> `Failed to create wl_display` seguido de um aviso sobre o plugin `wayland`. Ambos são
> ruído do ambiente gráfico — a janela abre normalmente pelo backend X11.

Checklist:

| Ação | Resultado esperado | Se falhar |
|---|---|---|
| arrastar o mouse pela cena | a câmera orbita; **as posições relativas dos objetos entre si não mudam** | erro na base right/up/forward |
| girar a roda | o cubo cresce/encolhe suavemente, parando em 5 m e 35 m | grampeamento de `cam_radius_` |
| clicar **Esfera** 3 vezes | `Objetos:` sobe para 15 em até 250 ms | slot não conectado (nome no `.ui` ≠ nome do slot) |
| pôr **Elasticidade** em 1,00 e clicar **Reiniciar** | os objetos quicam quase até a altura inicial, quase sem perder energia | restituição não aplicada |
| pôr **Elasticidade** em 0,00 | os quiques encurtam bastante — mas **não** somem (as paredes seguem em 0,85) | comportamento esperado, não é bug |
| escolher **Zero-G** | os objetos passam a flutuar em linha reta, quicando nas 6 paredes | `set_gravity` sem `activate(true)` |
| clicar **💥 Explosão** | todos disparam radialmente para fora do centro, na mesma velocidade | |
| **Espaço** ou **⏸ Pausar** | a física congela, mas **arrastar o mouse ainda orbita** | prova de que render e física são independentes |
| **🗑 Limpar** | `Objetos: 0`, sala vazia | |
| clicar na cena esperando criar um objeto | **nada acontece** — é o comportamento correto | veja "A armadilha central" |

---

## Referências

Subprojetos irmãos deste repositório:

- [qt_sfml](../qt_sfml/README.md) — **pré-requisito**: como embarcar uma `sf::RenderWindow`
  num `QWidget`, sem física e sem 3D.
- [sfml_bullet](../sfml_bullet/README.md) — **pré-requisito de física**: o laço
  SFML + Bullet mínimo, em 2D.
- [sfml_bullet2](../sfml_bullet2/README.md) — o mesmo Bullet levado adiante: juntas
  articuladas, soft body, veículo por raycast e ray picking.
- [collision](../collision/README.md) — o que broadphase e narrowphase fazem por baixo:
  AABB, SAT, GJK.
- [smart-pointers](../smart-pointers/README.md) — o `unique_ptr` que `PhysicsObject` usa
  para segurar shape, motion state e body.

Fontes externas canônicas:

- <https://doc.qt.io/qt-6/signalsandslots.html> — sinais e slots, e o papel do moc.
- <https://doc.qt.io/qt-6/qwidget.html#winId> e
  <https://doc.qt.io/qt-6/qt.html#WidgetAttribute-enum> — `winId()` e os atributos
  `WA_PaintOnScreen` / `WA_OpaquePaintEvent` / `WA_NoSystemBackground`.
- <https://www.sfml-dev.org/tutorials/2.6/window-external.php> — o tutorial oficial do SFML
  sobre desenhar dentro de uma janela que você não criou.
- **Bullet Physics User Manual** — capítulos *Rigid Body Dynamics* e *Collision Shapes*.
- <https://pybullet.org/Bullet/BulletFull/> — documentação Doxygen da API do Bullet.
- *Real-Time Rendering* (Akenine-Möller et al.), capítulo 4 — transformações e projeção
  perspectiva, se você quiser a dedução formal do que a seção de projeção faz na prática.
