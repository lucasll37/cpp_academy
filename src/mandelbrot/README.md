# mandelbrot — Fractal de Mandelbrot com Qt (Multithreaded)

## O que é este projeto?

Renderiza o **conjunto de Mandelbrot** com interface gráfica Qt, usando múltiplas threads para paralelizar o cálculo. Demonstra como dividir trabalho pesado em tiles e sincronizar threads com mutex e condition variables.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| Algoritmo de tempo de escape | Como calcular se um ponto pertence ao conjunto |
| Paralelismo por tiles | Dividir o frame em faixas, cada thread computa uma |
| Qt widget customizado | Renderizar pixels diretamente em um QWidget |
| Mapeamento de cores | Converter iterações → cor RGBA com paleta HSV |
| `condition_variable` | Sincronizar quando todos os tiles terminaram |

---

## Estrutura de arquivos

```
mandelbrot/
├── main.cpp                    ← cria QApplication e MainWindow
├── mainwindow.hpp/cpp          ← janela Qt com controles
├── mandelbrot_renderer.hpp/cpp ← computação paralela do fractal
└── meson.build
```

---

## O algoritmo de Mandelbrot

O conjunto de Mandelbrot é o conjunto de números complexos `c` para os quais a sequência:

```
z₀ = 0
z_{n+1} = z_n² + c
```

**não diverge** (|z_n| permanece ≤ 2 para todo n).

```cpp
// Para cada pixel (px, py) na janela, mapeamos para c = (real, imag)
double real = (px - w/2.0) / (w/2.0) * scale + cx;
double imag = (py - h/2.0) / (h/2.0) * scale + cy;

std::complex<double> c(real, imag);
std::complex<double> z(0, 0);

int iter = 0;
while (std::abs(z) <= 2.0 && iter < max_iter) {
    z = z * z + c;  // z_{n+1} = z_n² + c
    iter++;
}

// iter == max_iter → ponto provavelmente NO conjunto (preto)
// iter < max_iter  → escapou → colorir pela velocidade de escape
```

---

## Paralelismo por tiles

```cpp
// Divide o frame em num_threads faixas horizontais
void MandelbrotRenderer::render(const Viewport& vp) {
    int tile_height = h_ / num_threads_;
    done_count_ = 0;

    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads_; ++i) {
        int y0 = i * tile_height;
        int y1 = (i == num_threads_ - 1) ? h_ : y0 + tile_height;

        workers.emplace_back([this, y0, y1, &vp]{
            compute_tile(y0, y1, vp);
            // Sinaliza que este tile terminou
            {
                std::lock_guard<std::mutex> lock(done_mutex_);
                done_count_++;
            }
            done_cv_.notify_one();
        });
    }

    // Aguarda todos os tiles terminarem
    {
        std::unique_lock<std::mutex> lock(done_mutex_);
        done_cv_.wait(lock, [this]{ return done_count_ == num_threads_; });
    }

    for (auto& t : workers) t.join();
    return pixels_; // buffer RGBA pronto
}
```

---

## Mapeamento de cores

```cpp
std::array<uint8_t,4> MandelbrotRenderer::colorize(int iter, int max_iter) {
    if (iter == max_iter) return {0, 0, 0, 255}; // dentro do conjunto → preto

    // Suaviza a borda usando logaritmo (smooth coloring)
    double t = static_cast<double>(iter) / max_iter;

    // Paleta cíclica baseada em HSV:
    double hue = fmod(t * 360.0 * 5.0, 360.0); // cicla 5x
    // HSV → RGB com saturação e valor fixos

    // Resultado: gradiente suave de azul → verde → vermelho → violeta
    return {r, g, b, 255};
}
```

---

## `Viewport` — a janela do plano complexo

```cpp
struct Viewport {
    double cx    = -0.5; // centro no eixo real
    double cy    =  0.0; // centro no eixo imaginário
    double scale = 3.0;  // largura total visível do plano complexo
};

// Mapeamento pixel → complexo:
// real = (px - w/2) / (w/2) * (scale/2) + cx
// imag = (py - h/2) / (h/2) * (scale/2) + cy
```

Para fazer zoom, basta reduzir `scale` e centralizar em `cx`, `cy`.

---

## Interface Qt

```
MainWindow
    │
    ├── SFMLWidget (ou QWidget) ← área de renderização
    ├── Spinner: max_iter        ← controla detalhes
    ├── Spinner: threads         ← número de threads
    └── Botões: zoom in/out, reset
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja mandelbrot
./bin/mandelbrot
```

---

## Diferença para `mandelbrot_openmp`

| Aspecto | `mandelbrot` | `mandelbrot_openmp` |
|---|---|---|
| Paralelismo | `std::thread` manual | OpenMP `#pragma omp parallel for` |
| Controle | Explícito (você gerencia threads) | Implícito (compilador paraleliza) |
| Complexidade | Maior | Menor |
| Flexibilidade | Total | Limitada ao padrão OpenMP |

---

## Dependências

- `Qt6` (ou Qt5) — framework GUI
- `fmt` — formatação de saída
