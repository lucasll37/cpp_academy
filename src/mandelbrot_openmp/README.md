# mandelbrot_openmp — Fractal de Mandelbrot com OpenMP

## O que é este projeto?

Renderiza o **conjunto de Mandelbrot** usando **OpenMP** para paralelizar o cálculo entre múltiplos núcleos da CPU. Demonstra como adicionar paralelismo shared-memory com uma única diretiva de compilador.

---

## Conceitos ensinados

| Conceito | Descrição |
|---|---|
| `#pragma omp parallel for` | Paraleliza automaticamente um loop for |
| `schedule(dynamic)` | Distribui iterações dinamicamente (balanceamento de carga) |
| `schedule(static)` | Distribui iterações estaticamente (previsível) |
| `num_threads(N)` | Controla o número de threads |
| Reduction | Acumular resultado de múltiplas threads com `reduction(+:soma)` |

---

## Estrutura de arquivos

```
mandelbrot_openmp/
├── main.cpp                    ← QApplication + MainWindow
├── mainwindow.hpp/cpp          ← janela Qt com controles OpenMP
├── mandelbrot_renderer.hpp/cpp ← renderização com OpenMP
└── meson.build
```

---

## OpenMP vs `std::thread` manual

| Aspecto | OpenMP | `std::thread` |
|---|---|---|
| Código necessário | 1 linha (`#pragma`) | Dezenas de linhas |
| Controle fino | Limitado | Total |
| Portabilidade | Alta (padrão) | Alta (STL) |
| Overhead | Baixo | Variável |
| Balanceamento | `dynamic` scheduling | Manual |

---

## O `#pragma` central

```cpp
void MandelbrotRenderer::render(const Viewport& vp) {
    // Esta diretiva paraleliza o loop entre os núcleos disponíveis.
    // Cada iteração (linha y) é executada por uma thread diferente.
    // As threads são gerenciadas pelo runtime OpenMP — não por você.

    #pragma omp parallel for schedule(dynamic, 4) num_threads(num_threads_)
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            // Calcula qual ponto complexo este pixel representa
            double real = cx_ + (x - w_/2.0) * (scale_ / w_);
            double imag = cy_ + (y - h_/2.0) * (scale_ / h_);

            // Iterações de Mandelbrot
            std::complex<double> c(real, imag), z(0, 0);
            int iter = 0;
            while (std::abs(z) <= 2.0 && iter < max_iter_) {
                z = z * z + c;
                ++iter;
            }

            // Sem race condition: cada thread escreve em pixels_ diferentes
            // (y diferente → posição diferente no buffer)
            auto color = colorize(iter, max_iter_);
            int idx = (y * w_ + x) * 4;
            pixels_[idx + 0] = color[0]; // R
            pixels_[idx + 1] = color[1]; // G
            pixels_[idx + 2] = color[2]; // B
            pixels_[idx + 3] = color[3]; // A
        }
    }
}
```

### Por que não há race condition?

Cada thread escreve em um intervalo diferente do buffer `pixels_`:
- Thread 0 → linhas 0..99
- Thread 1 → linhas 100..199
- Thread 2 → linhas 200..299
...

Não há sobreposição → sem mutex necessário.

---

## Scheduling — estratégias de distribuição

### `schedule(static)` — distribuição previsível

```
Linhas: 0 1 2 3 4 5 6 7 8 9 10 11 ...
Thread: 0 0 0 1 1 1 2 2 2 3  3  3  ...  (chunk = h/N)
```

Boa quando cada iteração tem custo similar. O Mandelbrot tem custo variável (linhas no centro do conjunto são mais lentas) → `static` causa desbalanceamento.

### `schedule(dynamic, chunk)` — distribuição dinâmica

```
Cada thread pega o próximo chunk disponível quando terminar o atual.
Chunk menor → melhor balanceamento, maior overhead de sincronização.
Chunk=4 é um bom compromisso para Mandelbrot.
```

---

## Medindo o speedup

```cpp
auto t0 = std::chrono::high_resolution_clock::now();

// Com 1 thread
#pragma omp parallel for num_threads(1)
for (int y = 0; y < h_; ++y) { /* ... */ }
auto single = duration(now - t0); // ex: 400ms

// Com N threads
#pragma omp parallel for num_threads(omp_get_max_threads())
for (int y = 0; y < h_; ++y) { /* ... */ }
auto multi = duration(now - t0);  // ex: 75ms em 8 cores

// Speedup teórico de Amdahl: S = 1 / (1 - p + p/N)
// Onde p = fração paralelizável (~0.99 para Mandelbrot)
```

---

## Outras diretivas OpenMP úteis

```cpp
// Seção crítica (mutual exclusion)
#pragma omp critical
{ contador++; }

// Redução paralela (soma distribuída)
int soma = 0;
#pragma omp parallel for reduction(+:soma)
for (int i = 0; i < N; ++i) soma += arr[i];

// Barreira — todas as threads esperam aqui
#pragma omp barrier

// Número de threads disponíveis em runtime
int n = omp_get_max_threads();
int thread_id = omp_get_thread_num();
```

---

## Como compilar e executar

```bash
meson setup dist
cd dist && ninja mandelbrot_openmp
./bin/mandelbrot_openmp
```

**Requer**: OpenMP suporte no compilador (flag `-fopenmp`, configurado no meson.build).

---

## Dependências

- `Qt6` (ou Qt5) — framework GUI
- OpenMP — paralelismo shared-memory (parte do GCC/Clang)
- `fmt` — formatação
