# mandelbrot_openmp — o mesmo fractal, uma diretiva em vez de trinta linhas

## O que é este projeto?

Uma janela Qt que desenha o **conjunto de Mandelbrot** e deixa você aproximar com a roda do
mouse — só que o cálculo é espalhado pelos núcleos da CPU com **uma única linha**:
`#pragma omp parallel for`. É o gêmeo de [`../mandelbrot/`](../mandelbrot/), que faz a mesma
imagem com `std::thread`, `mutex` e `condition_variable` escritos à mão.

```bash
make build                       # compila o repositório inteiro
./dist/bin/mandelbrot_openmp     # abre a janela
```

Sem servidor gráfico (SSH, WSL sem WSLg, CI), dá para checar que o programa sobe:

```bash
QT_QPA_PLATFORM=offscreen timeout 5 ./dist/bin/mandelbrot_openmp; echo "saiu com $?"
# "saiu com 124" = o timeout matou o processo, isto é, ele estava vivo: OK
```

Na janela: uma barra com o slider **Threads:** (1 até o número de threads da máquina, no mínimo
16), o campo **Iter:** (32 a 4096, padrão 256) e o botão **Reset**; abaixo, o fractal; e no
rodapé uma barra de status como

```
12 thread(s)  |  256 iter  |  cx=-0.50000000  cy=0.00000000  |  scale=3.000e+00  |  8.4 ms
```

Roda do mouse aproxima **no ponto sob o cursor**; arrastar desloca a vista; `Ctrl+R` ou **Reset**
voltam ao enquadramento inicial. Os milissegundos no fim da barra são o tempo real de um quadro
— é o instrumento de medição deste subprojeto, e é dele que saem as tabelas adiante.

---

## Antes de começar: os conceitos que você precisa

Esta seção não pressupõe nada além de C++ básico (laços, `std::vector`, classes).

### 1. O conjunto de Mandelbrot, do zero

Pegue um número complexo `c` — que é só um par de doubles, uma parte **real** `cr` e uma parte
**imaginária** `ci`. Comece com `z = 0` e aplique repetidamente:

```
z  ←  z² + c
```

Ou `z` fica girando perto da origem para sempre (então `c` **pertence** ao conjunto, e pintamos
de preto), ou `z` dispara para o infinito (então `c` está **fora**, e a cor conta em quantos
passos ele escapou). Multiplicar complexos é mecânico: se `z = zr + zi·i`, então
`z² = (zr² − zi²) + (2·zr·zi)·i`. Somando `c`:

```cpp
double zr2 = zr*zr - zi*zi + cr;   // nova parte real
zi         = 2.0 * zr * zi + ci;   // nova parte imaginária
zr         = zr2;                  // ⚠ só agora, senão zi usaria o zr NOVO
```

Como saber que escapou? **Se o módulo de `z` passar de 2, nunca mais volta** — é teorema, não
chute. E módulo é caro (tem raiz quadrada), então comparamos os quadrados: `zr² + zi² < 4.0`, o
teste do laço em `mandelbrot_renderer.cpp:89`.

Se depois de `max_iter` passos não escapou, desistimos e chamamos de "dentro". Cada pixel custa
entre 1 e `max_iter` voltas desse laço, e **é essa variação que torna o problema interessante
para paralelismo**: alguns pixels custam 1 volta, outros 256.

### 2. Viewport, aspect e o mapeamento pixel → complexo

A tela é uma grade de pixels; o fractal vive no plano complexo. O `struct Viewport`
(`mandelbrot_renderer.hpp:23`) guarda o retângulo do plano que está na tela:

```cpp
struct Viewport {
    double cx    = -0.5;   // centro, parte real
    double cy    =  0.0;   // centro, parte imaginária
    double scale =  3.0;   // largura do retângulo, em unidades do plano
};
```

```
        px = 0                        px = w-1
  py=0 ───┬───────────────────────────────┬───  ci = cy - (scale/aspect)/2
          │            (cx, cy)           │
          │               ×               │     scale = largura do retângulo,
          │                               │             em unidades do plano
py=h-1 ───┴───────────────────────────────┴───  ci = cy + (scale/aspect)/2
       cr = cx - scale/2          cr = cx + scale/2
```

**Aspect ratio** é `largura ÷ altura` da janela. Sem ele, uma janela 880×640 espremeria o
fractal: 3 unidades do plano na horizontal e 3 na vertical, mas 880 pixels contra 640. Dividir a
altura por `aspect` devolve o pixel quadrado — é o `/ aspect` de `mandelbrot_renderer.cpp:81`.

### 3. O buffer de pixels: RGBA e índice linear

