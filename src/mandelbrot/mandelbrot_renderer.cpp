#include "mandelbrot_renderer.hpp"

#include <cmath>
#include <algorithm>

// ============================================================
// Paleta de cores: HSV → RGB suavizado
// Iterações altas → tons frios (azul/ciano)
// Borda do conjunto → laranja/vermelho quente
// Interior (max_iter) → preto
// ============================================================
static std::array<uint8_t,4> hsv_to_rgba(float h, float s, float v) {
    // h ∈ [0,360), s,v ∈ [0,1]
    float c  = v * s;
    float x  = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m  = v - c;
    float r1, g1, b1;
    int   hi = (int)(h / 60.0f) % 6;
    if      (hi==0){ r1=c; g1=x; b1=0; }
    else if (hi==1){ r1=x; g1=c; b1=0; }
    else if (hi==2){ r1=0; g1=c; b1=x; }
    else if (hi==3){ r1=0; g1=x; b1=c; }
    else if (hi==4){ r1=x; g1=0; b1=c; }
    else           { r1=c; g1=0; b1=x; }
    return {
        (uint8_t)((r1+m)*255),
        (uint8_t)((g1+m)*255),
        (uint8_t)((b1+m)*255),
        255
    };
}

std::array<uint8_t,4> MandelbrotRenderer::colorize(int iter, int max_iter) {
    if (iter == max_iter) return {0, 0, 0, 255}; // interior → preto

    // smooth coloring: interpola entre paleta usando log contínuo
    float t = (float)iter / (float)max_iter;

    // hue varia de 200° (azul frio) a 40° (laranja quente) conforme
    // iter aumenta, com brilho reduzido perto do limite
    float hue = std::fmod(240.0f - t * 200.0f + 360.0f, 360.0f);
    float sat = 0.85f;
    float val = t < 0.01f ? t * 100.0f : 1.0f; // fade suave no limite

    return hsv_to_rgba(hue, sat, val);
}

// ------------------------------------------------------------
MandelbrotRenderer::MandelbrotRenderer(int w, int h, int num_threads, int max_iter)
    : w_(w), h_(h), num_threads_(num_threads), max_iter_(max_iter)
{
    pixels_.resize(w_ * h_ * 4, 0);
}

void MandelbrotRenderer::resize(int w, int h) {
    w_ = w; h_ = h;
    pixels_.resize(w_ * h_ * 4, 0);
}

void MandelbrotRenderer::set_threads(int n) {
    num_threads_ = std::max(1, n);
}

void MandelbrotRenderer::set_max_iter(int n) {
    max_iter_ = std::max(16, n);
}

// ------------------------------------------------------------
// Computa tile [y0, y1).
// tile_mutex_ é adquirido por linha inteira antes de escrever —
// isso demonstra o padrão de uso do mutex mesmo que as faixas
// sejam disjuntas (sem contenção real em steady-state).
// ------------------------------------------------------------
void MandelbrotRenderer::compute_tile(int y0, int y1, const Viewport& vp) {
    // buffer local da tile (sem precisar do mutex durante o cálculo)
    std::vector<uint8_t> local((y1 - y0) * w_ * 4);

    double aspect = (double)w_ / (double)h_;
    double half_w = vp.scale * 0.5;
    double half_h = half_w / aspect;

    for (int py = y0; py < y1; ++py) {
        for (int px = 0; px < w_; ++px) {
            // pixel → plano complexo
            double cr = vp.cx + (px / (double)(w_ - 1) - 0.5) * vp.scale;
            double ci = vp.cy + (py / (double)(h_ - 1) - 0.5) * 2.0 * half_h;

            // iteração de Mandelbrot:  z_{n+1} = z_n^2 + c
            double zr = 0.0, zi = 0.0;
            int iter = 0;
            while (iter < max_iter_ && zr*zr + zi*zi < 4.0) {
                double zr2 = zr*zr - zi*zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = zr2;
                ++iter;
            }

            auto col = colorize(iter, max_iter_);
            int off = ((py - y0) * w_ + px) * 4;
            local[off+0] = col[0];
            local[off+1] = col[1];
            local[off+2] = col[2];
            local[off+3] = col[3];
        }
    }

    // --------------------------------------------------------
    // Escreve no buffer compartilhado com o mutex.
    // Cada thread escreve em faixa disjunta → sem corrida real,
    // mas o lock é educativo e garante visibilidade de memória.
    // --------------------------------------------------------
    {
        std::lock_guard<std::mutex> lock(tile_mutex_);
        int dst_off = y0 * w_ * 4;
        std::copy(local.begin(), local.end(), pixels_.begin() + dst_off);
    }

    // Sinaliza conclusão desta tile
    {
        std::lock_guard<std::mutex> lk(done_mutex_);
        ++done_count_;
    }
    done_cv_.notify_one();
}

// ------------------------------------------------------------
const std::vector<uint8_t>& MandelbrotRenderer::render(const Viewport& vp) {
    // Divide linhas em num_threads_ tiles
    std::vector<std::thread> workers;
    workers.reserve(num_threads_);

    done_count_ = 0;

    int rows_per_tile = h_ / num_threads_;
    int y0 = 0;

    for (int t = 0; t < num_threads_; ++t) {
        int y1 = (t == num_threads_ - 1) ? h_ : y0 + rows_per_tile;
        // Captura vp por valor para que cada thread tenha sua cópia
        workers.emplace_back([this, y0, y1, vp](){
            compute_tile(y0, y1, vp);
        });
        y0 = y1;
    }

    // Aguarda todos os tiles via condition_variable
    {
        std::unique_lock<std::mutex> lk(done_mutex_);
        done_cv_.wait(lk, [this]{ return done_count_ == num_threads_; });
    }

    for (auto& w : workers) w.join();

    return pixels_;
}
