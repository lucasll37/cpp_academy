# mandelbrot — o fractal como laboratório de balanceamento de carga

## O que é este projeto?

Um visualizador interativo do **conjunto de Mandelbrot** (zoom com a roda do mouse, pan
arrastando) cujo cálculo é dividido à mão entre `std::thread`, com `std::mutex` e
`std::condition_variable`. O fractal é a desculpa; o assunto real é **como repartir trabalho
entre threads quando cada pedaço custa um tempo diferente**.

```bash
make build                 # Conan + Meson + Ninja, instala em dist/
./dist/bin/mandelbrot
```

O executável se chama `mandelbrot` — veja `executable('mandelbrot', ...)` em
[`meson.build`](meson.build).

Abre uma janela 900×700: barra de ferramentas em cima (slider `Threads:`, spin box `Iter:`,
botão `Reset`), o fractal no meio, e uma barra de status embaixo:

```
8 thread(s)  |  256 iter  |  cx=-0.50000000  cy=0.00000000  |  scale=3.000e+00  |  21.4 ms
```

Esse `21.4 ms` é o tempo real do último quadro — é ele que dá os números de [Medindo](#medindo).

---

## Antes de começar: os conceitos que você precisa

Nada aqui pressupõe mais que C++ básico — sintaxe, funções, classes. Nem matemática, nem Qt,
nem threads.

### Números complexos em 3 minutos

Um **número complexo** é um par de reais escrito `a + bi`, onde `i` obedece a uma única regra
nova: `i² = -1`. `a` é a **parte real**, `b` a **parte imaginária**. Pense num ponto do plano:
`a` horizontal, `b` vertical.

Duas operações bastam aqui. **Soma**: `(a+bi) + (c+di) = (a+c) + (b+d)i`. **Quadrado**:
multiplique como binômio e use `i² = -1`:

```
(a + bi)²  =  a² + 2abi + b²i²  =  (a² - b²) + (2ab)i
                                   \_______/   \____/
                                   parte real  parte imaginária
```

Por isso o código **não usa `std::complex`**: `z² + c` vira duas linhas de aritmética `double`,
que o compilador otimiza muito melhor.

```cpp
double zr2 = zr*zr - zi*zi + cr;   // nova parte real
zi         = 2.0 * zr * zi + ci;   // nova parte imaginária  (usa o zr ANTIGO)
zr         = zr2;                  // só agora sobrescreve
```

⚠ A ordem importa: `zi` precisa do `zr` **antigo**. Por isso a temporária `zr2`. Trocar as duas
linhas produz um fractal errado — e silenciosamente plausível.

**Módulo** é a distância até a origem: `|a + bi| = √(a² + b²)`.

### O conjunto, e por que o raio de escape é 2

Escolha um complexo `c` e itere a partir do zero: `z₀ = 0`, `z₁ = z₀² + c`, `z₂ = z₁² + c`, …
Ou a sequência fica presa perto da origem para sempre, ou foge para o infinito. O **conjunto
de Mandelbrot** é o conjunto dos `c` para os quais ela **não** foge.

O que torna isso computável é um teorema: **se em algum passo `|zₙ| > 2`, a sequência diverge
com certeza**. A intuição: se `|z| > 2` e `|z| ≥ |c|`, então
`|z² + c| ≥ |z|² - |c| ≥ |z|² - |z| = |z|(|z| - 1) > |z|` — cada passo cresce por um fator
maior que 1, e o crescimento se acelera. Logo basta iterar até `|z| > 2` (escapou, não
pertence) ou até um teto `max_iter` (desistimos, pintamos de preto).

Como comparar módulos é comparar quadrados, o código evita a raiz e testa
`zr*zr + zi*zi < 4.0`. Uma raiz quadrada a menos por iteração, milhões de vezes por quadro.

### Viewport: o retângulo do plano complexo que está na tela

**Viewport** é o pedaço do plano complexo visível agora. Três `double`
([`mandelbrot_renderer.hpp`](mandelbrot_renderer.hpp)):

```cpp
struct Viewport {
    double cx    = -0.5;   // centro, coordenada real
    double cy    =  0.0;   // centro, coordenada imaginária
    double scale =  3.0;   // LARGURA total visível do plano complexo
};
```

Zoom é dividir `scale`; pan é mexer em `cx`/`cy`. O padrão enquadra o fractal inteiro, que
vive aproximadamente em `real ∈ [-2, 0.5]`.

### Aspect ratio

**Aspect ratio** é `largura ÷ altura` em pixels (880×640 → ≈1,375). Se `scale` fosse aplicado
aos dois eixos, cada pixel cobriria mais plano na vertical e o fractal apareceria esticado. A
correção é dar à altura só `scale / aspect` ([`mandelbrot_renderer.cpp:78-80`](mandelbrot_renderer.cpp)):

```cpp
double aspect = (double)w_ / (double)h_;
double half_w = vp.scale * 0.5;
double half_h = half_w / aspect;      // 2*half_h == scale/aspect
```

### O buffer de pixels: RGBA, índice linear, stride

Uma imagem na memória não é matriz 2D — é **um vetor de bytes**, linha por linha, de cima para
baixo. Cada pixel ocupa 4 bytes em **RGBA**: vermelho, verde, azul e *alpha* (opacidade;
255 = opaco), 0 a 255 cada.

```
pixels_ : [R G B A][R G B A][R G B A] ... [R G B A] ...
          pix(0,0)  pix(1,0)  pix(2,0)    pix(0,1)
          \________ linha y=0, w pixels ________/\__ linha y=1 __
```

Endereço do pixel `(x, y)`: `int off = (y * w + x) * 4;` — índice linear vezes 4 bytes. O
tamanho de uma linha inteira (`w * 4`) chama-se **stride**; é o quarto argumento do `QImage`
em [`mainwindow.cpp:139`](mainwindow.cpp), porque a `QImage` precisa saber onde começa a
próxima linha.

### HSV: por que a paleta não é feita em RGB

Gradiente somando R, G e B é difícil; em **HSV** é trivial. HSV descreve a cor por três eixos:
**H** (*hue*, matiz) — a cor pura em graus num círculo; **S** (saturação) — 0 cinza, 1 pura;
**V** (valor) — 0 preto, 1 brilho máximo.

```
H:  0°      60°     120°    180°    240°    300°    360°
    vermelho amarelo verde   ciano   azul   magenta vermelho
```

Como o matiz é contínuo e circular, **varrer só o H já dá um degradê suave** — é o que este
projeto faz. A conversão HSV→RGB fica isolada em `hsv_to_rgba()`
([`mandelbrot_renderer.cpp:12-31`](mandelbrot_renderer.cpp)).

### Thread, mutex, lock_guard, condition_variable

Uma **thread** é uma linha de execução independente dentro do mesmo processo: várias threads
compartilham a mesma memória, então dois núcleos podem trabalhar no mesmo `pixels_`.
`std::thread t{f}` começa a executar `f` já; `t.join()` bloqueia o chamador até `f` terminar.

Um **mutex** é uma trava: no máximo uma thread por vez a segura.
`std::lock_guard<std::mutex> lk(m)` trava na construção e destrava no fim do bloco (RAII —
nunca esquece, nem sob exceção). `std::unique_lock` é o mesmo, porém destravável no meio — é o
que a `condition_variable` exige.

Uma **`condition_variable`** é uma sala de espera: `cv.wait(lock, pred)` solta o mutex e
adormece até alguém chamar `cv.notify_*()` **e** `pred` ser verdadeiro. Espera por um *evento*
sem queimar CPU. Se os três forem novidade, [`../concurrency/`](../concurrency/) os apresenta
um a um.

### Tile

Aqui um **tile** é uma **faixa horizontal contígua de linhas** do quadro — a unidade de
trabalho de uma thread. O tile `t` cobre o intervalo semiaberto `[y0, y1)`: inclui `y0`,
exclui `y1`.

### O mínimo de Qt

- **`QApplication`** representa o programa gráfico; `app.exec()` entra no **laço de eventos**,
  um `while` que retira eventos da fila do sistema (mouse, teclado, redesenho) e os despacha.
  Ele só retorna quando a última janela fecha.
- **`QWidget`** é a base de tudo que ocupa retângulo na tela; **`QMainWindow`** é um `QWidget`
  com barra de status pronta; **`QLabel`** exibe texto *ou uma imagem* — aqui é a tela de
  desenho.
- **`QImage`** é uma imagem em memória com formato de pixel explícito;
  `QImage::Format_RGBA8888` é exatamente o layout acima. **`QPixmap`** é a versão otimizada
  para exibir; `QPixmap::fromImage()` converte.
- **signals/slots** é o mecanismo de callback: `connect(obj, &C::sinal, dest, &D::slot)`. A
  macro **`Q_OBJECT`** habilita isso e obriga o header a passar pelo **moc** (*meta object
  compiler*), gerador de código que o Meson invoca com `qt6.compile_moc(...)`.
- **`QTimer`** é um despertador; em modo *single shot* dispara uma vez, e chamar `start()` de
  novo **reinicia** a contagem — detalhe que sustenta o *debounce* explicado adiante.

---

## Por que estudar isto?

Porque o Mandelbrot é o exemplo canônico de um problema **embaraçosamente paralelo com custo
irregular** — e essas duas metades puxam para lados opostos.

**Embaraçosamente paralelo**: nenhum pixel depende de outro. Zero comunicação, zero
dependência de dados, zero sincronização durante o cálculo. Em teoria, N núcleos dariam N×.

**Custo irregular**: os pixels não custam a mesma coisa. Um pixel do interior roda o laço
inteiro — 256, 1024, 4096 iterações. Um pixel bem fora escapa em 3 ou 4. Três ordens de
grandeza de diferença.

Resultado: a divisão ingênua — N threads, N faixas iguais — não entrega N×. As faixas do meio
pegam a barriga do fractal; as das pontas terminam cedo e ficam ociosas. Medido aqui, na vista
padrão:

| | 4 threads | 8 threads | 12 threads |
|---|---|---|---|
| faixas fixas (este projeto) | **2,43×** | **4,03×** | **5,51×** |
| chunks dinâmicos ([`../mandelbrot_openmp/`](../mandelbrot_openmp/)) | **3,97×** | **7,60×** | **11,1×** |

| Sem entender balanceamento | Com |
|---|---|
| "ganhei só 4× em 12 núcleos, deve ser limite de memória" | sabe que o gargalo é *uma* thread terminando por último |
| divide o trabalho por thread | divide em muitos pedaços pequenos e deixa as threads pegarem |
| mede uma vista só | mede a vista *pior*, onde o desbalanceamento aparece |

Este subprojeto mostra o problema com as mãos; o irmão
[`../mandelbrot_openmp/`](../mandelbrot_openmp/) o resolve com uma cláusula.

---

## Estrutura de arquivos

```
src/mandelbrot/
├── main.cpp                    ← QApplication + MainWindow + app.exec()  (13 linhas)
├── mainwindow.hpp              ← declara a janela: widgets, eventos, slots
├── mainwindow.cpp              ← layout, zoom/pan, medição de tempo, buffer → QImage
├── mandelbrot_renderer.hpp     ← struct Viewport + a classe do cálculo
├── mandelbrot_renderer.cpp     ← HSV→RGBA, laço de escape, tiles, mutex, condition_variable
├── meson.build                 ← moc de mainwindow.hpp + executable('mandelbrot')
└── README.md                   ← este arquivo
```

Não há `.ui`: o layout é todo programático, então o `uic` não entra na compilação — só o
`moc`, por causa do `Q_OBJECT`.

---

## Os módulos, um a um

### 1 · Mapear pixel para complexo

A fórmula real, em [`mandelbrot_renderer.cpp:85-86`](mandelbrot_renderer.cpp):

```cpp
double cr = vp.cx + (px / (double)(w_ - 1) - 0.5) * vp.scale;
double ci = vp.cy + (py / (double)(h_ - 1) - 0.5) * 2.0 * half_h;
```

Termo a termo, na coordenada real:

| trecho | o que faz |
|---|---|
| `px / (w_ - 1)` | normaliza a coluna para `[0, 1]`; o `-1` faz a última coluna dar exatamente 1,0 |
| `- 0.5` | recentra para `[-0.5, +0.5]`, pondo o zero no meio da tela |
| `* vp.scale` | escala para a largura desejada do plano complexo |
| `+ vp.cx` | translada para o centro do viewport |

A imaginária é idêntica, exceto por usar `2.0 * half_h`, que vale `vp.scale / aspect` — é aí
que entra a correção de aspecto. E como `py` cresce para **baixo**, o eixo imaginário aparece
invertido; o conjunto é simétrico no eixo real, então ninguém percebe. A mesma conta ao
contrário está em `pixel_to_complex` ([`mainwindow.cpp:221-227`](mainwindow.cpp)), para o zoom
saber que ponto está sob o cursor.

### 2 · O laço de escape

```cpp
double zr = 0.0, zi = 0.0;
int iter = 0;
while (iter < max_iter_ && zr*zr + zi*zi < 4.0) {
    double zr2 = zr*zr - zi*zi + cr;   // (a²-b²) + cr
    zi = 2.0 * zr * zi + ci;           //   2ab   + ci
    zr = zr2;
    ++iter;
}
```

Ao sair, `iter == max_iter_` significa "nunca escapou → provavelmente pertence"; `iter <
max_iter_` significa "escapou em `iter` passos", e esse número é toda a informação que a cor
usa.

⚠ Aumentar `max_iter` (o spin box) **não** melhora a imagem em toda parte: só afeta os pixels
que hoje batem no teto — o interior e a vizinhança da borda. Ou seja, custa caro exatamente
nos pixels que já eram os mais caros.

### 3 · Da contagem de iterações à cor

[`mandelbrot_renderer.cpp:33-46`](mandelbrot_renderer.cpp):

```cpp
if (iter == max_iter) return {0, 0, 0, 255};        // interior → preto opaco

float t   = (float)iter / (float)max_iter;          // t ∈ [0, 1)
float hue = std::fmod(240.0f - t * 200.0f + 360.0f, 360.0f);
float sat = 0.85f;
float val = t < 0.01f ? t * 100.0f : 1.0f;          // fade nos primeiros 1%
return hsv_to_rgba(hue, sat, val);
```

O que a fórmula do matiz faz de verdade: com `t = 0` (escape imediato) o matiz é 240° — azul;
com `t → 1` (escape só no fim) é `240 - 200 = 40°` — laranja. Como `240 - t*200` nunca sai de
`[40, 240]`, **o `fmod` nunca chega a agir e a paleta não cicla**: é uma varredura única e
monotônica.

O `val` escurece o exterior distante: nos primeiros 1% de `t` o brilho sobe de 0 a 1, para o
azul saturado não dominar a imagem.

Legenda correta: **preto (interior) → azul-escuro (exterior distante) → azul → ciano → verde →
amarelo → laranja (borda)**. Não há vermelho nem violeta.

### 4 · O paralelismo por faixas

`render()` corta o quadro em `num_threads_` faixas e dá uma a cada thread:

```
h_ = 640, num_threads_ = 4  →  rows_per_tile = 640 / 4 = 160

  y=0    ┌──────────────────────────────┐
         │  tile 0   [  0, 160)         │  thread 0   ← céu vazio: rápido
  y=160  ├──────────────────────────────┤
         │  tile 1   [160, 320)         │  thread 1   ← barriga do fractal: LENTO
  y=320  ├──────────────────────────────┤
         │  tile 2   [320, 480)         │  thread 2   ← barriga do fractal: LENTO
  y=480  ├──────────────────────────────┤
         │  tile 3   [480, 640) + resto │  thread 3   ← céu vazio: rápido
  y=640  └──────────────────────────────┘
```

```cpp
int rows_per_tile = h_ / num_threads_;
int y0 = 0;
for (int t = 0; t < num_threads_; ++t) {
    int y1 = (t == num_threads_ - 1) ? h_ : y0 + rows_per_tile;
    // Captura vp por VALOR para que cada thread tenha sua cópia
    workers.emplace_back([this, y0, y1, vp](){ compute_tile(y0, y1, vp); });
    y0 = y1;
}
```

A divisão inteira descarta o resto (640/12 = 53, sobram 4 linhas), então o **último** tile
recebe o que faltar: `y1 = h_` quando `t == num_threads_ - 1`.

⚠ **Capture o viewport por valor, não por referência.** `[this, y0, y1, vp]` copia `vp`. Com
`[…, &vp]` a lambda guardaria uma referência ao parâmetro `const Viewport&` de `render()`, e
uma thread que sobrevivesse ao retorno leria memória morta. A regra vale sempre: **lambda que
vai para outra thread captura por valor**, salvo tempo de vida garantido à mão. `Viewport` são
três `double` — a cópia é gratuita.

⚠ `this` na mesma lista é um ponteiro cru. Só está correto porque `render()` bloqueia até todas
as threads terminarem; o objeto não pode morrer antes delas.

### 5 · O mutex que talvez não precise

`compute_tile()` calcula sua faixa num **buffer local** e só no fim copia para `pixels_`, sob a
trava ([`mandelbrot_renderer.cpp:112-116`](mandelbrot_renderer.cpp)):

```cpp
{
    std::lock_guard<std::mutex> lock(tile_mutex_);
    std::copy(local.begin(), local.end(), pixels_.begin() + y0 * w_ * 4);
}
```

Sendo honesto: **esse mutex é didático, não necessário para a correção**. As faixas são
disjuntas — o tile `t` escreve em `[y0*w*4, y1*w*4)` e nenhum outro toca esse intervalo.
Escritas em endereços distintos de um `std::vector` que não é redimensionado não formam data
race. E a visibilidade dos dados para quem chamou `render()` já vem de graça do `join()`.

O que o mutex compra é a demonstração do padrão certo — *calcule fora da trava, copie dentro
dela* — que é o desenho correto quando as regiões **não** são disjuntas. Em produção ele
sairia.

⚠ Se esse mesmo mutex fosse tomado **por pixel** em vez de por tile, o programa ficaria ordens
de magnitude mais lento e efetivamente serial. É a armadilha clássica de granularidade.

⚠ O buffer local tem um segundo motivo, mais importante que o mutex: **localidade de cache**.
Escrever num vetor pequeno e recém-alocado é mais rápido que salpicar um buffer de 2,2 MB.
Veja [`../performance/`](../performance/).

### 6 · ⚠ A `condition_variable` é redundante aqui

`render()` espera duas vezes pela mesma coisa
([`mandelbrot_renderer.cpp:146-152`](mandelbrot_renderer.cpp)):

```cpp
{   // 1ª espera: bloqueia até done_count_ == num_threads_
    std::unique_lock<std::mutex> lk(done_mutex_);
    done_cv_.wait(lk, [this]{ return done_count_ == num_threads_; });
}
for (auto& w : workers) w.join();   // 2ª espera: bloqueia até cada thread acabar
```

O `join()` sozinho bastaria: se todas as threads foram *joined*, todos os tiles terminaram, por
definição. O par `done_cv_` + `done_count_` não acrescenta garantia nenhuma neste desenho —
acrescenta um mutex, uma CV, um contador que precisa ser zerado a cada quadro, e a chance de
esquecer esse zeramento.

Quando a CV valeria a pena de verdade:

- se `render()` fosse assíncrono e quisesse **mostrar progresso parcial** ("3 de 8 tiles") sem
  bloquear;
- se as threads viessem de um **pool persistente**, que não se pode `join()` porque não morrem
  no fim do quadro — aí a contagem é o único sinal disponível
  ([`../concurrency/demo_patterns.hpp`](../concurrency/demo_patterns.hpp));
- se houvesse mais de um consumidor esperando o mesmo evento.

⚠ `compute_tile` usa `notify_one()`. Com um único esperador está correto, mas `notify_all()`
seria a escolha defensiva: acrescente um segundo esperador amanhã e `notify_one` deixa um deles
dormindo para sempre.

### 7 · ⚠ Criar N threads a cada quadro

`render()` cria `num_threads_` threads novas e as destrói a **cada quadro**. Medido aqui
(Ryzen 9 5900X, `-O2`, mediana de 200 repetições criando e juntando threads vazias):

| threads | custo de criar + `join()` | fração de um quadro |
|---|---|---|
| 4 | 0,17 ms | 0,5% de 35 ms |
| 8 | 0,42 ms | 2,0% de 21 ms |
| 12 | 0,82 ms | 5,3% de 15 ms |
| 24 | 1,45 ms | ~18% de 8 ms |

Pequeno hoje, dominante se o quadro encolher. A cura é um **thread pool**: criar as threads uma
vez e alimentá-las com tarefas. É o que o runtime do OpenMP faz sozinho em
[`../mandelbrot_openmp/`](../mandelbrot_openmp/), e o que
[`../concurrency/demo_patterns.hpp`](../concurrency/demo_patterns.hpp) constrói à mão.

### 8 · `mainwindow` — pipeline gráfico e interações reais

```
QMainWindow
 └─ QWidget central + QVBoxLayout
     ├─ QHBoxLayout (barra de ferramentas)
     │   ├─ QLabel "Threads:" + QSlider  thread_slider_  [1 .. max(16, núcleos)]
     │   ├─ QLabel com o número atual de threads
     │   ├─ QLabel "Iter:"    + QSpinBox iter_spin_      [32 .. 4096, passo 64]
     │   └─ QPushButton "Reset"
     ├─ QLabel canvas_   ← a imagem do fractal (fundo preto, expande)
     └─ statusBar()      ← threads | iter | cx | cy | scale | ms
```

Não existe widget de SFML neste projeto, nem botões de zoom. As interações **reais** são:

| gesto | efeito | código |
|---|---|---|
| roda para cima | zoom in ×0,75, **centrado no cursor** | `wheelEvent` ([`mainwindow.cpp:170`](mainwindow.cpp)) |
| roda para baixo | zoom out ×1,333, centrado no cursor | idem |
| arrastar (botão esquerdo) | pan | `mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent` |
| `Ctrl+R` | volta ao viewport inicial | `keyPressEvent` ([`mainwindow.cpp:212`](mainwindow.cpp)) |
| botão `Reset` | idem | lambda em [`mainwindow.cpp:102`](mainwindow.cpp) |
| redimensionar a janela | re-renderiza no novo tamanho | `resizeEvent` |

O zoom centrado no cursor: converte-se a posição do mouse para o plano complexo e move-se o
centro de modo que aquele ponto continue exatamente sob o cursor.

```cpp
QPointF c = pixel_to_complex(pos);       // ponto sob o cursor, ANTES do zoom
vp_.scale *= factor;
vp_.cx = c.x() + (vp_.cx - c.x()) * factor;
vp_.cy = c.y() + (vp_.cy - c.y()) * factor;
```

**Do buffer até a tela:**

```
render() → const std::vector<uint8_t>& pixels_        (w*h*4 bytes, RGBA)
   │
   ▼  QImage(px.data(), W, H, W*4, QImage::Format_RGBA8888)   ← NÃO copia; aponta
   ▼  QPixmap::fromImage(img)                                 ← converte para exibição
canvas_->setPixmap(...)                                       ← o QLabel desenha
```

⚠ Esse construtor de `QImage` **não copia** os bytes: guarda o ponteiro. O buffer precisa
continuar vivo e imutável enquanto a `QImage` existir. Aqui está tudo bem — a `QPixmap` é feita
na linha seguinte e a `QImage` morre no fim da função. Guardar essa `QImage` num membro seria
um ponteiro pendurado esperando o próximo `resize()`.

**O `QTimer` que evita cem renders por segundo:** girar a roda gera dezenas de eventos por
segundo, redimensionar gera centenas. Por isso nenhum handler chama `do_render()` direto — todos
chamam `schedule_render()`, que faz `render_timer_->start()`. Como `start()` **reinicia** o
intervalo de 30 ms, uma rajada de N eventos produz **um único** render, 30 ms após o último.
O padrão se chama *debounce* e vale para qualquer entrada de alta frequência.

⚠ Todo o cálculo acontece dentro de `do_render()`, ou seja, **na thread da GUI** — as workers
existem, mas o chamador espera por elas com `join()`. Num quadro de 400 ms a janela congela
visivelmente. A evolução natural seria renderizar fora da thread de GUI e entregar por sinal —
e aí o `tile_mutex_` deixaria de ser decorativo.

---

## Medindo

**Ambiente**: AMD Ryzen 9 5900X (12 núcleos físicos, 24 threads SMT), Ubuntu 24.04 em WSL2,
g++ 13.3.0, `-O2 -std=c++20`. Janela 880×640, `max_iter = 256`, viewport padrão. Valor
reportado: **mínimo de 7 repetições** após um quadro de aquecimento.

| threads | tempo | speedup | eficiência |
|---|---|---|---|
| 1 | 85,4 ms | 1,00× | 100% |
| 2 | 43,1 ms | 1,98× | 99% |
| 4 | 35,1 ms | **2,43×** | **61%** |
| 8 | 21,2 ms | 4,03× | 50% |
| 12 | 15,5 ms | 5,51× | 46% |

**Speedup** = tempo com 1 thread ÷ tempo com N. **Eficiência** = speedup ÷ N: quanto de cada
núcleo foi realmente aproveitado.

Duas leituras valem mais que a tabela inteira:

1. **Com 4 threads a eficiência despenca para 61%** — pior do que se esperaria. Não é ruído:
   com 4 faixas na vista padrão, as duas centrais pegam a barriga do fractal (quase toda de
   interior, `max_iter` iterações por pixel) enquanto as de cima e de baixo pegam céu vazio e
   terminam quase de imediato. O quadro dura o que durar a faixa mais lenta.
2. **De 8 para 12 a eficiência continua caindo**, ainda que o tempo melhore. Faixas fixas nunca
   convergem para 100%.

Que o culpado é a **divisão fixa**, e não o `std::thread`, dá para provar trocando só a
estratégia de distribuição — mesmo laço, mesmas fórmulas:

| threads | faixas fixas (`std::thread`) | `schedule(static)` (OpenMP) | `schedule(dynamic, 4)` (OpenMP) |
|---|---|---|---|
| 1 | 85,4 ms | 84,7 ms | 86,6 ms |
| 2 | 43,1 ms | 42,6 ms | 43,6 ms |
| 4 | 35,1 ms | 35,2 ms | **21,8 ms** |
| 8 | 21,2 ms | 21,8 ms | **11,4 ms** |
| 12 | 15,5 ms | 15,0 ms | **7,8 ms** |

`std::thread` com faixas fixas e OpenMP com `schedule(static)` são **indistinguíveis** — como
tem de ser, pois fazem a mesma partição. O que muda o jogo é distribuir em pedaços pequenos por
demanda: **1,9× mais rápido com 8 e com 12 threads**, mesma máquina, mesmo quadro. A explicação
completa está em [`../mandelbrot_openmp/`](../mandelbrot_openmp/).

⚠ `make build` compila em `debug` (`BUILD_TYPE := debug` no `Makefile` da raiz). Os tempos da
barra de status serão **maiores** que os desta tabela, medida com `-O2`. Compare sempre
configurações iguais.

---

## Como validar

**1 · Compila e roda.**

```bash
make build && ls -l dist/bin/mandelbrot && ./dist/bin/mandelbrot
```

Sem servidor gráfico (SSH, CI), dá para checar que o programa sobe:

```bash
QT_QPA_PLATFORM=offscreen timeout 5 ./dist/bin/mandelbrot; echo "saiu com $?"
# 124 = o timeout matou o processo, isto é, ele estava vivo: OK
```

**2 · Confere o comportamento.** A barra de status mostra `... | 256 iter | cx=-0.50000000
... | XX.X ms`. Arrastar o slider `Threads:` de 1 para 12 derruba o tempo **sem mudar um único
pixel**. A roda aproxima o ponto sob o cursor, não o centro. `Ctrl+R` e `Reset` devolvem
`cx=-0.50000000  cy=0.00000000  scale=3.000e+00`.

**3 · Prova de que não há data race — ThreadSanitizer.** O renderizador não depende de Qt, então
dá para exercitá-lo fora da GUI. Salve em `/tmp/bench_mandel.cpp`:

```cpp
#include RENDERER_CPP                                  // escolhido na linha de comando
#include <chrono>
#include <cstdio>
#include <vector>
#include <algorithm>

int main() {
    const int W = 880, H = 640, iter = 256, reps = 7;
    Viewport vp;                                       // cx=-0.5 cy=0 scale=3
    for (int t : {1, 2, 4, 8, 12}) {
        MandelbrotRenderer r(W, H, t, iter);
        r.render(vp);                                  // aquecimento
        std::vector<double> ms;
        for (int k = 0; k < reps; ++k) {
            auto a = std::chrono::steady_clock::now();
            r.render(vp);
            auto b = std::chrono::steady_clock::now();
            ms.push_back(std::chrono::duration<double, std::milli>(b - a).count());
        }
        std::sort(ms.begin(), ms.end());
        printf("threads=%2d  min=%7.1f ms\n", t, ms.front());
    }
}
```

Da raiz do repositório — primeiro medindo, depois sob o sanitizador:

```bash
g++ -O2 -std=c++20 -pthread -I. -Isrc/mandelbrot \
    -DRENDERER_CPP='"src/mandelbrot/mandelbrot_renderer.cpp"' \
    /tmp/bench_mandel.cpp -o /tmp/bench_thr && /tmp/bench_thr

g++ -O1 -g -std=c++20 -pthread -fsanitize=thread -I. -Isrc/mandelbrot \
    -DRENDERER_CPP='"src/mandelbrot/mandelbrot_renderer.cpp"' \
    /tmp/bench_mandel.cpp -o /tmp/bench_tsan && /tmp/bench_tsan
```

Esperado: **nenhuma linha `WARNING: ThreadSanitizer`** e código de saída 0 — foi o que se obteve
aqui. TSan detecta acessos concorrentes não sincronizados em tempo de execução; custa 5–15× em
desempenho, então não use medições feitas sob ele.

**4 · Prova de que o resultado independe do paralelismo.** Cada pixel é puro e determinístico,
então 1 thread e 12 threads têm de produzir buffers idênticos — e o mesmo vale contra a versão
OpenMP: os dois `render()` geram o **mesmo** buffer de 2 252 800 bytes (verificado com `cmp`).

---

## Referências

Neste repositório:

- [`../mandelbrot_openmp/`](../mandelbrot_openmp/) — o mesmo fractal com uma diretiva; é lá que
  o desbalanceamento medido aqui é resolvido.
- [`../concurrency/`](../concurrency/) — `std::thread`, `mutex`, `condition_variable`, `future`
  e um thread pool, um demo para cada.
- [`../concurrency2/`](../concurrency2/) — modelo de memória: `happens-before`, `memory_order`,
  e por que data race é comportamento indefinido, não "valor errado".
- [`../performance/`](../performance/) — método de medição e localidade de cache; veja
  [`../performance/demo_false_sharing.hpp`](../performance/demo_false_sharing.hpp).
- [`../qt_sfml/`](../qt_sfml/) — mais Qt, para quem quer o framework em si.

Fora dele:

- [Wikipedia: Mandelbrot set](https://en.wikipedia.org/wiki/Mandelbrot_set) — inclui a prova do
  raio de escape 2 e o *smooth coloring* contínuo.
- [cppreference: `std::thread`](https://en.cppreference.com/w/cpp/thread/thread) ·
  [`std::condition_variable`](https://en.cppreference.com/w/cpp/thread/condition_variable)
- [Qt 6: `QImage::Format`](https://doc.qt.io/qt-6/qimage.html#Format-enum) — a lista completa de
  layouts de pixel.
- [ThreadSanitizer manual](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