A imagem mora num `std::vector<uint8_t>` de `w × h × 4` bytes — quatro por pixel: **R**ed,
**G**reen, **B**lue, **A**lpha (opacidade). Nada de matriz bidimensional: os pixels ficam linha
por linha, um atrás do outro.

```
       linha 0              linha 1              linha 2
 ┌───────────────────┬───────────────────┬─────────────── ⋯
 │ R G B A R G B A …│ R G B A R G B A …│ R G B A …
 └───────────────────┴───────────────────┴─────────────── ⋯
  ▲                   ▲                   ▲
  offset 0            offset w*4          offset 2*w*4

  endereço do pixel (px, py)  =  (py * w + px) * 4
```

Guarde a fórmula: **é ela a razão de este programa não precisar de mutex nenhum**.

### 4. O mínimo de Qt para entender `mainwindow.cpp`

| Termo | O que é |
|---|---|
| **widget** | qualquer elemento visual: um botão, um rótulo, a própria janela. Todo widget é um `QWidget` |
| **`QApplication`** | o objeto que inicializa o Qt. Tem de existir antes de qualquer widget |
| **event loop** | o laço infinito dentro de `app.exec()` que fica esperando eventos (clique, tecla, redesenho) e chamando o método certo. Enquanto o seu `render()` roda, o event loop está **parado** — por isso um quadro lento congela a janela |
| **`QLabel`** | widget que mostra texto ou imagem. Aqui é o canvas do fractal |
| **`QImage`/`QPixmap`** | `QImage` embrulha bytes crus (o buffer RGBA) sem copiar; `QPixmap` é a forma otimizada de desenhar na tela |
| **signal/slot** | o mecanismo de callback do Qt. O slider **emite** `valueChanged(int)`; `connect(...)` liga esse sinal ao **slot** `schedule_render()`, que será chamado |
| **`Q_OBJECT` e o `moc`** | a macro `Q_OBJECT` (`mainwindow.hpp:35`) marca a classe; o **moc** (Meta-Object Compiler) lê o cabeçalho e gera o C++ que faz signals/slots funcionarem — é o `qt6.compile_moc(...)` do `meson.build` |
| **parent-child** | `new QLabel(this)` não vaza: o Qt destrói os filhos junto com o pai. É RAII com dono explícito — por isso o código usa `new` para widgets e `std::unique_ptr` para o renderizador |

### 5. O que é OpenMP, de fato

OpenMP **não é uma biblioteca que você chama**. São três coisas ao mesmo tempo:

1. **Um padrão** (openmp.org) de diretivas `#pragma omp ...`;
2. **Um compilador que as entende** — com `-fopenmp`, o GCC transforma o corpo do laço numa
   função separada (nos backtraces: `render(...) [clone ._omp_fn.0]`) e chama o runtime;
3. **Uma biblioteca de runtime** (no GCC, `libgomp`) que mantém o pool de threads.

> ⚠ **Sem `-fopenmp`, o `#pragma` é silenciosamente ignorado** e o programa roda **serial**, sem
> erro nenhum. É a armadilha número um do assunto — a prova está logo abaixo.

**Memória compartilhada** (*shared memory*) é o modelo: todas as threads vivem no **mesmo
processo** e enxergam o **mesmo espaço de endereçamento**. Um `std::vector` visível na função é
visível para todas — sem mensagens, sem cópia. É o oposto de MPI ou de processos separados, onde
cada um tem sua memória e trocar dados exige serializar e enviar. É esse modelo que faz
`pixels_[off] = ...` funcionar de qualquer thread — e é ele que permite corridas de dados.

### 6. O menor programa OpenMP que existe

```cpp
// ola_omp.cpp
#include <cstdio>
#ifdef _OPENMP                                    // definida só com -fopenmp
  #include <omp.h>
#else
  static int omp_get_thread_num() { return 0; }   // fallback serial
#endif
int main() {
    #pragma omp parallel for
    for (int i = 0; i < 8; ++i)
        std::printf("iteracao %d  ->  thread %d\n", i, omp_get_thread_num());
}
```

Saída real desta máquina (24 threads de hardware, forçada a 4 com `OMP_NUM_THREADS=4`), após
`g++ -std=c++20 -O2 -fopenmp ola_omp.cpp -o ola_on`:

```
iteracao 0  ->  thread 0     iteracao 6  ->  thread 3   ← fora de ordem
iteracao 1  ->  thread 0     iteracao 7  ->  thread 3
iteracao 2  ->  thread 1     iteracao 4  ->  thread 2
iteracao 3  ->  thread 1     iteracao 5  ->  thread 2
```

As 8 iterações foram **divididas em 4 blocos de 2**, e os blocos **saíram fora de ordem**. Agora
o experimento que todo mundo devia fazer uma vez na vida — recompile **sem** `-fopenmp`:

```bash
g++ -std=c++20 -O2 ola_omp.cpp -o ola_off && ./ola_off
# iteracao 0 -> thread 0 ... iteracao 7 -> thread 0     (tudo na thread 0, tudo em ordem)
```

**Compilou, linkou, rodou e não paralelizou nada.** O único aviso possível vem de `-Wall`, e
ainda assim é discreto: `warning: ignoring '#pragma omp parallel' [-Wunknown-pragmas]`.

> ⚠ Se você `#include <omp.h>` e chamar `omp_get_max_threads()` sem `-fopenmp`, aí sim o erro é
> ruidoso — mas é de **link**, não de compilação:
> `undefined reference to 'omp_get_max_threads'`. O pragma some em silêncio; a função não.

### 7. O modelo fork-join

É o modelo mental inteiro do OpenMP. O programa é **serial** por padrão; ao encontrar uma região
paralela a thread mestre **bifurca** (*fork*) num time de N threads; no fim, todas esperam umas
pelas outras numa **barreira implícita** e o programa volta a ser serial (*join*).

```
  thread mestre  │  código serial (aloca o buffer, lê o slider …)
                 ▼
  ┌─── #pragma omp parallel for ──────────────────────┐   ← FORK
  │  T0 ████████████░░░░░░                            │
  │  T1 █████████████████░░░      cada Tn processa    │
  │  T2 ███████░░░░░░░░░░░░░      um subconjunto      │
  │  T3 ██████████████████████    das iterações       │
  └──────────── barreira implícita ───────────────────┘   ← JOIN
                 │   (ninguém passa antes do último)
                 ▼
  thread mestre  │  serial de novo (converte para QImage, mostra)
```

Quando `render()` retorna, **todas as linhas já foram calculadas**: a barreira é automática, e é
ela que substitui o `join()` manual e a `condition_variable` da versão `std::thread`.

> ⚠ `nowait` remove a barreira de um `#pragma omp for`, mas **não** de um `#pragma omp parallel
> for` — a região paralela sempre termina com barreira, e tentar dá
> `error: 'nowait' is not valid for '#pragma omp parallel for'`. É preciso separar as diretivas:
> `#pragma omp parallel { #pragma omp for nowait … }`.

### 8. Iteração, chunk e schedule

- **Iteração**: uma volta do laço paralelizado. Aqui, uma **linha** da imagem (`py`).
- **Chunk**: o bloco de iterações **consecutivas** entregue de uma vez a uma thread —
  `schedule(dynamic, 4)` quer dizer "chunks de 4 linhas".
- **Schedule**: a política que decide qual thread pega qual chunk. As duas que importam:

| schedule | como distribui | bom quando |
|---|---|---|
| `static` | divide tudo **antes** de começar, em blocos iguais e fixos | o custo por iteração é uniforme |
| `dynamic, N` | cada thread pega o **próximo chunk livre** ao terminar o atual | o custo por iteração é irregular |

### 9. A regra que mais causa bug: quem é `private`, quem é `shared`

| Variável | Escopo padrão |
|---|---|
| declarada **dentro** da região paralela | **privada** — cada thread tem a sua |
| declarada **fora** e usada dentro | **compartilhada** — uma só, para todas |
| a variável de indução do laço (`py`) | **sempre privada**, mesmo declarada fora |

No `render()`: `zr`, `zi`, `iter`, `px`, `cr`, `ci`, `off` e `col` são declarados dentro do laço
→ **privados**. `pixels_`, `w_`, `h_`, `max_iter_` e `vp` vêm de fora → **compartilhados**, que é
o desejado (todos leem; só `pixels_` é escrito, em posições que não se cruzam).

**O contra-exemplo, medido.** Mova uma declaração para fora do pragma:

```cpp
int iter = 0;                   // ⚠ FORA do pragma → COMPARTILHADA por todas as threads
#pragma omp parallel for schedule(dynamic, 4) num_threads(num_threads_)
for (int py = 0; py < h_; ++py)
    for (int px = 0; px < w_; ++px) {
        double zr = 0.0, zi = 0.0;
        iter = 0;               // N threads zerando e incrementando a MESMA variável
        while (iter < max_iter_ && zr*zr + zi*zi < 4.0) { /* … */ ++iter; }
    }
```

Renderizando 880×640 com 8 threads e comparando com a versão correta:

| build | resultado |
|---|---|
| `-O0` (como o `make build` deste repositório) | **~15% dos bytes errados, e diferentes a cada execução** (15,79% / 15,48% / 14,58% em três rodadas) |
| `-O2` | imagem **idêntica** à correta — o otimizador manteve `iter` num registrador por thread |

Retrato do bug de concorrência: **existe em `-O0`, some em `-O2` e muda a cada execução**. Não
confie no "funcionou aqui" — o ThreadSanitizer aponta o dedo direto (veja "Como validar").

**As cláusulas que sobrescrevem o padrão:**

| Cláusula | Efeito |
|---|---|
| `private(x)` | cada thread tem sua cópia, **não inicializada** |
| `firstprivate(x)` | cópia privada, **inicializada** com o valor de fora |
| `shared(x)` | força compartilhamento (é o padrão para variáveis de fora) |
| `reduction(+:x)` | cópia privada por thread + combinação automática no fim |

### 10. Data race, e o que `reduction` faz de verdade

**Data race** é o que acontece quando duas ou mais threads acessam a **mesma** posição de
memória ao mesmo tempo, **pelo menos uma escrevendo**, sem sincronização. O resultado não é só
"imprevisível": em C++ é **comportamento indefinido**.

O caso clássico é `soma += arr[i]`. Parece uma operação; são três (ler, somar, escrever), e duas
threads podem ler o mesmo valor antigo e uma sobrescrever o trabalho da outra:

```cpp
long errada = 0;
#pragma omp parallel for
for (size_t i = 0; i < arr.size(); ++i) errada += arr[i];   // ⚠ CORRIDA

long certa = 0;
#pragma omp parallel for reduction(+:certa)
for (size_t i = 0; i < arr.size(); ++i) certa += arr[i];    // correto
```

Com `arr` = um milhão de valores 1, três execuções seguidas nesta máquina:

| execução | sem `reduction` | com `reduction` | esperado |
|---|---|---|---|
| 1 | 375 000 | 1 000 000 | 1 000 000 |
| 2 | 166 667 | 1 000 000 | 1 000 000 |
| 3 | 125 000 | 1 000 000 | 1 000 000 |

O que o `reduction(+:certa)` manda o runtime fazer: **dar a cada thread uma variável privada
inicializada com o elemento neutro da operação** (0 para `+`, 1 para `*`), deixar cada uma somar
sem sincronização nenhuma, e **combinar as parciais na barreira do fim**:

```
T0: 250 000 ┐  T2: 250 000 ┐
T1: 250 000 ┴──────────────┴─► na barreira (4 threads): 0 + 250000×4 = 1 000 000
```

Este projeto **não usa `reduction`** — não acumula nada, escreve em posições distintas. Mas é o
segundo padrão de todo código OpenMP, e sem entendê-lo o primeiro `+=` dentro de um pragma vira
bug silencioso.

### 11. Speedup, eficiência e a lei de Amdahl

- **Speedup** = tempo com 1 thread ÷ tempo com N. "Quantas vezes mais rápido ficou."
- **Eficiência** = speedup ÷ N: quanto de cada núcleo foi realmente aproveitado. 100% é o ideal
  inatingível.

A **lei de Amdahl** diz por que. Se uma fração `p` do programa é paralelizável e `(1 − p)` é
irremediavelmente serial:

```
              1
S(N) = ─────────────────
        (1 − p) + p / N
```

Repare no que acontece com `N → ∞`: a parte `p/N` some, e sobra `S = 1/(1 − p)`. **A parte
serial vira um teto absoluto.**

Com `p = 0,99`: o teto é 7,5× em 8 threads, 19,5× em 24, 39,3× em 64 e **100× no infinito**.

Aqui a parte serial é minúscula (redimensionar o vetor, montar o `QImage`, atualizar a barra) e
o laço duplo domina. O gargalo real **não é Amdahl**: é o desbalanceamento entre linhas.

---

## Por que estudar isto?

O problema é concreto: uma janela de 880×640 tem **563 200 pixels**, cada um custando até 256
iterações de ponto flutuante, todos **independentes entre si**. Um núcleo trabalha, onze param.

| Sem OpenMP (`../mandelbrot/`) | Com OpenMP (aqui) |
|---|---|
| criar `std::vector<std::thread>`, calcular faixas, `emplace_back` com lambda | uma linha de `#pragma` |
| `std::mutex` + `std::lock_guard` para o buffer | nada — os offsets não se cruzam |
| `std::condition_variable` + contador para saber quando acabou | barreira implícita |
| `for (auto& w : workers) w.join();` | nada |
| criar e destruir N threads **a cada quadro** | pool de threads reaproveitado entre quadros |
| distribuição fixa: uma faixa por thread | `schedule(dynamic, 4)` reequilibra sozinho |
| ~40 linhas de infraestrutura | 1 linha |

E não é só menos código: com 12 threads na vista padrão, **8,0 ms contra 15,4 ms** — quase o
dobro da velocidade, pelo motivo que a seção "Medindo" explica.

---

## Estrutura de arquivos

```
mandelbrot_openmp/
├── main.cpp                    ← 13 linhas: QApplication + MainWindow + app.exec()
├── mainwindow.hpp              ← declaração da janela: widgets, slots, viewport
├── mainwindow.cpp              ← toolbar, canvas, zoom/pan, cronômetro do quadro
├── mandelbrot_renderer.hpp     ← struct Viewport + classe MandelbrotRenderer
├── mandelbrot_renderer.cpp     ← ⭐ o arquivo que importa: o laço com o #pragma
├── meson.build                 ← alvo 'mandelbrot_openmp', deps qt_dep + omp_dep, moc
└── README.md                   ← este arquivo
```

> **`main.cpp`, `mainwindow.hpp` e `mainwindow.cpp` são byte a byte idênticos aos de
> [`../mandelbrot/`](../mandelbrot/)** — confira com
> `for f in main.cpp mainwindow.hpp mainwindow.cpp; do diff -s src/mandelbrot/$f
> src/mandelbrot_openmp/$f; done` (três vezes "Files ... are identical").
>
> **Toda a diferença entre os dois subprojetos está em `mandelbrot_renderer.cpp`** (e no `.hpp`,
> que aqui não tem mutex nem condition_variable). É onde vale gastar sua atenção.

O `meson.build` liga o alvo a `qt_dep` e a `omp_dep` — este último é `dependency('openmp')`
(linha 19 do `meson.build` da raiz), que é quem acrescenta o `-fopenmp`. Sem essa dependência o
programa compilaria e rodaria **serial**.

---

## As peças, uma a uma

### 1 · O `#pragma` central — o código real

Copiado de `mandelbrot_renderer.cpp:76-107` sem edição:

```cpp
const std::vector<uint8_t>& MandelbrotRenderer::render(const Viewport& vp) {
    const double aspect = (double)w_ / (double)h_;

    #pragma omp parallel for schedule(dynamic, 4) num_threads(num_threads_)
    for (int py = 0; py < h_; ++py) {
        const double ci = vp.cy + (py / (double)(h_ - 1) - 0.5) * vp.scale / aspect;

        for (int px = 0; px < w_; ++px) {
            const double cr = vp.cx + (px / (double)(w_ - 1) - 0.5) * vp.scale;

            // z_{n+1} = z_n² + c
            double zr = 0.0, zi = 0.0;
            int iter = 0;
            while (iter < max_iter_ && zr*zr + zi*zi < 4.0) {
                double zr2 = zr*zr - zi*zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = zr2;
                ++iter;
            }
            const int off = (py * w_ + px) * 4;
            const auto col = colorize(iter, max_iter_);
            pixels_[off+0] = col[0];
            pixels_[off+1] = col[1];
            pixels_[off+2] = col[2];
            pixels_[off+3] = col[3];
        }
    }

    return pixels_;
}
```

Três detalhes que passam batido:

- **`ci` está fora do laço de `px`**: a parte imaginária depende só de `py`, e recalculá-la 880
  vezes por linha seria desperdício. Já `cr` depende de `px` e fica dentro.
- **`px / (double)(w_ - 1)`** vai de 0,0 a 1,0 **inclusive** — o `-1` põe o último pixel
  exatamente na borda direita do viewport.
- **`num_threads(num_threads_)`** amarra o time ao valor do slider, em vez do padrão do runtime.

### 2 · Por que não há corrida, sem depender do schedule

A resposta certa **não** menciona qual thread pegou qual linha. É esta: a iteração `py` escreve
exatamente nos bytes `[ (py*w)*4 , ((py+1)*w)*4 )`. Cada `py` tem o seu intervalo, e intervalos
de `py` diferentes **não se sobrepõem** — é aritmética, não sorte:

```
 pixels_  ┌──────────┬──────────┬──────────┬──────────┬─ ⋯
          │  py = 0  │  py = 1  │  py = 2  │  py = 3  │
          └──────────┴──────────┴──────────┴──────────┴─ ⋯
           0        w*4      2*w*4      3*w*4      4*w*4
```

Como cada `py` é executado por **exatamente uma** thread, duas nunca tocam o mesmo byte. Vale
para `static`, `dynamic`, qualquer `chunk`, qualquer número de threads. Por isso não há `mutex`
aqui — e o da versão `std::thread` é, por esse critério, desnecessário (o README de lá admite).

> ⚠ **A ressalva honesta: *false sharing*.** Correção e desempenho são coisas diferentes. A
> unidade de coerência de cache é a **linha de 64 bytes**, que cobre 16 pixels RGBA. Se o começo
> de uma linha da imagem cair no *meio* de uma linha de cache, dois núcleos escrevem na mesma
> linha de cache e ficam invalidando o cache um do outro — sem corrida, mas com perda de
> desempenho.
>
> Quando isso acontece? A linha da imagem ocupa `w × 4` bytes; o problema só existe se isso não
> for múltiplo de 64, ou seja, se `w` não for múltiplo de 16. Na janela padrão,
> `880 × 4 = 3520 = 55 × 64`: cada linha começa numa fronteira de cache e **não há false sharing
> nenhum**. Redimensione para 883 de largura e passa a haver, nas fronteiras entre chunks. O
> efeito é pequeno aqui (muito cálculo, pouca escrita); em código que escreve muito e calcula
> pouco, é devastador — medido em [`../performance/`](../performance/).

### 3 · `static` contra `dynamic`, medido

**A tese do código** (comentário em `mandelbrot_renderer.cpp:60-66`) é que `dynamic` compensa.
É verdade — mas **só quando o custo é irregular**. Como cada um distribui 12 linhas entre 4:

```
schedule(static)  — decidido ANTES  │ schedule(dynamic,4) — decidido EM EXECUÇÃO
linha  0 1 2 3 4 5 6 7 8 9 10 11    │ linha  0 1 2 3 | 4 5 6 7 | 8 9 10 11 | …
thread 0 0 0 1 1 1 2 2 2 3  3  3    │ quem terminar primeiro pega o próximo bloco
```

Na vista padrão (`cx=-0.5, cy=0, scale=3`), as linhas do meio atravessam a barriga do conjunto
(`max_iter` iterações por pixel) e as de cima e de baixo pegam céu vazio, que escapa em duas ou
três voltas. Com `static`, **o quadro dura o que durar a faixa mais lenta**.

**Vista padrão** (880×640, `max_iter=256`):

| threads | `schedule(static)` | `schedule(dynamic, 4)` | ganho |
|---|---|---|---|
| 1 | 85,1 ms | 86,2 ms | — |
| 2 | 42,8 ms | 45,0 ms | — |
| 4 | 34,9 ms | **22,4 ms** | 1,56× |
| 8 | 22,2 ms | **11,3 ms** | 1,96× |
| 12 | 15,0 ms | **8,0 ms** | 1,88× |
| 24 | 19,9 ms | **11,5 ms** | 1,73× |

Agora **aproxime no bulbo** (`cx=-0.75, cy=0, scale=0.05`), onde quase toda linha é cara e o
custo fica **uniforme** — e a vantagem evapora: 79,3 ms contra 80,6 ms com 4 threads, 28,5 ms
contra 28,2 ms com 12. Empate técnico. **Moral:** `dynamic` não é "melhor"; ele **paga
sincronização para comprar balanceamento**, e sem desbalanceamento a corrigir sobra só o custo.
Regra prática: custo por iteração uniforme → `static`; irregular ou desconhecido → `dynamic`.

> ⚠ O `chunk` é um compromisso: grande demais volta a desbalancear, 1 paga sincronização por
> linha. O 4 daqui não é mágico — troque por 1, 8, 16 e meça.

### 4 · Quem decide o número de threads: três mecanismos, uma precedência

Três formas de pedir N threads: `OMP_NUM_THREADS=5 ./programa` (variável de ambiente, lida no
início), `omp_set_num_threads(3)` (em execução, vale para as próximas regiões) e
`num_threads(2)` (na própria diretiva). Medido nesta máquina de 24 threads, imprimindo
`omp_get_num_threads()` de dentro da região:

| o que estava em vigor | sem `OMP_NUM_THREADS` | com `OMP_NUM_THREADS=5` |
|---|---|---|
| nada (padrão) | 24 | **5** |
| depois de `omp_set_num_threads(3)` | **3** | **3** |
| `num_threads(2)` na diretiva | **2** | **2** |

**Precedência: `num_threads(N)` > `omp_set_num_threads(N)` > `OMP_NUM_THREADS` > threads de
hardware.** Este projeto usa a forma mais forte porque o valor vem do slider e muda por quadro.

> ⚠ `omp_get_num_threads()` **fora** de uma região paralela devolve **1**, sempre — fora dela o
> time tem mesmo uma thread só. Quem responde "quantas eu poderia usar" é `omp_get_max_threads()`
> (aqui: 24). Trocar um pelo outro é erro clássico, e não sai aviso nenhum.

### 5 · Outras diretivas que você vai encontrar

```cpp
// Região crítica: uma thread por vez. Correto, porém serializa — use por último.
#pragma omp critical
{ contador++; }

// Redução: o jeito certo de acumular (item 10 de "Antes de começar").
int soma = 0;
#pragma omp parallel for reduction(+:soma)
for (int i = 0; i < N; ++i) soma += arr[i];

// Achata os dois laços num só espaço de iteração de h*w pontos.
#pragma omp parallel for collapse(2)
for (int py = 0; py < h; ++py)
    for (int px = 0; px < w; ++px) { /* … */ }

// Barreira explícita: ninguém passa daqui antes de todo o time chegar.
#pragma omp parallel
{ calcula_parte_1();
  #pragma omp barrier
  usa_resultado_da_parte_1(); }

int n  = omp_get_max_threads();   // exige #include <omp.h>
int id = omp_get_thread_num();    // 0..n-1, identidade da thread atual
```

> ⚠ **`collapse(2)` só vale para laços perfeitamente aninhados** — nada entre o `for` externo e o
> interno. Aqui não daria sem reescrever: existe a linha do `const double ci` entre os dois. E o
> ganho seria pequeno, pois 640 linhas já dão iterações de sobra para 12 threads. `collapse`
> brilha quando o laço externo tem **menos iterações que threads** (ex.: 4 linhas, 24 threads).

> ⚠ **`#pragma omp barrier` tem regras de posição.** Num escopo serial ele é aceito (liga-se ao
> time implícito de uma thread e não faz nada), mas dentro de uma região de *worksharing* é
> **erro de compilação**: `error: barrier region may not be closely nested inside of work-sharing,
> 'loop', 'critical', 'ordered', 'master', 'masked', explicit 'task' or 'taskloop' region`. E se
> apenas **algumas** threads do time o alcançarem, o programa **trava** esperando as que não vêm.

---

## Medindo

**Ambiente**: AMD Ryzen 9 5900X (12 núcleos físicos, 24 threads SMT), Ubuntu 24.04 em WSL2,
g++ 13.3.0, `-O2 -std=c++20`, janela 880×640, `max_iter = 256`, viewport padrão. Valor
reportado: **mínimo de 7 repetições** após um quadro de aquecimento.

| threads | tempo | speedup | eficiência |
|---|---|---|---|
| 1 | 86,2 ms | 1,00× | 100% |
| 2 | 45,0 ms | 1,92× | 96% |
| 4 | 22,4 ms | 3,85× | 96% |
| 8 | 11,3 ms | 7,63× | 95% |
| 12 | **8,0 ms** | **10,8×** | **90%** |
| 24 | 11,5 ms | 7,50× | 31% |

Três leituras:
1. **A eficiência fica em ~95% até 12 threads.** Na tabela de [`../mandelbrot/`](../mandelbrot/)
   as faixas fixas derrubam a eficiência para 61% com 4 threads. A diferença é o
   `schedule(dynamic, 4)`, nada mais.
2. **De 12 para 24 o tempo PIORA** (8,0 → 11,5 ms). A máquina tem 12 núcleos **físicos**; as
   outras 12 threads são SMT, que compartilham as unidades de execução do mesmo núcleo — e um
   laço de ponto flutuante denso já as satura. ⚠ **"Mais threads" não é "mais rápido", e este é
   o contraexemplo mais barato de reproduzir: arraste o slider de 12 para 24.**
3. **O teto de Amdahl não é o gargalo.** Com 12 threads o speedup foi 10,8×, ou 90% do ideal de
   12× — e com `p = 0,99` a fórmula dá exatamente 10,8×: já estamos no teto.

### Comparação direta com a versão `std::thread`

Mesma máquina, mesmo quadro, mesmas fórmulas — só muda quem distribui o trabalho:

| threads | `std::thread` (faixas fixas) | OpenMP `dynamic, 4` |
|---|---|---|
| 1 | 85,4 ms | 86,2 ms |
| 2 | 42,9 ms | 45,0 ms |
| 4 | 35,3 ms | **22,4 ms** |
| 8 | 21,7 ms | **11,3 ms** |
| 12 | 15,4 ms | **8,0 ms** |
| 24 | 9,7 ms | 11,5 ms |

Com 1 e 2 threads são idênticos (não há o que balancear). De 4 a 12 o OpenMP é **1,6× a 1,9×
mais rápido**, por uma causa só: a versão manual dá a cada thread **uma faixa contígua**, e a
faixa central é muito mais cara. Com 24 a manual passa à frente — 24 faixas finas já ficam quase
equilibradas por acaso, e o `dynamic` paga sincronização com mais threads do que ajudam.

> ⚠ `make build` compila em `debug` (`BUILD_TYPE := debug` no `Makefile` da raiz). Os tempos da
> barra de status serão **bem maiores** que os desta tabela, medida com `-O2`. Compare sempre
> configurações iguais.

---

## Como validar

**1 · Compila e sobe.**

```bash
make build && ls -l dist/bin/mandelbrot_openmp && ./dist/bin/mandelbrot_openmp
QT_QPA_PLATFORM=offscreen timeout 5 ./dist/bin/mandelbrot_openmp; echo "saiu com $?"  # 124 = OK
```

**2 · O `-fopenmp` está mesmo ligado?** Se faltasse, o programa rodaria serial em silêncio:

```bash
ldd ./dist/bin/mandelbrot_openmp | grep -i gomp
# libgomp.so.1 => /lib/x86_64-linux-gnu/libgomp.so.1 ...
```

Prova visual: arraste **Threads:** de 1 para 12 e olhe os milissegundos na barra de status. Se o
número **não** mudar, os pragmas estão sendo ignorados.

**3 · A imagem é idêntica à do subprojeto irmão.** É o teste de correção que o par dá de graça:
mesmas fórmulas, paralelismos diferentes, logo o resultado tem de bater **byte a byte**. O
renderizador não depende de Qt, então dá para exercitá-lo fora da GUI — salve `/tmp/dump.cpp`:

```cpp
#include "mandelbrot_renderer.hpp"
#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    MandelbrotRenderer r(880, 640, argc > 2 ? std::atoi(argv[2]) : 8, 256);
    Viewport vp;                                   // padrão: cx=-0.5, cy=0, scale=3
    const std::vector<uint8_t>& px = r.render(vp); // buffer RGBA do próprio renderer
    std::FILE* f = std::fopen(argv[1], "wb");
    std::fwrite(px.data(), 1, px.size(), f);
    std::fclose(f);
}
```

```bash
g++ -std=c++20 -O2 -fopenmp -Isrc/mandelbrot_openmp \
    src/mandelbrot_openmp/mandelbrot_renderer.cpp /tmp/dump.cpp -o /tmp/dump_omp
g++ -std=c++20 -O2 -pthread  -Isrc/mandelbrot \
    src/mandelbrot/mandelbrot_renderer.cpp        /tmp/dump.cpp -o /tmp/dump_thr
/tmp/dump_omp /tmp/omp8.raw 8 ; /tmp/dump_omp /tmp/omp1.raw 1 ; /tmp/dump_thr /tmp/thr8.raw 8
md5sum /tmp/omp8.raw /tmp/omp1.raw /tmp/thr8.raw
```

Os três md5 têm de ser **iguais** (aqui: `a44348ed1e73f5016ac779a4e200161c`); se o número de
threads mudasse um único pixel, haveria corrida.

**4 · Prova de que não há data race — ThreadSanitizer.**

```bash
g++ -std=c++20 -O1 -g -fsanitize=thread -fopenmp -Isrc/mandelbrot_openmp \
    src/mandelbrot_openmp/mandelbrot_renderer.cpp /tmp/dump.cpp -o /tmp/tsan_ok
/tmp/tsan_ok /tmp/t.raw 4 2>&1 | grep -c "WARNING: ThreadSanitizer"
# 0
```

Agora **quebre de propósito**: copie `mandelbrot_renderer.cpp`, mova `int iter = 0;` para antes
do `#pragma` (o de dentro vira `iter = 0;`) e recompile. O TSan aponta a linha exata:

```
WARNING: ThreadSanitizer: data race
    #0 MandelbrotRenderer::render(Viewport const&) [clone ._omp_fn.0] mandelbrot_renderer.cpp:89
```

E a imagem quebrada aparece: com `-O0`, ~15% dos bytes saem errados **e diferentes a cada
execução**; com `-O2`, ela volta a ficar correta por acidente do otimizador. É o retrato de por
que se usa sanitizer em vez de olhômetro.

**5 · Reproduza as tabelas.** No `main` acima, renderize 7 vezes cronometrando com
`std::chrono::steady_clock`, imprima o mínimo e varie o terceiro argumento do construtor. Para
`static` contra `dynamic`, copie o renderizador e troque só a cláusula do `schedule`.

---

## Referências

Neste repositório:
- [`../mandelbrot/`](../mandelbrot/) — **o gêmeo obrigatório**: o mesmo fractal com
  `std::thread`, `mutex` e `condition_variable`, mais a explicação longa de números complexos,
  paleta HSV e do pipeline Qt
- [`../concurrency/`](../concurrency/) — threads, mutex e atômicos: o que o OpenMP esconde
- [`../concurrency2/`](../concurrency2/) — `jthread`, `latch` e `std::barrier` do C++20, a versão
  explícita da barreira implícita daqui
- [`../performance/`](../performance/) — cache, linha de cache e *false sharing*, medidos
- [`../layout/`](../layout/) — por que 64 bytes é o número que importa

Fora dele:

- **openmp.org** — o texto do padrão e o *OpenMP Application Programming Interface Examples*,
  que é onde estão os exemplos canônicos de cada cláusula
- **Manual do `libgomp`** (`gcc.gnu.org/onlinedocs/libgomp/`) — o que o GCC implementa e as
  variáveis de ambiente: `OMP_NUM_THREADS`, `OMP_SCHEDULE`, `OMP_PROC_BIND`
- **Amdahl, G. (1967)** — as três páginas que ainda impõem o teto de todo paralelismo
